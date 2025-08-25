#ifndef __OKAYLIB_DETAIL_TYPE_TRAITS_H__
#define __OKAYLIB_DETAIL_TYPE_TRAITS_H__

#include "okay/detail/config.h"
#include <cstddef>

namespace ok::stdc {
template <typename T, T v> struct integral_constant
{
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;

    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};
} // namespace ok::stdc

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)

#include <compare> // so later, ordering.h can use std::strong_ordering and co
#include <type_traits>
namespace ok::detail::intrinsic {
template <typename T> inline constexpr bool is_empty = std::is_empty_v<T>;
template <typename T> inline constexpr bool is_final = std::is_final_v<T>;
template <typename T> inline constexpr bool is_enum = std::is_enum_v<T>;
template <typename base_t, typename derived_t>
inline constexpr bool is_base_of = std::is_base_of_v<base_t, derived_t>;
#define __ok_has_trivial_destructor(type) std::is_trivially_destructible_v<type>
#define __ok_has_trivial_constructor(...) \
    std::is_trivially_constructible_v<__VA_ARGS__>
#define __ok_is_trivially_assignable(from, to) \
    std::is_trivially_assignable_v<from, to>
} // namespace ok::detail::intrinsic

#elif defined(OKAYLIB_COMPAT_STRATEGY_NO_STD)
#include "okay/detail/compare.h"
// mimic the things from <type_traits> that we need with compiler intrinsics
namespace ok::detail::intrinsic {
template <typename T>
struct is_empty : public ok::stdc::integral_constant<bool, __is_empty(T)>
{};

template <typename T>
struct is_final : public ok::stdc::integral_constant<bool, __is_final(T)>
{};

template <typename T>
struct is_enum : public ok::stdc::integral_constant<bool, __is_enum(T)>
{};

template <typename base_t, typename derived_t>
struct is_base_of
    : public ok::stdc::integral_constant<bool, __is_base_of(base_t, derived_t)>
{};

template <typename T> inline constexpr bool is_empty = __is_empty(T);
template <typename T> inline constexpr bool is_final = __is_final(T);
template <typename T> inline constexpr bool is_enum = __is_enum(T);
template <typename base_t, typename derived_t>
inline constexpr bool is_base_of = __is_base_of(base_t, derived_t);

// #define __ok_has_trivial_destructor(type) __has_trivial_destructor(type)
#define __ok_has_trivial_destructor(type) __is_trivially_destructible(type)

#define __ok_has_trivial_constructor(...) \
    __is_trivially_constructible(__VA_ARGS__)

#define __ok_is_trivially_assignable(from, to) \
    __is_trivially_assignable(from, to)
} // namespace ok::detail::intrinsic

#elif defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)

// if using pure C++, you cannot detect if a type is final or empty, so they
// always return false.
namespace ok::detail::intrinsic {
template <typename T>
struct is_empty : public ok::stdc::integral_constant<bool, false>
{};
template <typename T>
struct is_final : public ok::stdc::integral_constant<bool, false>
{};
template <typename T>
struct is_enum : public ok::stdc::integral_constant<bool, false>
{};
template <typename base_t, typename derived_t>
struct is_base_of : public ok::stdc::integral_constant<bool, false>
{};
#define __ok_has_trivial_destructor(type) false

#define __ok_has_trivial_constructor(...) false

#define __ok_is_trivially_assignable(from, to) false
} // namespace ok::detail::intrinsic

#endif

