#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_IS_ALL_SAME_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_IS_ALL_SAME_H__
#include <type_traits>

namespace ok::detail {
// checks if all items in a parameter pack are the same type
template <typename T, typename... U>
concept is_all_same_c = requires {
    requires std::integral_constant<bool, (... && std::is_same_v<T, U>)>::value;
};
} // namespace ok::detail

#endif
