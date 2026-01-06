#ifndef __OKAYLIB_DETAIL_TRAITS_MATHOP_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_MATHOP_TRAITS_H__

#include "okay/detail/type_traits.h"
#include <cstdint>

namespace ok::detail {

template <typename T>
concept has_pre_decrement_c = requires(T& t) {
    { --t } -> same_as_c<T&>;
};

template <typename T>
concept has_pre_increment_c = requires(T& t) {
    { ++t } -> same_as_c<T&>;
};

template <typename T>
concept has_inplace_addition_with_i64_c = requires(T& t, const int64_t& i) {
    { t += i };
};

template <typename T, typename other_t>
concept is_equality_comparable_to_c = requires(const T& a, const other_t& b) {
    { a == b } -> same_as_c<bool>;
};

template <typename T>
concept is_equality_comparable_to_self_c = requires(const T& a, const T& b) {
    { a == b } -> same_as_c<bool>;
};

} // namespace ok::detail
#endif
