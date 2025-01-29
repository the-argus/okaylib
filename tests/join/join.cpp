#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/join.h"
#include <array>

using namespace ok;

TEST_SUITE("join")
{
    TEST_CASE("functionality")
    {
        SUBCASE("join three c style arrays")
        {
            int a[3] = {1, 2, 3};
            int b[3] = {1, 2, 3};
            int c[3] = {1, 2, 3};



            static_assert(detail::is_random_access_range_v<decltype(a)>);
            static_assert(!detail::is_random_access_range_v<decltype(a | join)>);

            size_t counter =
        }
    }
}
