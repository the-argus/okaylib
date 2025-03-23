#ifndef __OKAYLIB_CONTAINERS_BITSET_H__
#define __OKAYLIB_CONTAINERS_BITSET_H__

#include "okay/construct.h"
#include "okay/math/rounding.h"
#include <cstdint>

namespace ok {

namespace detail {
template <const char* str> constexpr size_t constexpr_strlen() noexcept
{
    size_t i = 0;
    while (str[0] != '\0') {
        ++i;
    }
    return i;
}
} // namespace detail

template <size_t num_bits> class bitset_t
{
    static constexpr size_t num_bytes =
        round_up_to_multiple_of<8>(num_bits) / 8;
    uint8_t bytes[num_bytes];

  public:
    constexpr void set_bit(size_t idx, bool on) OKAYLIB_NOEXCEPT
    {
        const size_t byte = idx / 8;
        const size_t bit = idx % 8;

        const uint8_t mask = uint8_t(1) << bit;

        if (on) {
            bytes[byte] |= mask;
        } else {
            bytes[byte] &= ~mask;
        }
    }

    [[nodiscard]] constexpr bool get_bit(size_t idx) const OKAYLIB_NOEXCEPT
    {
        const size_t byte = idx / 8;
        const size_t bit = idx % 8;
        const uint8_t mask = uint8_t(1) << bit;
        return bytes[byte] & mask;
    }

    constexpr void toggle_bit(size_t idx) OKAYLIB_NOEXCEPT
    {
        const size_t byte = idx / 8;
        const size_t bit = idx % 8;
        const uint8_t mask = uint8_t(1) << bit;
        bytes[byte] ^= mask;
    }
};

template <size_t max_elems> struct range_definition<bitset_t<max_elems>>
{
    using bitset_t = bitset_t<max_elems>;
    static constexpr size_t begin(const bitset_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }
    static constexpr bool is_inbounds(const bitset_t& bs,
                                      size_t cursor) OKAYLIB_NOEXCEPT
    {
        return cursor < max_elems;
    }

    static constexpr size_t size(const bitset_t& bs) OKAYLIB_NOEXCEPT
    {
        return max_elems;
    }

    static constexpr bool get(const bitset_t& range, size_t cursor)
    {
        return range.get_bit(cursor);
    }

    static constexpr void set(bitset_t& range, size_t cursor, bool value)
    {
        return range.set_bit(cursor, value);
    }
};

namespace bitset {
template <const char* str> struct bit_string
{
    template <typename... args_t>
    using associated_type = bitset_t<detail::constexpr_strlen<str>()>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bitset_t<detail::constexpr_strlen<str>()>
    make() const OKAYLIB_NOEXCEPT
    {
        constexpr size_t num_bits = detail::constexpr_strlen<str>();
        using out_t = bitset_t<num_bits>;

        // TODO
    }
};
} // namespace bitset
} // namespace ok

#endif