namespace ok::stdc {
template <typename T> struct remove_cv
{
    using type = T;
};
template <typename T> struct remove_cv<const T>
{
    using type = T;
};
template <typename T> struct remove_cv<volatile T>
{
    using type = T;
};
template <typename T> struct remove_cv<const volatile T>
{
    using type = T;
};
template <typename T> using remove_cv_t = typename remove_cv<T>::type;

struct true_type : ok::stdc::integral_constant<bool, true>
{};

struct false_type : ok::stdc::integral_constant<bool, false>
{};

template <bool B> using bool_constant = ok::stdc::integral_constant<bool, B>;

template <typename T, typename U> struct is_same : ok::stdc::false_type
{};

template <typename T> struct is_same<T, T> : ok::stdc::true_type
{};

template <typename T> struct remove_reference
{
    using type = T;
};
template <typename T> struct remove_reference<T&>
{
    using type = T;
};
template <typename T> struct remove_reference<T&&>
{
    using type = T;
};
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T> struct add_pointer
{
    using type = typename remove_reference<T>::type*;
};
template <typename T> using add_pointer_t = typename add_pointer<T>::type;

template <typename T> struct is_lvalue_reference : ok::stdc::false_type
{};
template <typename T> struct is_lvalue_reference<T&> : ok::stdc::true_type
{};

template <typename T> struct remove_extent
{
    using type = T;
};
template <typename T, size_t N> struct remove_extent<T[N]>
{
    using type = T;
};
template <typename T> struct remove_extent<T[]>
{
    using type = T;
};
template <typename T> using remove_extent_t = typename remove_extent<T>::type;

template <typename T> struct is_function : public false_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...)> : public true_type
{};
// c style variadic functions
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...)> : public true_type
{};
// cv-qualified function types
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) const> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) volatile> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) const volatile> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) const> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) volatile> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) const volatile> : public true_type
{};

// c++11 ref-qualified function types
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...)&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) const&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) volatile&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) const volatile&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...)&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) const&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) volatile&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) const volatile&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) &&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) const&&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) volatile&&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t...) const volatile&&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) &&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) const&&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) volatile&&> : public true_type
{};
template <typename Ret, typename... args_t>
struct is_function<Ret(args_t..., ...) const volatile&&> : public true_type
{};

template <typename> struct is_void : public false_type
{};
template <> struct is_void<void> : public true_type
{};
template <> struct is_void<const void> : public true_type
{};
template <> struct is_void<volatile void> : public true_type
{};
template <> struct is_void<const volatile void> : public true_type
{};

template <typename T> struct add_rvalue_reference
{
    using type = T&&;
};
template <typename T>
    requires is_void<T>::value
struct add_rvalue_reference<T>
{
    using type = T;
};
template <typename T> struct add_rvalue_reference<T&>
{
    using type = T&;
};
template <typename T> struct add_rvalue_reference<T&&>
{
    using type = T&&;
};
template <typename T>
    requires stdc::is_function<T>::value
struct add_rvalue_reference<T>
{
    using type = T;
};

template <typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

template <typename T> ok::stdc::add_rvalue_reference_t<T> declval() noexcept;

template <typename...> using void_t = void;

template <typename T, typename = void> struct add_lvalue_reference
{
    using type = T;
};

template <typename T> struct add_lvalue_reference<T, void_t<T&>>
{
    using type = T&;
};

template <typename T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template <typename> struct is_const : std::false_type
{};
template <typename T> struct is_const<const T> : std::true_type
{};

template <typename> struct is_reference : public false_type
{};
template <typename T> struct is_reference<T&> : public true_type
{};
template <typename T> struct is_reference<T&&> : public true_type
{};

template <typename T>
struct is_nothrow_move_constructible
    : ok::stdc::bool_constant<noexcept(T(ok::stdc::declval<T&&>()))>
{};

template <typename T>
struct is_nothrow_move_assignable
    : ok::stdc::bool_constant<noexcept(ok::stdc::declval<T&>() =
                                           ok::stdc::declval<T&&>())>
{};

template <typename T>
using is_empty = integral_constant<bool, ok::detail::intrinsic::is_empty<T>>;
template <typename T>
using is_final = integral_constant<bool, ok::detail::intrinsic::is_final<T>>;
template <typename T>
using is_enum = integral_constant<bool, ok::detail::intrinsic::is_enum<T>>;

template <bool B, typename T, typename F> struct conditional
{
    using type = T;
};
template <typename T, typename F> struct conditional<false, T, F>
{
    using type = F;
};

