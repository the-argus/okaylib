#ifndef __OKAYLIB_ALLOCATORS_LINKED_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_LINKED_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/math/rounding.h"
#include "okay/status.h"

namespace ok {

namespace linked_blockpool_allocator {
struct start_with_one_pool_t;
}

template <typename allocator_impl_t>
class linked_blockpool_allocator_t : public ok::allocator_t
{
  public:
    static_assert(detail::is_derived_from_v<allocator_impl_t, ok::allocator_t>,
                  "Invalid type given to linked_blockpool_allocator_t for "
                  "backing allocator");

    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_expand_back;

    linked_blockpool_allocator_t&
    operator=(const linked_blockpool_allocator_t&) = delete;
    linked_blockpool_allocator_t(const linked_blockpool_allocator_t&) = delete;

    friend class linked_blockpool_allocator::start_with_one_pool_t;

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

    constexpr size_t block_size() const noexcept { return m.blocksize; }
    constexpr size_t block_align() const noexcept
    {
        return m.minimum_alignment;
    }

    inline ~linked_blockpool_allocator_t() { destroy(); }

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final
    {
        __ok_assert(false,
                    "linked_blockpool_allocator_t cannot clear, this may cause "
                    "memory leaks. check features() before calling clear?");
    }

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<bytes_t>
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

            m.backing->deallocate(ok::raw_slice(
                *reinterpret_cast<uint8_t*>(pool_iter), pool_iter->byte_size));

            pool_iter = prev;
        }
    }

    // a block of blocks
    // NOTE: not using opt<&> to potentially avoid having to #include opt.h
    // in the future
    struct pool_t
    {
        pool_t* prev;
        size_t num_blocks;
        // total size of this pool, including its blocks and padding, in bytes
        size_t byte_size;
        size_t offset; // num bytes into `bytes` that blocks actually start
        uint8_t bytes[];

        // takes an uninitialized pool sitting at the start of a buffer and
        // initializes it, given the buffer and the alignment needed by the
        // blocks (determines where blocks start). Returns false if it is unable
        // to fit any blocks within the given buffer
        [[nodiscard]] constexpr bool init_in_buffer(bytes_t containing,
                                                    pool_t* prev,
                                                    size_t block_min_alignment,
                                                    size_t block_size) noexcept
        {
            __ok_internal_assert(static_cast<void*>(containing.data()) ==
                                 static_cast<void*>(this));
            __ok_internal_assert(containing.size() > sizeof(pool_t));

            size_t remaining_space = containing.size();
            void* start = bytes;

            if (!std::align(block_min_alignment, block_size, start,
                            remaining_space)) {
                return false;
            }

            this->byte_size = containing.size();
            this->offset = static_cast<uint8_t*>(start) - bytes;
            this->num_blocks = remaining_space / block_size;
            this->prev = prev;

            return true;
        }

        constexpr uint8_t* blocks_start() noexcept { return bytes + offset; }
        constexpr const uint8_t* blocks_start() const noexcept
        {
            return bytes + offset;
        }
        constexpr bytes_t as_slice(size_t blocksize) noexcept
        {
            return ok::raw_slice(*blocks_start(), blocksize * num_blocks);
        };
        constexpr slice<const uint8_t> as_slice(size_t blocksize) const noexcept
        {
            return ok::raw_slice(*blocks_start(), blocksize * num_blocks);
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
        // this being null indicates moved allocator
        allocator_impl_t* backing;
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
        return ok_memcontains(.outer = pool.as_slice(m.blocksize),
                              .inner = bytes);
    }
};

template <typename allocator_impl_t>
constexpr status<alloc::error>
linked_blockpool_allocator_t<allocator_impl_t>::alloc_new_blockpool()
    OKAYLIB_NOEXCEPT
{
    __ok_internal_assert(m.last_pool);
    const size_t next_size = (m.last_pool->byte_size) * m.growth_factor;

    auto allocation_result = m.backing->allocate({
        .num_bytes = next_size,
        .alignment = m.minimum_alignment,
        .flags = alloc::flags::leave_nonzeroed,
    });

    if (!allocation_result.okay()) [[unlikely]]
        return allocation_result.err();

    const bytes_t allocation = allocation_result.release_ref();

    // try to help folks implementing their own allocators
    __ok_assert(allocation.size() >= next_size,
                "Backing allocator for linked_block_allocator_t did not "
                "return the expected amount of memory.");
    __ok_assert((uintptr_t(allocation.data()) % m.minimum_alignment) == 0,
                "Backing allocator for linked_block_allocator_t gave "
                "misaligned memory.");

    pool_t* const new_pool = reinterpret_cast<pool_t*>(allocation.data());
    bool success = new_pool->init_in_buffer(allocation, m.last_pool,
                                            m.minimum_alignment, m.blocksize);
    if (!success) [[unlikely]] {
        // bad pool size, cant fit any blocks in it?
        __ok_internal_assert(false); // TODO: is this error necessary or can we
                                     // guarantee it never happens
        return alloc::error::oom;
    }
    m.last_pool = new_pool;

    __ok_internal_assert(!m.free_head);
    free_block_t* block_free_list_iter = nullptr;

    // iterate in reverse so that the first item on the free list will be
    // the first thing in this new blockpool
    for (int64_t i = int64_t(new_pool->num_blocks) - 1; i >= 0; --i) {
        auto* block_start = reinterpret_cast<free_block_t*>(
            &new_pool->blocks_start()[i * m.blocksize]);
        __ok_internal_assert(uintptr_t(block_start) % m.minimum_alignment);

        // write free block linked list entry
        *block_start = {.prev = block_free_list_iter};
        block_free_list_iter = block_start;
    }
    m.free_head = block_free_list_iter;
    return alloc::error::okay;
}

