#ifndef __OKAYLIB_TRAITS_STATUS_H__
#define __OKAYLIB_TRAITS_STATUS_H__

#include <type_traits>

#define OKAYLIB_IS_STATUS_ENUM_ERRMSG                                   \
    "Bad enum errorcode type provided. Make sure it is only a byte in " \
    "size, and that the okay and no_value entries are 0 and 1, " \
    "respectively."

namespace ok::detail {

template <typename maybe_status, typename = void>
struct is_status_enum : std::false_type
{};

template <typename maybe_status>
struct is_status_enum<
    maybe_status,
    std::enable_if_t<
        std::is_enum_v<maybe_status> && sizeof(maybe_status) == 1 &&
        std::underlying_type_t<maybe_status>(maybe_status::okay) == 0 &&
        std::underlying_type_t<maybe_status>(
            maybe_status::no_value) == 1>> : std::true_type
{};

template <typename T>
constexpr bool is_status_enum_v = is_status_enum<T>::value;

} // namespace ok::detail

#endif
