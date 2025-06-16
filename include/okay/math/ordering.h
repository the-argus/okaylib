#ifndef __OKAYLIB_MATH_ORDERING_H__
#define __OKAYLIB_MATH_ORDERING_H__

#include "okay/detail/abort.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/detail/traits/type_compare.h"
#include <cstdint>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {

// TODO: c++20 compat for these types to convert from std::partial_ordering and
// std::weak_ordering

using ordering_underlying_type = int8_t;

// underlying enum representation of ordering and partial ordering. can be
// extracted / converted from an ordering and are intended for use in switch
// statements and comparisons
enum class partial_ordering_enum : ordering_underlying_type
{
    equivalent = 0,
    less = -1,
    greater = 1,
    unordered = 2,
};

enum class ordering_enum : ordering_underlying_type
{
    equivalent = ordering_underlying_type(partial_ordering_enum::equivalent),
    less = ordering_underlying_type(partial_ordering_enum::less),
    greater = ordering_underlying_type(partial_ordering_enum::greater),
};

struct ordering;

// prevents default construction and allows conversion
struct partial_ordering
{
  private:
    partial_ordering_enum representation;
    constexpr explicit partial_ordering(partial_ordering_enum r) noexcept
        : representation(r)
    {
    }
    constexpr partial_ordering() = delete;

  public:
    friend struct ordering;

    constexpr operator auto() const noexcept { return representation; }
    constexpr auto as_enum() const noexcept { return representation; }

    static const partial_ordering less;
    static const partial_ordering greater;
    static const partial_ordering equivalent;
    static const partial_ordering unordered;
};

inline constexpr partial_ordering
    partial_ordering::less(partial_ordering_enum::less);
inline constexpr partial_ordering
    partial_ordering::equivalent(partial_ordering_enum::equivalent);
inline constexpr partial_ordering
    partial_ordering::greater(partial_ordering_enum::greater);
inline constexpr partial_ordering
    partial_ordering::unordered(partial_ordering_enum::unordered);

struct ordering
{
  private:
    ordering_enum representation;
    constexpr explicit ordering(ordering_enum r) noexcept : representation(r) {}
    constexpr ordering() = delete;

  public:
    constexpr explicit operator partial_ordering() const noexcept
    {
        return partial_ordering(
            partial_ordering_enum(ordering_underlying_type(representation)));
    }

    constexpr friend bool operator==(const ordering& lhs,
                                     const partial_ordering& rhs)
    {
        return ordering_underlying_type(lhs.representation) ==
               ordering_underlying_type(rhs.representation);
    }

    constexpr friend bool operator==(const partial_ordering& lhs,
                                     const ordering& rhs)
    {
        return ordering_underlying_type(lhs.representation) ==
               ordering_underlying_type(rhs.representation);
    }

    constexpr operator auto() const noexcept { return representation; }
    constexpr auto as_enum() const noexcept { return representation; }
    constexpr partial_ordering as_partial() const noexcept
    {
        return partial_ordering(
            partial_ordering_enum(ordering_underlying_type(representation)));
    }

    static const ordering less;
    static const ordering greater;
    static const ordering equivalent;
};

inline constexpr ordering ordering::less(ordering_enum::less);
inline constexpr ordering ordering::equivalent(ordering_enum::equivalent);
inline constexpr ordering ordering::greater(ordering_enum::greater);

// trait definitions
// structure:
// partially_equal ----------> equal
// |                            |
// |                            v
// |-> partially_orderable -> orderable
//
// Definition of more specific traits (the most "specific" is orderable) will
// allow the deduction of lesser traits. A type with std::strong_ordering or
// std::weak_ordering returning operator<=> in c++20 will satisfy all of these
// traits. Deduction will only occur for traits the user has not defined.

template <typename T, typename enable = void> struct partially_equal_definition
{
    static constexpr bool deleted = true;
};

