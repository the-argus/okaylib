#ifndef __OKAYLIB_DETAIL_REFLECTION_PRETTY_FUNCTION_H__
#define __OKAYLIB_DETAIL_REFLECTION_PRETTY_FUNCTION_H__

#include "okay/ascii_view.h"
#include "okay/detail/joined_ascii_view.h"
#include "okay/detail/reflection/to_tuple.h"

// clang-format off
#if defined(__clang__) || defined(__GNUC__)
    #define OKAYLIB_REFLECTION_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define OKAYLIB_REFLECTION_PRETTY_FUNCTION __FUNCSIG__
#else
    #error "No support for compile-time type information for this compiler."
#endif
// clang-format on

namespace ok::pretty_function {
template <typename T> constexpr ok::ascii_view type()
{
    return {OKAYLIB_REFLECTION_PRETTY_FUNCTION};
}
} // namespace ok::pretty_function

// clang-format off
#if defined(__clang__)
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX "ok::ascii_view ok::pretty_function::type() [T = "
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX "] "
#elif defined(__GNUC__) && !defined(__clang__)
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX "constexpr ok::ascii_view ok::pretty_function::type() [with T = "
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX "]"
#elif defined(_MSC_VER)
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX "struct ok::ascii_view __cdecl ok::pretty_function::type<"
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX ">(void)"
#else
    #error "No support for this compiler."
#endif

#define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_LEFT (sizeof(OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX) - 1)
#define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_RIGHT (sizeof(OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX) - 1)
// clang-format on

namespace ok {
template <class T> struct type_metainfo;
}

namespace ok::detail {

template <class T> struct ptr_t final
{
    const T* ptr;
};

template <size_t N, class T> constexpr auto get_ptr(T&& t) noexcept
{
    auto& p = get<N>(ok::detail::to_tie(t));
    return ptr_t<ok::remove_cvref_t<decltype(p)>>{&p};
}

template <class T> extern const T external;

template <auto ptr_t> [[nodiscard]] consteval ascii_view mangled_name()
{
    return OKAYLIB_REFLECTION_PRETTY_FUNCTION;
}

template <class T> [[nodiscard]] consteval ascii_view mangled_name()
{
    return OKAYLIB_REFLECTION_PRETTY_FUNCTION;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything" // external symbol not defined
#endif
template <auto N, class T>
constexpr ascii_view get_name_impl =
    mangled_name<get_ptr<N>(external<ok::stdc::remove_volatile_t<T>>)>();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

struct GLAZE_REFLECTOR
{
    int GLAZE_FIELD;
};

struct reflect_field
{
    static constexpr auto name = get_name_impl<0, GLAZE_REFLECTOR>;
    static constexpr auto end = name.substring(name.find_or("GLAZE_FIELD", -1) +
                                               sizeof("GLAZE_FIELD") - 1);
    static constexpr auto begin = name[name.find_or("GLAZE_FIELD", -1) - 1];
};

struct reflect_type
{
    static constexpr ascii_view name = mangled_name<GLAZE_REFLECTOR>();
    static constexpr auto end = name.substring(
        name.find_or("GLAZE_REFLECTOR", 0) + sizeof("GLAZE_REFLECTOR") - 1);
    static constexpr auto begin =
#if defined(__GNUC__) || defined(__clang__)
        ascii_view{"T = "};
#else
        ascii_view{"ok::detail::mangled_name<"};
#endif
};

template <auto N, class T> struct member_nameof_impl
{
    static constexpr auto name = ok::detail::get_name_impl<N, T>;
    static constexpr auto begin = name.find(detail::reflect_field::end);
    static constexpr auto tmp = name.substr(0, begin);
    static constexpr auto stripped =
        tmp.substr(tmp.find_last_of(detail::reflect_field::begin) + 1);
    static constexpr ascii_view stripped_literal = joined_ascii_view<stripped>;
};

template <auto N, class T>
inline constexpr auto member_nameof =
    []() constexpr { return member_nameof_impl<N, T>::stripped_literal; }();

template <class T>
constexpr auto type_name = [] {
    constexpr ascii_view name = detail::mangled_name<T>();
    constexpr auto begin = name.find_or(detail::reflect_type::end, -1);
    constexpr auto tmp = name.substring(0, begin);
#if defined(__GNUC__) || defined(__clang__)
    return tmp.substring(tmp.reverse_find_or(detail::reflect_type::begin, -1) +
                         detail::reflect_type::begin.size());
#else
    constexpr auto name_with_keyword =
        tmp.substring(tmp.reverse_find_or(detail::reflect_type::begin, -1) +
                      detail::reflect_type::begin.size());
    return name_with_keyword.substring(name_with_keyword.find(" ") + 1);
#endif
}();

template <class T, size_t... I>
[[nodiscard]] constexpr auto member_names_impl(ok::stdc::index_sequence<I...>)
{
    if constexpr (sizeof...(I) == 0) {
        return constexpr_array_t<ascii_view, 0>{};
    } else {
        return constexpr_array_t{member_nameof<I, T>...};
    }
}

template <class T>
concept metainfo_defines_rename_member_function_c =
    requires(T t, const ascii_view s) {
        {
            ok::type_metainfo<ok::remove_cvref_t<T>>::rename_member(s)
        } -> ok::stdc::convertible_to_c<ascii_view>;
    };

template <metainfo_defines_rename_member_function_c T, size_t... I>
[[nodiscard]] constexpr auto member_names_impl(ok::stdc::index_sequence<I...>)
{
    if constexpr (sizeof...(I) == 0) {
        return constexpr_array_t<ascii_view, 0>{};
    } else {
        return constexpr_array_t{
            ok::type_metainfo<ok::remove_cvref_t<T>>::rename_member(
                member_nameof<I, T>)...};
    }
}

template <class T>
inline constexpr auto member_names = [] {
    return member_names_impl<T>(
        ok::stdc::make_index_sequence<detail::count_members<T>>{});
}();

// For member object pointers
template <class T> struct remove_member_pointer
{
    using type = T;
};

template <class C, class T> struct remove_member_pointer<T C::*>
{
    using type = C;
};

template <class class_t, class return_t, class... args_t>
struct remove_member_pointer<return_t (class_t::*)(args_t...)>
{
    using type = class_t;
};

template <class T, auto P> consteval ascii_view get_name_msvc()
{
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.find_or("->", -1) + 2);
    return str.substring(0, str.find_or(">", -1));
}

template <class T, auto P> consteval ascii_view func_name_msvc()
{
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.reverse_find_or(type_name<T>, -1) +
                        type_name<T>.size());
    str = str.substring(str.find_or("::", -1) + 2);
    return str.substring(0, str.find_or("(", -1));
}

