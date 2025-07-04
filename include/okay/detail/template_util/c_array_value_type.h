#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_VALUE_TYPE_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_VALUE_TYPE_H__

#include "okay/detail/utility.h"

namespace ok::detail {

template <typename T>
using c_array_value_type =
    stdc::remove_reference_t<decltype(stdc::declval<T>()[0])>;

template <typename T> struct c_array_value_type_safe_t : public stdc::false_type
{
    using type = void;
};

template <typename T>
    requires std::is_array_v<T>
struct c_array_value_type_safe_t<T> : public std::true_type
{
    using type = c_array_value_type<T>;
};

template <typename T>
using c_array_value_type_or_void = typename c_array_value_type_safe_t<T>::type;

} // namespace ok::detail

#endif