template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

template <bool b, typename T = void> struct enable_if
{};

template <typename T> struct enable_if<true, T>
{
    using type = T;
};

template <bool condition, typename T = void>
using enable_if_t = typename enable_if<condition, T>::type;

template <typename...> using void_t = void;

namespace detail {
template <typename> struct is_array_unknown_bounds : public false_type
{};
template <typename T> struct is_array_unknown_bounds<T[]> : public true_type
{};

template <typename T> struct type_identity
{
    using type = T;
};

template <typename T, typename... args_t> struct is_constructible_impl
{
  private:
    // basic implementation of is_constructible: if the constructor syntax
    // works, then its constructible. there is an exception for references which
    // is the other `struct inner` specialization below
    template <typename U, typename = void> struct inner
    {
        static_assert(!is_reference<U>{}, "bad template specialization");
        template <typename V, typename = void>
        struct inner_again : public std::false_type
        {};
        template <typename V>
        struct inner_again<V, void_t<decltype(V(stdc::declval<args_t>()...))>>
            : public std::true_type
        {};

        using type = inner_again<U>::type;
    };

    // implementation if the type is a reference is that its only going to be
    // true if the construction argument is a single identical reference, or
    // a reference to a derived class
    template <typename U> struct inner<U, enable_if_t<is_reference<U>{}>>
    {
        template <typename... inner_args_t> struct inner_again
        {
            using type = stdc::false_type;
        };

        template <typename single_t> struct inner_again<single_t>
        {
            using type = stdc::integral_constant<
                bool, (stdc::is_same<single_t, U>{} ||
                       ::ok::detail::intrinsic::is_base_of<
                           typename remove_reference<U>::type,
                           typename remove_reference<single_t>::type>)>;
        };

        using type = inner_again<args_t...>::type;
    };

  public:
    using type = inner<T>::type;
};

template <typename T, size_t = sizeof(T)>
constexpr true_type is_complete_or_unbounded(type_identity<T>)
{
    return {};
}

template <typename T, typename nested_t = typename T::type>
constexpr stdc::integral_constant<
    bool, is_reference<nested_t>{} || is_function<nested_t>{} ||
              is_void<nested_t>{} || is_array_unknown_bounds<T>{}>
is_complete_or_unbounded(T)
{
    return {};
}

template <typename from_t, typename to_t> struct is_convertible_impl
{
  private:
    template <typename u_t, typename v_t>
    static auto test_convert(u_t) -> stdc::true_type;

    template <typename u_t, typename v_t>
    static auto test_convert(...) -> stdc::false_type;

    template <typename f_t, typename t_t>
    static auto
    test(int) -> decltype(test_convert<t_t, f_t>(stdc::declval<f_t>()));

    template <typename f_t, typename t_t> static stdc::false_type test(...);

  public:
    using type = decltype(test<from_t, to_t>(0));
};

struct is_implicitly_default_constructible_nontemplated
{
    template <typename T> static void helper(const T&);

    template <typename T>
    static true_type test(const T&, decltype(helper<const T&>({}))* = 0);

    static false_type test(...);
};

template <typename T>
struct is_implicitly_default_constructible_impl
    : public is_implicitly_default_constructible_nontemplated
{
    using type = decltype(test(declval<T>()));
};

// TODO: why is this bit of indirection here. what safety does this provde?
// gcc does this, so i am doing it just in case
template <typename T>
struct is_implicitly_default_constructible_safe
    : public is_implicitly_default_constructible_impl<T>::type
{};

template <typename T, typename U, typename = void>
struct is_assignable_impl : public false_type
{}; // Default to false

// This specialization is chosen if the assignment expression T = U is
// well-formed.
template <typename T, typename U>
struct is_assignable_impl<
    T, U, stdc::void_t<decltype(stdc::declval<T>() = stdc::declval<U>())>>
    : public true_type
{};
} // namespace detail

template <typename T, typename... args_t>
struct is_constructible : detail::is_constructible_impl<T, args_t...>::type
{};

