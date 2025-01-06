#ifndef __OKAYLIB_ITERABLE_RANGE_TRAITS_H__
#define __OKAYLIB_ITERABLE_RANGE_TRAITS_H__

#include "okay/iterable/range_accessors.h"

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {
namespace detail {
template <typename, typename = void>
struct is_valid_range_meta_t : std::false_type
{};

template <typename range_t>
struct is_valid_range_meta_t<
    range_t, std::void_t<decltype(ok::end(std::declval<const range_t&>()),
                                  ok::begin(std::declval<const range_t&>()))>>
    : std::true_type
{};
} // namespace detail

/// this will be true if its valid to call ok::begin() and ok::end() on the type
/// T
template <typename T>
inline constexpr bool is_valid_range_v =
    detail::is_valid_range_meta_t<T>::value;

} // namespace ok

#endif
