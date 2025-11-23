#ifndef __OKAYLIB_DETAIL_REFLECTION_PRETTY_FUNCTION_H__
#define __OKAYLIB_DETAIL_REFLECTION_PRETTY_FUNCTION_H__

#include "okay/detail/reflection/cstring.h"

// clang-format off
#if defined(__clang__)
    #define OKAYLIB_REFLECTION_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(__GNUC__) && !defined(__clang__)
    #define OKAYLIB_REFLECTION_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define OKAYLIB_REFLECTION_PRETTY_FUNCTION __FUNCSIG__
#else
    #error "No support for compile-time type information for this compiler."
#endif
// clang-format on

namespace ok::pretty_function {
template <typename T> constexpr ok::detail::cstring type()
{
    return {OKAYLIB_REFLECTION_PRETTY_FUNCTION};
}
} // namespace ok::pretty_function

// clang-format off
#if defined(__clang__)
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX "ok::detail::cstring ok::pretty_function::type() [T = "
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX "] "
#elif defined(__GNUC__) && !defined(__clang__)
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX "constexpr ok::detail::cstring ok::pretty_function::type() [with T = "
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX "]"
#elif defined(_MSC_VER)
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX "struct ok::detail::cstring __cdecl ok::pretty_function::type<"
    #define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX ">(void)"
#else
    #error "No support for this compiler."
#endif

#define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_LEFT (sizeof(OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_PREFIX) - 1)
#define OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_RIGHT (sizeof(OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_SUFFIX) - 1)
// clang-format on
#endif
