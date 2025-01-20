#ifndef __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__

#include <cstddef>
#include <type_traits>

namespace ok::detail {

template <typename T, typename = void>
struct is_destructible_inner_meta_t : public std::false_type
{};

template <typename T>
struct is_destructible_inner_meta_t<
    T, std::void_t<decltype(std::declval<T&>().~T())>> : public std::true_type
{};

template <typename T>
struct is_destructible_meta_t : public is_destructible_inner_meta_t<T>
{};

template <typename T> struct is_destructible_meta_t<T&> : public std::true_type
{};

template <typename T> struct is_destructible_meta_t<T&&> : public std::true_type
{};

// c style array is destructible if its contents are
template <typename T, size_t size>
struct is_destructible_meta_t<T[size]> : public is_destructible_meta_t<T>
{};

template <typename T>
inline constexpr bool is_destructible_v = is_destructible_meta_t<T>::value;

template <typename T, typename... args_t>
inline constexpr bool is_constructible_from_v =
    is_destructible_v<T> && std::is_constructible_v<T, args_t...>;

template <typename from_t, typename to_t, typename = void>
class is_convertible_to_meta_t : public std::false_type
{};
template <typename from_t, typename to_t>
class is_convertible_to_meta_t<
    from_t, to_t,
    std::enable_if_t<
        std::is_convertible_v<from_t, to_t> &&
        std::is_same_v<void, std::void_t<decltype(static_cast<to_t>(
                                 std::declval<from_t>()))>>>>
    : public std::true_type
{};

template <typename from_t, typename to_t>
inline constexpr bool is_convertible_to_v =
    is_convertible_to_meta_t<from_t, to_t>::value;

// TODO: impelement c++ 20 concepts version of is_move_constructible_v. involves
// backporting std::common_reference, not even sure if thats possible tbh
// template <typename T>
//   inline constexpr bool is_move_constructible_v = is_constructible_from_v<T,
//   T> && is_convertible_to_v<T, T>;
// template<typename T, typename U>
//   inline constexpr bool common_reference_with_v =
//   is_same_v<std::common_reference<T, U>::type, std::common_reference_t<U, T>>
//     && is_convertible_to_v<T, std::common_reference_t<T, U>>
//     && is_convertible_to_v<U, common_reference_t<T, U>>;

template <typename T>
inline constexpr bool is_move_constructible_v = std::is_move_constructible_v<T>;

// TODO: implement c++20 assignable_from concept as is_assignable_from_v, and
// swappable as is_swappable_v. also requires common_reference
// swappable concept uses ranges::swap. probably backport that, too

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

} // namespace ok::detail

#endif
