#ifndef __OKAYLIB_TRAITS_IS_INSTANCE_H__
#define __OKAYLIB_TRAITS_IS_INSTANCE_H__

#include "okay/detail/type_traits.h"

namespace ok::detail {
template <class, template <typename...> typename>
struct is_instance : public ok::false_type
{};

template <class... Y, template <typename...> typename U>
struct is_instance<U<Y...>, U> : public ok::true_type
{};

template <typename T, template <typename...> typename temp>
concept is_instance_c = is_instance<stdc::remove_cv_t<T>, temp>::value;

} // namespace ok::detail

#endif
