#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_CALL_FIRST_TYPE_WITH_REMAINING_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_CALL_FIRST_TYPE_WITH_REMAINING_H__

#include <utility>

#include "okay/detail/traits/special_member_traits.h"

namespace ok::detail {

template <typename T, typename... args_t>
inline constexpr bool is_function_and_arguments_v =
    is_std_invocable_v<T, args_t...>;

template <typename... args_t>
    requires is_function_and_arguments_v<args_t...>
decltype(auto) call_first_type_with_others(args_t&&... args)
{
    auto impl = [](auto&& first, auto&&... inner_args) -> decltype(auto) {
        return first(std::forward<decltype(inner_args)>(inner_args)...);
    };
    return impl(std::forward<args_t>(args)...);
}
} // namespace ok::detail

#endif
