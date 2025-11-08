#ifndef __OKAYLIB_CONTAINERS_array_t_H__
#define __OKAYLIB_CONTAINERS_array_t_H__

#include "okay/construct.h"
#include "okay/detail/abort.h"
#include "okay/detail/noexcept.h"
#include "okay/slice.h"
#include "okay/stdmem.h"

#include <cstddef>
#include <cstring>

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
#define __ok_internal_array_impl                                               \
                                                                               \
  public:                                                                      \
    static_assert(!stdc::is_reference_c<T>,                                    \
                  "ok::array_t cannot store references.");                     \
    static_assert(!std::is_void_v<T>, "Cannot create an array_t of void.");    \
                                                                               \
    static_assert(num_items != 0, "Cannot create an array_t of zero items.");  \
                                                                               \
    using value_type = T;                                                      \
                                                                               \
    [[nodiscard]] constexpr value_type& operator[](size_t index) &             \
        OKAYLIB_NOEXCEPT                                                       \
    {                                                                          \
        if (index >= num_items) [[unlikely]] {                                 \
            __ok_abort("Out of bounds access into ok::array_t");               \
        }                                                                      \
        return __m_items[index];                                               \
    }                                                                          \
                                                                               \
    [[nodiscard]] constexpr const value_type& operator[](size_t index)         \
        const& OKAYLIB_NOEXCEPT                                                \
    {                                                                          \
        if (index >= num_items) [[unlikely]] {                                 \
            __ok_abort("Out of bounds access into ok::array_t");               \
        }                                                                      \
        return __m_items[index];                                               \
    }                                                                          \
                                                                               \
    [[nodiscard]] constexpr value_type&& operator[](size_t index) &&           \
        OKAYLIB_NOEXCEPT                                                       \
    {                                                                          \
        if (index >= num_items) [[unlikely]] {                                 \
            __ok_abort("Out of bounds access into ok::array_t");               \
        }                                                                      \
        return stdc::move(__m_items[index]);                                   \
    }                                                                          \
                                                                               \
    constexpr value_type&& operator[](size_t index) const&& = delete;          \
                                                                               \
    [[nodiscard]] constexpr value_type* data()& noexcept { return __m_items; } \
                                                                               \
    [[nodiscard]] constexpr const value_type* data() const& noexcept           \
    {                                                                          \
        return __m_items;                                                      \
    }                                                                          \
                                                                               \
    [[nodiscard]] constexpr slice<const T> items() const& OKAYLIB_NOEXCEPT     \
    {                                                                          \
        return raw_slice(*data(), size());                                     \
    }                                                                          \
    [[nodiscard]] constexpr slice<T> items() & OKAYLIB_NOEXCEPT                \
    {                                                                          \
        return raw_slice(*data(), size());                                     \
    }                                                                          \
                                                                               \
    [[nodiscard]] constexpr size_t size() const noexcept { return num_items; } \
                                                                               \
    T __m_items[num_items];

// NOTE: this class was originally written for c++17 and only had aggregate
// initialization (default constructor was marked private). Unfortunately, C++20
// forces the class to have no declared constructors, so it is now possible to
// leave arrays of trivially constructible objects uninitialized by default
// constructing them.
template <typename T, size_t num_items> struct array_t
{
    static_assert(
        !stdc::is_trivially_constructible_v<T>,
        "Cannot use ok::array_t with a trivially constructible type. Either "
        "add a default constructor to your type (best practice) or use "
        "ok::undefined_array_t and be aware of undefined initialization.");

    __ok_internal_array_impl
};

template <typename T, size_t num_items> struct zeroed_array_t
{
    static_assert(stdc::is_trivially_constructible_v<T>,
                  "ok::zeroed_array_t can only be used with trivially "
                  "constructible types, use ok::array_t for types that have "
                  "some defined initialization / default construction.");
    static_assert(stdc::is_constructible_v<T, int>,
                  "type must be constructible with an integer 0 to be used in "
                  "zeroed_array_t");

    // no aggregate initialization for zeroed array, it's always zeroed
    constexpr zeroed_array_t() noexcept { ok::memfill(ok::slice(*this), 0); }

    __ok_internal_array_impl
};

/// A maybe_undefined_array_t is an array of trivially constructible objects
/// which will be left uninitialized if the array is default constructed. Use it
/// only if aggregate initialization is needed for an array containing a
/// trivially constructible type.
template <typename T, size_t num_items> struct maybe_undefined_array_t
{
    static_assert(stdc::is_trivially_constructible_v<T>,
                  "ok::undefined_array_t can only be used with trivially "
                  "constructible types, use ok::array_t for types that have "
                  "some defined initialization / default construction.");

    __ok_internal_array_impl
};

// aggregate initialization template deducation, so you can do
// `ok::array_t my_array_t = {...}`
template <typename T, typename... pack>
    requires(stdc::is_same_v<T, pack> && ...)
array_t(T, pack...) -> array_t<T, 1 + sizeof...(pack)>;
template <typename T, typename... pack>
    requires(stdc::is_same_v<T, pack> && ...)
maybe_undefined_array_t(T, pack...)
    -> maybe_undefined_array_t<T, 1 + sizeof...(pack)>;
} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename T, size_t num_items>
struct fmt::formatter<ok::array_t<T, num_items>>
{
    using formatted_type_t = ok::array_t<T, num_items>;
    static_assert(
        fmt::is_formattable<T>::value,
        "Attempt to format an arraylist whose items are not formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& array,
                                    format_context& ctx) const
    {
        // TODO: use CTTI to include nice type names in print here
        fmt::format_to(ctx.out(), "[ ");
        for (size_t i = 0; i < num_items; ++i) {
            fmt::format_to(ctx.out(), "{} ", array.data()[i]);
        }
        return fmt::format_to(ctx.out(), "]");
    }
};
#endif

#endif
