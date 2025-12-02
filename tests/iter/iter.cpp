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
            myiterable_t iterable;
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
            my_arraylike_iterable_t iterable;
            size_t num_items = 0;
            for (int& i : iterable.iter()) {
                REQUIRE(i == num_items);
                ++num_items;
            }
            REQUIRE(num_items == iterable.size());
        }

        SUBCASE("random access iterable, const")
        {
            my_arraylike_iterable_t iterable;
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
            myiterable_t iterable;
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

            my_arraylike_iterable_t iterable;
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

    TEST_CASE("zip view")
    {
        SUBCASE("zip view with arraylike value types")
        {
            int ints[] = {0, 1, 2, 3, 4};

            for (auto [i, index] : ok::iter(ints).enumerate()) {
                REQUIRE(i == index);
            }

            for (auto [i, index] : enumerate(ints)) {
                REQUIRE(i == index);
            }

            REQUIRE(ok::iter(ints).zip(ok::indices()).size() == 5);
            REQUIRE(zip(ints, indices()).size() == 5);

            for (auto [i, index] : zip(ints, indices())) {
                REQUIRE(i == index);
            }

            for (auto [enumerated_normal, enumerated_zip] :
                 enumerate(ints).zip(zip(ints, ok::indices()))) {
                REQUIRE(enumerated_normal == enumerated_zip);
            }
        }

        SUBCASE("zip view with forward value types")
        {
            myiterable_t iterable;

            for (auto [i, index] : iterable.iter().zip(indices())) {
                REQUIRE(i == index);
            }

            for (auto [enumerated_normal, enumerated_zip] :
                 enumerate(iterable).zip(zip(iterable, indices()))) {
                REQUIRE(enumerated_normal == enumerated_zip);
            }
        }

        SUBCASE("zip view with an lvalue reference and a value type")
        {
            int ints[] = {0, 1, 2, 3, 4};

            for (auto& [int_item, index] : ok::iter(ints).zip(indices())) {
                static_assert(same_as_c<decltype(int_item), int&>);
                static_assert(same_as_c<decltype(index), size_t>);
                REQUIRE(int_item == index);
            }

            myiterable_t iterable;

            for (auto& [iterable_item, int_item, index] :
                 iterable.iter().zip(ok::iter(ints), indices())) {
                static_assert(same_as_c<decltype(iterable_item), int&>);
                static_assert(same_as_c<decltype(int_item), int&>);
                static_assert(same_as_c<decltype(index), size_t>);
                REQUIRE(iterable_item == int_item);
            }
        }

        SUBCASE("zip view with only lvalue references")
        {
            static_assert(iterable_c<int(&)[5]>);

            int ints[] = {0, 1, 2, 3, 4};
            myiterable_t iterable;

            for (auto& [iterable_item, int_item] : zip(iterable, ints)) {
                static_assert(same_as_c<decltype(iterable_item), int&>);
                static_assert(same_as_c<decltype(int_item), int&>);

                int_item = iterable_item;
            }

            // constexpr maybe_undefined_array_t expected = {4, 3, 2, 1, 0};
            // static_assert(iterable_c<decltype(ok::iter(expected))>);
            // auto test = ok::iterators_equal(ok::iter(ints),
            // ok::iter(expected)); REQUIRE(test);
        }
    }
}
