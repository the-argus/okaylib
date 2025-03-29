#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_LENGTH_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_LENGTH_H__

#include <cstddef>
#include <type_traits>

namespace ok::detail {

template <typename T, size_t sz>
inline constexpr size_t c_array_length(T (&)[sz])
{
    return sz;
}

template <typename T, size_t sz>
std::integral_constant<size_t, sz> c_array_length_deduction(T (&)[sz]);

template <typename T>
using c_array_length_t = decltype(c_array_length_deduction(std::declval<T>()));

} // namespace ok::detail

#endif
