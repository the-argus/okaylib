#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_VALUE_TYPE_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_C_ARRAY_VALUE_TYPE_H__

#include <utility>

namespace ok::detail {

template <typename T>
using c_array_value_type =
    std::remove_reference_t<decltype(std::declval<T>()[0])>;

template <typename T, typename = void>
struct c_array_value_type_safe_t : public std::false_type
{
    using type = void;
};

template <typename T>
struct c_array_value_type_safe_t<T, std::enable_if_t<std::is_array_v<T>>>
    : public std::true_type
{
    using type = c_array_value_type<T>;
};

template <typename T>
using c_array_value_type_or_void = typename c_array_value_type_safe_t<T>::type;

} // namespace ok::detail

#endif
