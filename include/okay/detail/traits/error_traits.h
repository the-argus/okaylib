#ifndef __OKAYLIB_DETAIL_TRAITS_ERROR_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_ERROR_TRAITS_H__

#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/special_member_traits.h"
#include <concepts>
#include <type_traits>

namespace ok {
template <typename T>
concept has_make_success = requires() {
    { T::make_success("test string literal") } -> std::same_as<T>;
    requires T::make_success("test string literal").is_success();
    requires noexcept(T::make_success("test string literal"));
};

template <typename T>
concept has_make_success_noargs = requires() {
    { T::make_success() } -> std::same_as<T>;
    requires T::make_success().is_success();
    requires noexcept(T::make_success());
};

/// An object which describes the status of an operation, which can either be
/// good or some sort of failure.
template <typename T>
concept status_object = requires(const T& t) {
    // a status object must not throw, and it must at least be destructible,
    // and it must at least be move constructible
    requires std::is_nothrow_destructible_v<T>;
    requires std::is_nothrow_copy_assignable_v<T> ||
                 !std::is_copy_assignable_v<T>;
    requires std::is_nothrow_copy_constructible_v<T> ||
                 !std::is_copy_constructible_v<T>;
    requires std::is_nothrow_move_assignable_v<T> ||
                 !std::is_move_assignable_v<T>;
    requires std::is_nothrow_move_constructible_v<T>;

    // all member functions of a status object must be noexcept
    requires noexcept(t.is_success());

    requires(has_make_success<T> || has_make_success_noargs<T>);

    // a status object must have an `is_success()` member function
    { t.is_success() } -> std::same_as<bool>;
};

template <typename T>
concept status_enum = requires(T t) {
    requires std::is_enum_v<T>;
    // NOTE: this could be bigger, but anystatus wants it to be <= 4
    requires sizeof(t) == 1;
    { T::success };
    requires std::underlying_type_t<T>(T::success) == 0;
};

template <typename T>
concept status_type = status_enum<T> || status_object<T>;

#define __OK_RES_REQUIRES_CLAUSE                                      \
    requires(!std::is_same_v<ok::detail::remove_cvref_t<success_t>,   \
                             ok::detail::remove_cvref_t<status_t>> && \
             !std::is_constructible_v<success_t, status_t> &&         \
             !std::is_constructible_v<status_t, success_t> &&         \
             !is_convertible_to_c<success_t, status_t> &&             \
             !is_convertible_to_c<status_t, success_t> &&             \
             !std::is_rvalue_reference_v<success_t> &&                \
             !std::is_array_v<success_t> &&                           \
             ok::detail::is_nonthrowing_c<success_t>)

template <typename success_t, status_type status_t, typename = void>
__OK_RES_REQUIRES_CLAUSE class res;

template <status_type T> auto make_success(const char* strliteral = "") noexcept
{
    if constexpr (status_enum<T>) {
        return T::success;
    } else {
        if constexpr (has_make_success<T>) {
            return T::make_success(strliteral);
        } else {
            static_assert(has_make_success_noargs<T>);
            return T::make_success();
        }
    }
}

} // namespace ok

#endif
