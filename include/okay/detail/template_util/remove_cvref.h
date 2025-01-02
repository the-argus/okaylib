#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_REMOVE_CVREF_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_REMOVE_CVREF_H__

#include <type_traits>
namespace ok::detail {
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
}

#endif
