#ifndef __OKAYLIB_DETAIL_TRAITS_ERROR_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_ERROR_TRAITS_H__

#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/detail/type_traits.h"
#include <concepts>

namespace ok {
class abstract_status_t;

template <typename T>
concept has_make_success = requires() {
    { T::make_success("test string literal") } -> ok::same_as_c<T>;
    requires T::make_success("test string literal").is_success();
    requires noexcept(T::make_success("test string literal"));
};

template <typename T>
concept has_make_success_noargs = requires() {
    { T::make_success() } -> ok::same_as_c<T>;
    requires T::make_success().is_success();
    requires noexcept(T::make_success());
};

/// An object which describes the status of an operation, which can either be
/// good or some sort of failure.
/// NOTE: any object which is derived from anystatus_t is a status, do this to
/// reduce template recursion with anystatus and children classes
template <typename T>
concept status_object_c =
    (!stdc::is_reference_c<T> &&
     ok::detail::is_derived_from_c<stdc::remove_cv_t<T>, abstract_status_t>) ||
    requires(const T& t) {
        // a status object must not throw, and it must at least be destructible,
        // and it must at least be move constructible
        requires stdc::is_nothrow_destructible_v<T>;
        requires stdc::is_nothrow_copy_assignable_v<T> ||
                     !stdc::is_copy_assignable_v<T>;
        requires stdc::is_nothrow_copy_constructible_v<T> ||
                     !stdc::is_copy_constructible_v<T>;
        requires stdc::is_nothrow_move_assignable_v<T> ||
                     !stdc::is_move_assignable_v<T>;
        requires stdc::is_nothrow_move_constructible_v<T>;

        // all member functions of a status object must be noexcept
        requires noexcept(t.is_success());

        requires(has_make_success<T> || has_make_success_noargs<T>);

        // a status object must have an `is_success()` member function
        { t.is_success() } -> ok::same_as_c<bool>;

        // a status object must have a .or_panic() member function
        { t.or_panic() } -> ok::is_void_c;
    };

template <typename T>
concept status_enum_c = requires(T t) {
    requires stdc::is_enum_v<T>;
    requires sizeof(t) <= 4;
    { T::success };
    requires stdc::underlying_type_t<T>(T::success) == 0;
};

template <typename T>
concept status_type_c = status_enum_c<T> || status_object_c<T>;

#define __OK_RES_REQUIRES_CLAUSE                               \
    requires(!stdc::is_same_v<ok::remove_cvref_t<success_t>,   \
                              ok::remove_cvref_t<status_t>> && \
             !is_convertible_to_c<success_t, status_t> &&      \
             !is_convertible_to_c<status_t, success_t> &&      \
             !stdc::is_rvalue_reference_v<success_t> &&        \
             !stdc::is_array_v<success_t> &&                   \
             ok::detail::is_nonthrowing_c<success_t>)

template <typename success_t, status_type_c status_t, typename = void>
__OK_RES_REQUIRES_CLAUSE class res;

template <status_type_c T>
auto make_success(const char* strliteral = "") noexcept
{
    if constexpr (status_enum_c<T>) {
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
