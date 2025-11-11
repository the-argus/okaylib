#ifndef __OKAYLIB_DETAIL_INVOKE_H__
#define __OKAYLIB_DETAIL_INVOKE_H__

#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/type_traits.h"
#include "okay/detail/utility.h"

namespace ok {

namespace detail {

// TODO: reduce this, it is basically copied from gcc implementation which is
// compatible with old versions of C++. realistically a lot of this could be
// implemented in a constexpr function with `if constexpr` and `requires`. maybe
// even result_of could be entirely removed in favor of auto / decltype(auto)
// return value?

template <typename T> struct success_type_t
{
    using type = T;
};

struct failure_type_t
{};

// marker types
struct invoke_other_t
{};
struct invoke_memfun_ref_t
{};
struct invoke_memfun_deref_t
{};
struct invoke_memobj_ref_t
{};
struct invoke_memobj_deref_t
{};

template <typename T, typename tag_t>
struct result_of_success_t : success_type_t<T>
{
    using invoke_type = tag_t;
};

struct result_of_memfun_ref_impl_t
{
    template <typename fun_ptr_t, typename T1, typename... args_t>
    static result_of_success_t<
        decltype((std::declval<T1>().*
                  std::declval<fun_ptr_t>())(std::declval<args_t>()...)),
        invoke_memfun_ref_t>
    test_unevaluatedctx(int);

    template <typename...> static failure_type_t test_unevaluatedctx(...);
};

template <typename mem_fun_ptr_t, typename arg_t, typename... args_t>
struct result_of_memfun_ref_t : private result_of_memfun_ref_impl_t
{
    typedef decltype(test_unevaluatedctx<mem_fun_ptr_t, arg_t, args_t...>(
        0)) type;
};

struct result_of_memfun_deref_impl_t
{
    template <typename fun_ptr_t, typename T1, typename... args_t>
    static result_of_success_t<
        decltype(((*std::declval<T1>()).*
                  std::declval<fun_ptr_t>())(std::declval<args_t>()...)),
        invoke_memfun_deref_t>
    test_unevaluatedctx(int);

    template <typename...> static failure_type_t test_unevaluatedctx(...);
};

template <typename mem_fun_ptr_t, typename arg_t, typename... args_t>
struct result_of_memfun_deref_t : private result_of_memfun_deref_impl_t
{
    typedef decltype(test_unevaluatedctx<mem_fun_ptr_t, arg_t, args_t...>(
        0)) type;
};

// [func.require] paragraph 1 bullet 3:
struct result_of_memobj_ref_impl_t
{
    template <typename fun_ptr_t, typename T1>
    static result_of_success_t<decltype(std::declval<T1>().*
                                        std::declval<fun_ptr_t>()),
                               invoke_memobj_ref_t>
    test_unevaluatedctx(int);

    template <typename, typename>
    static failure_type_t test_unevaluatedctx(...);
};

template <typename mem_fun_ptr_t, typename arg_t>
struct result_of_memobj_ref_t : private result_of_memobj_ref_impl_t
{
    typedef decltype(test_unevaluatedctx<mem_fun_ptr_t, arg_t>(0)) type;
};

// [func.require] paragraph 1 bullet 4:
struct result_of_memobj_deref_impl_t
{
    template <typename fun_ptr_t, typename T1>
    static result_of_success_t<decltype((*std::declval<T1>()).*
                                        std::declval<fun_ptr_t>()),
                               invoke_memobj_deref_t>
    test_unevaluatedctx(int);

