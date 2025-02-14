#ifndef __OKAYLIB_ALLOCATORS_EMULATE_UNSUPPORTED_FEATURES_H__
#define __OKAYLIB_ALLOCATORS_EMULATE_UNSUPPORTED_FEATURES_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/traits/is_derived_from.h"
#include <cstring>
namespace ok {

namespace alloc {

template <typename allocator_impl_t> class disable_freeing : public allocator_t
{
  public:
    static_assert(std::is_base_of_v<allocator_t, allocator_impl_t>,
                  "disable_freeing_t given a type which does not inherit from "
                  "allocator_t");

    constexpr static feature_flags type_features =
        allocator_impl_t::type_features | feature_flags::can_expand_front;

    template <typename... args_t>
    constexpr explicit disable_freeing(args_t&&... args) OKAYLIB_NOEXCEPT
        : m_inner(std::forward<args_t>(args)...)
    {
        static_assert(
            std::is_nothrow_constructible_v<allocator_impl_t, args_t...>,
            "Attempt to use in-place constructor of emulate_expand_front, but "
            "the underlying object is not nothrow constructible with the given "
            "arguments.");
    }

  protected:
    [[nodiscard]] inline result_t<maybe_defined_memory_t>
    impl_allocate(const request_t& request) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_allocate(request);
    }

    inline void impl_clear() OKAYLIB_NOEXCEPT final { m_inner.impl_clear(); }

    [[nodiscard]] inline feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_features() & feature_flags::can_expand_front;
    }

    inline void impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT final {}

    [[nodiscard]] inline result_t<maybe_defined_memory_t>
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

template <typename allocator_impl_t> class disable_clearing : public allocator_t
{
  public:
    static_assert(std::is_base_of_v<allocator_t, allocator_impl_t>,
                  "disable_freeing_t given a type which does not inherit from "
                  "allocator_t");

    constexpr static feature_flags type_features =
        allocator_impl_t::type_features | feature_flags::can_expand_front;

    template <typename... args_t>
    constexpr explicit disable_clearing(args_t&&... args) OKAYLIB_NOEXCEPT
        : m_inner(std::forward<args_t>(args)...)
    {
        static_assert(
            std::is_nothrow_constructible_v<allocator_impl_t, args_t...>,
            "Attempt to use in-place constructor of emulate_expand_front, but "
            "the underlying object is not nothrow constructible with the given "
            "arguments.");
    }

  protected:
    [[nodiscard]] inline result_t<maybe_defined_memory_t>
    impl_allocate(const request_t& request) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_allocate(request);
    }

    inline void impl_clear() OKAYLIB_NOEXCEPT final {}

    [[nodiscard]] inline feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_features() & feature_flags::can_expand_front;
    }

    inline void impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT final
    {
        m_inner.impl_deallocate(bytes);
    }

    [[nodiscard]] inline result_t<maybe_defined_memory_t>
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
template <typename allocator_impl_t>
class emulate_expand_front : public allocator_t
{
  public:
    // TODO: add static assert or trait before doing this. if someone gets a
    // compile error on their custom implemented allocator, sorry. add static
    // constexpr type_features
    constexpr static feature_flags type_features =
        allocator_impl_t::type_features | feature_flags::can_expand_front;

    template <typename... args_t>
    constexpr explicit emulate_expand_front(args_t&&... args) OKAYLIB_NOEXCEPT
        : m_inner(std::forward<args_t>(args)...)
    {
        static_assert(
            std::is_nothrow_constructible_v<allocator_impl_t, args_t...>,
            "Attempt to use in-place constructor of emulate_expand_front, but "
            "the underlying object is not nothrow constructible with the given "
            "arguments.");
    }

    constexpr allocator_impl_t& inner() OKAYLIB_NOEXCEPT { return m_inner; }
    constexpr const allocator_impl_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m_inner;
    }

  protected:
    [[nodiscard]] inline result_t<maybe_defined_memory_t>
    impl_allocate(const request_t& request) OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_allocate(request);
    }

    inline void impl_clear() OKAYLIB_NOEXCEPT final { m_inner.impl_clear(); }

    [[nodiscard]] inline feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return m_inner.impl_features() & feature_flags::can_expand_front;
    }

    inline void impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT final
    {
        m_inner.impl_deallocate(bytes);
    }

    [[nodiscard]] inline result_t<maybe_defined_memory_t>
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
        if (!(options.flags & flags::expand_front)) {
            return m_inner.impl_reallocate_extended(options);
        }

        // trying to be in-place always fails as this allocator doesnt
        // natively support expand_front and we must make a new allocation
        // to emulate that.
        if (options.flags & flags::in_place_orelse_fail) [[unlikely]] {
            if (m_inner.impl_features() &
                feature_flags::can_predictably_realloc_in_place) {
                return error::couldnt_expand_in_place;
            } else {
                return error::unsupported;
            }
        }

        const bool zeroed = !(options.flags & flags::leave_nonzeroed);

        using flags_underlying_t = std::underlying_type_t<flags>;
        const auto leave_nonzeroed_bit =
            static_cast<flags_underlying_t>(options.flags) &
            static_cast<flags_underlying_t>(flags::leave_nonzeroed);

        const auto [bytes_offset_back, bytes_offset_front, new_size] =
            options.calculate_new_preferred_size();

        result_t<maybe_defined_memory_t> res =
            m_inner.impl_allocate(alloc::request_t{
                .num_bytes = new_size,
                // leave nonzeroed status for allocation is the same
                .flags = flags(leave_nonzeroed_bit),
            });

        if (!res.okay()) [[unlikely]]
            return res.err();

        const bytes_t memory =
            leave_nonzeroed_bit
                ? res.release_ref().as_undefined().leave_undefined()
                : res.release_ref().as_bytes();

        uint8_t* copy_dest = static_cast<uint8_t*>(memory.data());
        uint8_t* copy_src = options.memory.data();
        size_t size = options.memory.size();

        if (options.flags & flags::shrink_front) {
            copy_src += bytes_offset_front;
            size -= bytes_offset_front;
        } else {
            __ok_internal_assert(options.flags & flags::expand_front);
            copy_dest += bytes_offset_front;
        }

        if (options.flags & flags::shrink_back)
            size -= bytes_offset_back;
        std::memcpy(copy_dest, copy_src, size);

        impl_deallocate(options.memory);

        return reallocation_extended_t{
            .memory = memory,
            .bytes_offset_front = bytes_offset_front,
        };
    }

    static_assert(detail::is_derived_from_v<allocator_impl_t, allocator_t>,
                  "Type given to emulate_expand_front is not an allocator.");

  private:
    allocator_impl_t m_inner;
};

} // namespace alloc

} // namespace ok

#endif
