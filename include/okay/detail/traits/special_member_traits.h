#ifndef __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__

#include "okay/detail/template_util/first_type_in_pack.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/status.h"
#include <type_traits>

namespace ok {
template <typename contained_t, typename enum_t, typename> class res;

template <typename T, typename... args_t>
constexpr bool is_std_constructible_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_constructible_v<T, args_t...>;
#else
    std::is_constructible_v<T, args_t...>;
#endif

template <typename T>
constexpr bool is_std_default_constructible_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_default_constructible_v<T>;
#else
    std::is_default_constructible_v<T>;
#endif

template <typename T>
constexpr bool is_std_destructible_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_destructible_v<T>;
#else
    std::is_destructible_v<T>;
#endif

template <typename T, typename... args_t>
constexpr bool is_std_invocable_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_invocable_v<T, args_t...>;
#else
    std::is_invocable_v<T, args_t...>;
#endif

template <typename T, typename return_type, typename... args_t>
constexpr bool is_std_invocable_r_v =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_invocable_r_v<return_type, T, args_t...>;
#else
    std::is_invocable_r_v<return_type, T, args_t...>;
#endif

namespace detail {
template <typename from_t, typename to_t, typename = void>
class is_convertible_to : public std::false_type
{};
template <typename from_t, typename to_t>
class is_convertible_to<
    from_t, to_t,
    std::enable_if_t<
        std::is_convertible_v<from_t, to_t> &&
        std::is_same_v<void, std::void_t<decltype(static_cast<to_t>(
                                 std::declval<from_t>()))>>>>
    : public std::true_type
{};

template <typename from_t, typename to_t>
inline constexpr bool is_convertible_to_v =
    is_convertible_to<from_t, to_t>::value;

template <typename T>
inline constexpr bool is_move_constructible_v = std::is_move_constructible_v<T>;

template <typename LHS, typename RHS>
inline constexpr bool is_assignable_from_v = std::is_assignable_v<LHS, RHS>;

template <typename T>
inline constexpr bool is_swappable_v = std::is_swappable_v<T>;

template <typename T>
inline constexpr bool is_moveable_v =
    std::is_object_v<T> && is_move_constructible_v<T> &&
    std::is_move_assignable_v<T>
    // is_assignable_from_v<T&, T>
    && is_swappable_v<T>;

template <typename maybe_res_t, typename expected_contained_t, typename = void>
struct is_type_res_and_contains : std::false_type
{};
template <typename maybe_res_t, typename expected_contained_t>
struct is_type_res_and_contains<
    maybe_res_t, expected_contained_t,
    std::enable_if_t<
        detail::is_instance_v<maybe_res_t, res> &&
        std::is_same_v<typename maybe_res_t::type, expected_contained_t>>>
    : std::true_type
{};

template <typename constructor_t, typename... args_t>
struct fallible_construction_analyze
{
    template <typename T, typename = void> struct inner
    {
        using return_type = void;
        static constexpr bool is_return_type_valid = false;
    };

    template <typename T>
    struct inner<T, std::void_t<decltype(std::declval<const constructor_t&>()(
                        std::declval<T&>(), std::declval<args_t>()...))>>
    {
        using return_type = decltype(std::declval<const constructor_t&>()(
            std::declval<T&>(), std::declval<args_t>()...));
        static constexpr bool is_return_type_valid =
            is_instance_v<return_type, status>;
    };
};

template <typename T, typename constructor_t, typename... args_t>
using fallible_constructor_return_type_or_void_t =
    typename fallible_construction_analyze<
        constructor_t, args_t...>::template inner<T>::return_type;

struct template_failure
{};

template <typename... args_t>
struct invoke_with_T_ref_return_type_or_template_failure
{
    template <typename T, typename constructor_t, typename = void> struct inner
    {
        using type = template_failure;
    };

    template <typename T, typename constructor_t>
    struct inner<
        T, constructor_t,
        std::enable_if_t<is_std_invocable_r_v<T, constructor_t, T&, args_t...>>>
    {
        using type = decltype(std::declval<const constructor_t&>()(
            std::declval<T&>(), std::declval<args_t>()...));
    };
};

template <typename T, typename constructor_t, typename... args_t>
using invoke_with_T_ref_return_type_or_template_failure_t =
    typename invoke_with_T_ref_return_type_or_template_failure<
        args_t...>::template inner<T, constructor_t>::type;

template <typename constructor_t, typename... args_t>
struct infallible_construction_analyze
{
  private:
    template <typename T, typename = void>
    struct analyze_inplace : public std::false_type
    {
        using return_type = void;
        static constexpr bool is_return_type_valid = false;
        static constexpr bool is_inplace = false;
    };

    template <typename T>
    struct analyze_inplace<
        T, std::enable_if_t<!std::is_same_v<
               invoke_with_T_ref_return_type_or_template_failure_t<
                   T, constructor_t, args_t...>,
               template_failure>>> : public std::true_type
    {
        using return_type = invoke_with_T_ref_return_type_or_template_failure_t<
            T, constructor_t, args_t...>;
        static constexpr bool is_return_type_valid =
            std::is_void_v<return_type>;
        static constexpr bool is_inplace = true;
    };