namespace detail {
template <typename T, typename = void>
struct has_partially_equal_definition : std::true_type
{};
template <typename T>
struct has_partially_equal_definition<
    T, std::enable_if_t<partially_equal_definition<T>::deleted>>
    : std::false_type
{};
} // namespace detail

template <typename T, typename enable = void> struct equal_definition
{
    static constexpr bool deleted = true;
};

namespace detail {
template <typename T, typename = void>
struct has_equal_definition : std::true_type
{};
template <typename T>
struct has_equal_definition<T, std::enable_if_t<equal_definition<T>::deleted>>
    : std::false_type
{};
} // namespace detail

template <typename T, typename enable = void> struct orderable_definition
{
    static constexpr bool deleted = true;
};

namespace detail {
template <typename T, typename = void>
struct has_orderable_definition : std::true_type
{};
template <typename T>
struct has_orderable_definition<
    T, std::enable_if_t<orderable_definition<T>::deleted>> : std::false_type
{};
template <typename T, typename = void>
struct has_is_strong_orderable_enabled : std::false_type
{};
template <typename T>
struct has_is_strong_orderable_enabled<T,
                                       std::enable_if_t<T::is_strong_orderable>>
    : std::true_type
{};
} // namespace detail

template <typename T, typename enable = void>
struct partially_orderable_definition
{
    static constexpr bool deleted = true;
};

namespace detail {
template <typename T, typename = void>
struct has_partially_orderable_definition : std::true_type
{};
template <typename T>
struct has_partially_orderable_definition<
    T, std::enable_if_t<partially_orderable_definition<T>::deleted>>
    : std::false_type
{};
} // namespace detail

// default implementation of full orderability for integer types
template <typename T>
struct orderable_definition<T, std::enable_if_t<std::is_integral_v<T>>>
{
    // marker to enable features like min + max
    static constexpr bool is_strong_orderable = true;

    static constexpr ordering cmp(const T& lhs, const T& rhs) OKAYLIB_NOEXCEPT
    {
        // TODO: this can be optimized with sign bit magic? check clang output
        // on -O3 first though probably
        if (lhs == rhs) {
            return ordering::equivalent;
        } else if (lhs < rhs) {
            return ordering::less;
        } else {
            return ordering::greater;
        }
        // TODO: ifdef c++20, use spaceship operator
    }
};

// default impl of partial orderability for floating point types
template <typename T>
struct partially_orderable_definition<
    T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    // able to be applied to partially orderable type: all floats that are
    // orderable are also strongly orderable (ie. partial_min and partial_max
    // make sense for floats)
    static constexpr bool is_strong_orderable = true;

    static constexpr partial_ordering partial_cmp(const T& lhs,
                                                  const T& rhs) OKAYLIB_NOEXCEPT
    {
        // TODO: optimize this too, also check viability of using <=> in cpp20
        if (lhs == rhs) {
            return partial_ordering::equivalent;
        } else if (lhs < rhs) {
            return partial_ordering::less;
        } else if (lhs > rhs) {
            return partial_ordering::greater;
        } else {
            // nan
            return partial_ordering::unordered;
        }
    }
};

