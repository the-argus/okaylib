#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/any.h"
#include "okay/ranges/views/join.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/reverse.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/transform.h"

TEST_SUITE("ok::any range adaptor")
{
    const auto is_even = [](size_t i) { return i % 2 == 0; };
    const auto is_odd = [](size_t i) { return i % 2 != 0; };
    TEST_CASE("works on arrays")
    {
        SUBCASE("c style array")
        {
            int test[] = {2, 4, 6, 8, 10};

            bool any_odd = test | ok::any(is_odd);

            REQUIRE(!any_odd);

            int test_not_even[] = {2, 4, 6, 8, 11};

            any_odd = test_not_even | ok::any(is_odd);

            REQUIRE(any_odd);
        }

        SUBCASE("ok::array_t")
        {
            ok::array_t test = {2, 4, 6, 8, 10};

            bool any_odd = test | ok::any(is_odd);

            REQUIRE(!any_odd);

            ok::array_t test_not_even = {2, 4, 6, 8, 11};

            any_odd = test_not_even | ok::any(is_odd);

            REQUIRE(any_odd);
        }
    }

    TEST_CASE("join optional")
    {
        ok::array_t<ok::opt<int>, 6> test = {
            ok::nullopt, 1, ok::nullopt, 2, ok::nullopt, 3,
        };

        bool any_greater_than_four =
            test | ok::join | ok::any([](int i) { return i > 4; });

        REQUIRE(!any_greater_than_four);
    }

    TEST_CASE("any passes the same predicate as a filter")
    {
        auto range = ok::indices | ok::take_at_most(10);

        // predicate different so we dont match
        bool matches = range | ok::keep_if(is_even) | ok::any(is_odd);
        REQUIRE(!matches);

        // predicate is the same so we match
        matches = range | ok::keep_if(is_even) | ok::any(is_even);
        REQUIRE(matches);

        matches = range | ok::any(is_even);

        REQUIRE(matches);
    }

    TEST_CASE("reverse has no effect")
    {
        auto range = ok::indices | ok::take_at_most(10);

        bool matches =
            range | ok::reverse | ok::keep_if(is_even) | ok::any(is_odd);

        REQUIRE(!matches);

        matches = range | ok::any([](size_t i) { return i >= 10; });

        REQUIRE(!matches);

        matches =
            range | ok::reverse | ok::any([](size_t i) { return i >= 10; });

        REQUIRE(!matches);
    }

    TEST_CASE("transform and then any")
    {
        auto range = ok::indices | ok::take_at_most(1000);

        bool any_even = range | ok::any(is_even);

        REQUIRE(any_even);

        const auto times_two = [](size_t i) { return i * 2; };

        bool any_odd = range | ok::transform(times_two) | ok::any(is_odd);

        REQUIRE(!any_odd);

        const auto divisible_by_four = [](size_t i) { return i % 4 == 0; };

        any_odd = range | ok::transform(times_two) |
                  ok::keep_if(divisible_by_four) | ok::any(is_odd);

        REQUIRE(!any_odd);

        any_even = range | ok::transform(times_two) |
                   ok::keep_if(divisible_by_four) | ok::any(is_even);

        REQUIRE(any_even);
    }
}
