#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_FIRST_TYPE_IN_PACK_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_FIRST_TYPE_IN_PACK_H__

#include "okay/detail/utility.h"

namespace ok::detail {
template <typename T, typename... pack>
T get_first_type_impl(T&&, pack&&...) noexcept;

template <typename... pack>
using first_type_in_pack_t =
    decltype(get_first_type_impl(ok::stdc::declval<pack&&>()...));
} // namespace ok::detail
#endif
