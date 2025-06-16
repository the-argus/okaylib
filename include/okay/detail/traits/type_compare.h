#ifndef __OKAYLIB_DETAIL_TRAITS_TYPE_COMPARE_H__
#define __OKAYLIB_DETAIL_TRAITS_TYPE_COMPARE_H__

#include <type_traits>

namespace ok {
template <typename T, typename other_t>
concept same_as_c = requires { requires std::is_same_v<T, other_t>; };

template <typename T>
concept is_void_c = requires { requires std::is_void_v<T>; };

namespace detail {
/// NOTE: this does not work for function references. Will always return false.
template <class T>
concept is_complete_c = requires { sizeof(T); };
} // namespace detail

template <typename T>
concept is_const_c = requires { requires std::is_const_v<T>; };
} // namespace ok

#endif
