#ifndef __OKAYLIB_DETAIL_TRAITS_CONSTRUCTOR_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_CONSTRUCTOR_TRAITS_H__

#include "okay/detail/traits/error_traits.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/special_member_traits.h"
#include <type_traits>

namespace ok {
template <status_enum_c enum_t> class status;
}

namespace ok::detail {
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

        static_assert(!status_object_c<return_type>,
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
        static_assert(ok::stdc::is_void_v<return_type> ||
                          status_type_c<return_type>,
                      "Return type from make_into_uninit() should be void (if "
                      "the function can't fail) or a status object.");
        static_assert(
            !ok::detail::is_instance_c<return_type, ok::status>,
            "Avoid returning an ok::status from make_into_uninit(), "
            "just return the enum type directly instead (the return "
            "type should match the second template argument of the "
            "res that you want to be returned when you construct "
            "this object with ok::make(), like: ok::res<T, return_type>)");
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
    struct associated_type
        : public associated_type_by_explicit_decl<constructor_t>
    {};

    template <typename constructor_t>
    struct associated_type<
        constructor_t,
        std::enable_if_t<
            make_fn_analysis<constructor_t>::value &&
            !associated_type_by_explicit_decl<constructor_t>::value>>
        : public std::true_type
    {
        using type = typename make_fn_analysis<constructor_t>::return_type;
        static_assert(!status_object_c<type>,
                      "make() function should not return an error type- you "
                      "probably want to implement make_into_uninit and then "
                      "simply use ok::make to return a res<...> on the stack.");
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
            status_type_c<typename make_into_uninit_fn_analysis<
                associated_type, constructor_t>::return_type>;

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
} // namespace ok::detail

namespace ok {
template <typename T, typename... args_t>
concept is_infallible_constructible_c = requires {
    requires(decltype(detail::analyze_construction<args_t...>())::value &&
             !decltype(detail::analyze_construction<args_t...>())::can_fail &&
             std::is_same_v<
                 T, typename decltype(detail::analyze_construction<
                                      args_t...>())::associated_type>) ||
                is_std_constructible_c<T, args_t...>;
};

template <typename T, typename... args_t>
concept is_fallible_constructible_c = requires {
    requires decltype(detail::analyze_construction<args_t...>())::value;
    requires decltype(detail::analyze_construction<args_t...>())::can_fail;
    requires std::is_same_v<T,
                            typename decltype(detail::analyze_construction<
                                              args_t...>())::associated_type>;
};

template <typename T, typename... args_t>
concept is_constructible_c = requires {
    requires(is_infallible_constructible_c<T, args_t...> ||
             is_fallible_constructible_c<T, args_t...>);
};
} // namespace ok

#endif
