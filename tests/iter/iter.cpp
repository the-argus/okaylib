#include "test_header.h"
// test header must be first
#include "okay/ranges/iter.h"

using namespace ok;

TEST_SUITE("iter")
{
    TEST_CASE("forward iteration with standard for loop")
    {
        SUBCASE("forward only iterable")
        {
            myiterable_t iterable{};
            size_t num_items = 0;

            for (int i : iterable.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());

            // test enumerate too
            for (auto [i, idx] : iterable.iter().enumerate()) {
                REQUIRE(i == idx);
            }
        }

        SUBCASE("forward only iterable, rvalue")
        {
            size_t num_items = 0;

            for (int i : myiterable_t{}.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }

            // myiterable_t always has the same size
            REQUIRE(num_items == myiterable_t{}.size());
        }

        SUBCASE("random access iterable")
        {
            my_arraylike_iterable_t iterable{};
            size_t num_items = 0;
            for (int& i : iterable.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());
        }

        SUBCASE("random access iterable, const")
        {
            my_arraylike_iterable_t iterable{};
            size_t num_items = 0;
            for (const int& i : iterable.iter_const()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());
        }

        SUBCASE("random access iterable, rvalue")
        {
            size_t num_items = 0;
            for (int& i : my_arraylike_iterable_t{}.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == my_arraylike_iterable_t{}.size());
        }

        SUBCASE("random access iterable, rvalue const")
        {
            size_t num_items = 0;
            for (const int& i : my_arraylike_iterable_t{}.iter_const()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == my_arraylike_iterable_t{}.size());
        }
    }

    TEST_CASE("backward iteration with standard for loop")
    {
        SUBCASE("forward only iterable, but backwards")
        {
            myiterable_t iterable{};
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

            my_arraylike_iterable_t iterable{};
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
