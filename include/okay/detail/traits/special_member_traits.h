#ifndef __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__
#define __OKAYLIB_DETAIL_TRAITS_SPECIAL_MEMBER_TRAITS_H__

#include "okay/detail/type_traits.h"

namespace ok {
template <typename T, typename... args_t>
concept is_std_constructible_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_constructible_v<T, args_t...>;
#else
        std::is_constructible_v<T, args_t...>;
#endif
};

template <typename from_t, typename to_t>
concept is_convertible_to_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_convertible_v<from_t, to_t>;
#else
        std::is_convertible_v<from_t, to_t>;
#endif
};

template <typename T>
concept is_std_default_constructible_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_default_constructible_v<T>;
#else
        std::is_default_constructible_v<T>;
#endif
};

template <typename T>
concept is_std_destructible_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_destructible_v<T>;
#else
        std::is_destructible_v<T>;
#endif
};

template <typename T, typename... args_t>
concept is_std_invocable_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_invocable_v<T, args_t...>;
#else
        std::is_invocable_v<T, args_t...>;
#endif
};

template <typename T, typename return_type, typename... args_t>
concept is_std_invocable_r_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_invocable_r_v<return_type, T, args_t...>;
#else
        std::is_invocable_r_v<return_type, T, args_t...>;
#endif
};

namespace detail {
template <typename from_t, typename to_t, typename = void>
class is_convertible_to : public std::false_type
{};
template <typename from_t, typename to_t>
class is_convertible_to<
    from_t, to_t,
    std::enable_if_t<
        is_convertible_to_c<from_t, to_t> &&
        std::is_same_v<void, std::void_t<decltype(static_cast<to_t>(
                                 std::declval<from_t>()))>>>>
    : public std::true_type
{};

template <typename from_t, typename to_t>
concept is_convertible_to_c =
    requires { requires is_convertible_to<from_t, to_t>::value; };

template <typename T>
concept is_move_constructible_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_move_constructible_v<T>;
#else
        std::is_move_constructible_v<T>;
#endif
};
template <typename T>
concept is_move_assignable_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_move_assignable_v<T>;
#else
        std::is_move_assignable_v<T>;
#endif
};

template <typename T>
concept is_copy_constructible_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_copy_constructible_v<T>;
#else
        std::is_copy_constructible_v<T>;
#endif
};
template <typename T>
concept is_copy_assignable_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        std::is_nothrow_copy_assignable_v<T>;
#else
        std::is_copy_assignable_v<T>;
#endif
};

template <typename LHS, typename RHS>
concept is_assignable_from_c =
    requires { requires std::is_assignable_v<LHS, RHS>; };

template <typename T>
concept is_swappable_c = requires { requires std::is_swappable_v<T>; };

template <typename T>
concept is_object_c = requires { requires std::is_object_v<T>; };

template <typename T>
concept is_moveable_c = requires {
    requires is_object_c<T> && is_move_constructible_c<T> &&
                 is_move_assignable_c<T>
                 // is_assignable_from_v<T&, T>
                 && is_swappable_c<T>;
};

#define OKAYLIB_IS_NONTHROWING_ERRMSG                                     \
    "Given type must have all of its special member functions marked as " \
    "noexcept, or deleted."

/// Checks if a type does not throw in all of its special member functions. It
/// cannot check parameterized constructors
template <typename T>
concept is_nonthrowing_c = requires {
    requires
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
        (!std::is_destructible_v<T> || std::is_nothrow_destructible_v<T>) &&
            (!std::is_default_constructible_v<T> ||
             std::is_nothrow_default_constructible_v<T>) &&
            (!std::is_copy_assignable_v<T> ||
             std::is_nothrow_copy_assignable_v<T>) &&
            (!std::is_copy_constructible_v<T> ||
             std::is_nothrow_copy_constructible_v<T>) &&
            (!std::is_move_assignable_v<T> ||
             std::is_nothrow_move_assignable_v<T>) &&
            (!std::is_move_constructible_v<T> ||
             std::is_nothrow_move_constructible_v<T>);
#else
        true;
#endif
};

template <typename T, typename return_t, typename... args_t>
concept is_std_invocable_r_c =
    requires { requires std::is_invocable_r_v<return_t, T, args_t...>; };
} // namespace detail
} // namespace ok

#endif
