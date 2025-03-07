#ifndef __OKAYLIB_ALLOCATORS_SLAB_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_SLAB_ALLOCATOR_H__

#include "okay/allocators/block_allocator.h"
#include "okay/containers/array.h"

namespace ok {

namespace slab_allocator {
struct block_allocator_description
{
    size_t initial_elems;
    size_t blocksize;
    size_t alignment;
};

struct with_blocks;
} // namespace slab_allocator

template <typename allocator_impl_t, size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
class slab_allocator_t : public ok::allocator_t
{
  private:
    struct members_t
    {
        array_t<block_allocator_t<allocator_impl_t>,
                sizeof...(additional_block_sizes) + 2>
            allocators;
    } m;

  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_clear;

    friend ok::slab_allocator::with_blocks;

    constexpr explicit slab_allocator_t() = delete;

    constexpr slab_allocator_t(slab_allocator_t&& other)
        OKAYLIB_NOEXCEPT = default;
    constexpr slab_allocator_t&
    operator=(slab_allocator_t&& other) OKAYLIB_NOEXCEPT = default;

    slab_allocator_t& operator=(const slab_allocator_t&) = delete;
    slab_allocator_t(const slab_allocator_t&) = delete;

    inline ~slab_allocator_t() = default;

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final;

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
};

template <typename allocator_impl_t, size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
[[nodiscard]] inline alloc::result_t<bytes_t> slab_allocator_t<
    allocator_impl_t, block_size_a, block_size_b,
    additional_block_sizes...>::impl_allocate(const alloc::request_t&)
    OKAYLIB_NOEXCEPT
{
}

template <typename allocator_impl_t, size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
inline void
slab_allocator_t<allocator_impl_t, block_size_a, block_size_b,
                 additional_block_sizes...>::impl_clear() OKAYLIB_NOEXCEPT
{
}

template <typename allocator_impl_t, size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
inline void
slab_allocator_t<allocator_impl_t, block_size_a, block_size_b,
                 additional_block_sizes...>::impl_deallocate(bytes_t)
    OKAYLIB_NOEXCEPT
{
}

template <typename allocator_impl_t, size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
[[nodiscard]] inline alloc::result_t<bytes_t>
slab_allocator_t<allocator_impl_t, block_size_a, block_size_b,
                 additional_block_sizes...>::
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT
{
}

} // namespace ok

#endif
