#ifndef __OKAYLIB_CONTAINERS_BITSET_H__
#define __OKAYLIB_CONTAINERS_BITSET_H__

#include "okay/construct.h"
#include "okay/math/rounding.h"
#include "okay/ranges/ranges.h"
#include "okay/slice.h"
#include <cstdint>

namespace ok {

namespace bit_array::detail {
template <size_t num_bits> struct all_bits_on_t;
template <size_t num_bits> struct zeroed_t;
template <size_t num_bits> struct undefined_t;
struct bit_string_t;
} // namespace bit_array::detail

template <size_t num_bits> class bit_array_t
{
    static_assert(num_bits != 0, "Cannot create a bit_array of zero bits");
    static constexpr size_t num_bytes =
        round_up_to_multiple_of<8>(num_bits) / 8;
    static_assert(num_bytes != 0);
    uint8_t bytes[num_bytes];

    bit_array_t() = default;

  public:
    friend struct bit_array::detail::all_bits_on_t<num_bits>;
    friend struct bit_array::detail::zeroed_t<num_bits>;
    friend struct bit_array::detail::undefined_t<num_bits>;
    friend struct bit_array::detail::bit_string_t;

    constexpr bit_array_t operator&(const bit_array_t& other) const OKAYLIB_NOEXCEPT
    {
        bit_array_t out;
        for (size_t i = 0; i < num_bytes; ++i) {
            out.bytes[i] = other.bytes[i] & bytes[i];
        }
        return out;
    }

    constexpr bit_array_t& operator&=(const bit_array_t& other) OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] &= other.bytes[i];
        }
        return *this;
    }

    constexpr bit_array_t operator|(const bit_array_t& other) const OKAYLIB_NOEXCEPT
    {
        bit_array_t out;
        for (size_t i = 0; i < num_bytes; ++i) {
            out.bytes[i] = other.bytes[i] | bytes[i];
        }
        return out;
    }

    constexpr bit_array_t& operator|=(const bit_array_t& other) OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] |= other.bytes[i];
        }
        return *this;
    }

    constexpr bit_array_t operator^(const bit_array_t& other) const OKAYLIB_NOEXCEPT
    {
        bit_array_t out;
        for (size_t i = 0; i < num_bytes; ++i) {
            out.bytes[i] = other.bytes[i] ^ bytes[i];
        }
        return out;
    }

    constexpr bit_array_t& operator^=(const bit_array_t& other) OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] ^= other.bytes[i];
        }
        return *this;
    }

    [[nodiscard]] constexpr bit_slice_t items() & OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(slice(bytes), num_bits, 0);
    }

    [[nodiscard]] constexpr const_bit_slice_t items() const& OKAYLIB_NOEXCEPT
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

    constexpr operator bit_slice_t() & OKAYLIB_NOEXCEPT { return items(); }

    constexpr operator const_bit_slice_t() const& OKAYLIB_NOEXCEPT
    {
        return items();
    }

    constexpr operator bit_slice_t() && = delete;
    constexpr operator bit_slice_t() const&& = delete;
    constexpr operator const_bit_slice_t() && = delete;
    constexpr operator const_bit_slice_t() const&& = delete;
    constexpr bit_slice_t items() && = delete;
    constexpr const_bit_slice_t items() const&& = delete;

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

    friend constexpr bool operator==(const bit_array_t& lhs, const bit_array_t& rhs)
    {
        // TODO: use bytewise == comparison here instead of doing math on every
        // bit for no reeason
        for (size_t i = 0; i < num_bits; ++i) {
            if (lhs.get_bit(i) != rhs.get_bit(i)) {
                return false;
            }
        }
        return true;
    }
    friend constexpr bool operator!=(const bit_array_t& lhs, const bit_array_t& rhs)
    {
        return !(lhs == rhs);
    }
};

template <size_t max_elems> struct range_definition<bit_array_t<max_elems>>
{
    using bit_array_t = bit_array_t<max_elems>;
    static constexpr size_t begin(const bit_array_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }
    static constexpr bool is_inbounds(const bit_array_t& bs,
                                      size_t cursor) OKAYLIB_NOEXCEPT
    {
        return cursor < max_elems;
    }

    static constexpr size_t size(const bit_array_t& bs) OKAYLIB_NOEXCEPT
    {
        return max_elems;
    }

    static constexpr bool get(const bit_array_t& range, size_t cursor)
    {
        return range.get_bit(cursor);
    }

    static constexpr void set(bit_array_t& range, size_t cursor, bool value)
    {
        return range.set_bit(cursor, value);
    }
};

namespace bit_array {
namespace detail {
struct bit_string_t
{
    template <typename c_array_ref, typename... args_t>
    using associated_type = bit_array_t<ok::detail::c_array_length_t<
        std::remove_reference_t<c_array_ref>>::value>;

    template <size_t N>
    [[nodiscard]] constexpr auto
    operator()(const char (&literal)[N]) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, literal);
    }

    template <size_t N>
    [[nodiscard]] constexpr bit_array_t<N - 1>
    make(const char (&literal)[N]) const OKAYLIB_NOEXCEPT
    {
        bit_array_t<N - 1> out;

        for (size_t i = 0; i < N - 1; ++i) {
            out.set_bit(i, literal[i] == '1');
        }

        return out;
    }
};
template <size_t num_bits> struct zeroed_t
{
    template <typename...> using associated_type = bit_array_t<num_bits>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bit_array_t<num_bits> make() const OKAYLIB_NOEXCEPT
    {
        bit_array_t<num_bits> out;
        out.set_all_bits(false);
        return out;
    }
};
template <size_t num_bits> struct undefined_t
{
    template <typename...> using associated_type = bit_array_t<num_bits>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bit_array_t<num_bits> make() const OKAYLIB_NOEXCEPT
    {
        return bit_array_t<num_bits>();
        ;
    }
};
template <size_t num_bits> struct all_bits_on_t
{
    template <typename...> using associated_type = bit_array_t<num_bits>;

    [[nodiscard]] constexpr auto operator()() const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this);
    }

    [[nodiscard]] constexpr bit_array_t<num_bits> make() const OKAYLIB_NOEXCEPT
    {
        bit_array_t<num_bits> out;
        out.set_all_bits(true);
        return out;
    }
};
} // namespace detail
template <size_t num_bits>
inline constexpr detail::all_bits_on_t<num_bits> all_bits_on;
template <size_t num_bits> inline constexpr detail::zeroed_t<num_bits> zeroed;
template <size_t num_bits>
inline constexpr detail::undefined_t<num_bits> undefined;
inline constexpr detail::bit_string_t bit_string;
} // namespace bit_array
} // namespace ok

#endif
