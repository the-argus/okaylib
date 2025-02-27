#ifndef __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__

#include "okay/detail/template_util/first_type_in_pack.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/is_instance.h"
#include <type_traits>

namespace ok {

template <typename, typename, typename> class res_t;
template <typename T> class owning_ref;

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

template <typename maybe_res_t, typename expected_contained_t, typename = void>
struct is_type_res_and_contains : std::false_type
{};
template <typename maybe_res_t, typename expected_contained_t>
struct is_type_res_and_contains<
    maybe_res_t, expected_contained_t,
    std::enable_if_t<
        detail::is_instance_v<maybe_res_t, res_t> &&
        std::is_same_v<typename maybe_res_t::type, expected_contained_t>>>
    : std::true_type
{};

template <typename T, typename constructor_args_t, typename = void>
struct has_fallible_construct_for_args : public std::false_type
{
    static constexpr bool is_return_type_valid = false;
};

template <typename T, typename constructor_args_t>
struct has_fallible_construct_for_args<
    T, constructor_args_t,
    std::void_t<decltype(T::construct(
        std::declval<uninitialized_storage_t<T>&>(),
        std::declval<const constructor_args_t&>()))>> : public std::true_type
{
    using return_type =
        decltype(T::construct(std::declval<uninitialized_storage_t<T>&>(),
                              std::declval<const constructor_args_t&>()));
    static constexpr bool is_return_type_valid =
        is_type_res_and_contains<return_type, owning_ref<T>>::value;
};

template <typename T, typename constructor_args_t, typename = void>
struct has_infallible_construct_for_args : public std::false_type
{
    static constexpr bool is_return_type_valid = false;
};

template <typename T, typename constructor_args_t>
struct has_infallible_construct_for_args<
    T, constructor_args_t,
    std::void_t<decltype(T::construct(
        std::declval<const constructor_args_t&>()))>> : public std::true_type
{
    using return_type =
        decltype(T::construct(std::declval<const constructor_args_t&>()));
    static constexpr bool is_return_type_valid = std::is_same_v<return_type, T>;
};

template <typename T, typename = void>
struct has_default_infallible_construct : public std::false_type
{
    static constexpr bool is_return_type_valid = false;
};

template <typename T>
struct has_default_infallible_construct<T,
                                        std::void_t<decltype(T::construct())>>
    : public std::true_type
{
    using return_type = decltype(T::construct());
    static constexpr bool is_return_type_valid = std::is_same_v<return_type, T>;
};

template <typename T, typename constructor_args_t>
inline constexpr bool has_construct_for_args_v =
    has_fallible_construct_for_args<T, constructor_args_t>::value ||
    has_infallible_construct_for_args<T, constructor_args_t>::value;

template <typename... args_t> struct is_infallible_constructible
{
    template <typename T, typename = void> struct inner : public std::false_type
    {};

    template <typename T>
    struct inner<T, std::enable_if_t<is_std_constructible_v<T, args_t...>>>
        : public std::true_type
    {};

    template <typename T>
    struct inner<
        T, std::enable_if_t<sizeof...(args_t) == 1 &&
                            has_infallible_construct_for_args<
                                T, first_type_in_pack_t<args_t...>>::value>>
        : public std::true_type
    {};
};

template <typename... args_t> struct is_fallible_constructible
{
    template <typename T, typename = void> struct inner : public std::false_type
    {};

    template <typename T>
    struct inner<
        T, std::enable_if_t<sizeof...(args_t) == 1 &&
                            has_fallible_construct_for_args<
                                T, first_type_in_pack_t<args_t...>>::value>>
        : public std::true_type
    {};
};

} // namespace detail
template <typename T, typename... args_t>
inline constexpr bool is_infallible_constructible_v =
    detail::is_infallible_constructible<args_t...>::template inner<T>::value ||
    (sizeof...(args_t) == 0 &&
     detail::has_default_infallible_construct<T>::value);

template <typename T, typename... args_t>
inline constexpr bool is_fallible_constructible_v =
    detail::is_fallible_constructible<args_t...>::template inner<T>::value;

template <typename T, typename... args_t>
inline constexpr bool is_constructible_v =
    is_infallible_constructible_v<T, args_t...> ||
    is_fallible_constructible_v<T, args_t...>;
} // namespace ok

#endif
