#ifndef __OKAYLIB_MACROS_DEFER_H__
#define __OKAYLIB_MACROS_DEFER_H__

#include "okay/defer.h"
#include "okay/detail/unique_id.h"

namespace ok::detail {
template <bool reference> struct make_defer
{
    template <typename callable_t>
    constexpr static auto call(callable_t&& callable) noexcept
    {
        if constexpr (reference) {
            static_assert(!std::is_rvalue_reference_v<decltype(callable)>);
            return defer<callable_t, true>(std::forward<callable_t>(callable));
        } else {
            return defer<callable_t, false>(std::forward<callable_t>(callable));
        }
    }
};
} // namespace ok::detail

/// Make a defer object with a given name (so it can be addressed/cancelled
/// later) and a lambda/callable with capture group specified. The lambda must
/// be wrapped in parens like so: ok_maydefer_ex(myname, ([...]{...}))
#define ok_named_defer_ex(name, lambda)     \
    const auto& __lambda_##name = (lambda); \
    auto name = ok::detail::make_defer<true>::call(__lambda_##name);

/// Create a deferred block of code which can be addressed by the given name in
/// order to be potentially cancelled later in the function.
#define ok_named_defer(name) defer name = [&]

/// Defer a block of code until the end of scope, like so:
/// `ok_defer { /*...*/ };`
#define ok_defer defer defer_##__FUNCTION__##__OKAYLIB_UNIQUE_ID = [&]

#endif