template <typename allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
linked_blockpool_allocator_t<allocator_impl_t>::impl_allocate(
    const alloc::request_t& request) OKAYLIB_NOEXCEPT
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
    }

    return ok::raw_slice(*reinterpret_cast<uint8_t*>(free), m.blocksize);
}

template <typename allocator_impl_t>
inline void linked_blockpool_allocator_t<allocator_impl_t>::impl_deallocate(
    bytes_t bytes) OKAYLIB_NOEXCEPT
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

template <typename allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
linked_blockpool_allocator_t<allocator_impl_t>::impl_reallocate(
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

namespace linked_blockpool_allocator {
struct options_t
{
    size_t num_bytes_per_block;
    size_t minimum_alignment = ok::alloc::default_align;
    size_t num_blocks_in_first_pool; // must be > 0
    float pool_growth_factor = 2.0f; // must be >= 1.0
};

struct start_with_one_pool_t
{
    template <typename allocator_impl_t_ref, typename...>
    using associated_type = linked_blockpool_allocator_t<
        std::remove_reference_t<allocator_impl_t_ref>>;

    template <typename allocator_impl_t>
    [[nodiscard]] constexpr auto
    operator()(allocator_impl_t& allocator,
               const options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <typename allocator_impl_t>
    constexpr status<alloc::error>
    make_into_uninit(linked_blockpool_allocator_t<allocator_impl_t>& uninit,
                     allocator_impl_t& allocator,
                     const options_t& options) const OKAYLIB_NOEXCEPT
    {
        using free_block_t = typename linked_blockpool_allocator_t<
            allocator_impl_t>::free_block_t;
        using pool_t =
            typename linked_blockpool_allocator_t<allocator_impl_t>::pool_t;
        const size_t actual_minimum_alignment =
            ok::max(options.minimum_alignment, alignof(free_block_t));
        const size_t actual_blocksize = runtime_round_up_to_multiple_of(
            actual_minimum_alignment,
            ok::max(options.num_bytes_per_block, sizeof(free_block_t)));

        __ok_assert(options.num_blocks_in_first_pool > 0,
                    "Bad params to linked_blockpool_allocator::construct()");
        __ok_assert(options.pool_growth_factor >= 1.0f,
                    "Bad params to linked_blockpool_allocator::construct()");

        auto allocation_result = allocator.allocate({
            .num_bytes = sizeof(pool_t) + actual_minimum_alignment +
                         (actual_blocksize * options.num_blocks_in_first_pool),
            .alignment = ok::max(actual_minimum_alignment, alignof(pool_t)),
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!allocation_result.okay()) [[unlikely]]
            return allocation_result.err();

        bytes_t allocation = allocation_result.release();

        auto* pool = reinterpret_cast<pool_t*>(allocation.data());
        const bool success = pool->init_in_buffer(
            allocation, nullptr, actual_minimum_alignment, actual_blocksize);
        if (!success) [[unlikely]] {
            // bad buffer size, cant fit anything in it
            __ok_internal_assert(
                false); // TODO: can this be guaranteed avoided? required num
                        // blocks is always > 0
            return alloc::error::oom;
        }

        __ok_internal_assert(pool->offset < actual_minimum_alignment);

        // initialize the linked list of free blocks
        free_block_t* free_list_iter = nullptr;
        for (int64_t i = pool->num_blocks - 1; i >= 0; --i) {
            auto* block = reinterpret_cast<free_block_t*>(
                &pool->blocks_start()[i * actual_blocksize]);
            *block = {.prev = free_list_iter};
            free_list_iter = block;
        }
        __ok_internal_assert(free_list_iter);

        __ok_internal_assert(pool->num_blocks >=
                             options.num_blocks_in_first_pool);

        using return_type = linked_blockpool_allocator_t<allocator_impl_t>;
        using M = typename linked_blockpool_allocator_t<allocator_impl_t>::M;
        new (ok::addressof(uninit)) return_type(M{
            .last_pool = pool,
            .blocksize = actual_blocksize,
            .minimum_alignment = actual_minimum_alignment,
            .backing = ok::addressof(allocator),
            .free_head = free_list_iter,
            .growth_factor = options.pool_growth_factor,
        });

        return alloc::error::okay;
    }
};

inline constexpr start_with_one_pool_t start_with_one_pool;

} // namespace linked_blockpool_allocator
} // namespace ok

#endif
