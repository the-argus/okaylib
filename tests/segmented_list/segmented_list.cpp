#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/bit_array.h"
#include "okay/containers/segmented_list.h"
#include "okay/ranges/views/transform.h"

using namespace ok;

TEST_SUITE("segmented list")
{
    c_allocator_t c_allocator;
    TEST_CASE("initialization with trivial type (ints)")
    {
        SUBCASE("empty constructor")
        {
            segmented_list_t a = segmented_list::empty<int>(
                                     c_allocator,
                                     {
                                         .num_initial_contiguous_spots = 0,
                                     })
                                     .release();

            segmented_list_t b = segmented_list::empty<int>(
                                     c_allocator,
                                     {
                                         .num_initial_contiguous_spots = 1,
                                     })
                                     .release();

            segmented_list_t c = segmented_list::empty<int>(
                                     c_allocator,
                                     {
                                         .num_initial_contiguous_spots = 21384,
                                     })
                                     .release();
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
                    c_allocator,
                    rng | transform([](ok::bit b) { return bool(b); }), {})
                    .release();
            REQUIRE_RANGES_EQUAL(rng, bools);
        }
    }

    TEST_CASE("move constructor")
    {
        SUBCASE("move construct empty segmented lists")
        {
            auto res_a = segmented_list::empty<int>(c_allocator, {});
            auto& list_a = res_a.release_ref();

            segmented_list_t list_b(std::move(list_a));

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
            REQUIRE(&(list_a.allocator()) == &c_allocator);

            // list_b should be the same, they're both empty anyways
            REQUIRE(list_b.capacity() == 0);
            REQUIRE(list_b.size() == 0);
            REQUIRE(&(list_b.allocator()) == &c_allocator);
        }

        SUBCASE("move construct copy_items_from_range segmented lists")
        {
            constexpr array_t initial = {0, 1, 2, 3};
            auto res_a =
                segmented_list::copy_items_from_range(c_allocator, initial, {});
            auto& list_a = res_a.release_ref();

            size_t original_capacity = list_a.capacity();
            REQUIRE_RANGES_EQUAL(initial, list_a);
            segmented_list_t list_b(std::move(list_a));
            REQUIRE(list_b.capacity() == original_capacity);
            REQUIRE_RANGES_EQUAL(initial, list_b);

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
            REQUIRE(&(list_a.allocator()) == &c_allocator);
        }

        SUBCASE("move construct first_allocation segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .num_initial_contiguous_spots = 4,
                             });
            auto& list_a = res_a.release_ref();
            REQUIRE(list_a.append(0).okay());

            size_t original_capacity = list_a.capacity();
            REQUIRE(original_capacity == 4);
            REQUIRE(list_a.size() == 1);
            segmented_list_t list_b(std::move(list_a));

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
            REQUIRE(&(list_a.allocator()) == &c_allocator);

            REQUIRE(list_b.capacity() == original_capacity);
            REQUIRE(list_b.size() == 1);
            REQUIRE(list_b[0] == 0);
            REQUIRE(&(list_b.allocator()) == &c_allocator);
        }

        SUBCASE("move construct reallocated segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .num_initial_contiguous_spots = 4,
                             });
            auto& list_a = res_a.release_ref();
            REQUIRE(list_a.append(0).okay());
            REQUIRE(list_a.capacity() == 4);
            REQUIRE(list_a.append(1).okay());
            REQUIRE(list_a.append(2).okay());
            REQUIRE(list_a.append(3).okay());
            REQUIRE(list_a.append(4).okay());

            size_t original_capacity = list_a.capacity();
            REQUIRE(original_capacity == 12); // added 2^2 + 2^3
            REQUIRE(list_a.size() == 1);
            segmented_list_t list_b(std::move(list_a));

            // original list still in valid state
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
            REQUIRE(&(list_a.allocator()) == &c_allocator);

            REQUIRE(list_b.capacity() == original_capacity);
            REQUIRE(list_b.size() == 5);
            REQUIRE(list_b[0] == 0);
            REQUIRE(list_b[1] == 1);
            REQUIRE(list_b[2] == 2);
            REQUIRE(list_b[3] == 3);
            REQUIRE(list_b[4] == 4);
            REQUIRE(&(list_b.allocator()) == &c_allocator);
        }
    }

    TEST_CASE("move assignment")
    {
        SUBCASE("move assign empty segmented lists")
        {
            auto res_a = segmented_list::empty<int>(c_allocator, {});
            auto& list_a = res_a.release_ref();
            auto res_b = segmented_list::empty<int>(c_allocator, {});
            auto& list_b = res_b.release_ref();

            list_a = std::move(list_b);
            REQUIRE(list_a.capacity() == 0);
            REQUIRE(list_a.size() == 0);
            REQUIRE(list_a.append(0).okay());
            REQUIRE(list_b.capacity() == 0);
            REQUIRE(list_b.size() == 0);
            REQUIRE(list_b.append(0).okay());
        }

        SUBCASE("move assign copy_items_from_range segmented lists")
        {
            auto res_a = segmented_list::empty<int>(c_allocator, {});
            auto& list_a = res_a.release_ref();
            constexpr array_t initial = {0, 1, 2, 3, 4, 5};
            auto res_b =
                segmented_list::copy_items_from_range(c_allocator, initial, {});
            auto& list_b = res_b.release_ref();

            REQUIRE(list_b.capacity() == 8);
            REQUIRE_RANGES_EQUAL(list_b, initial);

            REQUIRE(list_a.append(0).okay());
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
                                 .num_initial_contiguous_spots = 16,
                             });
            auto& list_a = res_a.release_ref();
            auto res_b = segmented_list::empty<int>(
                c_allocator, {
                                 .num_initial_contiguous_spots = 16,
                             });
            auto& list_b = res_b.release_ref();

            REQUIRE(list_a.append(0).okay());
            REQUIRE(list_b.append(0).okay());
            REQUIRE(list_a.capacity() == 16);
            REQUIRE(list_b.capacity() == 16);

            for (size_t i = 1; i < 20; ++i) {
                REQUIRE(list_b.append(i).okay());
            }

            size_t list_b_cap = list_b.capacity();
            size_t list_a_cap = list_a.capacity();
            REQUIRE(list_a_cap != list_b_cap); // we need to observe the swap

            list_a = std::move(list_b);

            // optimization where they switch buffers
            REQUIRE(list_a.capacity() == list_b_cap);
            REQUIRE(list_b.capacity() == list_a_cap);
            REQUIRE(list_b.is_empty());
            REQUIRE(list_b.append(1).okay());
        }

        // NOTE: this test is identical to the prev. test with the lists swapped
        // at the move assignment.
        SUBCASE("move assign reallocated segmented lists")
        {
            auto res_a = segmented_list::empty<int>(
                c_allocator, {
                                 .num_initial_contiguous_spots = 16,
                             });
            auto& list_a = res_a.release_ref();
            auto res_b = segmented_list::empty<int>(
                c_allocator, {
                                 .num_initial_contiguous_spots = 16,
                             });
            auto& list_b = res_b.release_ref();

            REQUIRE(list_a.append(0).okay());
            REQUIRE(list_b.append(0).okay());
            REQUIRE(list_a.capacity() == 16);
            REQUIRE(list_b.capacity() == 16);

            for (size_t i = 1; i < 20; ++i) {
                REQUIRE(list_b.append(i).okay());
            }

            size_t list_b_cap = list_b.capacity();
            size_t list_a_cap = list_a.capacity();
            REQUIRE(list_a_cap != list_b_cap); // we need to observe the swap

            list_b = std::move(list_a);

            // optimization where they switch buffers
            REQUIRE(list_a.capacity() == list_b_cap);
            REQUIRE(list_b.capacity() == list_a_cap);
            REQUIRE(list_a.is_empty());
            REQUIRE(list_a.append(1).okay());
        }
    }

    TEST_CASE("insert_at")
    {
        SUBCASE(
            "insert into segmented list after different amounts of allocation")
        {
            auto res = segmented_list::empty<int>(c_allocator, {});
            auto& list = res.release_ref();

            REQUIREABORTS(auto&& _ = list.insert_at(1, 0));
            REQUIRE(list.insert_at(0, 1).okay());
            // make sure a reallocation is going to happen
            REQUIRE(list.capacity() < 5);
            REQUIRE(list[0] == 1);
            REQUIRE_RANGES_EQUAL(list, ok::array_t{1});
            REQUIRE(list.insert_at(0, 0).okay());
            REQUIRE(list.insert_at(2, 2).okay());
            REQUIRE(list.insert_at(3, 3).okay());
            REQUIRE(list.insert_at(4, 4).okay());
            constexpr auto expected = ok::array_t{0, 1, 2, 3, 4};
            REQUIRE_RANGES_EQUAL(list, expected);
            REQUIRE(list.capacity() >= 5);
        }

        SUBCASE("insert into copy_items_from_range segmented lists")
        {
            constexpr array_t initial = {0, 1, 2, 3};
            segmented_list_t list =
                segmented_list::copy_items_from_range(c_allocator, initial, {})
                    .release();

            REQUIRE_RANGES_EQUAL(initial, list);
            REQUIREABORTS(auto&& _ = list.insert_at(initial.size(), 0));
            REQUIRE(list.insert_at(0, 0).okay());
            REQUIRE(list.insert_at(5, 4).okay());
            REQUIRE_RANGES_EQUAL(list, ok::array_t{0, 0, 1, 2, 3, 4});
        }

        SUBCASE("append")
        {
            auto res = segmented_list::empty<int>(c_allocator, {});
            auto& list = res.release_ref();

            for (size_t i = 0; i < 50; ++i) {
                REQUIREABORTS(list[i]);
                REQUIRE(list.append(i).okay());
            }

            REQUIRE_RANGES_EQUAL(list, indices);
        }
    }

    TEST_CASE("get_blocks")
    {
        SUBCASE("const") {}
        SUBCASE("nonconst") {}
    }

    TEST_CASE("clear() calls destructors") {}

    TEST_CASE("being move-assigned over calls destructors") {}

    TEST_CASE("remove calls move constructor then destructor") {}

    TEST_CASE("resize() calls destructors") {}

    TEST_CASE("pop_last()") {}

    TEST_CASE("remove()") {}

    TEST_CASE("remove_and_swap_last()") {}

    TEST_CASE("last()") {}

    TEST_CASE("first()") {}

    TEST_CASE("increase_capacity_by_at_least()") {}

    TEST_CASE("append_range") {}
}