#if defined(__clang__)
inline constexpr ascii_view pretty_function_tail = "]";
#elif defined(__GNUC__) || defined(__GNUG__)
inline constexpr ascii_view pretty_function_tail = ";";
#elif defined(_MSC_VER)
#endif

template <auto P>
    requires(ok::stdc::is_member_pointer_v<decltype(P)>)
consteval ascii_view get_name()
{
#if defined(_MSC_VER) && !defined(__clang__)
    if constexpr (ok::stdc::is_member_object_pointer_v<decltype(P)>) {
        using T = remove_member_pointer<ok::stdc::decay_t<decltype(P)>>::type;
        constexpr auto p = P;
        return get_name_msvc<T, &(detail::external<T>.*p)>();
    } else {
        using T = remove_member_pointer<ok::stdc::decay_t<decltype(P)>>::type;
        return func_name_msvc<T, P>();
    }
#else
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.find_or("&", -1) + 1);
    str = str.substring(0, str.find_or(pretty_function_tail, -1));
    return str.substring(str.reverse_find_or("::", -1) + 2);
#endif
}

template <auto E>
    requires(ok::stdc::is_enum_v<decltype(E)> &&
             ok::stdc::is_scoped_enum_v<decltype(E)>)
consteval auto get_name()
{
#if defined(_MSC_VER) && !defined(__clang__)
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.reverse_find_or("::", -1) + 2);
    return str.substring(0, str.find_or(">", -1));
#else
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.reverse_find_or("::", -1) + 2);
    return str.substring(0, str.find_or("]", -1));
#endif
}

template <auto E>
    requires(ok::stdc::is_enum_v<decltype(E)> &&
             !ok::stdc::is_scoped_enum_v<decltype(E)>)
consteval auto get_name()
{
#if defined(_MSC_VER) && !defined(__clang__)
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.reverse_find_or("= ", -1) + 2);
    return str.substring(0, str.find_or(">", -1));
#else
    ascii_view str = OKAYLIB_REFLECTION_PRETTY_FUNCTION;
    str = str.substring(str.reverse_find_or("= ", -1) + 2);
    return str.substring(0, str.find_or("]", -1));
#endif
}
} // namespace ok::detail
#endif
