#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/drop.h"
#include "testing_types.h"
#include <array>

using namespace ok;

int dummy[500];
static_assert(detail::random_access_range_c<decltype(dummy | drop(10))>);
static_assert(detail::range_is_arraylike_v<decltype(dummy)>);
// TODO: make this keep arraylike property
// static_assert(detail::range_is_arraylike_v<decltype(dummy | drop(10))>);

TEST_SUITE("drop")
{
    TEST_CASE("functionality")
    {
        SUBCASE("get second half of array of constant size")
        {
            std::array<int, 50> array;
            static_assert(detail::random_access_range_c<decltype(array)>);
            auto half_view = array | drop(25);
            static_assert(
                detail::random_access_range_c<decltype(half_view)>);
            REQUIRE(half_view.amount() == 25);
            REQUIRE(ok::size(half_view) == 25);
        }

        SUBCASE("can't drop more than sized range")
        {
            std::array<int, 10> array;
            auto big_view = array | drop(20);
            REQUIRE(ok::size(big_view) == 0);
        }

        SUBCASE("can't drop more than unknown sized range")
        {
            example_range_bidirectional items;
            auto big_view = items | drop(300);

            // this loop should not run, we dropped everything in the collection
            ok_foreach(auto&& _, big_view) { REQUIRE(false); }
        }

        SUBCASE("drop from infinite view")
        {
            ok_foreach(auto num, indices | drop(10))
            {
                REQUIRE(num == 10);
                break;
            }
        }

        SUBCASE("drop zero")
        {
            int arr[10];
            size_t counter = 0;
            REQUIRE(ok::size(arr | drop(0)) == ok::size(arr));
            ok_foreach(auto&& _, arr | drop(0))
            {
                ++counter;
            }
            REQUIRE(counter == 10);
        }

        // // NOTE: can't test this- drop doesnt currently work with random
        // // access ranges which are also finite
        // SUBCASE("drop container which uses before/after boundschecks")
        // {
        //     fifty_items_unknown_size_before_after_t items;

        //     size_t counter = 0;
        //     ok_foreach(auto&& _, items | drop(10))
        //     {
        //         ++counter;
        //     }
        //     REQUIRE(counter == 40);
        // }
    }
}
