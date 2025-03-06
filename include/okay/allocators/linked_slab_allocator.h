#ifndef __OKAYLIB_ALLOCATORS_LINKED_SLAB_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_LINKED_SLAB_ALLOCATOR_H__

#include "okay/allocators/linked_blockpool_allocator.h"
#include "okay/containers/array.h"

namespace ok {

namespace linked_slab_allocator::detail {

inline constexpr size_t default_block_size_a = 64;
inline constexpr size_t default_block_size_b = 256;

template <size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
struct start_with_one_pool_per_blocksize_t;
} // namespace linked_slab_allocator::detail

template <size_t block_size_a = linked_slab_allocator::detail::default_block_size_a,
          size_t block_size_b = linked_slab_allocator::detail::default_block_size_b,
          size_t... additional_block_sizes>
class linked_slab_allocator_t : public ok::allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_expand_back;

    friend struct linked_slab_allocator::detail::start_with_one_pool_per_blocksize_t<
        block_size_a, block_size_b, additional_block_sizes...>;

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT final
    {
        linked_blockpool_allocator_t* selected = nullptr;
        for (size_t i = 0; i < m_blockpools.size(); ++i) {
            linked_blockpool_allocator_t& blockpool = m_blockpools[i];
            if (blockpool.block_size() > request.num_bytes &&
                blockpool.block_align() > request.alignment) {
                selected = ok::addressof(blockpool);
                break;
            }
        }
        if (!selected) [[unlikely]] {
            return alloc::error::oom;
        }
        return selected->allocate(request);
    }

    inline void impl_clear() OKAYLIB_NOEXCEPT final
    {
        __ok_assert(false,
                    "linked_slab_allocator cannot clear, this may cause "
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
    array_t<linked_blockpool_allocator_t, sizeof...(additional_block_sizes) + 2>
        m_blockpools;
};

namespace linked_slab_allocator {
namespace detail {
template <size_t block_size_a, size_t block_size_b,
          size_t... additional_block_sizes>
struct start_with_one_pool_per_blocksize_t
{
    constexpr void operator()() const OKAYLIB_NOEXCEPT {}
};
} // namespace detail

template <size_t block_size_a = detail::default_block_size_a,
          size_t block_size_b = detail::default_block_size_b,
          size_t... additional_block_sizes>
inline constexpr detail::start_with_one_pool_per_blocksize_t<
    block_size_a, block_size_b, additional_block_sizes...>
    start_with_one_pool_per_blocksize;

} // namespace linked_slab_allocator

} // namespace ok

#endif
