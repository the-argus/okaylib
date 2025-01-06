#ifndef __OKAYLIB_DETAIL_TRAITS_IS_SAME_H__
#define __OKAYLIB_DETAIL_TRAITS_IS_SAME_H__

#include <type_traits>

namespace ok::detail {

template <typename T, typename U>
inline constexpr bool is_same_v = std::is_same_v<T, U> && std::is_same_v<U, T>;

}

#endif