// deal with inheritance of equal/orderable traits
namespace detail {

// partial equal
template <typename T, typename = void> struct partially_equal_definition_inner;
template <typename T>
struct partially_equal_definition_inner<
    T, std::enable_if_t<has_partially_equal_definition<T>::value>>
    : public partially_equal_definition<T>
{};
template <typename T>
struct partially_equal_definition_inner<
    T, std::enable_if_t<!has_partially_equal_definition<T>::value &&
                        has_equal_definition<T>::value>>
{
    static constexpr bool is_partially_equal(const T& lhs,
                                             const T& rhs) OKAYLIB_NOEXCEPT
    {
        return equal_definition<T>::is_equal(lhs, rhs);
    }
};
template <typename T>
struct partially_equal_definition_inner<
    T, std::enable_if_t<!has_partially_equal_definition<T>::value &&
                        !has_equal_definition<T>::value &&
                        has_partially_orderable_definition<T>::value>>
{
    static constexpr bool is_partially_equal(const T& lhs,
                                             const T& rhs) OKAYLIB_NOEXCEPT
    {
        return partially_orderable_definition<T>::partial_cmp(lhs, rhs) ==
               partial_ordering::equivalent;
    }
};
template <typename T>
struct partially_equal_definition_inner<
    T, std::enable_if_t<!has_partially_equal_definition<T>::value &&
                        !has_equal_definition<T>::value &&
                        !has_partially_orderable_definition<T>::value &&
                        has_orderable_definition<T>::value>>
{
    static constexpr bool is_partially_equal(const T& lhs,
                                             const T& rhs) OKAYLIB_NOEXCEPT
    {
        return orderable_definition<T>::cmp(lhs, rhs) == ordering::equivalent;
    }
};

// equal
template <typename T, typename = void> struct equal_definition_inner;
template <typename T>
struct equal_definition_inner<T,
                              std::enable_if_t<has_equal_definition<T>::value>>
    : public equal_definition<T>
{};
template <typename T>
struct equal_definition_inner<
    T, std::enable_if_t<!has_equal_definition<T>::value &&
                        has_orderable_definition<T>::value>>
{
    static constexpr bool is_equal(const T& lhs, const T& rhs) OKAYLIB_NOEXCEPT
    {
        return orderable_definition<T>::cmp(lhs, rhs) == ordering::equivalent;
    }
};

// partially orderable
template <typename T, typename = void>
struct partially_orderable_definition_inner;
template <typename T>
struct partially_orderable_definition_inner<
    T, std::enable_if_t<has_partially_orderable_definition<T>::value>>
    : public partially_orderable_definition<T>
{};
template <typename T>
struct partially_orderable_definition_inner<
    T, std::enable_if_t<!has_partially_orderable_definition<T>::value &&
                        has_orderable_definition<T>::value>>
{
    // propagate strong orderable-ness downwards. no need to check
    // orderable_definition_inner because the inner ones handle inheritance, and
    // orderable will never be inherited from anything else
    static constexpr bool is_strong_orderable =
        has_is_strong_orderable_enabled<orderable_definition<T>>::value;

    static constexpr partial_ordering partial_cmp(const T& lhs,
                                                  const T& rhs) OKAYLIB_NOEXCEPT
    {
        return partial_ordering(orderable_definition<T>::cmp(lhs, rhs));
    }
};

// orderable
template <typename T, typename = void> struct orderable_definition_inner;
template <typename T>
struct orderable_definition_inner<
    T, std::enable_if_t<has_orderable_definition<T>::value>>
    : public orderable_definition<T>
{};

// is strong orderable trait impl, which reads from inner (inherited) definition
// instead of the direct user input
template <typename T, typename = void>
struct is_strong_fully_orderable_inner : std::false_type
{};
template <typename T>
struct is_strong_fully_orderable_inner<
    T, std::enable_if_t<orderable_definition_inner<T>::is_strong_orderable>>
    : std::true_type
{};

template <typename T, typename = void>
struct is_strong_partially_orderable_inner : std::false_type
{};
template <typename T>
struct is_strong_partially_orderable_inner<
    T, std::enable_if_t<
           partially_orderable_definition_inner<T>::is_strong_orderable>>
    : std::true_type
{};

} // namespace detail

// in ok namespace now, make publicly accessible type traits which take into
// account trait inheritance/deduction
template <typename T>
constexpr bool is_strong_fully_orderable_v =
    detail::is_strong_fully_orderable_inner<T>::value;

template <typename T>
constexpr bool is_strong_partially_orderable_v =
    detail::is_strong_partially_orderable_inner<T>::value;

template <typename T>
constexpr bool is_orderable_v =
    detail::is_complete_c<detail::orderable_definition_inner<T>>;

