#ifndef __OKAYLIB_REFLECTION_NAMEOF_H__
#define __OKAYLIB_REFLECTION_NAMEOF_H__

#include "okay/detail/reflection/pretty_function.h"

namespace ok {

namespace detail {
[[nodiscard]] constexpr ascii_view remove_struct(const ascii_view& type_name)
{
    return type_name.trim_front().remove_prefix("struct").trim_front();
}
[[nodiscard]] constexpr ascii_view remove_class(const ascii_view& type_name)
{
    return type_name.trim_front().remove_prefix("class").trim_front();
}
[[nodiscard]] constexpr ascii_view
remove_typename_prefix(const ascii_view& type_name)
{
    return remove_struct(remove_class(type_name));
}
[[nodiscard]] constexpr ascii_view
ascii_view_pad(const ascii_view& str, size_t begin_offset, size_t end_offset)
{
    return str.substring(begin_offset, str.size() - end_offset);
}

template <typename T>
inline constexpr ascii_view nameof_inner = remove_typename_prefix(
    ascii_view_pad(pretty_function::type<T>(),
                   OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_LEFT,
                   OKAYLIB_REFLECTION_TYPE_PRETTY_FUNCTION_RIGHT));
} // namespace detail

template <typename T> constexpr ascii_view nameof()
{
    return detail::nameof_inner<T>;
}

} // namespace ok
#endif
