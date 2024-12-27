#ifndef __OKAYLIB_TRAITS_STATUS_H__
#define __OKAYLIB_TRAITS_STATUS_H__

#include <type_traits>

#define OKAYLIB_IS_STATUS_ENUM_ERRMSG                                   \
    "Bad enum errorcode type provided. Make sure it is only a byte in " \
    "size, and that the okay and result_released entries are 0 and 1, " \
    "respectively."

namespace ok::detail {
template <typename maybe_status_t>
inline constexpr bool is_status_enum() noexcept
{
    constexpr bool is_enum = std::is_enum_v<maybe_status_t>;

    constexpr bool is_byte_sized = sizeof(maybe_status_t) == 1;

    constexpr bool okay_represented_by_zero =
        std::underlying_type_t<maybe_status_t>(maybe_status_t::okay) == 0;

    // required to be at one so in okaylib we can eliminate enum type params
    // and just compare for 0, 1, or neither
    constexpr bool result_released_represented_by_one =
        std::underlying_type_t<maybe_status_t>(
            maybe_status_t::result_released) == 1;

    return is_enum && is_byte_sized && okay_represented_by_zero &&
           result_released_represented_by_one;
}
} // namespace ok::detail

#endif
