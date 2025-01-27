#ifndef __OKAYLIB_DETAIL_TRAITS_IS_COMPLETE_H__
#define __OKAYLIB_DETAIL_TRAITS_IS_COMPLETE_H__

#include <type_traits>

namespace ok::detail {

template <class T, class = void> struct is_complete : std::false_type
{};

template <class T>
struct is_complete<T, decltype(void(sizeof(T)))> : std::true_type
{};

/// NOTE: this does not work for function references. Will always return false.
template <class T> inline constexpr bool is_complete_v = is_complete<T>::value;

} // namespace ok::detail

#endif