template <typename T, typename... args_t>
struct is_trivially_constructible
    : public std::integral_constant<bool,
                                    __ok_has_trivial_constructor(T, args_t...)>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(
            stdc::detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename from_t, typename to_t>
struct is_convertible : detail::is_convertible_impl<from_t, to_t>::type
{};

template <typename T>
struct is_implicitly_default_constructible
    : public stdc::integral_constant<
          bool, (detail::is_constructible_impl<T>::type::value &&
                 detail::is_implicitly_default_constructible_safe<T>::value)>
{};

template <typename T>
struct is_trivially_copy_constructible
    : public is_trivially_constructible<T, add_lvalue_reference_t<const T>>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_trivially_move_constructible
    : public is_trivially_constructible<T, add_rvalue_reference_t<T>>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename from_t, typename to_t>
struct is_trivially_assignable
    : public stdc::integral_constant<bool,
                                     __ok_is_trivially_assignable(from_t, to_t)>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<from_t>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_trivially_copy_assignable
    : public stdc::integral_constant<bool, __ok_is_trivially_assignable(
                                               add_lvalue_reference_t<T>,
                                               add_lvalue_reference_t<const T>)>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_trivially_move_assignable
    : public stdc::integral_constant<bool, __ok_is_trivially_assignable(
                                               add_lvalue_reference_t<T>,
                                               add_rvalue_reference_t<T>)>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_default_constructible : public is_constructible<T>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_copy_constructible : public is_constructible<T, const T&>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_move_constructible : public is_constructible<T, T&&>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T, typename U>
struct is_assignable : detail::is_assignable_impl<T, U>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_copy_assignable
    : public is_assignable<add_lvalue_reference_t<T>,
                           add_lvalue_reference_t<const T>>
{};

template <typename T>
struct is_move_assignable
    : public is_assignable<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>>
{};

template <typename T> struct is_array : public false_type
{};
template <typename T, size_t N> struct is_array<T[N]> : public true_type
{};
template <typename T> struct is_array<const T[]> : public true_type
{};

template <typename T> struct decay
{
  private:
    // remove reference
    using U = typename stdc::remove_reference<T>::type;

    // array to pointer
    using V = typename stdc::conditional<
        is_array<U>::value,
        typename add_pointer<typename remove_extent<U>::type>::type, U>::type;

    // function to function pointer
    using W =
        typename stdc::conditional<is_function<V>::value,
                                   typename add_pointer<V>::type, V>::type;

  public:
    // remove cv qualification as the last step
    using type = typename remove_cv<W>::type;
};

template <typename T> using decay_t = typename decay<T>::type;

namespace detail {
template <typename> struct is_member_object_pointer_impl : public false_type
{};

template <typename T, typename member_t>
    requires is_function<T>::value
struct is_member_object_pointer_impl<T member_t::*> : public true_type
{};

template <typename> struct is_member_function_pointer_impl : public false_type
{};

template <typename T, typename member_t>
struct is_member_function_pointer_impl<T member_t::*>
    : public is_function<T>::type
{};

template <typename> struct is_pointer_impl : public false_type
{};

template <typename T> struct is_pointer_impl<T*> : public true_type
{};

template <typename> struct is_member_pointer_impl : public false_type
{};

template <typename T, typename member_t>
struct is_member_pointer_impl<T member_t::*> : public true_type
{};
} // namespace detail

template <typename T>
struct is_member_object_pointer
    : public detail::is_member_object_pointer_impl<remove_cv_t<T>>::type
{};

template <typename T>
struct is_member_function_pointer
    : public detail::is_member_function_pointer_impl<remove_cv_t<T>>::type
{};

template <typename T>
struct is_pointer : public detail::is_pointer_impl<remove_cv_t<T>>::type
{};

template <typename T>
struct is_member_pointer
    : public detail::is_member_pointer_impl<remove_cv_t<T>>::type
{};

namespace detail {
template <typename> struct is_integral_impl : public false_type
{};
template <> struct is_integral_impl<bool> : public true_type
{};
template <> struct is_integral_impl<char> : public true_type
{};
template <> struct is_integral_impl<signed char> : public true_type
{};
template <> struct is_integral_impl<unsigned char> : public true_type
{};
template <> struct is_integral_impl<wchar_t> : public true_type
{};
#ifdef _GLIBCXX_USE_CHAR8_T // char8_t is a glibc extension
template <> struct is_integral_impl<char8_t> : public true_type
{};
#endif
template <> struct is_integral_impl<char16_t> : public true_type
{};
template <> struct is_integral_impl<char32_t> : public true_type
{};
template <> struct is_integral_impl<short> : public true_type
{};
template <> struct is_integral_impl<unsigned short> : public true_type
{};
template <> struct is_integral_impl<int> : public true_type
{};
template <> struct is_integral_impl<unsigned int> : public true_type
{};
template <> struct is_integral_impl<long> : public true_type
{};
template <> struct is_integral_impl<unsigned long> : public true_type
{};
template <> struct is_integral_impl<long long> : public true_type
{};
template <> struct is_integral_impl<unsigned long long> : public true_type
{};

template <typename> struct is_floating_point_impl : public false_type
{};
template <> struct is_floating_point_impl<float> : public true_type
{};
template <> struct is_floating_point_impl<double> : public true_type
{};
template <> struct is_floating_point_impl<long double> : public true_type
{};

// NOTE: not supporting other extensions like _Float16, _Float32, Float64,
// _Float128, or __gnu_cxx::__bfloat16_t or __float128

} // namespace detail

template <typename T>
struct is_integral : public detail::is_integral_impl<remove_cv_t<T>>::type
{};

template <typename T>
struct is_floating_point
    : public detail::is_floating_point_impl<remove_cv_t<T>>::type
{};

template <typename T>
struct is_arithmetic
    : public integral_constant<bool, is_floating_point<T>{} || is_integral<T>{}>
{};

using nullptr_t = decltype(nullptr);

template <typename> struct is_null_pointer : public false_type
{};
template <> struct is_null_pointer<stdc::nullptr_t> : public true_type
{};
template <> struct is_null_pointer<const stdc::nullptr_t> : public true_type
{};
template <> struct is_null_pointer<volatile stdc::nullptr_t> : public true_type
{};
template <>
struct is_null_pointer<const volatile stdc::nullptr_t> : public true_type
{};

template <typename T>
struct is_scalar
    : public stdc::integral_constant<
          bool, is_arithmetic<T>{} || is_enum<T>{} || is_pointer<T>{} ||
                    is_member_pointer<T>{} || is_null_pointer<T>{}>
{};

template <typename T> struct remove_all_extents
{
    using type = T;
};
template <typename T, size_t N> struct remove_all_extents<T[N]>
{
    using type = typename remove_all_extents<T>::type;
};
template <typename T> struct remove_all_extents<T[]>
{
    using type = typename remove_all_extents<T>::type;
};

namespace detail {
struct is_destructible_overloaded_function_impl
{
    template <typename T, typename = decltype(declval<T&>().~T())>
    static true_type func(int);

    template <typename> static false_type func(...);
};

template <typename T>
struct is_destructible_impl : public is_destructible_overloaded_function_impl
{
    using type = decltype(func<T>(0));
};

template <typename T,
          bool = (ok::stdc::is_void<T>{} ||
                  ok::stdc::detail::is_array_unknown_bounds<T>{} ||
                  ok::stdc::is_function<T>{}),
          bool = (ok::stdc::is_reference<T>{} || ok::stdc::is_scalar<T>{})>
struct is_destructible_safe;

template <typename T>
struct is_destructible_safe<T, false, false>
    : public is_destructible_impl<
          typename ok::stdc::remove_all_extents<T>::type>::type
{};

template <typename T>
struct is_destructible_safe<T, true, false> : public false_type
{};

template <typename T>
struct is_destructible_safe<T, false, true> : public true_type
{};
} // namespace detail

template <typename T>
struct is_destructible : public detail::is_destructible_safe<T>::type
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(
            stdc::detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T>
struct is_trivially_destructible
    : public std::integral_constant<bool, detail::is_destructible_safe<T>{} &&
                                              __ok_has_trivial_destructor(T)>
{
    static_assert(
        stdc::detail::is_complete_or_unbounded(
            stdc::detail::type_identity<T>{}),
        "template argument must be a complete class or an unbounded array");
};

template <typename T> inline constexpr bool is_void_v = is_void<T>::value;

template <typename T>
inline constexpr bool is_trivially_destructible_v =
    is_trivially_destructible<T>{};

template <typename T>
concept is_reference_c = is_reference<T>::value;

template <typename T>
concept is_const_c = is_const<T>::value;

template <typename T>
concept is_const_reference_c =
    is_reference<T>::value && is_const_c<remove_reference_t<T>>;

template <typename T>
concept is_nonconst_reference_c =
    is_reference<T>::value && !is_const_c<remove_reference_t<T>>;

template <typename T> inline constexpr bool is_scalar_v = is_scalar<T>{};

template <typename T>
inline constexpr bool is_null_pointer_v = is_null_pointer<T>{};

template <typename T>
using remove_all_extents_t = typename remove_all_extents<T>::type;

template <typename T>
inline constexpr bool is_member_function_pointer_v =
    is_member_function_pointer<T>{};
template <typename T>
inline constexpr bool is_member_object_pointer_v =
    is_member_object_pointer<T>{};
template <typename T>
inline constexpr bool is_member_pointer_v = is_member_pointer<T>{};
template <typename T>
inline constexpr bool is_pointer_v = is_member_object_pointer<T>{};

template <typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>{};

template <typename T> inline constexpr bool is_integral_v = is_integral<T>{};

template <typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>{};

template <typename T> inline constexpr bool is_array_v = is_array<T>{};

template <typename T>
inline constexpr bool is_function_v = is_function<T>::value;

template <typename T, typename U>
inline constexpr bool is_assignable_v = is_assignable<T, U>::value;

template <typename T>
inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;

template <typename T>
inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;

template <typename T, typename... args_t>
inline constexpr bool is_constructible_v =
    is_constructible<T, args_t...>::value;

template <typename T>
inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

template <typename T>
inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

template <typename from_t, typename to_t>
inline constexpr bool is_convertible_v = is_convertible<from_t, to_t>::value;

template <typename T, typename... args_t>
inline constexpr bool is_trivially_constructible_v =
    is_trivially_constructible<T, args_t...>::value;

template <typename T>
inline constexpr bool is_implicitly_default_constructible_v =
    is_implicitly_default_constructible<T>::value;

template <typename T>
inline constexpr bool is_trivially_copy_constructible_v =
    is_trivially_copy_constructible<T>::value;

template <typename T>
inline constexpr bool is_trivially_move_constructible_v =
    is_trivially_move_constructible<T>::value;

template <typename T, typename... args_t>
inline constexpr bool is_trivially_assignable_v =
    is_trivially_assignable<T, args_t...>::value;

template <typename T>
inline constexpr bool is_trivially_copy_assignable_v =
    is_trivially_copy_assignable<T>::value;

template <typename T>
inline constexpr bool is_trivially_move_assignable_v =
    is_trivially_move_assignable<T>::value;

template <typename T>
inline constexpr bool is_nothrow_move_assignable_v =
    is_nothrow_move_assignable<T>::value;

template <typename T>
inline constexpr bool is_default_constructible_v =
    is_default_constructible<T>::value;

template <typename T>
inline constexpr bool is_nothrow_move_constructible_v =
    is_nothrow_move_constructible<T>::value;

template <typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

} // namespace ok::stdc

// publicize some declarations
namespace ok {
using true_type = ok::stdc::true_type;
using false_type = ok::stdc::false_type;
} // namespace ok

#endif
