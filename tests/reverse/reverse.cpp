#include "okay/ranges/views/transform.h"
#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/reverse.h"
#include "okay/ranges/views/std_for.h"
#include "okay/ranges/views/take_at_most.h"
#include <array>

using namespace ok;

TEST_SUITE("reverse")
{
    TEST_CASE("functionality")
    {
        SUBCASE("reverse c style array")
        {
            int forward[3] = {1, 2, 3};

            static_assert(random_access_range_c<decltype(forward)>);
            static_assert(
                detail::well_declared_range_c<decltype(forward | reverse)>);
            static_assert(random_access_range_c<decltype(forward | reverse)>);
            static_assert(detail::range_marked_arraylike_c<decltype(forward)>);
            static_assert(
                detail::range_marked_arraylike_c<decltype(forward | reverse)>);

            auto reversed = forward | reverse;
            REQUIRE(ok::size(reversed) == ok::size(forward));

            int prev = 4;
            ok_foreach(int i, reversed)
            {
                REQUIRE(i < prev);
                REQUIRE(prev - 1 == i);
                prev = i;
            }
        }

        SUBCASE("reversed c style array, checked with enumeration")
        {
            int forward[] = {5, 4, 3, 2, 1, 0};

            auto size_minus =
                transform([&](int i) { return ok::size(forward) - 1 - i; });

            // fmt::println("Range using size_minus:");
            for (auto [value, idx] :
                 forward | size_minus | enumerate | std_for) {
                assert(value == idx);
                const char* sep = idx == ok::size(forward) - 1 ? "\n" : " -> ";
                // fmt::print("{}: {}{}", value, idx, sep);
            }

            // fmt::println("Range using reverse:");
            for (auto [value, idx] : forward | reverse | enumerate | std_for) {
                assert(value == idx);
                const char* sep = idx == ok::size(forward) - 1 ? "\n" : " -> ";
                // fmt::print("{}: {}{}", value, idx, sep);
            }
        }

        SUBCASE("reverse 1-sized array")
        {
            std::array<int, 1> forward = {42};

            REQUIRE(ok::size(forward) == ok::size(forward | reverse));

            size_t counter = 0;
            ok_foreach(int item, forward | reverse)
            {
                REQUIRE(item == 42);
                REQUIRE(counter == 0);
                ++counter;
            }
        }

        SUBCASE("reverse 0-sized array")
        {
            std::array<int, 0> null = {};

            REQUIRE(ok::size(null) == ok::size(null | reverse));

            size_t counter = 0;
            ok_foreach(int item, null | reverse) { ++counter; }
            REQUIRE(counter == 0);
        }

        SUBCASE("take and reverse indices to count backwards")
        {
            auto count_backwards_from_ten =
                indices | take_at_most(10) | reverse;

            static_assert(
                random_access_range_c<decltype(count_backwards_from_ten)>);

            ok_foreach(ok_pair(item, index),
                       count_backwards_from_ten | enumerate)
            {
                REQUIRE(9 - item == index);
            }
        }
    }
}
