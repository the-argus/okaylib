#ifndef __OKAYLIB_DETAIL_TRAITS_MATHOP_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_MATHOP_TRAITS_H__

#include <cstdint>
#include <type_traits>

namespace ok::detail {

template <typename, typename = void>
class has_pre_increment_meta_t : public std::false_type
{};
template <typename T>
class has_pre_increment_meta_t<
    T, std::enable_if_t<std::is_same_v<decltype(++std::declval<T&>()), T&>>>
    : public std::true_type
{};

template <typename, typename = void>
class has_pre_decrement_meta_t : public std::false_type
{};
template <typename T>
class has_pre_decrement_meta_t<
    T, std::enable_if_t<std::is_same_v<decltype(--std::declval<T&>()), T&>>>
    : public std::true_type
{};

template <typename, typename = void>
class has_inplace_addition_with_i64_meta_t : public std::false_type
{};
template <typename T>
class has_inplace_addition_with_i64_meta_t<
    T,
    std::void_t<decltype(std::declval<T&>() += std::declval<const int64_t&>())>>
    : public std::true_type
{};

template <typename LHS, typename RHS, typename = void>
class is_equality_comparable_to_meta_t : public std::false_type
{};
template <typename LHS, typename RHS>
class is_equality_comparable_to_meta_t<
    LHS, RHS,
    std::enable_if_t<std::is_same_v<decltype(std::declval<const LHS&>() ==
                                             std::declval<const RHS&>()),
                                    bool>>> : public std::true_type
{};

template <typename T>
inline constexpr bool has_pre_increment_v = has_pre_increment_meta_t<T>::value;
template <typename T>
inline constexpr bool has_pre_decrement_v = has_pre_decrement_meta_t<T>::value;
template <typename T>
inline constexpr bool has_inplace_addition_with_i64_v =
    has_inplace_addition_with_i64_meta_t<T>::value;
template <typename LHS, typename RHS>
inline constexpr bool is_equality_comparable_to_v =
    is_equality_comparable_to_meta_t<LHS, RHS>::value;
template <typename T>
inline constexpr bool is_equality_comparable_to_self_v =
    is_equality_comparable_to_meta_t<T, T>::value;

} // namespace ok::detail
#endif
