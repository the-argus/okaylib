#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/bit_array.h"
#include "okay/containers/bit_arraylist.h"
#include "okay/ranges/algorithm.h"
#include "okay/ranges/for_each.h"
#include "okay/ranges/views/all.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/transform.h"

using namespace ok;

void print_bit_arraylist(const bit_arraylist_t<ok::allocator_t>& bs)
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
static_assert(
    !std::is_convertible_v<const bit_arraylist_t<c_allocator_t>&, bit_slice_t>);

TEST_SUITE("bit_arraylist_t")
{
    c_allocator_t c_allocator;
    TEST_CASE("dynamic bit_array")
    {
        SUBCASE("construction from allocator")
        {
            bit_arraylist_t test(c_allocator);
            REQUIRE(test.size_bits() == 0);
        }

        SUBCASE("move constructor upcast to ok::allocator_t")
        {
            bit_arraylist_t<c_allocator_t> first(c_allocator);
            static_assert(
                std::is_same_v<c_allocator_t, decltype(first)::allocator_type>);
            bit_arraylist_t second(std::move(first));
            REQUIRE(second.size_bits() == 0);
            static_assert(std::is_same_v<decltype(second)::allocator_type,
                                         decltype(first)::allocator_type>);
            // upcast, only possible implicitly in move assignment
            bit_arraylist_t<ok::allocator_t> third(c_allocator);
            third = std::move(second);
        }

        SUBCASE("upcasting move constructor")
        {
            bit_arraylist_t first(c_allocator);
            static_assert(
                std::is_same_v<c_allocator_t, decltype(first)::allocator_type>);

            bit_arraylist_t<ok::allocator_t> second(bit_arraylist::upcast_tag{},
                                                    std::move(first));

            REQUIRE(second.size_bits() == 0);
        }

        SUBCASE("items returns the correct thing by constness")
        {
            bit_arraylist_t dbs(c_allocator);

            bit_slice_t bits = dbs.items();
            const_bit_slice_t bits_const = dbs.items();
            const_bit_slice_t bits_const_2 =
                static_cast<const bit_arraylist_t<c_allocator_t>&>(dbs).items();
        }

        SUBCASE("can implicitly convert dynamic bit_array into bit_slice_t")
        {
            constexpr auto gets_slice = [](const_bit_slice_t bs) {
                bs | ok::for_each(
                         [](bool item) { fmt::print("{}", item ? "0" : "1"); });
                fmt::print("\n");
            };

            bit_arraylist_t dbs(c_allocator);

            gets_slice(dbs);
        }

        SUBCASE("copy booleans from range constructor")
        {
            array_t bools = {true, false, true, true};
            bit_arraylist_t copied =
                bit_arraylist::copy_booleans_from_range(c_allocator, bools)
                    .release();

            bit_arraylist_t copied2 =
                bit_arraylist::copy_booleans_from_range(
                    c_allocator, bit_array::bit_string("010011011"))
                    .release();

            REQUIRE(ranges_equal(copied2, bit_array::bit_string("010011011")));
            REQUIRE(ranges_equal(bools, copied));
            REQUIRE(ranges_equal(bit_array::bit_string("1011"), copied));
        }

        SUBCASE("preallocated and zeroed constructor")
        {
            bit_arraylist_t dbs = bit_arraylist::preallocated_and_zeroed(
                                      c_allocator,
                                      {
                                          .num_initial_bits = 100,
                                          .additional_capacity_in_bits = 500,
                                      })
                                      .release();
            REQUIRE(dbs.size_bits() == 100);
            REQUIRE(dbs.capacity_bits() >= 600);

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
            bit_arraylist_t dbs = bit_arraylist::preallocated_and_zeroed(
                                      c_allocator,
                                      {
                                          .num_initial_bits = 100,
                                          .additional_capacity_in_bits = 500,
                                      })
                                      .release();
            bit_arraylist_t dbs2 = bit_arraylist::preallocated_and_zeroed(
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

        SUBCASE("bit_string constructor")
        {
            const char literal[] = "1010101011";

            bit_arraylist_t dbs2 =
                bit_arraylist::bit_string(c_allocator, literal).release();

            REQUIRE(ranges_equal(dbs2, bit_array::bit_string(literal)));
            // take_at_most is here to skip null terminator
            REQUIRE(ranges_equal(
                dbs2, literal |
                          take_at_most(detail::c_array_length(literal) - 1) |
                          transform([](char c) { return c == '1'; })));
        }

        SUBCASE("insert_at on empty dbs")
        {
            bit_arraylist_t dbs(c_allocator);
            REQUIREABORTS(auto&& out_of_range = dbs.insert_at(1, true));
            REQUIRE(dbs.insert_at(0, true).okay());
            constexpr auto bs = bit_array::bit_string("1");
            REQUIRE(ranges_equal(dbs, bs));
        }

        SUBCASE(
            "insert_at on preinitialized, first_allocation dynamic bit_array")
        {
            bit_arraylist_t dbs =
                bit_arraylist::copy_booleans_from_range(
                    c_allocator, bit_array::bit_string("01010011"))
                    .release();
        }
    }
}
