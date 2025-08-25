#ifndef __OKAYLIB_DETAIL_COMPARE_H__
#define __OKAYLIB_DETAIL_COMPARE_H__

/*
 * Minimal replacement for <compare> when using a weird compatibility strategy
 * with no libc++ headers.
 *
 * This header provides the potential return values from the builtin operator
 * <=> which are std::strong_ordering, std::weak_ordering, and
 * std::partial_ordering, as well as their respective value values such as
 * strong_ordering::equal, strong_ordering::less, strong_ordering::greater and
 * strong_ordering::equivalent.
 *
 * Symbols are defined directly in namespace std.
 */

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
#error Okaylib compare.h header was included, but okaylib is not set to compile without the STL, so the STL <compare> might get included and then cause multiple definition errors.
#endif

namespace std {
namespace detail::compare {
using type = signed char;

enum class orderable_enum : type
{
    equivalent = 0,
    less = -1,
    greater = 1
};

enum class unordered_enum : type
{
    unordered = 2
};

struct unspecified
{
    consteval unspecified(unspecified*) noexcept {}
};
} // namespace detail::compare

class partial_ordering;
class strong_ordering;
class weak_ordering;

class partial_ordering
{
    // less=0xff, equiv=0x00, greater=0x01, unordered=0x02
    detail::compare::type m_value;

    constexpr explicit partial_ordering(
        detail::compare::orderable_enum value) noexcept
        : m_value(detail::compare::type(value))
    {
    }

    constexpr explicit partial_ordering(
        detail::compare::unordered_enum value) noexcept
        : m_value(detail::compare::type(value))
    {
    }

    friend class weak_ordering;
    friend class strong_ordering;

  public:
    static const partial_ordering less;
    static const partial_ordering equivalent;
    static const partial_ordering greater;
    static const partial_ordering unordered;

    [[nodiscard]]
    friend constexpr bool operator==(partial_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value == 0;
    }

    [[nodiscard]]
    friend constexpr bool operator==(partial_ordering,
                                     partial_ordering) noexcept = default;

    [[nodiscard]]
    friend constexpr bool operator<(partial_ordering value,
                                    detail::compare::unspecified) noexcept
    {
        return value.m_value == -1;
    }

    [[nodiscard]]
    friend constexpr bool operator>(partial_ordering value,
                                    detail::compare::unspecified) noexcept
    {
        return value.m_value == 1;
    }

    [[nodiscard]]
    friend constexpr bool operator<=(partial_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value <= 0;
    }

