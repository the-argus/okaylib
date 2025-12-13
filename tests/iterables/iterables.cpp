#include "okay/ascii_view.h"
#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include "testing_types.h"

using namespace ok;

TEST_SUITE("iter")
{
    TEST_CASE("forward iteration with standard for loop")
    {
        SUBCASE("forward only iterable")
        {
            forward_iterable_size_test_t<size_mode::known_sized> iterable;
            size_t num_items = 0;

            for (int i : iterable.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == ok::size(iterable));

            // test enumerate too
            for (auto [i, idx] : iterable.iter().enumerate()) {
                REQUIRE(i == idx);
            }
        }

        SUBCASE("forward only iterable, rvalue")
        {
            using iterable_t =
                forward_iterable_size_test_t<size_mode::known_sized>;
            size_t num_items = 0;

            for (int i : iterable_t{}.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }

            REQUIRE(num_items == ok::size(iterable_t{}));
        }

        SUBCASE("random access iterable")
        {
            using iterable_t = arraylike_iterable_reftype_test_t;
            iterable_t iterable;
            size_t num_items = 0;
            for (int& i : iterable.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());
        }

        SUBCASE("random access iterable, const")
        {
            using iterable_t = arraylike_iterable_reftype_test_t;
            iterable_t iterable;
            size_t num_items = 0;
            for (const int& i : iterable.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());
        }

        SUBCASE("random access iterable, rvalue")
        {
            using iterable_t = arraylike_iterable_reftype_test_t;
            size_t num_items = 0;
            for (int& i : iterable_t{}.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable_t{}.size());
        }

        SUBCASE("random access iterable, rvalue const")
        {
            using iterable_t = arraylike_iterable_reftype_test_t;
            size_t num_items = 0;
            for (const int& i : iterable_t{}.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable_t{}.size());
        }
    }

    TEST_CASE("backward iteration with standard for loop")
    {
        SUBCASE("forward only iterable, but backwards")
        {
            using iterable_t = forward_iterable_reftype_test_t;
            iterable_t iterable;
            size_t num_items = 0;

            for (int i : iterable.reverse_iter()) {
                REQUIRE(i == (iterable.size() - num_items - 1));
                ++num_items;
            }

            REQUIRE(num_items == iterable.size());
        }

        SUBCASE("random access iterable")
        {
            using T = std::tuple<int&, size_t>;
            static_assert(std::is_move_assignable_v<T>);

            arraylike_iterable_reftype_test_t iterable;
            size_t num_items = 0;
            for (auto& [i, idx] : iterable.iter().reverse().enumerate()) {
                REQUIRE(i == (iterable.size() - num_items - 1));
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());
            for (auto [i, idx] : iterable.iter().enumerate()) {
                REQUIRE(i == idx);
            }
        }
    }
}
