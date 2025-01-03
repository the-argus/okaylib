#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_LENGTH_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_LENGTH_H__

#include <cstddef>

namespace ok::detail {

template <typename T, size_t sz>
inline constexpr size_t c_array_length(T (&)[sz])
{
    return sz;
}

} // namespace ok::detail

#endif
