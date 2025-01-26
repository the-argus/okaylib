#ifndef __OKAYLIB_MATH_ORDERING_H__
#define __OKAYLIB_MATH_ORDERING_H__

#include <cstdint>

namespace ok {

// TODO: c++20 compat for these types to convert from std::partial_ordering and
// std::weak_ordering

namespace detail::cmp_constants {
using type = int8_t;

enum class ordering_constants : type
{
    equivalent = 0,
    less = -1,
    greater = 1,
};
enum class unordered_constants : type
{
    unordered = 2,
};
}; // namespace detail::cmp_constants

enum class partial_ordering : detail::cmp_constants::type
{
    equivalent = detail::cmp_constants::type(
        detail::cmp_constants::ordering_constants::equivalent),
    less = detail::cmp_constants::type(
        detail::cmp_constants::ordering_constants::less),
    greater = detail::cmp_constants::type(
        detail::cmp_constants::ordering_constants::greater),
    unordered = detail::cmp_constants::type(
        detail::cmp_constants::unordered_constants::unordered),
};

// like an enum, but we can provide some nice operators/conversions
struct ordering
{
  private:
    detail::cmp_constants::ordering_constants representation;
    constexpr explicit ordering(
        detail::cmp_constants::ordering_constants r) noexcept
        : representation(r)
    {
    }
    constexpr ordering() = delete;

  public:
    // implicitly convert ordering to partial ordering
    constexpr operator partial_ordering() const noexcept
    {
        return partial_ordering(detail::cmp_constants::type(representation));
    }

    constexpr friend bool operator==(const ordering& self,
                                     const partial_ordering& rhs) noexcept
    {
        // compare underlying int types
        return detail::cmp_constants::type(self.representation) ==
               detail::cmp_constants::type(rhs);
    }
    constexpr friend bool operator==(const partial_ordering& rhs,
                                     const ordering& self) noexcept
    {
        return detail::cmp_constants::type(self.representation) ==
               detail::cmp_constants::type(rhs);
    }

    static const ordering equivalent;
    static const ordering less;
    static const ordering greater;
};

inline constexpr ordering
    ordering::less(detail::cmp_constants::ordering_constants::less);
inline constexpr ordering
    ordering::equivalent(detail::cmp_constants::ordering_constants::equivalent);
inline constexpr ordering
    ordering::greater(detail::cmp_constants::ordering_constants::greater);

namespace detail {
template <typename T> struct compare_fn_t
{
    constexpr ordering operator()(T&& lhs, T&& rhs)
    {
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

template <typename T> struct partial_compare_fn_t
{
    constexpr partial_ordering operator()(T&& lhs, T&& rhs)
    {
        if (lhs == rhs) {
            return partial_ordering::equivalent;
        } else if (lhs < rhs) {
            return partial_ordering::less;
        } else if (lhs > rhs) {
            return partial_ordering::greater;
        } else {
            return partial_ordering::unordered;
        }
        // TODO: ifdef c++20, use spaceship operator
    }
};

} // namespace detail

template <typename T> inline constexpr detail::compare_fn_t<T> cmp;

template <typename T>
inline constexpr detail::partial_compare_fn_t<T> partial_cmp;

} // namespace ok

#endif
