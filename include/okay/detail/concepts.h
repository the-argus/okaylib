#ifndef __OKAY_DETAIL_CONCEPTS_H__
#define __OKAY_DETAIL_CONCEPTS_H__

#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/type_traits.h"

namespace ok {
template <typename T, typename other_t>
concept same_as_without_cvref_c =
    same_as_c<detail::remove_cvref_t<T>, detail::remove_cvref_t<other_t>>;

template <typename T>
concept reference_c = stdc::is_reference_c<T>;
} // namespace ok

#endif
