#ifndef __OKAYLIB_ALLOCATORS_EMULATE_UNSUPPORTED_FEATURES_H__
#define __OKAYLIB_ALLOCATORS_EMULATE_UNSUPPORTED_FEATURES_H__

#include "okay/allocators/allocator.h"
namespace ok {

namespace alloc {

template <allocator_c allocator_impl_t>
class disable_freeing : public allocator_t
{
  public:
    static_assert(stdc::is_base_of_v<allocator_t, allocator_impl_t>,
                  "disable_freeing_t given a type which does not inherit from "
                  "allocator_t");

    constexpr static feature_flags type_features =
        allocator_impl_t::type_features | feature_flags::can_expand_front;

    template <typename... args_t>
        requires is_std_constructible_c<allocator_impl_t, args_t...>
    explicit disable_freeing(args_t&&... args) OKAYLIB_NOEXCEPT
        : m_inner(stdc::forward<args_t>(args)...)
    {
    }

  protected:
    [[nodiscard]] inline result_t<bytes_t>
    impl_allocate(const request_t& request) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_allocate(request);
    }

    [[nodiscard]] inline feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_features() & feature_flags::can_expand_front;
    }

    inline void impl_deallocate(void*) OKAYLIB_NOEXCEPT final {}

    [[nodiscard]] inline result_t<bytes_t>
    impl_reallocate(const reallocate_request_t& options) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_reallocate(options);
    }

    [[nodiscard]] inline result_t<reallocation_extended_t>
    impl_reallocate_extended(const reallocate_extended_request_t& options)
        OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_reallocate_extended(options);
    }

  private:
    allocator_impl_t m_inner;
};

/// Template wrapper around an allocator to pretend that it supports
/// expand_front. This will never happen in-place/it will always behave like
/// an allocator with expand_front which was not able to reallocate in-place.
template <allocator_c allocator_impl_t>
class emulate_expand_front : public allocator_t
{
  public:
    // TODO: add static assert or trait before doing this. if someone gets a
    // compile error on their custom implemented allocator, sorry. add static
    // constexpr type_features
    constexpr static feature_flags type_features =
        allocator_impl_t::type_features | feature_flags::can_expand_front;

    template <typename... args_t>
    explicit emulate_expand_front(args_t&&... args) OKAYLIB_NOEXCEPT
        : m_inner(stdc::forward<args_t>(args)...)
    {
        static_assert(
            stdc::is_nothrow_constructible_v<allocator_impl_t, args_t...>,
            "Attempt to use in-place constructor of emulate_expand_front, but "
            "the underlying object is not nothrow constructible with the given "
            "arguments.");
    }

    allocator_impl_t& inner() OKAYLIB_NOEXCEPT { return m_inner; }
    const allocator_impl_t& inner() const OKAYLIB_NOEXCEPT { return m_inner; }

  protected:
    [[nodiscard]] inline result_t<bytes_t>
    impl_allocate(const request_t& request) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_allocate(request);
    }

    [[nodiscard]] inline feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_features() & feature_flags::can_expand_front;
    }

    inline void impl_deallocate(void* memory) OKAYLIB_NOEXCEPT final
    {
        m_inner.impl_deallocate(memory);
    }

    [[nodiscard]] inline result_t<bytes_t>
    impl_reallocate(const reallocate_request_t& options) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_reallocate(options);
    }

    [[nodiscard]] inline result_t<reallocation_extended_t>
    impl_reallocate_extended(const reallocate_extended_request_t& options)
        OKAYLIB_NOEXCEPT final
    {
        // if we're not trying to expand front, just pass to the underlying
        // allocator implementation
        if (!(options.flags & realloc_flags::expand_front)) {
            return m_inner.impl_reallocate_extended(options);
        }

        // trying to be in-place always fails as this allocator doesnt
        // natively support expand_front and we must make a new allocation
        // to emulate that.
        if (options.flags & realloc_flags::in_place_orelse_fail) [[unlikely]] {
            if (m_inner.impl_features() &
                feature_flags::can_predictably_realloc_in_place) {
                return error::couldnt_expand_in_place;
            } else {
                return error::unsupported;
            }
        }

        const bool zeroed = !(options.flags & realloc_flags::leave_nonzeroed);

        using flags_underlying_t = stdc::underlying_type_t<realloc_flags>;

        const auto [bytes_offset_back, bytes_offset_front, new_size] =
            options.calculate_new_preferred_size();

        result_t<bytes_t> res = m_inner.impl_allocate(alloc::request_t{
            .num_bytes = new_size,
            // leave nonzeroed status for allocation is the same
            .leave_nonzeroed = options.flags & realloc_flags::leave_nonzeroed,
        });

        if (!res.is_success()) [[unlikely]]
            return res.status();

        const bytes_t& memory = res.unwrap();

        uint8_t* copy_dest =
            static_cast<uint8_t*>(memory.unchecked_address_of_first_item());
        uint8_t* copy_src = options.memory.unchecked_address_of_first_item();
        size_t size = options.memory.size();

        if (options.flags & realloc_flags::shrink_front) {
            copy_src += bytes_offset_front;
            size -= bytes_offset_front;
        } else {
            __ok_internal_assert(options.flags & realloc_flags::expand_front);
            copy_dest += bytes_offset_front;
        }

        if (options.flags & realloc_flags::shrink_back)
            size -= bytes_offset_back;
        ::memcpy(copy_dest, copy_src, size);

        impl_deallocate(options.memory);

        return reallocation_extended_t{
            .memory = memory,
            .bytes_offset_front = bytes_offset_front,
        };
    }

  private:
    allocator_impl_t m_inner;
};

} // namespace alloc

} // namespace ok

#endif
