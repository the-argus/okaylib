#ifndef __OKAYLIB_MATH_ORDERING_H__
#define __OKAYLIB_MATH_ORDERING_H__

#include "okay/detail/noexcept.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/type_traits.h"
#include "okay/opt.h"
#include <cstdint>

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
#include <compare>
#elif defined(OKAYLIB_COMPAT_STRATEGY_NO_STD)
#include "okay/detail/compare.h"
#elif defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
#include "okay/detail/compare.h"
#endif

namespace ok {

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

    constexpr partial_ordering(std::partial_ordering po)
    {
        if (po == std::partial_ordering::less) {
            representation = partial_ordering_enum::less;
        } else if (po == std::partial_ordering::greater) {
            representation = partial_ordering_enum::greater;
        } else if (po == std::partial_ordering::equivalent) {
            representation = partial_ordering_enum::equivalent;
        } else {
            representation = partial_ordering_enum::unordered;
        }
    }

    constexpr partial_ordering(std::weak_ordering po)
        : partial_ordering(std::partial_ordering(po))
    {
    }

    constexpr partial_ordering(std::strong_ordering po)
        : partial_ordering(std::partial_ordering(po))
    {
    }

    constexpr operator std::partial_ordering()
    {
        switch (representation) {
        case ok::partial_ordering_enum::greater:
            return std::partial_ordering::greater;
        case ok::partial_ordering_enum::less:
            return std::partial_ordering::less;
        case ok::partial_ordering_enum::equivalent:
            return std::partial_ordering::equivalent;
        case ok::partial_ordering_enum::unordered:
            return std::partial_ordering::unordered;
        }
    }

    constexpr bool operator==(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == partial_ordering_enum::equivalent;
    }

    constexpr bool operator!=(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation != partial_ordering_enum::equivalent;
    }

    constexpr bool operator<(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == partial_ordering_enum::less;
    }

    constexpr bool operator>(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == partial_ordering_enum::greater;
    }

    constexpr bool operator>=(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == partial_ordering_enum::greater ||
               representation == partial_ordering_enum::equivalent;
    }

    constexpr bool operator<=(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == partial_ordering_enum::less ||
               representation == partial_ordering_enum::equivalent;
    }

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

    constexpr explicit operator std::partial_ordering() const noexcept
    {
        return std::partial_ordering(this->operator std::weak_ordering());
    }

    constexpr explicit operator std::weak_ordering() const noexcept
    {
        return std::weak_ordering(this->operator std::strong_ordering());
    }

