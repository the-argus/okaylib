#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/take_at_most.h"
#include "testing_types.h"
#include <array>
#include <vector>

using namespace ok;

TEST_SUITE("take_at_most")
{
    TEST_CASE("functionality")
    {
        SUBCASE("get first half of array of constant size")
        {
            std::array<int, 50> array;
            auto half_view = array | take_at_most(25);
            REQUIRE(half_view.size() == 25);
        }
        SUBCASE("get first half of vector of unknown size")
        {
            std::vector<int> vec;
            vec.resize(50);
            auto half_view = vec | take_at_most(25);
            REQUIRE(half_view.size() == 25);
        }
        SUBCASE("get first half of container of unknown size")
        {
            fifty_items_unknown_size_t items;
            size_t count = 0;
            ok_foreach(auto&& _, items) { ++count; }
            REQUIRE(count == 50);

            auto half_view = items | take_at_most(25);

            count = 0;
            ok_foreach(size_t i, half_view) { ++count; }
            REQUIRE(count == 25);
        }
        SUBCASE("can't take more than container")
        {
            std::array<int, 50> array;
            auto big_view = array | take_at_most(100);
            REQUIRE(big_view.size() == 50);
        }
        SUBCASE("can't take more than container of unknown size")
        {
            fifty_items_unknown_size_t items;
            auto big_view = items | take_at_most(100);
            size_t count = 0;
            ok_foreach(auto&& blank, big_view) { ++count; }
            REQUIRE(count == 50);
        }
        SUBCASE("can't take more than container of unknown size w/ before "
                "after boundscheck")
        {
            fifty_items_unknown_size_before_after_t items;
            auto big_view = items | take_at_most(100);
            size_t count = 0;
            for (auto cursor = ok::begin(big_view);
                 ok::is_inbounds(big_view, cursor);
                 ok::increment(big_view, cursor)) {
                ++count;
            }
            REQUIRE(count == 50);
            count = 0;
            for (auto cursor = ok::begin(big_view); ok::is_inbounds(
                     big_view, cursor, ok::prefer_after_bounds_check_t{});
                 ok::increment(big_view, cursor)) {
                ++count;
            }
            REQUIRE(count == 50);
        }
        SUBCASE("get first half of container of unknown size w/ before after "
                "boundscheck")
        {
            fifty_items_unknown_size_before_after_t items;
            size_t count = 0;
            ok_foreach(auto&& _, items) { ++count; }
            REQUIRE(count == 50);

            auto half_view = items | take_at_most(25);

            count = 0;
            ok_foreach(size_t i, half_view) { ++count; }
            REQUIRE(count == 25);
        }
    }
}
