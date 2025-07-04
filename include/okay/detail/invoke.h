#ifndef __OKAYLIB_DETAIL_INVOKE_H__
#define __OKAYLIB_DETAIL_INVOKE_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/detail/type_traits.h"
#include "okay/detail/utility.h"

namespace ok {

// invoker for member function pointers
template <typename F, typename T1, typename... Args>
    requires(std::is_member_function_pointer_v<stdc::decay_t<F>> &&
             is_std_invocable_c<F, T1, Args...>)
constexpr decltype(auto) invoke_impl(F&& f, T1&& t1, Args&&... args)
{
    if constexpr (stdc::is_pointer_v<stdc::decay_t<T1>>) {
        return (stdc::forward<T1>(t1)->*stdc::forward<F>(f))(
            stdc::forward<Args>(args)...);
    } else {
        return (stdc::forward<T1>(t1).*
                stdc::forward<F>(f))(stdc::forward<Args>(args)...);
    }
}

// invoker for member object pointers
template <typename F, typename T1>
    requires(stdc::is_member_object_pointer_v<stdc::decay_t<F>> &&
             is_std_invocable_c<F, T1>)
constexpr decltype(auto) invoke_impl(F&& f, T1&& t1)
{
    if constexpr (stdc::is_pointer_v<stdc::decay_t<T1>>) {
        return stdc::forward<T1>(t1)->*stdc::forward<F>(f);
    } else {
        return stdc::forward<T1>(t1).*stdc::forward<F>(f);
    }
}

// functions, lambdas, function objects
template <typename F, typename... Args>
    requires(!stdc::is_member_pointer_v<stdc::decay_t<F>> &&
             is_std_invocable_c<F, Args...>)
constexpr decltype(auto) invoke_impl(F&& f, Args&&... args)
{
    return stdc::forward<F>(f)(stdc::forward<Args>(args)...);
}

template <typename F, typename... Args>
constexpr decltype(auto) invoke(F&& f, Args&&... args) OKAYLIB_NOEXCEPT
    requires is_std_invocable_c<F, Args...>
{
    return invoke_impl(stdc::forward<F>(f), stdc::forward<Args>(args)...);
}

} // namespace ok

#endif