    template <typename T, typename = void>
    struct analyze_returning_constructor : public std::false_type
    {
        using return_type = void;
        static constexpr bool is_return_type_valid = false;
        static constexpr bool is_inplace = false;
    };

    template <typename T>
    struct analyze_returning_constructor<
        T, std::enable_if_t<is_std_invocable_r_v<constructor_t, T, args_t...>>>
        : public std::true_type
    {
        using return_type = decltype(std::declval<const constructor_t&>()(
            std::declval<args_t>()...));
        static constexpr bool is_return_type_valid =
            std::is_same_v<T, return_type>;
        static constexpr bool is_inplace = false;
    };

  public:
    template <typename T, typename = void> struct inner
    {
        using return_type = void;
        static constexpr bool is_return_type_valid = false;
        static constexpr bool is_inplace = false;
    };

    template <typename T>
    struct inner<T, std::enable_if_t<analyze_returning_constructor<T>::value ||
                                     analyze_inplace<T>::value>>
        : public std::conditional_t<analyze_returning_constructor<T>::value,
                                    analyze_returning_constructor<T>,
                                    analyze_inplace<T>>
    {};
};

/// NOTE: this may be void but actually be valid, check
/// is_valid_infallible_construction_v
template <typename T, typename constructor_t, typename... args_t>
using infallible_constructor_return_type_or_void_t =
    typename infallible_construction_analyze<
        constructor_t, args_t...>::template inner<T>::return_type;

template <typename T, typename constructor_t, typename... args_t>
inline constexpr bool is_valid_infallible_inplace_construction_v =
    infallible_construction_analyze<constructor_t,
                                    args_t...>::template inner<T>::is_inplace;

template <typename T, typename = void> struct has_default_infallible_construct
{
    using return_type = void;
    static constexpr bool is_return_type_valid = false;
};

template <typename T, typename... args_t>
auto is_valid_fallible_construction_safe_args()
{
    if constexpr (sizeof...(args_t) == 0) {
        return std::false_type{};
    } else {
        if constexpr (detail::fallible_construction_analyze<
                          args_t...>::template inner<T>::is_return_type_valid) {
            return std::true_type{};
        } else {
            return std::false_type{};
        }
    }
}

template <typename T, typename... args_t>
auto is_valid_infallible_construction_safe_args()
{
    if constexpr (sizeof...(args_t) == 0) {
        return std::false_type{};
    } else {
        if constexpr (detail::infallible_construction_analyze<
                          args_t...>::template inner<T>::is_return_type_valid) {
            return std::true_type{};
        } else {
            return std::false_type{};
        }
    }
}
} // namespace detail

template <typename T, typename constructor_t, typename... args_t>
inline constexpr bool is_valid_fallible_construction_v =
    decltype(detail::is_valid_fallible_construction_safe_args<
             T, constructor_t, args_t...>())::value;

template <typename T, typename constructor_t, typename... args_t>
inline constexpr bool is_valid_infallible_construction_v =
    decltype(detail::is_valid_infallible_construction_safe_args<
             T, constructor_t, args_t...>())::value;

namespace detail {
template <typename constructor_t, typename... args_t>
struct is_inplace_factory_constructible
{
    template <typename T, typename = void> struct inner : std::false_type
    {};

    template <typename T>
    struct inner<
        T, std::enable_if_t<
               is_valid_fallible_construction_v<T, constructor_t> ||
               detail::infallible_construction_analyze<
                   constructor_t, args_t...>::template inner<T>::is_inplace>>
        : std::true_type
    {};
};

template <typename... args_t> struct is_inplace_factory_constructible_safe_args
{
    template <typename T, typename = void> struct inner : std::false_type
    {};

    template <typename T>
    struct inner<T, std::void_t<typename is_inplace_factory_constructible<
                        args_t...>::template inner<T>::value>> : std::true_type
    {};
};

} // namespace detail

template <typename T, typename... args_t>
inline constexpr bool is_inplace_constructible_v =
    decltype(detail::is_valid_fallible_construction_safe_args<
             T, args_t...>())::value ||
    detail::is_inplace_factory_constructible_safe_args<
        args_t...>::template inner<T>::value ||
    is_std_constructible_v<T, args_t...>;

template <typename T, typename... args_t>
inline constexpr bool is_infallible_constructible_v =
    // can be constructed using the first argument as a factory
    decltype(detail::is_valid_infallible_construction_safe_args<
             T, args_t...>())::value ||
    // can be constructed normally
    is_std_constructible_v<T, args_t...>;

template <typename T, typename... args_t>
inline constexpr bool is_fallible_constructible_v =
    // can be fallible constructed using the first argument as a factory
    decltype(detail::is_valid_fallible_construction_safe_args<
             T, args_t...>())::value;

template <typename T, typename... args_t>
inline constexpr bool is_constructible_v =
    is_infallible_constructible_v<T, args_t...> ||
    is_fallible_constructible_v<T, args_t...>;

} // namespace ok

#endif
