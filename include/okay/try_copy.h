#ifndef __OKAYLIB_COPY_H__
#define __OKAYLIB_COPY_H__

#include "okay/detail/traits/is_complete.h"
#include <type_traits>
#include <utility> // addressof

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

namespace detail {
template <typename T>
inline constexpr bool is_valid_copy_assignable =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_copy_assignable_v<T>
#else
    std::is_copy_assignable_v<T>
#endif
    ;

template <typename T>
inline constexpr bool is_valid_copy_constructible =
#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
    std::is_nothrow_copy_constructible_v<T>
#else
    std::is_copy_constructible_v<T>
#endif
    ;

#ifdef OKAYLIB_DISALLOW_EXCEPTIONS
#define __OKAYLIB_TRY_COPY_EXCEPTION__ noexcept
#else
#define __OKAYLIB_TRY_COPY_EXCEPTION__
#endif

} // namespace detail
template <typename T>
inline constexpr bool is_valid_type_for_copy =
    !std::is_reference_v<T> && detail::is_complete<T> && !std::is_array_v<T>;

/// Copy an object. Can be specialized to return a result in order to return an
/// error by value. A program is ill formed if it specializes try_copy for
/// primitive or non-user defined types, and in any case where the
/// specialization for a type T are not visible at all source locations where
/// the declaration for type T is visible.
/// Specializations may not change the arguments of the function, only its
/// return type.
template <typename T>
[[nodiscard]] inline constexpr T
try_copy(const T& input) __OKAYLIB_TRY_COPY_EXCEPTION__
{
    // copy this one line to all specializations
    static_assert(ok::is_valid_type_for_copy<T>);

    static_assert(std::is_copy_constructible_v<T>,
                  "Attempt to copy object with deleted copy constructor. It "
                  "may need to specialize ok::try_copy().");

    static_assert(detail::is_valid_copy_constructible<T>,
                  "Exceptions disallowed for okaylib, but type given to "
                  "ok::try_copy() can throw when copied.");
    return input;
}

/// Copy an object into existing memory. Can be specialized to return a status
/// in order to return an error by value. A program is ill formed if it
/// specializes try_copy for primitive or non-user defined types, and in any
/// case where the specialization for a type T are not visible at all source
/// locations where the declaration for type T is visible. Specializations may
/// not change the arguments of the function, only its return type.
template <typename T>
inline constexpr void
try_copy_into_uninitialized(T& uninitialized_output,
                            const T& input) __OKAYLIB_TRY_COPY_EXCEPTION__
{
    // copy this one line to all specializations
    static_assert(ok::is_valid_type_for_copy<T>);

    static_assert(std::is_copy_constructible_v<T>,
                  "Attempt to copy object with deleted copy constructor. It "
                  "may need to specialize ok::try_copy_into_uninitialized().");

    static_assert(detail::is_valid_copy_constructible<T>,
                  "Exceptions disallowed for okaylib, but type given to "
                  "ok::try_copy_into_uninitialized() can throw when copied.");

    new (std::addressof(uninitialized_output)) T(input);
}

/// Copy an object over an existing object. Can be specialized to return a
/// status in order to return an error by value. A program is ill formed if it
/// specializes try_copy_assign for primitive or non-user defined types, and in
/// any case where the specialization for a type T are not visible at all source
/// locations where the declaration for type T is visible. Specializations may
/// not change the arguments of the function, only its return type.
template <typename T>
inline constexpr void
try_copy_assign(T& output, const T& input) __OKAYLIB_TRY_COPY_EXCEPTION__
{
    // copy this one line to all specializations
    static_assert(ok::is_valid_type_for_copy<T>);

    static_assert(
        std::is_copy_assignable_v<T>,
        "Attempt to copy object with deleted copy assignment operator. It "
        "may need to specialize ok::try_copy_assign().");

    static_assert(detail::is_valid_copy_assignable<T>,
                  "Exceptions disallowed for okaylib, but type given to "
                  "ok::try_copy_assign() can throw when copied.");

    output = input;
}

#undef __OKAYLIB_TRY_COPY_EXCEPTION__

namespace detail {
struct is_okaylib_copy_constructible_t
{
  private:
    template <class T, typename enable =
                           decltype(ok::try_copy<T>(std::declval<const T&>()))>
    static inline constexpr bool exists(int)
    {
        return true;
    }

    template <class T> static inline constexpr bool exists(char)
    {
        return false;
    }

  public:
    template <class T> static inline constexpr bool value = exists<T>(42);
};

struct is_okaylib_copy_assignable_t
{
  private:
    template <class T, typename enable = decltype(ok::try_copy_assign<T>(
                           std::declval<T&>(), std::declval<const T&>()))>
    static inline constexpr bool exists(int)
    {
        return true;
    }

    template <class T> static inline constexpr bool exists(char)
    {
        return false;
    }

  public:
    template <class T> static inline constexpr bool value = exists<T>(42);
};
} // namespace detail

/// Whether it is valid to call `ok::try_copy(const T&)`. Does not detect
/// specializations made for non-copyable types such as references.
template <typename T>
inline constexpr bool is_okaylib_copy_constructible =
    is_valid_type_for_copy<T> &&
    detail::is_okaylib_copy_constructible_t::value<T>;

/// Whether it is valid to call `ok::try_copy_assign(T&, const T&)`. Does not
/// detect specializations made for non-copyable types such as references.
template <typename T>
inline constexpr bool is_okaylib_copy_assignable =
    is_valid_type_for_copy<T> && detail::is_okaylib_copy_assignable_t::value<T>;

/// Whether try_copy exists and returns T by value. Will be false if the type
/// cannot be try_copy'ied or if the try_copy returns something other than T,
/// like a result.
template <typename T>
inline constexpr bool is_try_copy_nonfailing =
    is_okaylib_copy_constructible<T> &&
    std::is_same_v<decltype(ok::try_copy(std::declval<const T&>())), T>;

/// Whether try_copy_assign exists and returns void. Will be false if the type
/// cannot be try_copy_assign'ed or if the try_copy_assign returns something
/// other than void.
template <typename T>
inline constexpr bool is_try_copy_assign_nonfailing =
    is_okaylib_copy_assignable<T> &&
    std::is_same_v<decltype(ok::try_copy_assign(std::declval<T&>(),
                                                std::declval<const T&>())),
                   void>;

} // namespace ok

#endif