    [[nodiscard]]
    friend constexpr bool operator>=(partial_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return detail::compare::type(value.m_value & 1) == value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator<(detail::compare::unspecified,
                                    partial_ordering value) noexcept
    {
        return value.m_value == 1;
    }

    [[nodiscard]]
    friend constexpr bool operator>(detail::compare::unspecified,
                                    partial_ordering value) noexcept
    {
        return value.m_value == -1;
    }

    [[nodiscard]]
    friend constexpr bool operator<=(detail::compare::unspecified,
                                     partial_ordering value) noexcept
    {
        return detail::compare::type(value.m_value & 1) == value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator>=(detail::compare::unspecified,
                                     partial_ordering value) noexcept
    {
        return 0 >= value.m_value;
    }

    [[nodiscard]]
    friend constexpr partial_ordering
    operator<=>(partial_ordering value, detail::compare::unspecified) noexcept
    {
        return value;
    }

    [[nodiscard]]
    friend constexpr partial_ordering
    operator<=>(detail::compare::unspecified, partial_ordering value) noexcept
    {
        if (value.m_value & 1)
            return partial_ordering(
                detail::compare::orderable_enum(-value.m_value));
        else
            return value;
    }
};

inline constexpr partial_ordering
    partial_ordering::less(detail::compare::orderable_enum::less);

inline constexpr partial_ordering
    partial_ordering::equivalent(detail::compare::orderable_enum::equivalent);

inline constexpr partial_ordering
    partial_ordering::greater(detail::compare::orderable_enum::greater);

inline constexpr partial_ordering
    partial_ordering::unordered(detail::compare::unordered_enum::unordered);

class weak_ordering
{
    detail::compare::type m_value;

    constexpr explicit weak_ordering(
        detail::compare::orderable_enum value) noexcept
        : m_value(detail::compare::type(value))
    {
    }

    friend class strong_ordering;

  public:
    static const weak_ordering less;
    static const weak_ordering equivalent;
    static const weak_ordering greater;

    [[nodiscard]]
    constexpr operator partial_ordering() const noexcept
    {
        return partial_ordering(detail::compare::orderable_enum(m_value));
    }

    // comparisons
    [[nodiscard]]
    friend constexpr bool operator==(weak_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value == 0;
    }

    [[nodiscard]]
    friend constexpr bool operator==(weak_ordering,
                                     weak_ordering) noexcept = default;

    [[nodiscard]]
    friend constexpr bool operator<(weak_ordering value,
                                    detail::compare::unspecified) noexcept
    {
        return value.m_value < 0;
    }

    [[nodiscard]]
    friend constexpr bool operator>(weak_ordering value,
                                    detail::compare::unspecified) noexcept
    {
        return value.m_value > 0;
    }

    [[nodiscard]]
    friend constexpr bool operator<=(weak_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value <= 0;
    }

    [[nodiscard]]
    friend constexpr bool operator>=(weak_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value >= 0;
    }

    [[nodiscard]]
    friend constexpr bool operator<(detail::compare::unspecified,
                                    weak_ordering value) noexcept
    {
        return 0 < value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator>(detail::compare::unspecified,
                                    weak_ordering value) noexcept
    {
        return 0 > value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator<=(detail::compare::unspecified,
                                     weak_ordering value) noexcept
    {
        return 0 <= value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator>=(detail::compare::unspecified,
                                     weak_ordering value) noexcept
    {
        return 0 >= value.m_value;
    }

    [[nodiscard]]
    friend constexpr weak_ordering
    operator<=>(weak_ordering value, detail::compare::unspecified) noexcept
    {
        return value;
    }

    [[nodiscard]]
    friend constexpr weak_ordering operator<=>(detail::compare::unspecified,
                                               weak_ordering value) noexcept
    {
        return weak_ordering(detail::compare::orderable_enum(-value.m_value));
    }
};

// valid values' definitions
inline constexpr weak_ordering
    weak_ordering::less(detail::compare::orderable_enum::less);

inline constexpr weak_ordering
    weak_ordering::equivalent(detail::compare::orderable_enum::equivalent);

inline constexpr weak_ordering
    weak_ordering::greater(detail::compare::orderable_enum::greater);

class strong_ordering
{
    detail::compare::type m_value;

    constexpr explicit strong_ordering(
        detail::compare::orderable_enum value) noexcept
        : m_value(detail::compare::type(value))
    {
    }

  public:
    // valid values
    static const strong_ordering less;
    static const strong_ordering equal;
    static const strong_ordering equivalent;
    static const strong_ordering greater;

    [[nodiscard]]
    constexpr operator partial_ordering() const noexcept
    {
        return partial_ordering(detail::compare::orderable_enum(m_value));
    }

    [[nodiscard]]
    constexpr operator weak_ordering() const noexcept
    {
        return weak_ordering(detail::compare::orderable_enum(m_value));
    }

    // comparisons
    [[nodiscard]]
    friend constexpr bool operator==(strong_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value == 0;
    }

    [[nodiscard]]
    friend constexpr bool operator==(strong_ordering,
                                     strong_ordering) noexcept = default;

    [[nodiscard]]
    friend constexpr bool operator<(strong_ordering value,
                                    detail::compare::unspecified) noexcept
    {
        return value.m_value < 0;
    }

    [[nodiscard]]
    friend constexpr bool operator>(strong_ordering value,
                                    detail::compare::unspecified) noexcept
    {
        return value.m_value > 0;
    }

    [[nodiscard]]
    friend constexpr bool operator<=(strong_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value <= 0;
    }

    [[nodiscard]]
    friend constexpr bool operator>=(strong_ordering value,
                                     detail::compare::unspecified) noexcept
    {
        return value.m_value >= 0;
    }

    [[nodiscard]]
    friend constexpr bool operator<(detail::compare::unspecified,
                                    strong_ordering value) noexcept
    {
        return 0 < value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator>(detail::compare::unspecified,
                                    strong_ordering value) noexcept
    {
        return 0 > value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator<=(detail::compare::unspecified,
                                     strong_ordering value) noexcept
    {
        return 0 <= value.m_value;
    }

    [[nodiscard]]
    friend constexpr bool operator>=(detail::compare::unspecified,
                                     strong_ordering value) noexcept
    {
        return 0 >= value.m_value;
    }

    [[nodiscard]]
    friend constexpr strong_ordering
    operator<=>(strong_ordering value, detail::compare::unspecified) noexcept
    {
        return value;
    }

    [[nodiscard]]
    friend constexpr strong_ordering operator<=>(detail::compare::unspecified,
                                                 strong_ordering value) noexcept
    {
        return strong_ordering(detail::compare::orderable_enum(-value.m_value));
    }
};

// valid values' definitions
inline constexpr strong_ordering
    strong_ordering::less(detail::compare::orderable_enum::less);

inline constexpr strong_ordering
    strong_ordering::equal(detail::compare::orderable_enum::equivalent);

inline constexpr strong_ordering
    strong_ordering::equivalent(detail::compare::orderable_enum::equivalent);

inline constexpr strong_ordering
    strong_ordering::greater(detail::compare::orderable_enum::greater);
} // namespace ok::stdc

#endif