template <typename T>
constexpr bool is_partially_orderable_v =
    detail::is_complete_c<detail::partially_orderable_definition_inner<T>>;

template <typename T>
constexpr bool is_equateable_v =
    detail::is_complete_c<detail::equal_definition_inner<T>>;

template <typename T>
constexpr bool is_partially_equateable_v =
    detail::is_complete_c<detail::partially_equal_definition_inner<T>>;

namespace detail {

// TODO: test if std::is_same_v<T, T> is necessary here
template <typename T> struct no_primitive_definitions_assert
{
    // We provide definitions for numeric types- none of these should be
    // provided for any integer or floating point types by the user
    static_assert(!has_partially_equal_definition<float>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<double>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<float>::value && std::is_same_v<T, T>);
    static_assert(!has_equal_definition<double>::value && std::is_same_v<T, T>);
    static_assert(!has_orderable_definition<float>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_orderable_definition<double>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<int8_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<int16_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<int32_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<int64_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<uint8_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<uint16_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<uint32_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_orderable_definition<uint64_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<int8_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<int16_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<int32_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<int64_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<uint8_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<uint16_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<uint32_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_partially_equal_definition<uint64_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<int8_t>::value && std::is_same_v<T, T>);
    static_assert(!has_equal_definition<int16_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<int32_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<int64_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<uint8_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<uint16_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<uint32_t>::value &&
                  std::is_same_v<T, T>);
    static_assert(!has_equal_definition<uint64_t>::value &&
                  std::is_same_v<T, T>);
};

template <typename T, typename = void>
struct is_orderable_definition_valid : std::false_type
{};
template <typename T>
struct is_orderable_definition_valid<
    T, std::enable_if_t<std::is_same_v<
           decltype(orderable_definition_inner<T>::cmp(
               std::declval<const T&>(), std::declval<const T&>())),
           ordering>>> : std::true_type
{};

template <typename T, typename = void>
struct is_partially_orderable_definition_valid : std::false_type
{};
template <typename T>
struct is_partially_orderable_definition_valid<
    T, std::enable_if_t<std::is_same_v<
           decltype(partially_orderable_definition_inner<T>::partial_cmp(
               std::declval<const T&>(), std::declval<const T&>())),
           partial_ordering>>> : std::true_type
{};

template <typename T, typename = void>
struct is_equal_definition_valid : std::false_type
{};
template <typename T>
struct is_equal_definition_valid<
    T, std::enable_if_t<std::is_same_v<
           decltype(equal_definition_inner<T>::is_equal(
               std::declval<const T&>(), std::declval<const T&>())),
           bool>>> : std::true_type
{};

template <typename T, typename = void>
struct is_partially_equal_definition_valid : std::false_type
{};
template <typename T>
struct is_partially_equal_definition_valid<
    T, std::enable_if_t<std::is_same_v<
           decltype(partially_equal_definition_inner<T>::is_partially_equal(
               std::declval<const T&>(), std::declval<const T&>())),
           bool>>> : std::true_type
{};

template <typename T> struct orderable_asserts_t
{
    static_assert(is_complete_c<orderable_definition_inner<T>>,
                  "Cannot call an orderable function (cmp, min, max, clamp) on "
                  "type which is not strictly orderable. If using a float or "
                  "other not strictly comparable type, try the partial variant "
                  "(partial_cmp, partial_min, partial_max, etc), or otherwise "
                  "implement ok::orderable_definition<T> for your type.");
    static_assert(
        is_orderable_definition_valid<T>::value,
        "Given type's orderable definition is invalid- make sure it "
        "contains a function called cmp() which returns an ok::ordering "
        "and takes two const references to your type.");
};

template <typename T> struct partially_orderable_asserts_t
{
    static_assert(
        is_partially_orderable_v<T>,
        "Cannot call a `partial_*` function on a type which is not partially "
        "orderable- if this is your own type then you may need to implement "
        "ok::orderable_definition or ok::partially_orderable_definition to get "
        "this functionality.");
    static_assert(is_partially_orderable_definition_valid<T>::value,
                  "Given type's partially orderable definition is invalid- "
                  "make sure it contains a function called partial_cmp() "
                  "which returns an ok::partial_ordering and takes two "
                  "const references to your type.");
};

template <typename T, bool checking_partial> struct minmaxclamp_asserts_t
{
    static constexpr bool valid_arguments_for_min =
        (!checking_partial && is_strong_fully_orderable_v<T>) ||
        (checking_partial && is_strong_partially_orderable_v<T>);

    // example of why you need strong ordering with ok::min():
    // we return `min` if `lhs < min` but return `lhs` if `min == lhs`. but if
    // `min` and `lhs` have observable differences that `min` doesn't know
    // about, (ie. T is only weakly orderable) it may be important which
    // arg you get a copy of- in which case you should write your own `min`
    // function for that type.
    static_assert(
        valid_arguments_for_min,
        "Given type is orderable, but not strongly orderable. Calling "
        "ok::min/max/clamp (or similar) on it may lead to confusing behavior. "
        "If this error message is mistaken, add `static constexpr bool "
        "is_strong_orderable = true;` to the ok::orderable_definition for the "
        "type.");
};

#define __okaylib_convertible_relop_decl(rettype)               \
    template <typename LHS, typename RHS>                       \
    constexpr auto operator()(const LHS& lhs,                   \
                              RHS&& rhs) const OKAYLIB_NOEXCEPT \
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS>, rettype>

struct compare_fn_t
{
    __okaylib_convertible_relop_decl(ordering)
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr orderable_asserts_t<LHS> orderable_asserts;

