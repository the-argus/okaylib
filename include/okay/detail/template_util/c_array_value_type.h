#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_VALUE_TYPE_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_VALUE_TYPE_H__

#include <utility>

namespace ok::detail {

template <typename T>
using c_array_value_type =
    std::remove_reference_t<decltype(std::declval<T>()[0])>;

} // namespace ok::detail

#endif
