#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/all.h"
#include "okay/ranges/views/join.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/reverse.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/transform.h"

TEST_SUITE("ok::all range adaptor")
{
    const auto is_even = [](size_t i) { return i % 2 == 0; };
    TEST_CASE("works on arrays")
    {
        SUBCASE("c style array")
        {
            int test[] = {2, 4, 6, 8, 10};

            bool all_even = test | ok::all([](int i) { return i % 2 == 0; });

            REQUIRE(all_even);

            int test_not_even[] = {2, 4, 6, 8, 11};

            all_even =
                test_not_even | ok::all([](int i) { return i % 2 == 0; });

            REQUIRE(!all_even);
        }

        SUBCASE("ok::array_t")
        {
            ok::array_t test = {2, 4, 6, 8, 10};

            bool all_even = test | ok::all([](int i) { return i % 2 == 0; });

            REQUIRE(all_even);

            ok::array_t test_not_even = {2, 4, 6, 8, 11};

            all_even =
                test_not_even | ok::all([](int i) { return i % 2 == 0; });

            REQUIRE(!all_even);
        }
    }

    TEST_CASE("join optional")
    {
        ok::array_t<ok::opt<int>, 6> test = {
            ok::nullopt, 1, ok::nullopt, 2, ok::nullopt, 3,
        };

        bool all_less_than_four =
            test | ok::join | ok::all([](int i) { return i < 4; });

        REQUIRE(all_less_than_four);
    }

    TEST_CASE("all passes the same predicate as a filter")
    {
        auto range = ok::indices | ok::take_at_most(10);

        bool matches = range | ok::keep_if(is_even) | ok::all(is_even);

        REQUIRE(matches);

        matches = range | ok::all(is_even);

        REQUIRE(!matches);
    }

    TEST_CASE("reverse has no effect")
    {
        auto range = ok::indices | ok::take_at_most(10);

        bool matches =
            range | ok::reverse | ok::keep_if(is_even) | ok::all(is_even);

        REQUIRE(matches);

        matches = range | ok::all([](size_t i) { return i < 10; });

        REQUIRE(matches);

        matches =
            range | ok::reverse | ok::all([](size_t i) { return i < 10; });

        REQUIRE(matches);
    }

    TEST_CASE("transform and then all")
    {
        auto range = ok::indices | ok::take_at_most(1000);

        bool all_even = range | ok::all(is_even);

        REQUIRE(!all_even);

        const auto times_two = [](size_t i) { return i * 2; };

        all_even = range | ok::transform(times_two) | ok::all(is_even);

        REQUIRE(all_even);

        const auto divisible_by_four = [](size_t i) { return i % 4 == 0; };

        all_even = range | ok::transform(times_two) |
                   ok::keep_if(divisible_by_four) | ok::all(is_even);

        REQUIRE(all_even);
    }
}
