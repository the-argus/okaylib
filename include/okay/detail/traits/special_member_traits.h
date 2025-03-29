#ifndef __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__

#include "okay/detail/template_util/first_type_in_pack.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_derived_from.h"
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

struct bad_construction_analysis : public std::false_type
{
    constexpr static bool has_inplace = false;
    constexpr static bool has_rvo = false;
    constexpr static bool can_fail = false;
    using associated_type = void;
};

struct stdstyle_construction_analysis : public std::true_type
{
    constexpr static bool has_inplace = true;
    constexpr static bool has_rvo = false;
    constexpr static bool can_fail = false;
    using associated_type = void;
};

template <typename... args_t> struct constructor_analysis
{
    template <typename constructor_t, typename = void>
    struct make_fn_analysis : public std::false_type
    {
        using return_type = void;
    };
    template <typename constructor_t>
    struct make_fn_analysis<
        constructor_t,
        std::void_t<decltype(std::declval<const constructor_t&>().make(
            std::declval<args_t>()...))>> : public std::true_type
    {
        using return_type = decltype(std::declval<const constructor_t&>().make(
            std::declval<args_t>()...));

        static_assert(!detail::is_instance_v<return_type, res> &&
                          !detail::is_instance_v<return_type, status>,
                      "Do not return a status or res from a make() function- "
                      "this function is only used for non-failing constructors "
                      "and should return the constructed object directly.");
    };

    template <typename T, typename constructor_t, typename = void>
    struct has_nonconst_make_into_uninit : public std::false_type
    {};
    template <typename T, typename constructor_t>
    struct has_nonconst_make_into_uninit<
        T, constructor_t,
        std::void_t<decltype(std::declval<constructor_t&>().make_into_uninit(
            std::declval<T&>(), std::declval<args_t>()...))>>
        : public std::true_type
    {};

    template <typename T, typename constructor_t, typename = void>
    struct has_nonconst_make : public std::false_type
    {};
    template <typename T, typename constructor_t>
    struct has_nonconst_make<
        T, constructor_t,
        std::void_t<decltype(std::declval<constructor_t&>().make(
            std::declval<args_t>()...))>> : public std::true_type
    {};

    template <typename T, typename constructor_t, typename = void>
    struct make_into_uninit_fn_analysis : public std::false_type
    {
        using return_type = void;
    };
    template <typename T, typename constructor_t>
    struct make_into_uninit_fn_analysis<
        T, constructor_t,
        std::void_t<
            decltype(std::declval<const constructor_t&>().make_into_uninit(
                std::declval<T&>(), std::declval<args_t>()...))>>
        : public std::true_type
    {
        using return_type =
            decltype(std::declval<const constructor_t&>().make_into_uninit(
                std::declval<T&>(), std::declval<args_t>()...));
        static_assert(std::is_void_v<return_type> ||
                          detail::is_instance_v<return_type, status>,
                      "Return type from make_into_uninit() should be void (if "
                      "the function can't fail) or an ok::status.");
    };

    template <typename constructor_t, typename = void>
    struct associated_type_by_explicit_decl : public std::false_type
    {
        using type = void;
    };

    template <typename constructor_t>
    struct associated_type_by_explicit_decl<
        constructor_t,
        std::void_t<
            typename constructor_t::template associated_type<args_t...>>>
        : public std::true_type
    {
        using type =
            typename constructor_t::template associated_type<args_t...>;
    };

    template <typename constructor_t, typename = void>
    struct associated_type : public std::false_type
    {
        using type =
            typename associated_type_by_explicit_decl<constructor_t>::type;
    };

    template <typename constructor_t>
    struct associated_type<
        constructor_t,
        std::enable_if_t<make_fn_analysis<constructor_t>::value &&
                         !associated_type_by_explicit_decl<constructor_t>{}>>
        : public std::true_type
    {
        using type = typename make_fn_analysis<constructor_t>::return_type;
        static_assert(!detail::is_instance_v<type, status> &&
                          !detail::is_instance_v<type, res>,
                      "make() function should not return an error type- you "
                      "probably want to implement make_into_uninit and then "
                      "simly use ok::make to return a res<...> on the stack.");
    };

    template <typename constructor_t, typename = void>
    struct inner : public bad_construction_analysis
    {};

    template <typename constructor_t>
    struct inner<constructor_t> : public std::true_type
    {
        using associated_type = typename associated_type<constructor_t>::type;

        constexpr static bool has_rvo = make_fn_analysis<constructor_t>::value;
        constexpr static bool has_inplace =
            make_into_uninit_fn_analysis<associated_type, constructor_t>::value;

        // debug values just for static_asserts, in case you forget to mark a
        // make / make_into_uninit function as const
        constexpr static bool has_nonconst_make_debug =
            has_nonconst_make<associated_type, constructor_t>::value;
        constexpr static bool has_nonconst_make_into_uninit_debug =
            has_nonconst_make_into_uninit<associated_type,
                                          constructor_t>::value;

        static_assert(has_rvo || !has_nonconst_make_debug,
                      "Missing const specifier on make() method.");
        static_assert(has_inplace || !has_nonconst_make_into_uninit_debug,
                      "Missing const specifier on make_into_uninit() method.");

        constexpr static bool can_fail =
            has_inplace && !has_rvo &&
            detail::is_instance_v<
                typename make_into_uninit_fn_analysis<
                    associated_type, constructor_t>::return_type,
                status>;

        static_assert(can_fail || !has_inplace ||
                          std::is_void_v<typename make_into_uninit_fn_analysis<
                              associated_type, constructor_t>::return_type>,
                      "For a non-failing in-place constructor, the return type "
                      "should be void.");
    };
};

template <typename... args_t> auto analyze_construction()
{
    if constexpr (sizeof...(args_t) == 0) {
        return bad_construction_analysis{};
    } else {
        constexpr auto split = [](auto&& constructor, auto&&... args) ->
            typename constructor_analysis<decltype(args)...>::template inner<
                detail::remove_cvref_t<decltype(constructor)>> { return {}; };
        using out_t = decltype(split(std::declval<args_t>()...));
        return out_t{};
    }
}

} // namespace detail

template <typename T, typename... args_t>
inline constexpr bool is_infallible_constructible_v =
    (decltype(detail::analyze_construction<args_t...>())::value &&
     !decltype(detail::analyze_construction<args_t...>())::can_fail &&
     std::is_same_v<T, typename decltype(detail::analyze_construction<
                                         args_t...>())::associated_type>) ||
    is_std_constructible_v<T, args_t...>;

template <typename T, typename... args_t>
inline constexpr bool is_fallible_constructible_v =
    (decltype(detail::analyze_construction<args_t...>())::value &&
     decltype(detail::analyze_construction<args_t...>())::can_fail &&
     std::is_same_v<T, typename decltype(detail::analyze_construction<
                                         args_t...>())::associated_type>);

template <typename T, typename... args_t>
inline constexpr bool is_constructible_v =
    is_infallible_constructible_v<T, args_t...> ||
    is_fallible_constructible_v<T, args_t...>;

} // namespace ok

#endif
