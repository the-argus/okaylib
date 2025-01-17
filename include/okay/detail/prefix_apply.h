#ifndef __OKAYLIB_DETAIL_PREFIX_APPLY_H__
#define __OKAYLIB_DETAIL_PREFIX_APPLY_H__

#include "okay/detail/invoke.h"
#include <utility>

namespace ok::detail {
template <class tuple_t, std::size_t... indices, typename... args_t>
constexpr decltype(auto) prefix_apply_impl(std::index_sequence<indices...>,
                                           tuple_t&& t, args_t&&... args)
{
    return ok::invoke(std::forward<args_t>(args)...,
                      std::get<indices>(std::forward<tuple_t>(t))...);
}

/// Invoke a callable with a series of arguments followed by an unpack of a
/// tuple
template <class func_t, typename... args_t, class tuple_t>
constexpr decltype(auto) prefix_apply(func_t&& f, args_t&&... args, tuple_t&& t)
{
    return prefix_apply_impl(
        std::make_index_sequence<
            std::tuple_size_v<std::remove_reference_t<tuple_t>>>{},
        std::forward<tuple_t>(t), std::forward<func_t>(f),
        std::forward<args_t>(args)...);
}
} // namespace ok::detail

#endif