        return orderable_definition_inner<LHS>::cmp(lhs, rhs);
    }
};

struct partial_compare_fn_t
{
    __okaylib_convertible_relop_decl(partial_ordering)
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS>
            partially_orderable_asserts;
        return detail::partially_orderable_definition_inner<LHS>::partial_cmp(
            lhs, rhs);
    }
};

struct is_equal_fn_t
{
    __okaylib_convertible_relop_decl(bool)
    {
        static_assert(
            is_equateable_v<LHS>,
            "Cannot call ok::is_equal on type which is not fully equality "
            "comparable. If passing floating point values or other partially "
            "comparable type, use ok::is_partially_equal. Or, if this is your "
            "type, you may need to define ok::equal_definition<T> for it.");
        static_assert(
            is_equal_definition_valid<LHS>::value,
            "Given type's equal definition is invalid- make sure it "
            "contains a function called is_equal() which returns a boolean and "
            "takes two const references to your type.");

        constexpr no_primitive_definitions_assert<LHS> asserts;
        return detail::equal_definition_inner<LHS>::is_equal(lhs, rhs);
    }
};

struct is_partially_equal_fn_t
{
    __okaylib_convertible_relop_decl(bool)
    {
        static_assert(
            is_partially_equateable_v<LHS>,
            "Cannot call ok::is_equal on type which is not equality "
            "comparable. If passing floating point values or other partially "
            "comparable type, use ok::is_partially_equal. Or, if this is your "
            "type, you may need to define ok::partially_equal_definition<T> "
            "for "
            "it.");
        static_assert(
            is_partially_equal_definition_valid<LHS>::value,
            "Given type's partially equal definition is invalid- make sure it "
            "contains a function called is_partially_equal() which returns a "
            "boolean and takes two const references to your type.");

        constexpr no_primitive_definitions_assert<LHS> asserts;
        return detail::partially_equal_definition_inner<
            LHS>::is_partially_equal(lhs, rhs);
    }
};

struct min_fn_t
{
    template <typename LHS, typename RHS>
    constexpr auto operator()(const LHS& lhs, RHS&& rhs) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr orderable_asserts_t<LHS> orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, false> nonpartial_min_asserts;

