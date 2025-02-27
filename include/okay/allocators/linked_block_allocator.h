#ifndef __OKAYLIB_ALLOCATORS_LINKED_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_LINKED_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"

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
        m = M{
            .first_pool = std::exchange(other.m.first_pool, nullptr),
            .blocksize = other.m.blocksize,
            .metablocksize = other.m.metablocksize,
            .backing = std::exchange(other.m.backing, nullptr),
        };
        return *this;
    }

    constexpr linked_blockpool_allocator_t(linked_blockpool_allocator_t&& other)
        OKAYLIB_NOEXCEPT : m(other.m)
    {
        other.m.backing = nullptr;
        // not really necessary but makes mistakes a little more obvious
        other.m.first_pool = nullptr;
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
    constexpr void destroy() noexcept
    {
        if (!m.backing) {
            return;
        }

        auto* pool_iter = m.first_pool;
        while (pool_iter) {
            auto* next = pool_iter->next;

            m.backing->deallocate(
                ok::raw_slice(*reinterpret_cast<uint8_t*>(pool_iter),
                              sizeof(*pool_iter) + pool_iter->size));

            pool_iter = next;
        }
    }

    // a block of blocks
    // NOTE: not using opt<&> to potentially avoid having to #include opt.h
    // in the future
    struct pool_t
    {
        pool_t* next;
        size_t size;
        uint8_t blocks[];

        constexpr bytes_t as_slice() noexcept
        {
            return ok::raw_slice(blocks[0], size);
        };
        constexpr slice_t<const uint8_t> as_slice() const noexcept
        {
            return ok::raw_slice(blocks[0], size);
        };
    };

    struct free_block_t
    {
        free_block_t* next = nullptr;
    };

    struct M
    {
        pool_t* first_pool = nullptr;
        size_t blocksize;
        size_t metablocksize;
        allocator_t* backing = nullptr;
        free_block_t* free_head = nullptr;
    } m;

    constexpr linked_blockpool_allocator_t(M&& members) noexcept : m(members) {}

    constexpr bool pool_contains(const pool_t& pool,
                                 slice_t<const uint8_t> bytes) const
    {
        return ok_memcontains(.outer = pool.as_slice(), .inner = bytes);
    }
};

[[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
linked_blockpool_allocator_t::impl_allocate(const alloc::request_t&)
    OKAYLIB_NOEXCEPT
{
}

inline void
linked_blockpool_allocator_t::impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT
{
}

[[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
linked_blockpool_allocator_t::impl_reallocate(
    const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT
{
}

struct linked_blockpool_allocator
{
    linked_blockpool_allocator() = delete;
    struct options
    {
        using associated_type = linked_blockpool_allocator_t;

        size_t num_bytes_per_block;
        size_t minimum_alignment = ok::alloc::default_align;
        size_t num_blocks_in_first_pool;
        float pool_growth_factor = 2.0f;
        ok::allocator_t& backing_allocator;
    };

    constexpr static alloc::result_t<owning_ref<linked_blockpool_allocator_t>>
    construct(
        detail::uninitialized_storage_t<linked_blockpool_allocator_t>& uninit,
        const options& options)
    {
        auto allocation_result = options.backing_allocator.allocate({
            .num_bytes =
                options.num_bytes_per_block * options.num_blocks_in_first_pool,
            .alignment = options.minimum_alignment,
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!allocation_result.okay()) [[unlikely]]
            return allocation_result.err();

        uninit.value = linked_blockpool_allocator_t::M{
            .first_pool = allocation_result.release().data_maybe_defined(),
        };
    }
} // namespace linked_block_allocator

} // namespace ok

#endif