    constexpr operator std::strong_ordering() const noexcept
    {
        switch (representation) {
        case ok::ordering_enum::greater:
            return std::strong_ordering::greater;
        case ok::ordering_enum::less:
            return std::strong_ordering::less;
        case ok::ordering_enum::equivalent:
            return std::strong_ordering::equivalent;
        }
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

    constexpr ordering(std::strong_ordering po)
        : ordering(std::weak_ordering(po))
    {
    }

    constexpr ordering(std::weak_ordering po)
    {
        if (po == std::weak_ordering::equivalent) {
            representation = ordering_enum::equivalent;
        } else if (po == std::weak_ordering::greater) {
            representation = ordering_enum::greater;
        } else {
            representation = ordering_enum::less;
        }
    }

    constexpr operator auto() const noexcept { return representation; }
    constexpr auto as_enum() const noexcept { return representation; }
    constexpr partial_ordering as_partial() const noexcept
    {
        return partial_ordering(
            partial_ordering_enum(ordering_underlying_type(representation)));
    }

    constexpr bool operator==(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == ordering_enum::equivalent;
    }

    constexpr bool operator!=(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation != ordering_enum::equivalent;
    }

    constexpr bool operator<(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == ordering_enum::less;
    }

    constexpr bool operator>(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == ordering_enum::greater;
    }

    constexpr bool operator>=(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == ordering_enum::greater ||
               representation == ordering_enum::equivalent;
    }

    constexpr bool operator<=(int cmp) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(cmp == 0, "attempt to compare partial_ordering against "
                              "something other than integer literal zero");
        return representation == ordering_enum::less ||
               representation == ordering_enum::equivalent;
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

namespace detail {
template <typename T>
concept partially_orderable_okaylib_c =
    requires(const T& item, const T& other) {
        { item <=> other } -> ok::same_as_c<ok::partial_ordering>;
    };

template <typename T>
concept orderable_okaylib_c = requires(const T& item, const T& other) {
    { item <=> other } -> ok::same_as_c<ok::ordering>;
};

template <typename T>
concept partially_orderable_stdlib_c = requires(const T& item, const T& other) {
    { item <=> other } -> ok::same_as_c<std::partial_ordering>;
};

template <typename T>
concept weak_orderable_stdlib_c = requires(const T& item, const T& other) {
    { item <=> other } -> ok::same_as_c<std::weak_ordering>;
};

template <typename T>
concept strong_orderable_stdlib_c = requires(const T& item, const T& other) {
    { item <=> other } -> ok::same_as_c<std::strong_ordering>;
};

template <typename T>
concept orderable_stdlib_c =
    strong_orderable_stdlib_c<T> || weak_orderable_stdlib_c<T>;

template <typename T>
concept spaceship_comparable_c = requires(const T& a, const T& b) {
    { a <=> b };
};

template <typename T>
concept equality_comparable_c = requires(const T& a, const T& b) {
    { a == b } -> ok::same_as_c<bool>;
};
} // namespace detail

template <typename T>
concept orderable_c =
    detail::orderable_stdlib_c<T> || detail::orderable_okaylib_c<T>;

template <typename T>
concept exclusively_partially_orderable_c =
    detail::partially_orderable_stdlib_c<T> ||
    detail::partially_orderable_okaylib_c<T>;

template <typename T>
concept partially_orderable_c =
    detail::partially_orderable_stdlib_c<T> ||
    detail::partially_orderable_okaylib_c<T> || orderable_c<T>;

/// Can be specialized to declared a type with only operator== to be considered
/// strongly equality comparable (ie. comparing a type against itself will
/// always result in true for all possible instances of that type)
template <typename T>
struct strongly_equality_comparable_declaration : ok::false_type
{};

template <typename T>
concept strongly_equality_comparable_c =
    (strongly_equality_comparable_declaration<T>::value &&
     !detail::spaceship_comparable_c<T> && detail::equality_comparable_c<T>) ||
    orderable_c<T>;

template <typename T>
concept partially_equality_comparable_c =
    (!strongly_equality_comparable_declaration<T>::value &&
     !detail::spaceship_comparable_c<T> && detail::equality_comparable_c<T>) ||
    partially_orderable_c<T> || strongly_equality_comparable_c<T>;

// all optionals whose payloads are strongly comparable are strongly comparable
// also
template <typename T>
    requires strongly_equality_comparable_declaration<T>::value
struct strongly_equality_comparable_declaration<opt<T>> : public std::true_type
{};

namespace detail {

struct compare_fn_t
{
    template <orderable_c T>
    [[nodiscard]] constexpr ok::ordering
    operator()(const T& lhs, const T& rhs) const OKAYLIB_NOEXCEPT
    {
        static_assert(
            stdc::is_convertible_v<decltype(lhs <=> rhs), ok::ordering>);
        return ok::ordering(lhs <=> rhs);
    }
};

struct partial_compare_fn_t
{
    template <partially_orderable_c T>
    [[nodiscard]] constexpr ok::partial_ordering
    operator()(const T& lhs, const T& rhs) const OKAYLIB_NOEXCEPT
    {
        static_assert(stdc::is_convertible_v<decltype(lhs <=> rhs),
                                             ok::partial_ordering>);
        return ok::partial_ordering(lhs <=> rhs);
    }
};

struct is_equal_fn_t
{
    template <strongly_equality_comparable_c T>
    [[nodiscard]] constexpr bool operator()(const T& lhs,
                                            const T& rhs) const OKAYLIB_NOEXCEPT
    {
        return lhs == rhs;
    }
};

struct is_partially_equal_fn_t
{
    template <partially_equality_comparable_c T>
    [[nodiscard]] constexpr bool operator()(const T& lhs,
                                            const T& rhs) const OKAYLIB_NOEXCEPT
    {
        return lhs == rhs;
    }
};

struct min_fn_t
{
    template <orderable_c T>
        requires stdc::is_copy_constructible_v<T>
    [[nodiscard]] constexpr T operator()(const T& lhs,
                                         const T& rhs) const OKAYLIB_NOEXCEPT
    {
        constexpr compare_fn_t compare;
        switch (compare(lhs, rhs).as_enum()) {
        case ordering_enum::less:
        case ordering_enum::equivalent:
            return lhs;
        case ordering_enum::greater:
            return rhs;
        }
    }
};

struct partial_min_fn_t
{
    template <partially_orderable_c T>
        requires stdc::is_copy_constructible_v<T>
    [[nodiscard]] constexpr ok::opt<T>
    operator()(const T& lhs, const T& rhs) const OKAYLIB_NOEXCEPT
    {
        constexpr partial_compare_fn_t partial_compare;
        switch (partial_compare(lhs, rhs).as_enum()) {
        case partial_ordering_enum::less:
        case partial_ordering_enum::equivalent:
            return lhs;
        case partial_ordering_enum::greater:
            return rhs;
        case partial_ordering_enum::unordered:
            return ok::nullopt;
        }
    }
};

struct max_fn_t
{
    template <orderable_c T>
        requires stdc::is_copy_constructible_v<T>
    [[nodiscard]] constexpr T operator()(const T& lhs,
                                         const T& rhs) const OKAYLIB_NOEXCEPT
    {
        constexpr compare_fn_t compare;
        switch (compare(lhs, rhs).as_enum()) {
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
    template <partially_orderable_c T>
        requires stdc::is_copy_constructible_v<T>
    [[nodiscard]] constexpr ok::opt<T>
    operator()(const T& lhs, const T& rhs) const OKAYLIB_NOEXCEPT
    {
        constexpr partial_compare_fn_t partial_compare;
        switch (partial_compare(lhs, rhs).as_enum()) {
        case partial_ordering_enum::greater:
        case partial_ordering_enum::equivalent:
            return lhs;
        case partial_ordering_enum::less:
            return rhs;
        case partial_ordering_enum::unordered:
            return ok::nullopt;
        }
    }
};
} // namespace detail

template <typename T> struct bounds
{
    const T& min;
    const T& max;
};

namespace detail {

struct make_bounds_fn_t
{
    template <orderable_c T>
    constexpr bounds<T> operator()(const T& a,
                                   const T& b) const OKAYLIB_NOEXCEPT
    {
        constexpr compare_fn_t compare;
        switch (compare(a, b).as_enum()) {
        case ordering_enum::less:
            return bounds{.min = a, .max = b};
        case ordering_enum::greater:
        case ordering_enum::equivalent:
            return bounds{.min = b, .max = a};
        }
    }
};

struct clamp_fn_t
{
    template <orderable_c T>
        requires stdc::is_copy_constructible_v<T>
    constexpr T operator()(const T& value,
                           const bounds<T>& bounds) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(bounds.min <= bounds.max,
                    "min and max passed to clamp are in the wrong order");

        constexpr compare_fn_t compare;

        if (compare(value, bounds.min) == ordering::less)
            return T(bounds.min);

        if (compare(value, bounds.max) == ordering::greater)
            return T(bounds.max);

        return T(value);
    }
};

struct partial_clamp_fn_t
{
    template <partially_orderable_c T>
        requires stdc::is_copy_constructible_v<T>
    constexpr opt<T> operator()(const T& value,
                                const bounds<T>& bounds) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(bounds.min <= bounds.max,
                    "min and max passed to clamp are in the wrong order");

        constexpr partial_compare_fn_t compare;

        switch (compare(value, bounds.min).as_enum()) {
        case partial_ordering_enum::greater:
            break;
        case partial_ordering_enum::equivalent:
            return T(value);
        case partial_ordering_enum::less:
            return T(bounds.min);
        case partial_ordering_enum::unordered:
            return ok::nullopt;
        }

        switch (compare(value, bounds.max).as_enum()) {
        case partial_ordering_enum::less:
            break;
        case partial_ordering_enum::equivalent:
            return T(value);
        case partial_ordering_enum::greater:
            return T(bounds.max);
        case partial_ordering_enum::unordered:
            return ok::nullopt;
        }

        return T(value);
    }
};
} // namespace detail

inline constexpr detail::compare_fn_t cmp;

inline constexpr detail::partial_compare_fn_t partial_cmp;

inline constexpr detail::is_equal_fn_t is_equal;

inline constexpr detail::is_partially_equal_fn_t is_partial_equal;

inline constexpr detail::min_fn_t min;

/// Returns nullopt if its arguments are unorderable
inline constexpr detail::partial_min_fn_t partial_min;

inline constexpr detail::max_fn_t max;

inline constexpr detail::partial_max_fn_t partial_max;

inline constexpr detail::clamp_fn_t clamp;

inline constexpr detail::partial_clamp_fn_t partial_clamp;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
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
