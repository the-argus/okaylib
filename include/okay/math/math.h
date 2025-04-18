#ifndef __OKAYLIB_MATH_H__
#define __OKAYLIB_MATH_H__

#include "okay/detail/ok_assert.h"
#include <type_traits>

namespace ok {

template <typename int_t>
[[nodiscard]] constexpr bool is_power_of_two(int_t number) noexcept
{
    static_assert(
        std::is_integral_v<int_t>,
        "Attempt to call is_power_of_two with a type which is not an integer.");
    if (number <= 0) [[unlikely]] {
        return false;
    }
    return (number & (number - 1)) == 0;
}

/// Version of is_power_of_two which just asserts that the number is greater
/// than zero and avoids a check in release mode.
template <typename uint_t>
[[nodiscard]] constexpr bool positive_is_power_of_two(uint_t number) noexcept
{
    static_assert(
        std::is_integral_v<uint_t>,
        "Attempt to call is_power_of_two with a type which is not an integer.");
    __ok_assert(number > 0,
                "Attempt to call positive_is_power_of_two with non-positive "
                "value, which incorrectly returns true.");
    return (number & (number - 1)) == 0;
}

template <typename T>
[[nodiscard]] constexpr T log2_uint(T number) OKAYLIB_NOEXCEPT
{
    static_assert(
        std::is_unsigned_v<T>,
        "Attempt to call log2_int with a non-(unsigned integer) type.");
    __ok_assert(number != 0, "Attempt to call log2_uint with zero.");
    // TODO: do this and detect x86 and ARM. ARM has clz instruction
    // uint32_t y;
    // asm("\tbsr %1, %0\n" : "=r"(y) : "r"(x));
    // return y;

    T targetlevel = 0;
    while (number >>= 1)
        ++targetlevel;
    return targetlevel;
}

template <typename T>
[[nodiscard]] constexpr T log2_uint_ceil(T number) OKAYLIB_NOEXCEPT
{
    const T log2 = ok::log2_uint(number);
    if (T(1) << log2 == number)
        return log2;
    return log2 + 1;
}

static_assert(log2_uint(1U) == 0); // power of zero gives 1
static_assert(log2_uint(3U) == 1); // power of 1 gives 2
static_assert(log2_uint_ceil(3U) == 2);
static_assert(log2_uint(4U) == 2);
static_assert(log2_uint(7U) == 2);
static_assert(log2_uint(8U) == 3);
static_assert(log2_uint(8U) == 3);
static_assert(log2_uint(16U) == 4);
static_assert(log2_uint(32U) == 5);
static_assert(log2_uint(64U) == 6);

[[nodiscard]] constexpr size_t
two_to_the_power_of(const size_t exponent) noexcept
{
    return 1UL << exponent;
}

static_assert(two_to_the_power_of(0) == 1);
static_assert(two_to_the_power_of(1) == 2);
static_assert(two_to_the_power_of(2) == 4);
static_assert(two_to_the_power_of(3) == 8);

} // namespace ok

#endif
