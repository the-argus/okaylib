#ifndef __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"

namespace ok {
template <typename allocator_impl_t>
class block_allocator_t : public ok::allocator_t
{
  private:
    struct free_block_t
    {
        free_block_t* prev;
    };

    struct members_t
    {
        bytes_t memory;
        size_t blocksize;
        size_t minimum_alignment;
        free_block_t* free_head;
        opt<allocator_impl_t&> backing;
    } m;

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (!m.backing)
            return;

        m.backing.ref_or_panic().deallocate(m.memory);
    }

  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_clear;

    constexpr explicit block_allocator_t(bytes_t initial_buffer);
    constexpr explicit block_allocator_t(bytes_t&& initial_buffer,
                                         allocator_impl_t& allocator);

    constexpr block_allocator_t(block_allocator_t&& other) OKAYLIB_NOEXCEPT
        : m(other.m)
    {
        other.m.backing.reset();
    }

    constexpr block_allocator_t&
    operator=(block_allocator_t&& other) OKAYLIB_NOEXCEPT
    {
        destroy();
        m = other.m;
        other.m.backing.reset();
        return *this;
    }

    block_allocator_t& operator=(const block_allocator_t&) = delete;
    block_allocator_t(const block_allocator_t&) = delete;

    inline ~block_allocator_t() OKAYLIB_NOEXCEPT_FORCE { destroy(); }

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

block_allocator_t(bytes_t static_buffer) -> block_allocator_t<ok::allocator_t>;

template <typename allocator_impl_t>
constexpr block_allocator_t<allocator_impl_t>::block_allocator_t(
    bytes_t initial_buffer)
{
}

template <typename allocator_impl_t>
constexpr block_allocator_t<allocator_impl_t>::block_allocator_t(
    bytes_t&& initial_buffer, allocator_impl_t& allocator)
{
}

template <typename allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
block_allocator_t<allocator_impl_t>::impl_allocate(const alloc::request_t&)
    OKAYLIB_NOEXCEPT
{
}

template <typename allocator_impl_t>
inline void block_allocator_t<allocator_impl_t>::impl_clear() OKAYLIB_NOEXCEPT
{
}

template <typename allocator_impl_t>
inline void
block_allocator_t<allocator_impl_t>::impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT
{
}

template <typename allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
block_allocator_t<allocator_impl_t>::impl_reallocate(
    const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT
{
}

} // namespace ok

#endif
