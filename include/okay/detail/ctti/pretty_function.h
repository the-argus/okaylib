#ifndef __OKAYLIB_DETAIL_CTTI_PRETTY_FUNCTION_H__
#define __OKAYLIB_DETAIL_CTTI_PRETTY_FUNCTION_H__

#include "okay/detail/ctti/cstring.h"

// clang-format off
#if defined(__clang__)
    #define CTTI_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(__GNUC__) && !defined(__clang__)
    #define CTTI_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define CTTI_PRETTY_FUNCTION __FUNCSIG__
#else
    #error "No support for compile-time type information for this compiler."
#endif
// clang-format on

namespace ok::ctti::pretty_function {
template <typename T> constexpr ok::ctti::detail::cstring type()
{
    return {CTTI_PRETTY_FUNCTION};
}
} // namespace ok::ctti::pretty_function

// clang-format off
#if defined(__clang__)
    #define CTTI_TYPE_PRETTY_FUNCTION_PREFIX "ok::ctti::detail::cstring ok::ctti::pretty_function::type() [T = "
    #define CTTI_TYPE_PRETTY_FUNCTION_SUFFIX "] "
#elif defined(__GNUC__) && !defined(__clang__)
    #define CTTI_TYPE_PRETTY_FUNCTION_PREFIX "constexpr ok::ctti::detail::cstring ok::ctti::pretty_function::type() [with T = "
    #define CTTI_TYPE_PRETTY_FUNCTION_SUFFIX "]"
#elif defined(_MSC_VER)
    #define CTTI_TYPE_PRETTY_FUNCTION_PREFIX "struct ok::ctti::detail::cstring __cdecl ok::ctti::pretty_function::type<"
    #define CTTI_TYPE_PRETTY_FUNCTION_SUFFIX ">(void)"
#else
    #error "No support for this compiler."
#endif

#define CTTI_TYPE_PRETTY_FUNCTION_LEFT (sizeof(CTTI_TYPE_PRETTY_FUNCTION_PREFIX) - 1)
#define CTTI_TYPE_PRETTY_FUNCTION_RIGHT (sizeof(CTTI_TYPE_PRETTY_FUNCTION_SUFFIX) - 1)
// clang-format on
#endif
