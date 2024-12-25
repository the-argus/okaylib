#ifndef __OKAYLIB_TRAITS_STATUS_H__
#define __OKAYLIB_TRAITS_STATUS_H__

#include <type_traits>

namespace ok::detail {
template <typename maybe_status_t>
inline constexpr bool is_status_enum() noexcept
{
    constexpr bool is_enum = std::is_enum_v<maybe_status_t>;

    constexpr bool is_byte_sized = sizeof(maybe_status_t) == 1;

    constexpr bool okay_represented_by_zero =
        std::underlying_type_t<maybe_status_t>(maybe_status_t::okay) == 0;

    constexpr bool has_result_released_member =
        std::underlying_type_t<maybe_status_t>(
            maybe_status_t::result_released) !=
        std::underlying_type_t<maybe_status_t>(maybe_status_t::okay);

    return is_enum && is_byte_sized && okay_represented_by_zero &&
           has_result_released_member;
}
} // namespace ok::detail

#endif
