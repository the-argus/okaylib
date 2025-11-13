#ifndef __OKAYLIB_DETAIL_INVOKE_H__
#define __OKAYLIB_DETAIL_INVOKE_H__

#include "okay/detail/utility.h"

namespace ok {

namespace detail {

template <typename callable_t, typename self_t, typename... remaining_args_t>
constexpr void invoke_function_that_works_for_memfun_with_deref(
    callable_t&& callable_inner, self_t&& self,
    remaining_args_t&&... remaining_args)
    requires requires {
        ((*stdc::forward<self_t>(self)).*
         callable_inner)(stdc::forward<remaining_args_t>(remaining_args)...);
    };

template <typename callable_t, typename self_t, typename... remaining_args_t>
constexpr void invoke_function_that_works_for_normal_memfun(
    callable_t&& callable_inner, self_t&& self,
    remaining_args_t&&... remaining_args)
    requires requires {
        (stdc::forward<self_t>(self).*
         callable_inner)(stdc::forward<remaining_args_t>(remaining_args)...);
    };

template <typename callable_t, typename self_t>
constexpr void invoke_function_that_works_for_memobj_ptr_with_deref(
    callable_t&& callable_inner, self_t&& self)
    requires requires { (*stdc::forward<self_t>(self)).*callable_inner; };

template <typename callable_t, typename self_t>
constexpr void
invoke_function_that_works_for_normal_memobj_ptr(callable_t&& callable_inner,
                                                 self_t&& self)
    requires requires { stdc::forward<self_t>(self).*callable_inner; };

template <typename callable_t, typename... args_t>
constexpr inline bool invoke_is_memfun_with_deref = requires {
    invoke_function_that_works_for_memfun_with_deref(
        stdc::declval<callable_t>(), stdc::declval<args_t>()...);
};

template <typename callable_t, typename... args_t>
constexpr inline bool invoke_is_normal_memfun = requires {
    invoke_function_that_works_for_normal_memfun(stdc::declval<callable_t>(),
                                                 stdc::declval<args_t>()...);
};

template <typename callable_t, typename... args_t>
constexpr inline bool invoke_is_memobj_ptr = requires {
    invoke_function_that_works_for_normal_memobj_ptr(
        stdc::declval<callable_t>(), stdc::declval<args_t>()...);
};

template <typename callable_t, typename... args_t>
constexpr inline bool invoke_is_memobj_ptr_with_deref = requires {
    invoke_function_that_works_for_memobj_ptr_with_deref(
        stdc::declval<callable_t>(), stdc::declval<args_t>()...);
};

template <typename callable_t, typename... args_t>
constexpr inline bool invoke_is_normal_callable =
    requires { stdc::declval<callable_t>()(stdc::declval<args_t>()...); };

template <typename callable_t, typename... args_t>
concept invocable_c = requires {
    requires invoke_is_normal_callable<callable_t, args_t...> ||
                 invoke_is_memobj_ptr<callable_t, args_t...> ||
                 invoke_is_memobj_ptr_with_deref<callable_t, args_t...> ||
                 invoke_is_memfun_with_deref<callable_t, args_t...> ||
                 invoke_is_normal_memfun<callable_t, args_t...>;
};

} // namespace detail

template <typename callable_t, typename... args_t>
constexpr decltype(auto) invoke(callable_t&& callable,
                                args_t&&... args) OKAYLIB_NOEXCEPT
    requires ok::detail::invocable_c<decltype(callable), decltype(args)...>
{
    using namespace detail;

    constexpr bool is_normal_callable =
        invoke_is_normal_callable<decltype(callable), decltype(args)...>;
    constexpr bool is_memobj_ptr =
        invoke_is_memobj_ptr<decltype(callable), decltype(args)...>;
    constexpr bool is_memobj_ptr_with_deref =
        invoke_is_memobj_ptr_with_deref<decltype(callable), decltype(args)...>;
    constexpr bool is_memfun_with_deref =
        invoke_is_memfun_with_deref<decltype(callable), decltype(args)...>;
    constexpr bool is_normal_memfun =
        invoke_is_normal_memfun<decltype(callable), decltype(args)...>;

    static_assert((int(is_normal_callable) + int(is_memobj_ptr_with_deref) +
                   int(is_memobj_ptr) + int(is_memfun_with_deref) +
                   int(is_normal_memfun)) == 1,
                  "multiple types of function matched the given callable, "
                  "there is a bug in template logic");

    if constexpr (is_memfun_with_deref) {
        constexpr auto invoke_memfun_with_deref =
            []<typename self_t, typename... remaining_args_t>(
                callable_t&& callable, self_t&& self,
                remaining_args_t&&... remaining_args) {
                return ((*stdc::forward<self_t>(self)).*callable)(
                    stdc::forward<remaining_args_t>(remaining_args)...);
            };
        return invoke_memfun_with_deref(stdc::forward<callable_t>(callable),
                                        stdc::forward<args_t>(args)...);
    } else if constexpr (is_normal_memfun) {
        constexpr auto invoke_normal_memfun =
            []<typename self_t, typename... remaining_args_t>(
                callable_t&& callable_inner, self_t&& self,
                remaining_args_t&&... remaining_args) {
                return (stdc::forward<self_t>(self).*callable_inner)(
                    stdc::forward<remaining_args_t>(remaining_args)...);
            };
        return invoke_normal_memfun(stdc::forward<callable_t>(callable),
                                    stdc::forward<args_t>(args)...);
    } else if constexpr (is_memobj_ptr) {
        constexpr auto invoke_memobj_ptr =
            []<typename self_t>(callable_t&& callable_inner, self_t&& self) {
                return stdc::forward<self_t>(self).*callable_inner;
            };
        return invoke_memobj_ptr(stdc::forward<callable_t>(callable),
                                 stdc::forward<args_t>(args)...);
    } else if constexpr (is_memobj_ptr_with_deref) {
        constexpr auto invoke_memobj_ptr_with_deref =
            []<typename self_t>(callable_t&& callable_inner, self_t&& self) {
                return (*stdc::forward<self_t>(self)).*callable_inner;
            };
        return invoke_memobj_ptr_with_deref(stdc::forward<callable_t>(callable),
                                            stdc::forward<args_t>(args)...);
    } else {
        static_assert(is_normal_callable);
        return stdc::forward<callable_t>(callable)(
            stdc::forward<args_t>(args)...);
    }
}

} // namespace ok

#endif
