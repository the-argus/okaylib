#ifndef __OKAYLIB_CONTAINERS_BITSET_H__
#define __OKAYLIB_CONTAINERS_BITSET_H__

#include "okay/construct.h"
#include "okay/math/rounding.h"
#include "okay/slice.h"
#include <cstdint>

namespace ok {

template <size_t num_bits> class bitset_t;

namespace detail {
template <const char* str> constexpr size_t constexpr_strlen() noexcept
{
    size_t i = 0;
    while (str[0] != '\0') {
        ++i;
    }
    return i;
}

template <size_t num_bits>
constexpr bitset_t<num_bits> default_construct_bitset();

} // namespace detail

template <size_t num_bits> class bitset_t
{
    static_assert(num_bits != 0, "Cannot create a bitset of zero bits");
    static constexpr size_t num_bytes =
        round_up_to_multiple_of<8>(num_bits) / 8;
    static_assert(num_bytes != 0);
    uint8_t bytes[num_bytes];

    bitset_t() = default;

  public:
    friend constexpr bitset_t default_construct_bitset();

    constexpr bitset_t operator&(const bitset_t& other) const OKAYLIB_NOEXCEPT
    {
        bitset_t out;
        for (size_t i = 0; i < num_bytes; ++i) {
            out.bytes[i] = other.bytes[i] & bytes[i];
        }
        return out;
    }

    constexpr bitset_t& operator&=(const bitset_t& other) OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] &= other.bytes[i];
        }
        return *this;
    }

    constexpr bitset_t operator|(const bitset_t& other) const OKAYLIB_NOEXCEPT
    {
        bitset_t out;
        for (size_t i = 0; i < num_bytes; ++i) {
            out.bytes[i] = other.bytes[i] | bytes[i];
        }
        return out;
    }

    constexpr bitset_t& operator|=(const bitset_t& other) OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] |= other.bytes[i];
        }
        return *this;
    }

    constexpr bitset_t operator^(const bitset_t& other) const OKAYLIB_NOEXCEPT
    {
        bitset_t out;
        for (size_t i = 0; i < num_bytes; ++i) {
            out.bytes[i] = other.bytes[i] ^ bytes[i];
        }
        return out;
    }

    constexpr bitset_t& operator^=(const bitset_t& other) OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] ^= other.bytes[i];
        }
        return *this;
    }

    [[nodiscard]] constexpr bit_slice_t items() OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(slice(bytes), num_bits, 0);
    }

    [[nodiscard]] constexpr const_bit_slice_t items() const OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(slice<const uint8_t>(bytes), num_bits, 0);
    }

    [[nodiscard]] constexpr size_t size_bytes() const OKAYLIB_NOEXCEPT
    {
        return num_bytes;
    }

    [[nodiscard]] constexpr size_t size_bits() const OKAYLIB_NOEXCEPT
    {
        return num_bits;
    }

    constexpr operator bit_slice_t() OKAYLIB_NOEXCEPT { return items(); }

    constexpr operator const_bit_slice_t() const OKAYLIB_NOEXCEPT
    {
        return items();
    }

    constexpr void set_all_bits(bool value) OKAYLIB_NOEXCEPT
    {
        std::memset(bytes, value ? char(-1) : char(0), num_bytes);
    }

    constexpr void set_bit(size_t idx, bool value) OKAYLIB_NOEXCEPT
    {
        items().set_bit(idx, value);
    }

    constexpr void toggle_bit(size_t idx) OKAYLIB_NOEXCEPT
    {
        items().toggle_bit(idx);
    }

    [[nodiscard]] constexpr bool get_bit(size_t idx) const OKAYLIB_NOEXCEPT
    {
        return items().get_bit(idx);
    }
};

namespace detail {
template <size_t num_bits>
constexpr bitset_t<num_bits> default_construct_bitset()
{
    return bitset_t<num_bits>();
}
} // namespace detail

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

        auto out = detail::default_construct_bitset<num_bits>();

        for (size_t i = 0; i < num_bits; ++i) {
            out.set_bit(i, str[i] == '1');
        }

        return out;
    }
};
template <size_t num_bits> struct zeroed
{
    template <typename...> using associated_type = bitset_t<num_bits>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bitset_t<num_bits> make() const OKAYLIB_NOEXCEPT
    {
        auto out = detail::default_construct_bitset<num_bits>();
        out.set_all_bits(false);
        return out;
    }
};
template <size_t num_bits> struct undefined
{
    template <typename...> using associated_type = bitset_t<num_bits>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bitset_t<num_bits> make() const OKAYLIB_NOEXCEPT
    {
        return detail::default_construct_bitset<num_bits>();
    }
};
template <size_t num_bits> struct all_bits_on
{
    template <typename...> using associated_type = bitset_t<num_bits>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bitset_t<num_bits> make() const OKAYLIB_NOEXCEPT
    {
        auto out = detail::default_construct_bitset<num_bits>();
        out.set_all_bits(true);
        return out;
    }
};
} // namespace bitset
} // namespace ok

#endif
