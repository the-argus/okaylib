#ifndef __OKAY_DETAIL_CONCEPTS_H__
#define __OKAY_DETAIL_CONCEPTS_H__

#include "okay/detail/type_traits.h"

namespace ok {
template <typename T, typename other_t>
concept same_as_without_cvref_c =
    same_as_c<remove_cvref_t<T>, remove_cvref_t<other_t>>;

template <typename T>
concept reference_c = stdc::is_reference_c<T>;

template <typename T>
concept integral_c = stdc::is_integral_v<T>;

template <typename T>
concept floating_point_c = stdc::is_floating_point_v<T>;
} // namespace ok

#endif
