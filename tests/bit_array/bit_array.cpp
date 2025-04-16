#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/bit_array.h"
#include "okay/containers/bit_arraylist.h"
#include "okay/ranges/algorithm.h"

using namespace ok;

template <size_t bits> void print_bit_array(const bit_array_t<bits>& bs)
{
    for (size_t i = 0; i < bs.size_bits(); ++i) {
        fmt::print("{}", bs.get_bit(i) ? "1" : "0");
    }
    fmt::print("\n");
}

// cant take a slice of rvalue
static_assert(
    !std::is_convertible_v<bit_arraylist_t<c_allocator_t>&&, bit_slice_t>);
static_assert(!std::is_convertible_v<bit_arraylist_t<c_allocator_t>&&,
                                     const_bit_slice_t>);
// cant convert const to nonconst
static_assert(!std::is_convertible_v<const bit_arraylist_t<c_allocator_t>&,
                                     bit_slice_t>);
static_assert(!std::is_convertible_v<bit_array_t<1>&&, bit_slice_t>);
static_assert(!std::is_convertible_v<bit_array_t<1>&&, const_bit_slice_t>);
// cant convert const to nonconst
static_assert(!std::is_convertible_v<const bit_array_t<1>&, bit_slice_t>);

TEST_SUITE("bit_array containers")
{
    c_allocator_t c_allocator;
    TEST_CASE("static bit_array")
    {
        SUBCASE("bit_array zeroed")
        {
            bit_array_t bs = bit_array::zeroed<16>();
            fmt::print("zeroed: ");
            print_bit_array(bs);

            // clang-format off
            REQUIRE(ok::ranges_equal(array_t{
                    false, false, false, false,
                    false, false, false, false,
                    false, false, false, false,
                    false, false, false, false,
                    }, bs));
            // clang-format on
        }

        SUBCASE("bit_array all on")
        {
            bit_array_t bs = bit_array::all_bits_on<16>();
            fmt::print("all on: ");
            print_bit_array(bs);

            // clang-format off
            REQUIRE(ok::ranges_equal(array_t{
                    true, true, true, true,
                    true, true, true, true,
                    true, true, true, true,
                    true, true, true, true,
                    }, bs));
            // clang-format on
        }

        SUBCASE("bit_array undefined")
        {
            // cant really test anything about this one
            bit_array_t<16> bs = bit_array::undefined<16>();
            fmt::print("undefined: ");
            print_bit_array(bs);
        }

        SUBCASE("bit_array from bit string")
        {
            bit_array_t bs = bit_array::bit_string("0101");
            static_assert(bs.size_bits() == 4);
            fmt::print("bit string: ");
            print_bit_array(bs);
            REQUIRE(ranges_equal(bs, ok::array_t{false, true, false, true}));
        }

        SUBCASE("bit_array == and != operators")
        {
            REQUIRE(bit_array::bit_string("01010") == bit_array::bit_string("01010"));
            REQUIRE(bit_array::bit_string("11010") != bit_array::bit_string("01010"));
            REQUIRE(bit_array::bit_string("100000000000000000000000000000000000000"
                                       "000000000000000000") !=
                    bit_array::bit_string("000000000000000000000000000000000000000"
                                       "000000000000000000"));
        }

        SUBCASE("toggle bit")
        {
            bit_array_t bs = bit_array::bit_string("00100");
            bs.toggle_bit(2);
            REQUIRE(bs == bit_array::bit_string("00000"));
            bs.toggle_bit(0);
            REQUIRE(bs == bit_array::bit_string("10000"));
            bs.toggle_bit(0);
            REQUIRE(bs == bit_array::bit_string("00000"));
        }

        SUBCASE("bit_array operator |")
        {
            bit_array_t a = bit_array::bit_string("0101");
            bit_array_t b = bit_array::bit_string("1010");
            REQUIRE((a | b) == bit_array::bit_string("1111"));

            a |= bit_array::bit_string("1100");
            REQUIRE(a == bit_array::bit_string("1101"));
        }

        SUBCASE("bit_array operator &")
        {
            bit_array_t a = bit_array::bit_string("0101");
            bit_array_t b = bit_array::bit_string("1010");
            static_assert(sizeof(a) == 1);
            REQUIRE((a & b) == bit_array::bit_string("0000"));

            a &= bit_array::bit_string("1100");
            REQUIRE(a == bit_array::bit_string("0100"));
        }

        SUBCASE("bit_array operator ^")
        {
            bit_array_t a = bit_array::bit_string("0101");
            bit_array_t b = bit_array::bit_string("1001");
            static_assert(sizeof(a) == 1);
            REQUIRE((a ^ b) == bit_array::bit_string("1100"));

            a ^= bit_array::bit_string("1100");
            REQUIRE(a == bit_array::bit_string("1001"));
        }

        SUBCASE("bit_array set_all_bits")
        {
            bit_array_t a = bit_array::bit_string("01010000111");
            a.set_all_bits(false);
            REQUIRE(a == bit_array::bit_string("00000000000"));
            a.set_all_bits(true);
            REQUIRE(a == bit_array::bit_string("11111111111"));
        }
    }
}
