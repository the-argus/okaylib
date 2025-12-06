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
            int ints[] = {0, 1, 2, 3, 4, 5};
            myiterable_t iterable;

            for (auto& [iterable_item, int_item] :
                 zip(iterable, reverse(ints))) {
                static_assert(same_as_c<decltype(iterable_item), int&>);
                static_assert(same_as_c<decltype(int_item), int&>);

                REQUIRE(int_item != iterable_item);
                int_item = iterable_item; // swap the two ranges
            }

            REQUIRE(ok::iterators_equal(
                ints, maybe_undefined_array_t{5, 4, 3, 2, 1, 0}));
        }
    }

    TEST_CASE("keep_if view")
    {
        constexpr auto is_even = [](const auto& i) { return i % 2 == 0; };

        SUBCASE("filter odd numbers out")
        {
            int myints[] = {0, 1, 2, 3, 4, 5};
            maybe_undefined_array_t myints_array = {0, 1, 2, 3, 4, 5};
            maybe_undefined_array_t expected = {0, 2, 4};

            REQUIRE(ok::iterators_equal(keep_if(myints, is_even),
                                        keep_if(myints_array, is_even)));
            REQUIRE(ok::iterators_equal(keep_if(myints, is_even), expected));
            REQUIRE(
                ok::iterators_equal(keep_if(myints_array, is_even), expected));
        }

        SUBCASE("filter odd indices out of non-integer iterable")
        {
            const char* strings[] = {
                "keep", "removeodd", "keep", "removeodd, again", "keep",
            };

            size_t runs = 0;
            for (const char*& item :
                 enumerate(strings)
                     .keep_if([&](const auto& pair) {
                         const auto& [str, index] = pair;
                         return is_even(index);
                     })
                     .transform([](auto&& a) { return ok::get<0>(a); })) {
                ++runs;
                REQUIRE(ascii_view::from_cstring(item) == ascii_view("keep"));
            }
            REQUIRE(runs == 3);
        }
    }

    TEST_CASE("flatten view")
    {
        SUBCASE("2d array")
        {
            maybe_undefined_array_t outer{
                maybe_undefined_array_t{0, 1},
                maybe_undefined_array_t{2, 3},
            };
            maybe_undefined_array_t expected{0, 1, 2, 3};

            REQUIRE(iterators_equal(iter(outer).flatten(), expected));
        }

        SUBCASE("2d array const")
        {
            const maybe_undefined_array_t outer{
                maybe_undefined_array_t{0, 1},
                maybe_undefined_array_t{2, 3},
            };
            const maybe_undefined_array_t expected{0, 1, 2, 3};
            REQUIRE(iterators_equal(iter(outer).flatten(), expected));
        }

        SUBCASE("3d array")
        {
            constexpr maybe_undefined_array_t outer{
                maybe_undefined_array_t{
                    maybe_undefined_array_t{0, 1},
                    maybe_undefined_array_t{2, 3},
                    maybe_undefined_array_t{4, 5},
                },
                maybe_undefined_array_t{
                    maybe_undefined_array_t{6, 7},
                    maybe_undefined_array_t{8, 9},
                    maybe_undefined_array_t{10, 11},
                },
            };

            constexpr maybe_undefined_array_t expected_flatten_once{
                maybe_undefined_array_t{0, 1}, maybe_undefined_array_t{2, 3},
                maybe_undefined_array_t{4, 5}, maybe_undefined_array_t{6, 7},
                maybe_undefined_array_t{8, 9}, maybe_undefined_array_t{10, 11},
            };

            constexpr maybe_undefined_array_t expected_flatten_twice{
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

            REQUIRE(
                iterators_equal(iter(outer).flatten(), expected_flatten_once));
            REQUIRE(iterators_equal(iter(outer).flatten().flatten(),
                                    expected_flatten_twice));
        }
    }

    TEST_CASE("all_satisfy algorithm")
    {
        constexpr auto is_even = [](int i) { return i % 2 == 0; };

        SUBCASE("c style array")
        {
            int test[] = {2, 4, 6, 8, 10};

            bool all_even =
                iter(test).all_satisfy([](int i) { return i % 2 == 0; });

            REQUIRE(all_even);

            int test_not_even[] = {2, 4, 6, 8, 11};

            all_even = iter(test_not_even).all_satisfy([](int i) {
                return i % 2 == 0;
            });

            REQUIRE(!all_even);
        }

        SUBCASE("ok::array_t")
        {
            ok::maybe_undefined_array_t test = {2, 4, 6, 8, 10};

            bool all_even = iter(test).all_satisfy(is_even);

            REQUIRE(all_even);

            ok::maybe_undefined_array_t test_not_even = {2, 4, 6, 8, 11};

            all_even = iter(test_not_even).all_satisfy([](int i) {
                return i % 2 == 0;
            });

            REQUIRE(!all_even);
        }

        SUBCASE("flatten optional")
        {
            ok::array_t<ok::opt<int>, 6> test = {
                ok::nullopt, 1, ok::nullopt, 2, ok::nullopt, 3,
            };

            bool all_less_than_four =
                iter(test).flatten().all_satisfy([](int i) { return i < 4; });

            REQUIRE(all_less_than_four);
        }

        SUBCASE("all passes the same predicate as a filter")
        {
            auto range = indices().take_at_most(10);

            bool matches =
                stdc::move(range).keep_if(is_even).all_satisfy(is_even);

            REQUIRE(matches);

            matches = indices().take_at_most(10).all_satisfy(is_even);

            REQUIRE(!matches);
        }

        SUBCASE("reverse has no effect")
        {
            auto range = ok::indices().take_at_most(10);

            bool matches =
                iter(range).reverse().keep_if(is_even).all_satisfy(is_even);

            REQUIRE(matches);

            matches = iter(range).all_satisfy([](size_t i) { return i < 10; });

            REQUIRE(matches);

            matches = iter(range).reverse().all_satisfy(
                [](size_t i) { return i < 10; });

            REQUIRE(matches);
        }

        SUBCASE("transform and then all")
        {
            auto range = ok::indices().take_at_most(1000);

            bool all_even = iter(range).all_satisfy(is_even);

            REQUIRE(!all_even);

            const auto times_two = [](size_t i) { return i * 2; };

            all_even = iter(range).transform(times_two).all_satisfy(is_even);

            REQUIRE(all_even);

            const auto divisible_by_four = [](size_t i) { return i % 4 == 0; };

            all_even = iter(range)
                           .transform(times_two)
                           .keep_if(divisible_by_four)
                           .all_satisfy(is_even);

            REQUIRE(all_even);
        }
    }
}
