#ifndef __OKAYLIB_TRAITS_IS_CONTAINER_H__
#define __OKAYLIB_TRAITS_IS_CONTAINER_H__

#include <cstddef>
#include <type_traits>

namespace ok::detail {

template <typename T, typename = void>
struct has_size_member_meta_t : std::false_type
{};

template <typename T>
struct has_size_member_meta_t<
    T, std::enable_if_t<
           std::is_same_v<size_t, decltype(std::declval<const T&>().size())>>>
    : std::true_type
{};

template <typename T, typename = void>
struct has_data_member_meta_t : std::false_type
{};

template <typename T>
struct has_data_member_meta_t<
    T, std::enable_if_t<
           std::is_pointer_v<decltype(std::declval<const T&>().data())>>>
    : std::true_type
{};

template <typename T>
inline constexpr bool is_std_container_v =
    has_size_member_meta_t<T>::value && has_data_member_meta_t<T>::value;

} // namespace ok::detail

#endif
