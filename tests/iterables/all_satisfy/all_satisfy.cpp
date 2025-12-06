#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"

using namespace ok;

TEST_SUITE("all_satisfy and all_true iterator operations")
{
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
