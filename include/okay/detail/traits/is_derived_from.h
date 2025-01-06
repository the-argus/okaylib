#ifndef __OKAYLIB_DETAIL_TRAITS_IS_DERIVED_FROM_H__
#define __OKAYLIB_DETAIL_TRAITS_IS_DERIVED_FROM_H__

#include <type_traits>

namespace ok::detail {

template <typename derived_t, typename base_t, typename = void>
struct is_derived_from_meta_t : std::false_type
{};

template <typename derived_t, typename base_t>
struct is_derived_from_meta_t<
    derived_t, base_t,
    std::enable_if_t<std::is_base_of_v<base_t, derived_t> &&
                     std::is_convertible_v<const volatile derived_t*,
                                           const volatile base_t*>>>
    : std::true_type
{};

template <typename derived_t, typename base_t>
inline constexpr bool is_derived_from_v =
    is_derived_from_meta_t<derived_t, base_t>::value;

} // namespace ok::detail

#endif