    template <typename, typename>
    static failure_type_t test_unevaluatedctx(...);
};

template <typename mem_fun_ptr_t, typename arg_t>
struct result_of_memobj_deref_t : private result_of_memobj_deref_impl_t
{
    typedef decltype(test_unevaluatedctx<mem_fun_ptr_t, arg_t>(0)) type;
};

template <typename mem_fun_ptr_t, typename arg_t> struct result_of_memobj_t;

template <typename result_t, typename class_t, typename arg_t>
struct result_of_memobj_t<result_t class_t::*, arg_t>
{
    typedef remove_cvref_t<arg_t> argval_t;
    typedef result_t class_t::*mem_fun_ptr_t;
    using type = stdc::conditional_t<
        ok::stdc::is_same_v<argval_t, class_t> ||
            ok::detail::is_derived_from_c<argval_t, class_t>,
        result_of_memobj_ref_t<mem_fun_ptr_t, arg_t>,
        result_of_memobj_deref_t<mem_fun_ptr_t, arg_t>>;
};

template <typename mem_fun_ptr_t, typename arg_t, typename... args_t>
struct result_of_memfun_t;

template <typename result_t, typename class_t, typename arg_t,
          typename... args_t>
struct result_of_memfun_t<result_t class_t::*, arg_t, args_t...>
{
    typedef stdc::remove_reference_t<arg_t> argval_t;
    typedef result_t class_t::*mem_fun_ptr_t;
    using type = typename stdc::conditional_t<
        ok::detail::is_derived_from_c<argval_t, class_t>,
        result_of_memfun_ref_t<mem_fun_ptr_t, arg_t, args_t...>,
        result_of_memfun_deref_t<mem_fun_ptr_t, arg_t, args_t...>>::type;
};

template <bool, bool, typename callable_t, typename... args_t>
struct result_of_impl_t
{
    typedef failure_type_t type;
};

template <typename mem_fun_ptr_t, typename arg_t>
struct result_of_impl_t<true, false, mem_fun_ptr_t, arg_t>
    : public result_of_memobj_t<stdc::decay_t<mem_fun_ptr_t>, arg_t>
{};

template <typename mem_fun_ptr_t, typename arg_t, typename... args_t>
struct result_of_impl_t<false, true, mem_fun_ptr_t, arg_t, args_t...>
    : public result_of_memfun_t<stdc::decay_t<mem_fun_ptr_t>, arg_t, args_t...>
{};

struct result_of_other_impl_t
{
    template <typename _Fn, typename... args_t>
    static result_of_success_t<
        decltype(std::declval<_Fn>()(std::declval<args_t>()...)),
        invoke_other_t>
    test_unevaluatedctx(int);

    template <typename...> static failure_type_t test_unevaluatedctx(...);
};

template <typename callable_t, typename... args_t>
struct result_of_impl_t<false, false, callable_t, args_t...>
    : private result_of_other_impl_t
{
    typedef decltype(test_unevaluatedctx<callable_t, args_t...>(0)) type;
};

template <typename callable_t, typename... args_t>
struct invoke_result_t
    : public result_of_impl_t<stdc::is_member_object_pointer_v<
                                  stdc::remove_reference_t<callable_t>>,
                              stdc::is_member_function_pointer_v<
                                  stdc::remove_reference_t<callable_t>>,
                              callable_t, args_t...>::type
{};

template <typename result_t, typename function_t, typename... args_t>
constexpr result_t invoke_impl(invoke_other_t, function_t&& fun_ptr,
                               args_t&&... args)
{
    return stdc::forward<function_t>(fun_ptr)(stdc::forward<args_t>(args)...);
}

template <typename result_t, typename member_fun_t, typename T,
          typename... args_t>
constexpr result_t invoke_impl(invoke_memfun_ref_t, member_fun_t&& fun_ptr,
                               T&& value, args_t&&... args)
{
    return (stdc::forward<T>(value).*fun_ptr)(stdc::forward<args_t>(args)...);
}

template <typename result_t, typename member_fun_t, typename T,
          typename... args_t>
constexpr result_t invoke_impl(invoke_memfun_deref_t, member_fun_t&& fun_ptr,
                               T&& value, args_t&&... args)
{
    return ((*stdc::forward<T>(value)).*
            fun_ptr)(stdc::forward<args_t>(args)...);
}

template <typename result_t, typename member_ptr_t, typename T>
constexpr result_t invoke_impl(invoke_memobj_ref_t, member_ptr_t&& fun_ptr,
                               T&& value)
{
    return stdc::forward<T>(value).*fun_ptr;
}

template <typename result_t, typename member_ptr_t, typename T>
constexpr result_t invoke_impl(invoke_memobj_deref_t, member_ptr_t&& fun_ptr,
                               T&& value)
{
    return (*stdc::forward<T>(value)).*fun_ptr;
}

} // namespace detail

template <typename callable_t, typename... args_t>
constexpr typename detail::invoke_result_t<callable_t, args_t...>::type
invoke(callable_t&& callable, args_t&&... args) OKAYLIB_NOEXCEPT
{
    using result_t = detail::invoke_result_t<callable_t, args_t...>;
    using type = typename result_t::type;
    using tag = typename result_t::invoke_type;
    return ok::detail::invoke_impl<type>(tag{},
                                         stdc::forward<callable_t>(callable),
                                         stdc::forward<args_t>(args)...);
}

} // namespace ok

#endif
