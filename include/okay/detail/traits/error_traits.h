#ifndef __OKAYLIB_DETAIL_TRAITS_ERROR_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_ERROR_TRAITS_H__

#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_nonthrowing.h"
#include <concepts>
#include <type_traits>

namespace ok {
/// An object which describes the status of an operation, which can either be
/// good or some sort of failure.
template <typename T>
concept status_object = requires(const T& t) {
    // a status object must not throw, and it must at least be destructible,
    // and it must at least be move constructible
    std::is_nothrow_destructible_v<T>;
    std::is_nothrow_copy_assignable_v<T> || !std::is_copy_assignable_v<T>;
    std::is_nothrow_copy_constructible_v<T> || !std::is_copy_constructible_v<T>;
    std::is_nothrow_move_assignable_v<T> || !std::is_move_assignable_v<T>;
    std::is_nothrow_move_constructible_v<T>;

    // all member functions of a status object must be noexcept
    noexcept(T::make_success());
    noexcept(t.is_success());

    // a status object must have an `is_success()` member function
    { t.is_success() } -> std::same_as<bool>;

    // a status object must define a variant which is success
    { T::make_success() } -> std::same_as<T>;

    // both make_success() and is_success() must be constexpr, and the object
    // resulting from make_success must return true for is_success().
    T::make_success().is_success();
};

template <typename T>
concept status_enum = requires(T t) {
    std::is_enum_v<T>;
    sizeof(t) == 1;
    { T::success };
    std::underlying_type_t<T>(T::success) == 0;
};

template <typename T>
concept status_type = requires { status_enum<T> || status_object<T>; };

#define __OK_RES_REQUIRES_CLAUSE                                      \
    requires(!std::is_same_v<ok::detail::remove_cvref_t<success_t>,   \
                             ok::detail::remove_cvref_t<status_t>> && \
             !std::is_constructible_v<success_t, status_t> &&         \
             !std::is_constructible_v<status_t, success_t> &&         \
             !std::is_convertible_v<success_t, status_t> &&           \
             !std::is_convertible_v<status_t, success_t> &&           \
             !std::is_rvalue_reference_v<success_t> &&                \
             !std::is_array_v<success_t> &&                           \
             ok::detail::is_nonthrowing<success_t>)

template <typename success_t, status_type status_t, typename = void>
__OK_RES_REQUIRES_CLAUSE class res;

template <status_type T> auto make_success() noexcept
{
    if constexpr (status_enum<T>) {
        return T::success;
    } else {
        return T::make_success();
    }
}

} // namespace ok

#endif
