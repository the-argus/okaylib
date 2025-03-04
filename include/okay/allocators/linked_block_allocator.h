#ifndef __OKAYLIB_ALLOCATORS_LINKED_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_LINKED_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/status.h"

namespace ok {
struct linked_blockpool_allocator;

class linked_blockpool_allocator_t : public ok::allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_expand_back | alloc::feature_flags::can_clear;

    friend struct linked_blockpool_allocator;

    linked_blockpool_allocator_t&
    operator=(const linked_blockpool_allocator_t&) = delete;
    linked_blockpool_allocator_t(const linked_blockpool_allocator_t&) = delete;

    constexpr linked_blockpool_allocator_t&
    operator=(linked_blockpool_allocator_t&& other) OKAYLIB_NOEXCEPT
    {
        destroy();
        m = other.m;
        other.m.backing = nullptr;
        // not necessary but makes sure use-after-move is a segfault and not
        // potentially silent
        other.m.last_pool = nullptr;
        return *this;
    }

    constexpr linked_blockpool_allocator_t(linked_blockpool_allocator_t&& other)
        OKAYLIB_NOEXCEPT : m(other.m)
    {
        other.m.backing = nullptr;
        other.m.last_pool = nullptr;
    }

    inline ~linked_blockpool_allocator_t() { destroy(); }

