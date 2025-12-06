#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"

TEST_SUITE("any_satisfy and any_true iterator operations")
{
    const auto is_even = [](size_t i) { return i % 2 == 0; };
    const auto is_odd = [](size_t i) { return i % 2 != 0; };
    TEST_CASE("works on arrays")
    {
        SUBCASE("c style array")
        {
            int test[] = {2, 4, 6, 8, 10};

            bool any_odd = ok::iter(test).any_satisfy(is_odd);

            REQUIRE(!any_odd);

            int test_not_even[] = {2, 4, 6, 8, 11};

            any_odd = ok::iter(test_not_even).any_satisfy(is_odd);

            REQUIRE(any_odd);
        }

        SUBCASE("ok::array_t")
        {
            ok::maybe_undefined_array_t test = {2, 4, 6, 8, 10};

            bool any_odd = ok::iter(test).any_satisfy(is_odd);

            REQUIRE(!any_odd);

            ok::maybe_undefined_array_t test_not_even = {2, 4, 6, 8, 11};

            any_odd = ok::iter(test_not_even).any_satisfy(is_odd);

            REQUIRE(any_odd);
        }
    }

    TEST_CASE("join optional")
    {
        ok::array_t<ok::opt<int>, 6> test = {
            ok::nullopt, 1, ok::nullopt, 2, ok::nullopt, 3,
        };

        bool any_greater_than_four =
            ok::iter(test).flatten().any_satisfy([](int i) { return i > 4; });

        REQUIRE(!any_greater_than_four);
    }

    TEST_CASE("any passes the same predicate as a filter")
    {
        auto range = ok::indices().take_at_most(10);

        // predicate different so we dont match
        bool matches = ok::iter(range).keep_if(is_even).any_satisfy(is_odd);
        REQUIRE(!matches);

        // predicate is the same so we match
        matches = ok::iter(range).keep_if(is_even).any_satisfy(is_even);
        REQUIRE(matches);

        matches = ok::iter(range).any_satisfy(is_even);

        REQUIRE(matches);
    }

    TEST_CASE("reverse has no effect")
    {
        auto range = ok::indices().take_at_most(10);

        bool matches =
            ok::iter(range).reverse().keep_if(is_even).any_satisfy(is_odd);

        REQUIRE(!matches);

        matches = ok::iter(range).any_satisfy([](size_t i) { return i >= 10; });

        REQUIRE(!matches);

        matches = ok::iter(range).reverse().any_satisfy(
            [](size_t i) { return i >= 10; });

        REQUIRE(!matches);
    }

    TEST_CASE("transform and then any")
    {
        auto range = ok::indices().take_at_most(1000);

        bool any_even = ok::iter(range).any_satisfy(is_even);

        REQUIRE(any_even);

        const auto times_two = [](size_t i) { return i * 2; };

        bool any_odd = ok::iter(range).transform(times_two).any_satisfy(is_odd);

        REQUIRE(!any_odd);

        const auto divisible_by_four = [](size_t i) { return i % 4 == 0; };

        any_odd = ok::iter(range)
                      .transform(times_two)
                      .keep_if(divisible_by_four)
                      .any_satisfy(is_odd);

        REQUIRE(!any_odd);

        any_even = ok::iter(range)
                       .transform(times_two)
                       .keep_if(divisible_by_four)
                       .any_satisfy(is_even);

        REQUIRE(any_even);
    }
}
