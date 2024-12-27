#ifndef __OKAYLIB_DETAIL_TRAITS_IS_NOTHROW_TYPE_H__
#define __OKAYLIB_DETAIL_TRAITS_IS_NOTHROW_TYPE_H__

#include <type_traits>

#define OKAYLIB_IS_NONTHROWING_ERRMSG                                     \
    "Given type must have all of its special member functions marked as " \
    "noexcept, or deleted."

namespace ok::detail {
/// Checks if a type does not throw in all of its special member functions. It
/// cannot check parameterized constructors
template <typename T>
inline constexpr bool is_nonthrowing =
    (!std::is_destructible_v<T> || std::is_nothrow_destructible_v<T>) &&
    (!std::is_default_constructible_v<T> ||
     std::is_nothrow_default_constructible_v<T>) &&
    (!std::is_copy_assignable_v<T> || std::is_nothrow_copy_assignable_v<T>) &&
    (!std::is_copy_constructible_v<T> ||
     std::is_nothrow_copy_constructible_v<T>) &&
    (!std::is_move_assignable_v<T> || std::is_nothrow_move_assignable_v<T>) &&
    (!std::is_move_constructible_v<T> ||
     std::is_nothrow_move_constructible_v<T>);
} // namespace ok::detail

#endif
