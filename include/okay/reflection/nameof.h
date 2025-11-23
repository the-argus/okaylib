#ifndef __OKAYLIB_REFLECTION_NAMEOF_H__
#define __OKAYLIB_REFLECTION_NAMEOF_H__

#include "okay/detail/reflection/pretty_function.h"

namespace ok {
template <typename T> constexpr ascii_view nameof()
{
    // using joined_ascii_view to ensure we put the name into a static variable
    // with a null terminator at the end
    return ok::joined_ascii_view<detail::type_name<T>>;
}

} // namespace ok
#endif