  protected:
    [[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final
    {
        __ok_assert(false,
                    "linked_blockpool_allocator_t cannot clear, this may cause "
                    "memory leaks. check feature() before calling clear?");
    }

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final
    {
        return alloc::error::unsupported;
    }

  private:
    constexpr status<alloc::error> alloc_new_blockpool() OKAYLIB_NOEXCEPT;

    constexpr void destroy() noexcept
    {
        if (!m.backing) {
            return;
        }

        auto* pool_iter = m.last_pool;
        while (pool_iter) {
            auto* prev = pool_iter->prev;

            m.backing->deallocate(
                ok::raw_slice(*reinterpret_cast<uint8_t*>(pool_iter),
                              sizeof(*pool_iter) + pool_iter->size));

            pool_iter = prev;
        }
    }

    // a block of blocks
    // NOTE: not using opt<&> to potentially avoid having to #include opt.h
    // in the future
    struct pool_t
    {
        pool_t* prev;
        size_t size;
        uint8_t blocks[];

        constexpr bytes_t as_slice() noexcept
        {
            return ok::raw_slice(blocks[0], size);
        };
        constexpr slice<const uint8_t> as_slice() const noexcept
        {
            return ok::raw_slice(blocks[0], size);
        };
    };

    struct free_block_t
    {
        free_block_t* prev = nullptr;
    };

    struct M
    {
        // most recently allocated pool, never nullptr
        pool_t* last_pool;
        // information about induvidual blocks, to refuse allocations
        size_t blocksize;
        size_t minimum_alignment;
        // backing allocator, used to create new blockpools
        allocator_t* backing;
        // first free block, returned when allocate is called
        free_block_t* free_head;
        // how much to increase each new blockpool's block count by, with each
        // new one allocated. usually 2.0
        float growth_factor;
    } m;

    constexpr linked_blockpool_allocator_t(M&& members) noexcept : m(members) {}

    constexpr bool pool_contains(const pool_t& pool,
                                 slice<const uint8_t> bytes) const
    {
        return ok_memcontains(.outer = pool.as_slice(), .inner = bytes);
    }
};

constexpr status<alloc::error>
linked_blockpool_allocator_t::alloc_new_blockpool() OKAYLIB_NOEXCEPT
{
    __ok_internal_assert(m.last_pool);
    const size_t next_size =
        (m.last_pool->size * m.blocksize) * m.growth_factor;

    auto allocation_result = m.backing->allocate({
        .num_bytes = next_size,
        .alignment = m.minimum_alignment,
        .flags = alloc::flags::leave_nonzeroed,
    });

    if (!allocation_result.okay()) [[unlikely]]
        return allocation_result.err();

    const bytes_t allocation =
        allocation_result.release_ref().as_undefined().leave_undefined();

    // try to help folks implementing their own allocators
    __ok_assert(allocation.size() >= next_size,
                "Backing allocator for linked_block_allocator_t did not "
                "return the expected amount of memory.");
    __ok_assert((uintptr_t(allocation.data()) % m.minimum_alignment) != 0,
                "Backing allocator for linked_block_allocator_t gave "
                "misaligned memory.");

    pool_t* const new_pool = reinterpret_cast<pool_t*>(allocation.data());
    new_pool->prev = m.last_pool;
    new_pool->size = (allocation.size() - sizeof(pool_t)) / m.blocksize;
    m.last_pool = new_pool;

    __ok_internal_assert(!m.free_head);
    free_block_t* block_free_list_iter = nullptr;

    // iterate in reverse so that the first item on the free list will be
    // the first thing in this new blockpool
    for (int64_t i = new_pool->size - 1; i >= 0; --i) {
        auto* block_start =
            reinterpret_cast<free_block_t*>(&new_pool->blocks[i * m.blocksize]);
        __ok_internal_assert(uintptr_t(block_start) % m.minimum_alignment);

        // write free block linked list entry
        *block_start = {.prev = block_free_list_iter};
        block_free_list_iter = block_start;
    }
    m.free_head = block_free_list_iter;
    return alloc::error::okay;
}

[[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
linked_blockpool_allocator_t::impl_allocate(const alloc::request_t& request)
    OKAYLIB_NOEXCEPT
{
    if (request.num_bytes > m.blocksize ||
        request.alignment > m.minimum_alignment) [[unlikely]] {
        return alloc::error::unsupported;
    }

    auto* const free = m.free_head;

    // allocate new blockpool if needed
    if (!free) [[unlikely]] {
        const auto memstatus = this->alloc_new_blockpool();
        if (!memstatus.okay()) [[unlikely]] {
            return memstatus.err();
        }
    }

    __ok_internal_assert(m.free_head);
    m.free_head = free->prev;

    if (!(request.flags & alloc::flags::leave_nonzeroed)) {
        // we always give back the full block, so zero the full block
        std::memset(free, 0, m.blocksize);
        return maybe_defined_memory_t(
            ok::raw_slice(*reinterpret_cast<uint8_t*>(free), m.blocksize));
    }

    return undefined_memory_t(*reinterpret_cast<uint8_t*>(free), m.blocksize);
}

inline void
linked_blockpool_allocator_t::impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT
{
#ifndef NDEBUG
#define __ok_bytes_within_linked_blockpool_check(bytes_sym)                 \
    pool_t* iter = m.last_pool;                                             \
    bool found = false;                                                     \
    while (iter) {                                                          \
        if (pool_contains(*iter, bytes_sym)) {                              \
            found = true;                                                   \
            break;                                                          \
        }                                                                   \
        iter = iter->prev;                                                  \
    }                                                                       \
    if (!found) {                                                           \
        __ok_abort(                                                         \
            "Attempt to operate on some bytes with a "                      \
            "linked_blockpool_allocator but the bytes were not fully "      \
            "contained within a memory pool belonging to that allocator."); \
    }
#else
#define __ok_bytes_within_linked_blockpool_check(bytes_sym)
#endif
    __ok_bytes_within_linked_blockpool_check(bytes);
    __ok_assert(uintptr_t(bytes.data()) % m.minimum_alignment == 0,
                "Attempt to deallocate pointer from linked_blockpool_allocator "
                "which does not appear to have come from that allocator.");

    free_block_t* new_free = reinterpret_cast<free_block_t*>(bytes.data());
#ifndef NDEBUG
    // hopefully catch some use-after-frees this way
    std::memset(new_free, 111, m.blocksize);
#endif
    new_free->prev = m.free_head;
    m.free_head = new_free;
}

[[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
linked_blockpool_allocator_t::impl_reallocate(
    const alloc::reallocate_request_t& request) OKAYLIB_NOEXCEPT
{
    __ok_bytes_within_linked_blockpool_check(request.memory);
#undef __ok_bytes_within_linked_blockpool_check
    __ok_assert(uintptr_t(request.memory.data()) % m.minimum_alignment == 0,
                "Attempt to reallocate pointer from linked_blockpool_allocator "
                "which does not appear to have come from that allocator.");
    if (request.new_size_bytes > m.blocksize) {
        return alloc::error::unsupported;
    }

    const size_t newsize =
        request.preferred_size_bytes == 0
            ? request.new_size_bytes
            : ok::min(request.preferred_size_bytes, m.blocksize);

    if (!(request.flags & alloc::flags::leave_nonzeroed)) {
        std::memset(request.memory.data() + request.memory.size(), 0,
                    newsize - request.memory.size());
    }

    return ok::raw_slice(*request.memory.data(), newsize);
}

struct linked_blockpool_allocator
{
    linked_blockpool_allocator() = delete;
    struct options
    {
        using associated_type = linked_blockpool_allocator_t;

        size_t num_bytes_per_block;
        size_t minimum_alignment = ok::alloc::default_align;
        size_t num_blocks_in_first_pool; // must be > 0
        float pool_growth_factor = 2.0f; // must be >= 1.0
        ok::allocator_t& backing_allocator;
    };

    static inline alloc::result_t<owning_ref<linked_blockpool_allocator_t>>
    construct(
        detail::uninitialized_storage_t<linked_blockpool_allocator_t>& uninit,
        const options& options)
    {
        const size_t actual_blocksize =
            ok::max(options.num_bytes_per_block,
                    sizeof(linked_blockpool_allocator_t::free_block_t));
        const size_t actual_minimum_alignment =
            ok::max(options.minimum_alignment,
                    alignof(linked_blockpool_allocator_t::pool_t));

        __ok_assert(options.num_blocks_in_first_pool > 0,
                    "Bad params to linked_blockpool_allocator::construct()");
        __ok_assert(options.pool_growth_factor >= 1.0f,
                    "Bad params to linked_blockpool_allocator::construct()");

        auto allocation_result = options.backing_allocator.allocate({
            .num_bytes = actual_blocksize * options.num_blocks_in_first_pool,
            .alignment = actual_minimum_alignment,
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!allocation_result.okay()) [[unlikely]]
            return allocation_result.err();

        bytes_t allocation =
            allocation_result.release_ref().as_undefined().leave_undefined();

        // initialize the linked list of free blocks
        linked_blockpool_allocator_t::free_block_t* free_list_iter = nullptr;
        for (int64_t i = (allocation.size() / actual_blocksize) - 1; i >= 0;
             --i) {
            auto* block =
                reinterpret_cast<linked_blockpool_allocator_t::free_block_t*>(
                    &allocation[i * actual_blocksize]);
            *block = {.prev = free_list_iter};
            free_list_iter = block;
        }
        __ok_internal_assert(free_list_iter);

        uninit.value = linked_blockpool_allocator_t::M{
            .last_pool =
                reinterpret_cast<linked_blockpool_allocator_t::pool_t*>(
                    allocation.data()),
            .blocksize = actual_blocksize,
            .minimum_alignment = actual_minimum_alignment,
            .backing = ok::addressof(options.backing_allocator),
            .free_head = free_list_iter,
            .growth_factor = options.pool_growth_factor,
        };

        return uninit.value;
    }
};

} // namespace ok

#endif