        switch (
            detail::orderable_definition_inner<LHS>::cmp(lhs, rhs).as_enum()) {
        case ordering_enum::less:
        case ordering_enum::equivalent:
            return lhs;
        case ordering_enum::greater:
            return rhs;
        }
    }
};

struct unchecked_min_fn_t
{
    template <typename LHS, typename RHS>
    constexpr auto operator()(const LHS& lhs, RHS&& rhs) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS> orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, true> partial_min_asserts;

        switch (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                    lhs, rhs)
                    .as_enum()) {
        case partial_ordering_enum::less:
        case partial_ordering_enum::equivalent:
        case partial_ordering_enum::unordered:
            return lhs;
        case partial_ordering_enum::greater:
            return rhs;
        }
    }
};

struct partial_min_fn_t
{
    template <typename LHS, typename RHS>
    constexpr auto operator()(const LHS& lhs, RHS&& rhs) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS> orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, true> partial_min_asserts;

        switch (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                    lhs, rhs)
                    .as_enum()) {
        case partial_ordering_enum::less:
        case partial_ordering_enum::equivalent:
            return lhs;
        case partial_ordering_enum::greater:
            return rhs;
        case partial_ordering_enum::unordered:
            __ok_abort("Attempt to find the minimum of some values which are "
                       "unordered (floating point NaNs, or similar?)");
        }
    }
};

struct max_fn_t
{
    template <typename LHS, typename RHS>
    constexpr auto operator()(const LHS& lhs, RHS&& rhs) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr orderable_asserts_t<LHS> orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, false> nonpartial_max_asserts;

        switch (
            detail::orderable_definition_inner<LHS>::cmp(lhs, rhs).as_enum()) {
        case ordering_enum::greater:
        case ordering_enum::equivalent:
            return lhs;
        case ordering_enum::less:
            return rhs;
        }
    }
};

struct partial_max_fn_t
{
    template <typename LHS, typename RHS>
    constexpr auto operator()(const LHS& lhs, RHS&& rhs) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS>
            partially_orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, true> partial_max_asserts;

        switch (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                    lhs, rhs)
                    .as_enum()) {
        case partial_ordering_enum::greater:
        case partial_ordering_enum::equivalent:
            return lhs;
        case partial_ordering_enum::less:
            return rhs;
        case partial_ordering_enum::unordered:
            __ok_abort("Attempt to find the maximum of some values which are "
                       "unordered (floating point NaNs, or similar?)");
        }
    }
};

struct unchecked_max_fn_t
{
    template <typename LHS, typename RHS>
    constexpr auto operator()(const LHS& lhs, RHS&& rhs) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(rhs), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS> orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, true> partial_max_asserts;

        switch (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                    lhs, rhs)
                    .as_enum()) {
        case partial_ordering_enum::less:
        case partial_ordering_enum::equivalent:
        case partial_ordering_enum::unordered:
            return lhs;
        case partial_ordering_enum::greater:
            return rhs;
        }
    }
};

struct clamp_fn_t
{
    template <typename LHS, typename arg_t>
    constexpr auto operator()(const LHS& lhs, arg_t&& min,
                              arg_t&& max) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(min), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        __ok_assert(min < max,
                    "min and max passed to clamp are in the wrong order");
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr orderable_asserts_t<LHS> orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, false> nonpartial_clamp_asserts;

        if (detail::orderable_definition_inner<LHS>::cmp(lhs, min) ==
            ordering::less)
            return min;
        if (detail::orderable_definition_inner<LHS>::cmp(lhs, max) ==
            ordering::greater)
            return max;

        // TODO: also need strong ordering here
        return lhs;
    }
};

struct partial_clamp_fn_t
{
    template <typename LHS, typename arg_t>
    constexpr auto operator()(const LHS& lhs, arg_t&& min,
                              arg_t&& max) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(min), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        __ok_assert(min < max,
                    "min and max passed to clamp are in the wrong order");
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS>
            partially_orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, true> partial_clamp_asserts;

