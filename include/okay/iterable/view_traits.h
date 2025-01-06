#ifndef __OKAYLIB_ITERABLE_VIEW_TRAITS_H__
#define __OKAYLIB_ITERABLE_VIEW_TRAITS_H__

#include "okay/detail/traits/is_same.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/iterable/iterable.h"
#include <type_traits>

namespace ok::detail {

// not defined, just for deduction
template <typename T, typename U>
auto derived_from_view_interface_fn(const T&, const view_interface<U>&)
    -> std::enable_if_t<!is_same_v<T, view_interface<U>>>;

template <typename T, typename = void>
struct is_derived_from_view_interface_meta_t : std::false_type
{};

template <typename T>
struct is_derived_from_view_interface_meta_t<
    T, std::void_t<decltype(derived_from_view_interface_fn(
           std::declval<T>(), std::declval<T>()))>> : std::true_type
{};

template <typename T>
inline constexpr bool is_derived_from_view_interface_v =
    is_derived_from_view_interface_meta_t<T>::value;

// a view is a moveable type derived from view interface
template <typename T>
inline constexpr bool is_view_v =
    is_derived_from_view_interface_v<T> && is_moveable_v<T> && is_iterable_v<T>;

} // namespace ok::detail

#endif
