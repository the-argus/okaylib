#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include "testing_types.h"
#include <array>

using namespace ok;

TEST_SUITE("drop view")
{
    TEST_CASE("get second half of array of constant size")
    {
        std::array<int, 50> array;
        auto half_view = iter(array).drop(25);
        REQUIRE(half_view.size() == 25);
        REQUIRE(ok::size(half_view) == 25);
    }

    TEST_CASE("can't drop more than sized range")
    {
        std::array<int, 10> array;
        auto big_view = drop(array, 20);
        REQUIRE(ok::size(big_view) == 0);
    }

    TEST_CASE("can't drop more than unknown sized range")
    {
        example_iterable_forward_and_array items;
        auto big_view = iter(items).drop(300);

        // this loop should not run, we dropped everything in the collection
        for (auto&& _ : big_view) {
            REQUIRE(false);
        }
    }

    TEST_CASE("drop from infinite view")
    {
        for (auto num : indices().drop(10)) {
            REQUIRE(num == 10);
            break;
        }
    }

    TEST_CASE("drop zero")
    {
        int arr[10];
        size_t counter = 0;
        REQUIRE(ok::size(drop(arr, 0)) == ok::size(arr));
        for (auto&& _ : drop(arr, 0)) {
            ++counter;
        }
        REQUIRE(counter == 10);
    }

    // // NOTE: can't test this- drop doesnt currently work with random
    // // access ranges which are also finite
    // TEST_CASE("drop container which uses before/after boundschecks")
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
