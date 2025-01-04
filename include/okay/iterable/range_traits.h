#ifndef __OKAYLIB_ITERABLE_RANGE_TRAITS_H__
#define __OKAYLIB_ITERABLE_RANGE_TRAITS_H__

#include "okay/detail/range_traits.h"

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {
/// Find the beginning of an iterable, using its default cursor type.
inline constexpr auto const& begin = detail::begin_fn_defaulted_t{};

/// Find the beginning of an iterable for a specific cursor type.
template <typename cursor_t>
inline constexpr auto const& begin_for_cursor = detail::begin_fn_t<cursor_t>{};

/// Find the sentinel of an iterable, using its default cursor/sentinel type.
/// Return type: sentinel associated with iterable and cursor. (default
/// sentinel)
inline constexpr auto const& end = detail::end_fn_defaulted_t{};

/// Find the sentinel of an iterable for a specific cursor type.
/// Return type: sentinel associated with iterable and cursor.
template <typename cursor_t>
inline constexpr auto const& end_for_cursor = detail::end_fn_t<cursor_t>{};
} // namespace ok

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
