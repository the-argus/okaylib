#include "okay/ranges/algorithm.h"
#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/bitset.h"
#include "okay/containers/dynamic_bitset.h"

using namespace ok;

template <size_t bits> void print_bitset(const bitset_t<bits>& bs)
{
    for (size_t i = 0; i < bs.size_bits(); ++i) {
        fmt::print("{}", bs.get_bit(i) ? "1" : "0");
    }
    fmt::print("\n");
}

TEST_SUITE("bitset containers")
{
    c_allocator_t c_allocator;
    TEST_CASE("static bitset")
    {
        SUBCASE("bitset zeroed")
        {
            bitset_t bs = bitset::zeroed<16>();
            fmt::print("zeroed: ");
            print_bitset(bs);

            // clang-format off
            REQUIRE(ok::ranges_equal(array_t{
                    false, false, false, false,
                    false, false, false, false,
                    false, false, false, false,
                    false, false, false, false,
                    }, bs));
            // clang-format on
        }

        SUBCASE("bitset all on")
        {
            bitset_t bs = bitset::all_bits_on<16>();
            fmt::print("all on: ");
            print_bitset(bs);

            // clang-format off
            REQUIRE(ok::ranges_equal(array_t{
                    true, true, true, true,
                    true, true, true, true,
                    true, true, true, true,
                    true, true, true, true,
                    }, bs));
            // clang-format on
        }

        SUBCASE("bitset undefined")
        {
            // cant really test anything about this one
            bitset_t<16> bs = bitset::undefined<16>();
            fmt::print("undefined: ");
            print_bitset(bs);
        }

        SUBCASE("bitset from bit string")
        {
            bitset_t bs = bitset::bit_string("0101");
            static_assert(bs.size_bits() == 4);
            fmt::print("bit string: ");
            print_bitset(bs);
            REQUIRE(ranges_equal(bs, ok::array_t{false, true, false, true}));
        }

        SUBCASE("bitset == and != operators")
        {
            REQUIRE(bitset::bit_string("01010") == bitset::bit_string("01010"));
            REQUIRE(bitset::bit_string("11010") != bitset::bit_string("01010"));
            REQUIRE(bitset::bit_string("100000000000000000000000000000000000000"
                                       "000000000000000000") !=
                    bitset::bit_string("000000000000000000000000000000000000000"
                                       "000000000000000000"));
        }

        SUBCASE("toggle bit")
        {
            bitset_t bs = bitset::bit_string("00100");
            bs.toggle_bit(2);
            REQUIRE(bs == bitset::bit_string("00000"));
            bs.toggle_bit(0);
            REQUIRE(bs == bitset::bit_string("10000"));
            bs.toggle_bit(0);
            REQUIRE(bs == bitset::bit_string("00000"));
        }

        SUBCASE("bitset operator |")
        {
            bitset_t a = bitset::bit_string("0101");
            bitset_t b = bitset::bit_string("1010");
            REQUIRE((a | b) == bitset::bit_string("1111"));

            a |= bitset::bit_string("1100");
            REQUIRE(a == bitset::bit_string("1101"));
        }

        SUBCASE("bitset operator &")
        {
            bitset_t a = bitset::bit_string("0101");
            bitset_t b = bitset::bit_string("1010");
            static_assert(sizeof(a) == 1);
            REQUIRE((a & b) == bitset::bit_string("0000"));

            a &= bitset::bit_string("1100");
            REQUIRE(a == bitset::bit_string("0100"));
        }

        SUBCASE("bitset operator ^")
        {
            bitset_t a = bitset::bit_string("0101");
            bitset_t b = bitset::bit_string("1001");
            static_assert(sizeof(a) == 1);
            REQUIRE((a ^ b) == bitset::bit_string("1100"));

            a ^= bitset::bit_string("1100");
            REQUIRE(a == bitset::bit_string("1001"));
        }
    }
}
