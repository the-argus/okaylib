#ifndef __OKAYLIB_REFLECTION_NAMEOF_H__
#define __OKAYLIB_REFLECTION_NAMEOF_H__

#include "okay/detail/reflection/pretty_function.h"

namespace ok {

namespace detail {

template <typename T>
inline constexpr cstring nameof_inner =
    filter_typename_prefix(pretty_function::type<T>().pad(
        OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_LEFT,
        OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_RIGHT));
} // namespace detail

/// Get a c-style null terminated string (not necessarily unique) identifier for
/// a given type
template <typename T> constexpr char* nameof()
{
    constexpr auto cstr = detail::nameof_inner<T>;
    return cstr.data();
}

} // namespace ok
#endif