        switch (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                    lhs, min)
                    .as_enum()) {
        case partial_ordering_enum::less:
            return min;
        case partial_ordering_enum::unordered:
            __ok_abort("Attempt to clamp two values which are unordered "
                       "(floating point NaNs, or similar?)");
        default:
            break;
        }

        switch (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                    lhs, max)
                    .as_enum()) {
        case partial_ordering_enum::greater:
            return max;
        case partial_ordering_enum::unordered:
            __ok_abort("Attempt to clamp two values which are unordered "
                       "(floating point NaNs, or similar?)");
        default:
            break;
        }

        return lhs;
    }
};

struct unchecked_clamp_fn_t
{
    template <typename LHS, typename arg_t>
    constexpr auto operator()(const LHS& lhs, arg_t&& min,
                              arg_t&& max) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_convertible_to_c<decltype(min), LHS> &&
                               std::is_copy_constructible_v<LHS>,
                           LHS>
    {
        // min != min protects from NaN causing this assert to fire
        __ok_assert(min != min || max != max || min < max,
                    "Floating-point NaN or swapped min/max arguments to clamp "
                    "detected");
        constexpr no_primitive_definitions_assert<LHS> asserts;
        constexpr partially_orderable_asserts_t<LHS>
            partially_orderable_asserts;
        constexpr minmaxclamp_asserts_t<LHS, true> partial_clamp_asserts;

        if (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                lhs, min) == ordering::less)
            return min;

        if (detail::partially_orderable_definition_inner<LHS>::partial_cmp(
                lhs, max) == ordering::greater)
            return max;

        return lhs;
    }
};

} // namespace detail

inline constexpr detail::compare_fn_t cmp;

inline constexpr detail::partial_compare_fn_t partial_cmp;

inline constexpr detail::is_equal_fn_t is_equal;

inline constexpr detail::is_partially_equal_fn_t is_partial_equal;

inline constexpr detail::min_fn_t min;

// variant of min which accepts partially orderable values and aborts the
// program in the case that they are not orderable
inline constexpr detail::partial_min_fn_t partial_min;

// variant of min which accepts partially orderable values and returns the
// left-hand side in the case that they are not orderable
// so unchecked_min(NaN, 3912.f) is NaN, unchecked_min(3912.f, NaN) is 3912.f
inline constexpr detail::unchecked_min_fn_t unchecked_min;

inline constexpr detail::max_fn_t max;

inline constexpr detail::partial_max_fn_t partial_max;

inline constexpr detail::unchecked_max_fn_t unchecked_max;

inline constexpr detail::clamp_fn_t clamp;

inline constexpr detail::partial_clamp_fn_t partial_clamp;

inline constexpr detail::unchecked_clamp_fn_t unchecked_clamp;

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <> struct fmt::formatter<ok::ordering>
{
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const ok::ordering& ordering,
                                    format_context& ctx) const
    {
        using namespace ok;
        switch (ordering.as_enum()) {
        case ordering_enum::less:
            return fmt::format_to(ctx.out(), "ordering::less");
        case ordering_enum::equivalent:
            return fmt::format_to(ctx.out(), "ordering::equivalent");
        case ordering_enum::greater:
            return fmt::format_to(ctx.out(), "ordering::greater");
        }
    }
};
template <> struct fmt::formatter<ok::partial_ordering>
{
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const ok::partial_ordering& ordering,
                                    format_context& ctx) const
    {
        using namespace ok;
        switch (ordering.as_enum()) {
        case partial_ordering_enum::less:
            return fmt::format_to(ctx.out(), "partial_ordering::less");
        case partial_ordering_enum::equivalent:
            return fmt::format_to(ctx.out(), "partial_ordering::equivalent");
        case partial_ordering_enum::greater:
            return fmt::format_to(ctx.out(), "partial_ordering::greater");
        case partial_ordering_enum::unordered:
            return fmt::format_to(ctx.out(), "partial_ordering::unordered");
        }
    }
};
#endif

#endif
