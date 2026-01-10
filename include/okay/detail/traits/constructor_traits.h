#ifndef __OKAYLIB_DETAIL_TRAITS_CONSTRUCTOR_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_CONSTRUCTOR_TRAITS_H__

#include "okay/detail/template_util/first_type_in_pack.h"
#include "okay/detail/traits/error_traits.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/special_member_traits.h"

namespace ok {
template <status_enum_c enum_t> class status;
enum class implemented_make_function
{
    make,
    make_into_uninit,
};
} // namespace ok

namespace ok::detail {
struct bad_construction_analysis : public ok::false_type
{
    constexpr static bool has_inplace = false;
    constexpr static bool has_rvo = false;
    constexpr static bool can_fail = false;
    using associated_type = void;
};

template <typename constructor_t, typename... args_t>
concept has_make_fn_c = requires(const constructor_t& c) {
    { c.make(stdc::declval<args_t>()...) };
};

template <typename constructor_t>
concept has_implemented_make_function_decl_c = requires {
    {
        constructor_t::implemented_make_function
    } -> same_as_c<ok::implemented_make_function>;
};

template <typename constructor_t>
concept marked_as_having_make_fn_c = requires {
    requires constructor_t::implemented_make_function ==
                 implemented_make_function::make;
};

template <typename T, typename constructor_t, typename... args_t>
concept has_make_into_uninit_fn_c = requires(T& t, const constructor_t& c) {
    { c.make_into_uninit(t, stdc::declval<args_t>()...) };
};
template <typename constructor_t>
concept marked_as_having_make_into_uninit_fn_c = requires {
    requires constructor_t::implemented_make_function ==
                 implemented_make_function::make_into_uninit;
};

// for better static asserts, try to detect when there is a nonconst make
// function but no const make function (you probably just forgot the const)
template <typename constructor_t, typename... args_t>
concept has_nonconst_make_fn_c = requires(constructor_t& c) {
    { c.make(stdc::declval<args_t>()...) };
};
template <typename T, typename constructor_t, typename... args_t>
concept has_nonconst_make_into_uninit_fn_c = requires(T& t, constructor_t& c) {
    { c.make_into_uninit(t, stdc::declval<args_t>()...) };
};

template <typename constructor_t, typename... args_t>
struct associated_type_for : public ok::false_type
{
    using type = void;
};

template <typename constructor_t, typename... args_t>
    requires(!stdc::is_void_v<
             typename constructor_t::template associated_type<args_t...>>)
struct associated_type_for<constructor_t, args_t...> : public ok::true_type
{
    using type = typename constructor_t::template associated_type<args_t...>;
};

template <typename constructor_t, typename... args_t>
    requires(!stdc::is_void_v<typename constructor_t::associated_type>)
struct associated_type_for<constructor_t, args_t...> : public ok::true_type
{
    using type = typename constructor_t::associated_type;
};

template <typename constructor_t, typename... args_t>
using associated_type_for_t =
    associated_type_for<constructor_t, args_t...>::type;

template <typename constructor_t, typename... args_t>
struct make_fn_analysis : public ok::false_type
{
    using return_type = void;
};
template <typename constructor_t, typename... args_t>
    requires has_make_fn_c<constructor_t, args_t...>
struct make_fn_analysis<constructor_t, args_t...> : public ok::true_type
{
    using return_type = decltype(stdc::declval<const constructor_t&>().make(
        stdc::declval<args_t>()...));
};

template <typename associated_type, typename constructor_t, typename... args_t>
concept well_declared_constructor_c = requires {
    // Constructors need to have an associated_type declared.
    requires !is_void_c<associated_type_for_t<constructor_t, args_t...>>;
    // Constructors must declare a public static variable of type
    // ok::implemented_make_function, and then implement either make() or
    // make_into_uninit() accordingly.
    requires(marked_as_having_make_fn_c<constructor_t> &&
             has_make_fn_c<constructor_t, args_t...>) ||
                (marked_as_having_make_into_uninit_fn_c<constructor_t> &&
                 has_make_into_uninit_fn_c<associated_type, constructor_t,
                                           args_t...>);
    // make() function in a constructor must always return the associated_type
    // exactly.
    requires(
        !has_make_fn_c<constructor_t, args_t...> ||
        !marked_as_having_make_fn_c<constructor_t> ||
        stdc::same_as<
            typename make_fn_analysis<constructor_t, args_t...>::return_type,
            associated_type>);
};

template <typename T, typename constructor_t, typename... args_t>
struct make_into_uninit_fn_analysis : public ok::false_type
{
    using return_type = void;
};
template <typename T, typename constructor_t, typename... args_t>
    requires has_make_into_uninit_fn_c<T, constructor_t, args_t...>
struct make_into_uninit_fn_analysis<T, constructor_t, args_t...>
    : public ok::true_type
{
    using return_type =
        decltype(stdc::declval<const constructor_t&>().make_into_uninit(
            stdc::declval<T&>(), stdc::declval<args_t>()...));
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

template <typename constructor_t, typename... args_t>
struct unknown_output_type_constructor_analysis : public stdc::true_type
{
    using associated_type =
        typename associated_type_for<constructor_t, args_t...>::type;

    static_assert(marked_as_having_make_fn_c<constructor_t> !=
                  marked_as_having_make_into_uninit_fn_c<constructor_t>);

    constexpr static bool has_rvo = marked_as_having_make_fn_c<constructor_t> &&
                                    has_make_fn_c<constructor_t, args_t...>;
    constexpr static bool has_inplace =
        marked_as_having_make_into_uninit_fn_c<constructor_t> &&
        has_make_into_uninit_fn_c<associated_type, constructor_t, args_t...>;

    static_assert(has_rvo != has_inplace);

    constexpr static bool can_fail =
        has_inplace &&
        status_type_c<typename make_into_uninit_fn_analysis<
            associated_type, constructor_t, args_t...>::return_type>;
};

// This function exists for two reasons:
// a) you can't pass parameter packs to concepts and alias templates which
// require at least one argument. b) constructor_t needs to be unqualified type,
// so this does remove_cvref_t on that. Otherwise stuff like associated_type
// would just be associated_type_for_t<args_including_constructor_t...> and this
// would all be in the analyze_construction() function.
template <typename qualified_constructor_t, typename... args_t>
auto analyze_construction_atleastonetemplateargument()
{
    using constructor_t = stdc::remove_cvref_t<qualified_constructor_t>;
    using associated_type = associated_type_for_t<constructor_t, args_t...>;

    if constexpr (!well_declared_constructor_c<associated_type, constructor_t,
                                               args_t...>) {
        static_assert(!(marked_as_having_make_fn_c<constructor_t> &&
                        !has_make_fn_c<constructor_t, args_t...> &&
                        has_nonconst_make_fn_c<constructor_t, args_t...>),
                      "Missing const specifier on make() method.");
        static_assert(
            !(marked_as_having_make_into_uninit_fn_c<constructor_t> &&
              !has_make_into_uninit_fn_c<associated_type, constructor_t,
                                         args_t...> &&
              has_nonconst_make_into_uninit_fn_c<associated_type, constructor_t,
                                                 args_t...>),
            "Missing const specifier on make_into_uninit() method.");

        return bad_construction_analysis{};
    } else {
        using analysis_t =
            unknown_output_type_constructor_analysis<constructor_t, args_t...>;
        return analysis_t{};
    }
}

template <typename... args_including_constructor_t> auto analyze_construction()
{
    if constexpr (sizeof...(args_including_constructor_t) == 0) {
        return bad_construction_analysis{};
    } else {
        return analyze_construction_atleastonetemplateargument<
            args_including_constructor_t...>();
    }
}

template <typename... args_t>
using analyze_construction_t =
    decltype(detail::analyze_construction<args_t...>());

} // namespace ok::detail

namespace ok {
template <typename T, typename... args_t>
concept is_infallible_constructible_c = requires {
    requires requires {
        requires detail::analyze_construction_t<args_t...>::value;
        requires !detail::analyze_construction_t<args_t...>::can_fail;
        requires stdc::is_same_v<T, typename detail::analyze_construction_t<
                                        args_t...>::associated_type>;
    } || is_std_constructible_c<T, args_t...>;
};

template <typename T, typename... args_t>
concept is_fallible_constructible_c = requires {
    requires detail::analyze_construction_t<args_t...>::value;
    requires detail::analyze_construction_t<args_t...>::can_fail;
    requires stdc::is_same_v<
        T, typename detail::analyze_construction_t<args_t...>::associated_type>;
};

template <typename T, typename... args_t>
concept is_constructible_c = requires {
    requires(is_infallible_constructible_c<T, args_t...> ||
             is_fallible_constructible_c<T, args_t...>);
};

template <typename T, typename... args_t>
concept is_inplace_constructible_c = requires {
    requires detail::analyze_construction_t<args_t...>::has_inplace ||
                 is_std_constructible_c<T, args_t...>;
};

/// The type can either be constructed in-place with the given arguments, or
/// it can be constructed with the given arguments with make() and then moved
/// into place.
template <typename T, typename... args_t>
concept is_inplace_constructible_or_move_makeable_c = requires {
    requires is_inplace_constructible_c<T, args_t...> ||
                 (is_constructible_c<T, args_t...> &&
                  stdc::is_move_constructible_v<T>);
};

} // namespace ok

#endif
