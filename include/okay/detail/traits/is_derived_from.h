#ifndef __OKAYLIB_DETAIL_TRAITS_IS_DERIVED_FROM_H__
#define __OKAYLIB_DETAIL_TRAITS_IS_DERIVED_FROM_H__

#include "okay/detail/traits/special_member_traits.h"
#include <type_traits>

namespace ok::detail {
template <typename derived_t, typename base_t>
concept is_derived_from_c = requires {
    requires std::is_base_of_v<base_t, derived_t>;
    requires is_convertible_to_c<const volatile derived_t*,
                                 const volatile base_t*>;
};
} // namespace ok::detail

#endif
