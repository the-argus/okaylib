#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/reverse.h"
#include <array>

using namespace ok;

TEST_SUITE("reverse")
{
    TEST_CASE("functionality")
    {
        SUBCASE("reverse c style array")
        {
            int forward[3] = {1, 2, 3};

            static_assert(detail::is_random_access_range_v<decltype(forward)>);
            static_assert(
                detail::is_random_access_range_v<decltype(forward | reverse)>);

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
            int forward[6] = {5, 4, 3, 2, 1, 0};

            int prev = 0;
            ok_foreach(ok_pair(value, idx), forward | reverse | enumerate)
            {
                REQUIRE(value == idx);
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
    }
}
