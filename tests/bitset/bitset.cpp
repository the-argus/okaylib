#include "okay/ranges/algorithm.h"
#include "okay/ranges/for_each.h"
#include "okay/ranges/views/all.h"
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

// cant take a slice of rvalue
static_assert(
    !std::is_convertible_v<dynamic_bitset_t<c_allocator_t>&&, bit_slice_t>);
static_assert(!std::is_convertible_v<dynamic_bitset_t<c_allocator_t>&&,
                                     const_bit_slice_t>);
// cant convert const to nonconst
static_assert(!std::is_convertible_v<const dynamic_bitset_t<c_allocator_t>&,
                                     bit_slice_t>);
static_assert(!std::is_convertible_v<bitset_t<1>&&, bit_slice_t>);
static_assert(!std::is_convertible_v<bitset_t<1>&&, const_bit_slice_t>);
// cant convert const to nonconst
static_assert(!std::is_convertible_v<const bitset_t<1>&, bit_slice_t>);

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

        SUBCASE("bitset set_all_bits")
        {
            bitset_t a = bitset::bit_string("01010000111");
            a.set_all_bits(false);
            REQUIRE(a == bitset::bit_string("00000000000"));
            a.set_all_bits(true);
            REQUIRE(a == bitset::bit_string("11111111111"));
        }
    }

    TEST_CASE("dynamic bitset")
    {
        SUBCASE("construction from allocator")
        {
            dynamic_bitset_t test(c_allocator);
            REQUIRE(test.size() == 0);
        }

        SUBCASE("move constructor upcast to ok::allocator_t")
        {
            dynamic_bitset_t<c_allocator_t> first(c_allocator);
            static_assert(
                std::is_same_v<c_allocator_t, decltype(first)::allocator_type>);
            dynamic_bitset_t second(std::move(first));
            REQUIRE(second.size() == 0);
            static_assert(std::is_same_v<decltype(second)::allocator_type,
                                         decltype(first)::allocator_type>);
            // upcast, only possible implicitly in move assignment
            dynamic_bitset_t<ok::allocator_t> third(c_allocator);
            third = std::move(second);
        }

        SUBCASE("upcasting move constructor")
        {
            dynamic_bitset_t first(c_allocator);
            static_assert(
                std::is_same_v<c_allocator_t, decltype(first)::allocator_type>);

            dynamic_bitset_t<ok::allocator_t> second(
                dynamic_bitset::upcast_tag{}, std::move(first));

            REQUIRE(second.size() == 0);
        }

        SUBCASE("items returns the correct thing by constness")
        {
            dynamic_bitset_t dbs(c_allocator);

            bit_slice_t bits = dbs.items();
            const_bit_slice_t bits_const = dbs.items();
            const_bit_slice_t bits_const_2 =
                static_cast<const dynamic_bitset_t<c_allocator_t>&>(dbs)
                    .items();
        }

        SUBCASE("can implicitly convert dynamic bitset into bit_slice_t")
        {
            constexpr auto gets_slice = [](const_bit_slice_t bs) {
                bs | ok::for_each(
                         [](bool item) { fmt::print("{}", item ? "0" : "1"); });
                fmt::print("\n");
            };

            dynamic_bitset_t dbs(c_allocator);

            gets_slice(dbs);
        }

        SUBCASE("copy booleans from range constructor")
        {
            array_t bools = {true, false, true, true};
            dynamic_bitset_t copied =
                dynamic_bitset::copy_booleans_from_range(c_allocator, bools)
                    .release();

            dynamic_bitset_t copied2 =
                dynamic_bitset::copy_booleans_from_range(
                    c_allocator, bitset::bit_string("010011011"))
                    .release();

            REQUIRE(ranges_equal(copied2, bitset::bit_string("010011011")));
            REQUIRE(ranges_equal(bools, copied));
            REQUIRE(ranges_equal(bitset::bit_string("1011"), copied));
        }

        SUBCASE("preallocated and zeroed constructor")
        {
            dynamic_bitset_t dbs = dynamic_bitset::preallocated_and_zeroed(
                                       c_allocator,
                                       {
                                           .num_initial_bits = 100,
                                           .additional_capacity_in_bits = 500,
                                       })
                                       .release();
            REQUIRE(dbs.size() == 100);
            REQUIRE(dbs.capacity() >= 600);

            const auto all_zeroed = all([](bool a) { return a == false; });
            const auto all_ones = all([](bool a) { return a == true; });
            bool good = all_zeroed(dbs);
            REQUIRE(good);

            dbs.set_all_bits(true);
            good = all_ones(dbs);
            REQUIRE(good);
        }

        SUBCASE("toggle and memcompare_with()")
        {
            dynamic_bitset_t dbs = dynamic_bitset::preallocated_and_zeroed(
                                       c_allocator,
                                       {
                                           .num_initial_bits = 100,
                                           .additional_capacity_in_bits = 500,
                                       })
                                       .release();
            dynamic_bitset_t dbs2 = dynamic_bitset::preallocated_and_zeroed(
                                        c_allocator,
                                        {
                                            .num_initial_bits = 100,
                                            .additional_capacity_in_bits = 500,
                                        })
                                        .release();

            REQUIRE(dbs.memcompare_with(dbs2));

            dbs.toggle_bit(1);

            REQUIRE(dbs.get_bit(1));

            REQUIRE(!dbs.memcompare_with(dbs2));

            dbs2.toggle_bit(1);
            REQUIRE(dbs2.get_bit(1));

            REQUIRE(dbs.memcompare_with(dbs2));
        }
    }
}
