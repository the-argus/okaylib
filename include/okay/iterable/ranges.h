#ifndef __OKAYLIB_ITERABLE_RANGES_H__
#define __OKAYLIB_ITERABLE_RANGES_H__

#include "okay/iterable/traits.h"
#include <cstddef>

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {
// eric neibler's std::begin
// https://ericniebler.com/2014/10/21/customization-point-design-in-c11-and-beyond/
namespace detail {
/// Template which has a static function ::begin(iterable) to call .begin() on
/// an iterable type, returning a cursor_t, if such a function exists. If the
/// type is an array, then return 0 as the beginning. Otherwise there is no such
/// function.
template <typename iterable_t, typename cursor_t, typename = void>
struct begin_meta_t
{};

template <typename iterable_t, typename cursor_t>
struct begin_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        is_input_or_output_cursor_for_iterable<iterable_t, cursor_t> &&
        std::is_same_v<
            std::decay_t<cursor_t>,
            std::decay_t<decltype(std::declval<const iterable_t&>().begin())>>>>
{
    static inline constexpr cursor_t begin(const iterable_t& iterable)
    {
        return iterable.begin();
    }
};

// identical to the case above, but this accounts for the possibility of a
// templated begin() function taking the cursor type as the first argument
template <typename iterable_t, typename cursor_t>
struct begin_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        is_input_or_output_cursor_for_iterable<iterable_t, cursor_t> &&
        std::is_same_v<
            std::decay_t<cursor_t>,
            std::decay_t<decltype(std::declval<const iterable_t&>()
                                      .template begin<cursor_t>())>>>>
{
    static inline constexpr cursor_t begin(const iterable_t& iterable)
    {
        return iterable.template begin<cursor_t>();
    }
};

template <typename iterable_t>
struct begin_meta_t<
    iterable_t, size_t,
    std::enable_if_t<std::is_array_v<iterable_t> ||
                     std::is_array_v<std::remove_reference_t<iterable_t>>>>
{
    static inline constexpr size_t begin(const iterable_t&) { return 0; }
};

// begin overload which can only be selected if type implements .begin() or is
// an array
template <typename iterable_t, typename cursor_t>
inline constexpr auto begin(const iterable_t& i)
    -> decltype(begin_meta_t<iterable_t,
                             typename detail::default_cursor_type_meta_t<
                                 iterable_t>::type>::begin(i))
{
    return begin_meta_t<iterable_t, typename detail::default_cursor_type_meta_t<
                                        iterable_t>::type>::begin(i);
}

/// Template to check if begin_meta_t is specialized for this type, otherwise
/// it can only rely on free function implementations of begin()
template <typename iterable_t, typename cursor_t, typename = void>
struct has_library_provided_begin_implementation : std::false_type
{};

template <typename iterable_t, typename cursor_t>
struct has_library_provided_begin_implementation<
    iterable_t, cursor_t,
    std::void_t<decltype(begin_meta_t<iterable_t, cursor_t>::begin(
        std::declval<iterable_t&>()))>> : std::true_type
{};

/// Template to check if you can call .begin<cursor_t>() on a const reference to
/// iterable_t, and get returned a cursor of that type.
template <typename iterable_t, typename cursor_t, typename = void>
struct has_member_function_begin_for_cursor_type_meta_t : std::false_type
{};

template <typename iterable_t, typename cursor_t>
struct has_member_function_begin_for_cursor_type_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<std::is_same_v<
        std::decay_t<cursor_t>,
        std::decay_t<decltype(std::declval<const iterable_t&>()
                                  .template begin<cursor_t>())>>>>
    : std::true_type
{};

// function object type which just calls begin_meta_t default begin()
// implementation. operator() only exists if the above overload exists (the type
// is an array or defines .begin() for the given cursor type)
// This implementation always uses the default cursor type for the iterable
struct begin_fn_defaulted_t
{
    template <typename iterable_t>
    constexpr auto operator()(const iterable_t& iterable) const
    {
        static_assert(
            has_default_cursor_type_meta_t<iterable_t>::value,
            "Attempt to call begin() on iterable which does not have a "
            "deducible default cursor type. Try defining `using "
            "default_cursor_type = ...` in the class, or use "
            "begin_for_cursor<...>() to explicitly specify cursor type.");

        using default_cursor_t =
            typename detail::default_cursor_type_meta_t<iterable_t>::type;
        if constexpr (has_library_provided_begin_implementation<
                          iterable_t, default_cursor_t>::value) {
            // use begin_meta_t implementation
            return begin_meta_t<iterable_t, default_cursor_t>::begin(iterable);
        } else {
            // allow free function overload- only for default cursor type.
            // NOTE: if this is fires, you need to define a free function
            // `begin()`, or a member function `.begin()`, either of which need
            // to return the default cursor type
            return begin(iterable);
        }
    }
};

template <typename cursor_t> struct begin_fn_t
{
    template <typename iterable_t>
    constexpr auto operator()(const iterable_t& iterable) const
    {
        // if using the default type, use default implementation which will call
        // free function overloads.
        if constexpr (has_default_cursor_type_meta_t<iterable_t>::value &&
                      std::is_same_v<
                          typename default_cursor_type_meta_t<iterable_t>::type,
                          cursor_t>) {
            constexpr begin_fn_defaulted_t default_impl = {};
            return default_impl(iterable);
        } else {
            constexpr bool has_begin_member =
                has_member_function_begin_for_cursor_type_meta_t<
                    iterable_t, cursor_t>::value;
            static_assert(
                has_begin_member,
                "Attempt to use begin_for_cursor, which requires a `template "
                "<typename cursor_t> cursor_t begin() const;` member function "
                "to be available on the iterable type, but could not find one "
                "to call.");
            // NOTE: not ideal, but we have to use member function here because
            // using undeclared free function template is c++20 only.
            return iterable.template begin<cursor_t>();
        }
    }
};
} // namespace detail

// To avoid ODR violations:
// template <class T> struct static_const_t
// {
//     inline static constexpr T value{};
// };

// template <class T> constexpr T static_const_t<T>::value;

namespace {
/// Find the beginning of an iterable, using its default cursor type.
inline constexpr auto const& begin = detail::begin_fn_defaulted_t{};

/// Find the beginning of an iterable for a specific cursor type.
template <typename cursor_t>
inline constexpr auto const& begin_for_cursor = detail::begin_fn_t<cursor_t>{};
} // namespace
} // namespace ok

#endif
