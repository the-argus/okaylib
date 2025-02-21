#ifndef __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__

#include "okay/anystatus.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/status.h"
#include <type_traits>

namespace ok {
struct default_constructor_tag
{};

template <typename T, typename... args_t>
constexpr bool is_std_constructible_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_constructible_v<T, args_t...>;
#else
    std::is_constructible_v<T, args_t...>;
#endif

template <typename T>
constexpr bool is_std_default_constructible_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_default_constructible_v<T>;
#else
    std::is_default_constructible_v<T>;
#endif

template <typename T>
constexpr bool is_std_destructible_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_destructible_v<T>;
#else
    std::is_destructible_v<T>;
#endif

template <typename T, typename... args_t>
constexpr bool is_std_invocable_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_invocable_v<T, args_t...>;
#else
    std::is_invocable_v<T, args_t...>;
#endif

template <typename T, typename return_type, typename... args_t>
constexpr bool is_std_invocable_r_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_invocable_r_v<T, return_type, args_t...>;
#else
    std::is_invocable_r_v<T, return_type, args_t...>;
#endif

namespace detail {

template <typename T, typename = void>
struct is_valid_memberfunc_error_type : public std::false_type
{};

template <typename T>
struct is_valid_memberfunc_error_type<
    T, std::enable_if_t<detail::is_instance_v<T, ok::status_t> ||
                        std::is_same_v<T, anystatus_t>>> : public std::true_type
{};

template <typename T>
constexpr bool is_valid_memberfunc_error_type_v =
    is_valid_memberfunc_error_type<T>::value;

template <typename T, typename error_type_t = void> struct memberfunc_error_type
{
    static_assert(
        std::is_class_v<T>,
        "There is no memberfunc_error_type for a non class/struct type.");
    using type = void;
};

template <typename T>
struct memberfunc_error_type<
    T, std::enable_if_t<
           is_valid_memberfunc_error_type_v<typename T::out_error_type>>>
{
    template <typename U, typename = void>
    struct has_enum_type_def : public std::false_type
    {};

    template <typename U>
    struct has_enum_type_def<U, std::void_t<typename U::enum_type>>
        : public std::true_type
    {};

    using type = typename T::out_error_type;
    static_assert(is_std_default_constructible_v<type> &&
                      has_enum_type_def<typename T::out_error_type>::value,
                  "internal assert fired, templates are broken");
};

template <typename T>
using memberfunc_error_type_t = typename memberfunc_error_type<T>::type;

template <typename... args_t> struct is_fallible_constructible
{
    template <typename T, typename constructor_tag_t, typename = void>
    struct inner : std::false_type
    {};
    template <typename T, typename constructor_tag_t>
    struct inner<T, constructor_tag_t,
                 std::enable_if_t<is_std_constructible_v<
                     T, constructor_tag_t, detail::memberfunc_error_type_t<T>&,
                     args_t...>>> : std::true_type
    {};
};

} // namespace detail

template <typename T, typename constructor_tag_t, typename... args_t>
constexpr bool is_infallible_constructible_v =
    is_std_constructible_v<T, constructor_tag_t, args_t...> ||
    is_std_constructible_v<T, args_t...>;

template <typename T, typename constructor_tag_t, typename... args_t>
constexpr bool is_fallible_constructible_v = detail::is_fallible_constructible<
    args_t...>::template inner<T, constructor_tag_t>::value;

template <typename T, typename constructor_tag_t, typename... args_t>
constexpr bool is_constructible_v =
    is_fallible_constructible_v<T, constructor_tag_t, args_t...> ||
    is_infallible_constructible_v<T, constructor_tag_t, args_t...>;

namespace detail {

template <typename T, typename constructor_tag_t, typename... args_t>
constexpr void
construct_into_uninitialized_infallible(T* uninit, constructor_tag_t tag,
                                        args_t&&... args) OKAYLIB_NOEXCEPT
{
    static_assert(
        is_infallible_constructible_v<T, constructor_tag_t, args_t...>,
        "Cannot construct with the given args");
    if constexpr (is_std_constructible_v<T, constructor_tag_t, args_t...>) {
        new (uninit) T(tag, std::forward<args_t>(args)...);
    } else {
        static_assert(is_std_constructible_v<T, args_t...>);
        new (uninit) T(std::forward<args_t>(args)...);
    }
}

template <typename from_t, typename to_t, typename = void>
class is_convertible_to : public std::false_type
{};
template <typename from_t, typename to_t>
class is_convertible_to<
    from_t, to_t,
    std::enable_if_t<
        std::is_convertible_v<from_t, to_t> &&
        std::is_same_v<void, std::void_t<decltype(static_cast<to_t>(
                                 std::declval<from_t>()))>>>>
    : public std::true_type
{};

template <typename from_t, typename to_t>
inline constexpr bool is_convertible_to_v =
    is_convertible_to<from_t, to_t>::value;

template <typename T>
inline constexpr bool is_move_constructible_v = std::is_move_constructible_v<T>;

template <typename LHS, typename RHS>
inline constexpr bool is_assignable_from_v = std::is_assignable_v<LHS, RHS>;

template <typename T>
inline constexpr bool is_swappable_v = std::is_swappable_v<T>;

template <typename T>
inline constexpr bool is_moveable_v =
    std::is_object_v<T> && is_move_constructible_v<T> &&
    std::is_move_assignable_v<T>
    // is_assignable_from_v<T&, T>
    && is_swappable_v<T>;

} // namespace detail
} // namespace ok

#endif
