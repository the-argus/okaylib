#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/bit_array.h"
#include "okay/containers/segmented_list.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/transform.h"
#include "testing_types.h"

using namespace ok;

TEST_SUITE("segmented list")
{
    c_allocator_t c_allocator;
    TEST_CASE("initialization with trivial type (ints)")
    {
        SUBCASE("empty constructor")
        {
            segmented_list_t a =
                segmented_list::empty<int>(c_allocator,
                                           {
                                               .expected_max_capacity = 0,
                                           })
                    .unwrap();

            segmented_list_t b =
                segmented_list::empty<int>(c_allocator,
                                           {
                                               .expected_max_capacity = 1,
                                           })
                    .unwrap();

            segmented_list_t c =
                segmented_list::empty<int>(c_allocator,
                                           {
                                               .expected_max_capacity = 21384,
                                           })
                    .unwrap();
            REQUIRE(a.size() == 0);
            REQUIRE(a.is_empty());
            REQUIRE(b.size() == 0);
            REQUIRE(b.is_empty());
            REQUIRE(c.size() == 0);
            REQUIRE(c.is_empty());
            auto&& ar = a.append(0);
            auto&& br = b.append(0);
            auto&& cr = c.append(0);
            REQUIRE(a.size() == 1);
            REQUIRE(!a.is_empty());
            REQUIRE(b.size() == 1);
            REQUIRE(!b.is_empty());
            REQUIRE(c.size() == 1);
            REQUIRE(!c.is_empty());
        }

        SUBCASE("copy items from range constructor")
        {
            constexpr auto rng = bit_array::bit_string("10101");

            segmented_list_t bools =
                segmented_list::copy_items_from_range(
                    c_allocator, rng | transform(&ok::bit::operator bool))
                    .unwrap();

            REQUIRE(ok::size(bools) == ok::size(rng));

            printf("bool 0: %s\n", bools[0] ? "true" : "false");
            printf("bool 1: %s\n", bools[1] ? "true" : "false");
            printf("bool 2: %s\n", bools[2] ? "true" : "false");
            printf("bool 3: %s\n", bools[3] ? "true" : "false");
            printf("bool 4: %s\n", bools[4] ? "true" : "false");
            REQUIREABORTS(auto&& _ = bools[5]);

            REQUIRE_RANGES_EQUAL(rng, bools);
        }
    }

    TEST_CASE("move constructor")
    {
        SUBCASE("move construct empty segmented lists")
        {
            auto res_a = segmented_list::empty<int>(c_allocator, {});
            auto& list_a = res_a.unwrap();

            segmented_list_t list_b(std::move(list_a));

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);

            // list_b should be the same, they're both empty anyways
            REQUIRE(list_b.capacity() == 0);
            REQUIRE(list_b.size() == 0);
        }

        SUBCASE("move construct copy_items_from_range segmented lists")
        {
            constexpr maybe_undefined_array_t initial = {0, 1, 2, 3};
            auto res_a =
                segmented_list::copy_items_from_range(c_allocator, initial);
            auto& list_a = res_a.unwrap();

            size_t original_capacity = list_a.capacity();
            REQUIRE_RANGES_EQUAL(initial, list_a);
            segmented_list_t list_b(std::move(list_a));
            REQUIRE(list_b.capacity() == original_capacity);
            REQUIRE_RANGES_EQUAL(initial, list_b);

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
        }

        SUBCASE("move construct first_allocation segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .expected_max_capacity = 4,
                             });
            auto& list_a = res_a.unwrap();
            REQUIRE(list_a.append(0).is_success());

            size_t original_capacity = list_a.capacity();
            REQUIRE(original_capacity == 4);
            REQUIRE(list_a.size() == 1);
            segmented_list_t list_b(std::move(list_a));

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);

            REQUIRE(list_b.capacity() == original_capacity);
            REQUIRE(list_b.size() == 1);
            REQUIRE(list_b[0] == 0);
        }

        SUBCASE("move construct reallocated segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .expected_max_capacity = 4,
                             });
            auto& list_a = res_a.unwrap();
            REQUIRE(list_a.append(0).is_success());
            REQUIRE(list_a.capacity() == 4);
            REQUIRE(list_a.append(1).is_success());
            REQUIRE(list_a.append(2).is_success());
            REQUIRE(list_a.append(3).is_success());
            REQUIRE(list_a.append(4).is_success());

            size_t original_capacity = list_a.capacity();
            REQUIRE(original_capacity == 12); // added 2^2 + 2^3
            REQUIRE(list_a.size() == 1);
            segmented_list_t list_b(std::move(list_a));

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);

            REQUIRE(list_b.capacity() == original_capacity);
            REQUIRE(list_b.size() == 5);
            REQUIRE(list_b[0] == 0);
            REQUIRE(list_b[1] == 1);
            REQUIRE(list_b[2] == 2);
            REQUIRE(list_b[3] == 3);
            REQUIRE(list_b[4] == 4);
        }
    }

    TEST_CASE("move assignment")
    {
        SUBCASE("move assign empty segmented lists")
        {
            auto res_a = segmented_list::empty<int>(c_allocator, {});
            auto& list_a = res_a.unwrap();
            auto res_b = segmented_list::empty<int>(c_allocator, {});
            auto& list_b = res_b.unwrap();

            list_a = std::move(list_b);
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
            REQUIRE(list_a.append(0).is_success());
            REQUIRE(list_b.capacity() == 0);
            REQUIRE(list_b.size() == 0);
            REQUIRE(list_b.append(0).is_success());
        }

        SUBCASE("move assign copy_items_from_range segmented lists")
        {
            auto res_a = segmented_list::empty<int>(c_allocator, {});
            auto& list_a = res_a.unwrap();
            constexpr maybe_undefined_array_t initial = {0, 1, 2, 3, 4, 5};
            auto res_b =
                segmented_list::copy_items_from_range(c_allocator, initial);
            auto& list_b = res_b.unwrap();

            REQUIRE(list_b.capacity() == 8);
            REQUIRE_RANGES_EQUAL(list_b, initial);

            REQUIRE(list_a.append(0).is_success());
            REQUIRE(list_a.capacity() > 0);

            list_a = std::move(list_b);

            REQUIRE(list_a.size() == initial.size());
            REQUIRE(list_a.capacity() == 8);

            // std::move capacity retaining optimization is implemented
            REQUIRE(list_b.capacity() > 0);
            REQUIRE(list_b.is_empty());
        }

        SUBCASE("move assign first_allocation segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .expected_max_capacity = 16,
                             });
            auto& list_a = res_a.unwrap();
            auto res_b = segmented_list::empty<int>(
                c_allocator, {
                                 .expected_max_capacity = 16,
                             });
            auto& list_b = res_b.unwrap();

            REQUIRE(list_a.append(0).is_success());
            REQUIRE(list_b.append(0).is_success());
            REQUIRE(list_a.capacity() == 16);
            REQUIRE(list_b.capacity() == 16);

            for (size_t i = 1; i < 20; ++i) {
                REQUIRE(list_b.append(i).is_success());
            }

            size_t list_b_cap = list_b.capacity();
            size_t list_a_cap = list_a.capacity();
            REQUIRE(list_a_cap != list_b_cap); // we need to observe the swap

            list_a = std::move(list_b);

            // optimization where they switch buffers
            REQUIRE(list_a.capacity() == list_b_cap);
            REQUIRE(list_b.capacity() == list_a_cap);
            REQUIRE(list_b.is_empty());
            REQUIRE(list_b.append(1).is_success());
        }

        // NOTE: this test is identical to the prev. test with the lists swapped
        // at the move assignment.
        SUBCASE("move assign reallocated segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .expected_max_capacity = 16,
                             });
            auto& list_a = res_a.unwrap();
            auto res_b = segmented_list::empty<int>(
                c_allocator, {
                                 .expected_max_capacity = 16,
                             });
            auto& list_b = res_b.unwrap();

            REQUIRE(list_a.append(0).is_success());
            REQUIRE(list_b.append(0).is_success());
            REQUIRE(list_a.capacity() == 16);
            REQUIRE(list_b.capacity() == 16);

            for (size_t i = 1; i < 20; ++i) {
                REQUIRE(list_b.append(i).is_success());
            }

            size_t list_b_cap = list_b.capacity();
            size_t list_a_cap = list_a.capacity();
            REQUIRE(list_a_cap != list_b_cap); // we need to observe the swap

            list_b = std::move(list_a);

            // optimization where they switch buffers
            REQUIRE(list_a.capacity() == list_b_cap);
            REQUIRE(list_b.capacity() == list_a_cap);
            REQUIRE(list_a.is_empty());
            REQUIRE(list_a.append(1).is_success());
        }
    }

    TEST_CASE("insert_at")
    {
        SUBCASE(
            "insert into segmented list after different amounts of allocation")
        {
            auto res = segmented_list::empty<int>(c_allocator, {});
            auto& list = res.unwrap();

            REQUIREABORTS(auto&& _ = list.insert_at(1, 0));
            REQUIRE(list.insert_at(0, 1).is_success());
            // make sure a reallocation is going to happen
            REQUIRE(list.capacity() < 5);
            REQUIRE(list[0] == 1);
            REQUIRE_RANGES_EQUAL(list, ok::maybe_undefined_array_t{1});
            REQUIRE(list.insert_at(0, 0).is_success());
            REQUIRE(list.insert_at(2, 2).is_success());
            REQUIRE(list.insert_at(3, 3).is_success());
            REQUIRE(list.insert_at(4, 4).is_success());
            constexpr auto expected =
                ok::maybe_undefined_array_t{0, 1, 2, 3, 4};
            REQUIRE_RANGES_EQUAL(list, expected);
            REQUIRE(list.capacity() >= 5);
        }

        SUBCASE("insert into copy_items_from_range segmented lists")
        {
            constexpr maybe_undefined_array_t initial = {0, 1, 2, 3};
            segmented_list_t list =
                segmented_list::copy_items_from_range(c_allocator, initial)
                    .unwrap();

            REQUIRE_RANGES_EQUAL(initial, list);
            REQUIREABORTS(auto&& _ = list.insert_at(initial.size(), 0));
            REQUIRE(list.insert_at(0, 0).is_success());
            REQUIRE(list.insert_at(5, 4).is_success());
            constexpr auto arr = ok::maybe_undefined_array_t{0, 0, 1, 2, 3, 4};
            REQUIRE_RANGES_EQUAL(list, arr);
        }

        SUBCASE("append")
        {
            auto res = segmented_list::empty<int>(c_allocator, {});
            auto& list = res.unwrap();

            for (size_t i = 0; i < 50; ++i) {
                REQUIREABORTS(auto&& ignored = list[i]);
                REQUIRE(list.append(i).is_success());
            }

            REQUIRE_RANGES_EQUAL(list, indices);
        }
    }

    TEST_CASE("clear() calls destructors")
    {
        counter_type::reset_counters();

        segmented_list_t list =
            segmented_list::empty<counter_type>(c_allocator, {}).unwrap();

        for (int i = 0; i < 5; ++i) {
            REQUIRE(list.append().is_success());
        }

        REQUIRE(list.size() == 5);
        REQUIRE(counter_type::counters.default_constructs == 5);
        REQUIRE(counter_type::counters.destructs == 0);

        list.clear();

        REQUIRE(list.size() == 0);
        REQUIRE(counter_type::counters.default_constructs == 5);
        REQUIRE(counter_type::counters.destructs == 5);
    }

    TEST_CASE("being move-assigned over calls destructors")
    {
        counter_type::reset_counters();

        segmented_list_t list_a =
            segmented_list::empty<counter_type>(c_allocator, {}).unwrap();
        segmented_list_t list_b =
            segmented_list::empty<counter_type>(c_allocator, {}).unwrap();

        REQUIRE(list_a.append().is_success());
        REQUIRE(list_a.append().is_success());
        REQUIRE(list_b.append().is_success());
        REQUIRE(counter_type::counters.default_constructs == 3);
        REQUIRE(counter_type::counters.destructs == 0);
        REQUIRE(counter_type::counters.copy_assigns == 0);
        REQUIRE(counter_type::counters.move_assigns == 0);
        REQUIRE(counter_type::counters.copy_constructs == 0);
        REQUIRE(counter_type::counters.move_constructs == 0);

        list_b = std::move(list_a);

        REQUIRE(counter_type::counters.default_constructs == 3);
        // one object was destroyed, the others were not moved at all as the
        // buffer was just traded
        REQUIRE(counter_type::counters.destructs == 1);
        REQUIRE(counter_type::counters.copy_assigns == 0);
        REQUIRE(counter_type::counters.move_assigns == 0);
        REQUIRE(counter_type::counters.copy_constructs == 0);
        REQUIRE(counter_type::counters.move_constructs == 0);
    }

    TEST_CASE("remove calls move constructor then destructor") {}

    TEST_CASE("pop_last()")
    {
        segmented_list_t list =
            segmented_list::empty<int>(c_allocator, {}).unwrap();
        REQUIRE(!list.pop_last().has_value()); // pop empty
        REQUIRE(list.append(10).is_success());
        REQUIRE(list.append(20).is_success());
        REQUIRE(list.append(30).is_success());
        REQUIRE(list.pop_last().ref_or_panic() == 30);
        REQUIRE(list.size() == 2);
        REQUIRE(list.last() == 20);
        REQUIRE(list.pop_last().ref_or_panic() == 20);
        REQUIRE(list.pop_last().ref_or_panic() == 10);
        REQUIRE(list.size() == 0);
        REQUIRE(!list.pop_last().has_value());
    }

    TEST_CASE("remove()")
    {
        segmented_list_t list =
            segmented_list::empty<int>(c_allocator, {}).unwrap();

        for (int i = 0; i < 7; ++i) { // creates blocks sized 1, 2, 4
            REQUIRE(list.append(i).is_success());
        }

        // remove from the middle of a block
        REQUIRE(list.remove(4) == 4);
        REQUIRE(list.size() == 6);
        constexpr ok::maybe_undefined_array_t four_removed{0, 1, 2, 3, 5, 6};
        REQUIRE_RANGES_EQUAL(list, four_removed);

        // test remove from the first item of a block (cross-block shift)
        REQUIRE(list.remove(1) == 1);
        constexpr ok::maybe_undefined_array_t one_and_four_removed{0, 2, 3, 5,
                                                                   6};
        REQUIRE_RANGES_EQUAL(list, one_and_four_removed);

        REQUIRE(list.remove(0) == 0);
        constexpr ok::maybe_undefined_array_t zero_one_and_four_removed{2, 3, 5,
                                                                        6};
        REQUIRE_RANGES_EQUAL(list, zero_one_and_four_removed);
    }

    TEST_CASE("remove_and_swap_last()")
    {
        segmented_list_t list =
            segmented_list::empty<int>(c_allocator, {}).unwrap();

        for (int i = 0; i < 7; ++i) {
            REQUIRE(list.append(i).is_success());
        }

        REQUIRE(list.remove_and_swap_last(1) == 1);
        // {0, 6, 2, 3, 4, 5}
        REQUIRE(list.size() == 6);
        REQUIRE(list[1] == 6);
        REQUIRE(list.last() == 5);

        REQUIRE(list.remove_and_swap_last(0) == 0);
        // {5, 6, 2, 3, 4}
        REQUIRE(list.size() == 5);
        REQUIRE(list[0] == 5);
        REQUIRE(list.last() == 4);
    }

    TEST_CASE("last()")
    {
        segmented_list_t list =
            segmented_list::empty<int>(c_allocator, {}).unwrap();
        REQUIREABORTS(list.last());

        REQUIRE(list.append(10).is_success());
        REQUIRE(list.last() == 10);
        list.last() = 20;
        REQUIRE(list.last() == 20);
        REQUIRE(list[0] == 20);
    }

    TEST_CASE("first()")
    {
        segmented_list_t list =
            segmented_list::empty<int>(c_allocator, {}).unwrap();
        REQUIREABORTS(list.first());

        REQUIRE(list.append(10).is_success());
        REQUIRE(list.first() == 10);
        list.first() = 5;
        REQUIRE(list.first() == 5);
        REQUIRE(list[0] == 5);
    }

    TEST_CASE("ensure_total_capacity_is_at_least()")
    {
        auto res = segmented_list::empty<int>(c_allocator, {});
        auto& list = res.unwrap();

        REQUIRE(list.ensure_total_capacity_is_at_least(0).is_success());

        // needs 3 blocks for 7 spots: 1, 2, 4
        REQUIRE(list.ensure_total_capacity_is_at_least(5).is_success());
        REQUIRE(list.capacity() == 7);
        REQUIRE(list.size() == 0);

        REQUIRE(list.append(0).is_success()); // size 1, cap 7
        REQUIRE(list.ensure_total_capacity_is_at_least(1).is_success());
        REQUIRE(list.capacity() == 7);

        // needs 4 blocks for 15 spots: 1, 2, 4, 8
        REQUIRE(list.ensure_total_capacity_is_at_least(10).is_success());
        REQUIRE(list.capacity() == 15);
    }
}
