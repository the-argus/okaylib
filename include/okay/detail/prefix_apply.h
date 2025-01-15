#ifndef __OKAYLIB_DETAIL_PREFIX_APPLY_H__
#define __OKAYLIB_DETAIL_PREFIX_APPLY_H__

#include "okay/detail/invoke.h"
#include <utility>

namespace ok::detail {
template <class func_t, class tuple_t, typename... args_t,
          std::size_t... indices>
constexpr decltype(auto) prefix_apply_impl(func_t&& f, args_t&&... args,
                                           tuple_t&& t,
                                           std::index_sequence<indices...>)
{
    return ok::invoke(std::forward<func_t>(f), std::forward<args_t>(args)...,
                      std::get<indices>(std::forward<tuple_t>(t))...);
}

/// Invoke a callable with a series of arguments followed by an unpack of a
/// tuple
template <class func_t, typename... args_t, class tuple_t>
constexpr decltype(auto) prefix_apply(func_t&& f, args_t&&... args, tuple_t&& t)
{
    return prefix_apply_impl(
        std::forward<func_t>(f), std::forward<args_t>(args)...,
        std::forward<tuple_t>(t),
        std::make_index_sequence<
            std::tuple_size_v<std::remove_reference_t<tuple_t>>>{});
}
} // namespace ok::detail

#endif
