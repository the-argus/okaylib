#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/indices.h"
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
            static_assert(detail::is_random_access_range_v<decltype(array)>);
            auto half_view = array | take_at_most(25);
            static_assert(
                detail::is_random_access_range_v<decltype(half_view)>);
            REQUIRE(half_view.size() == 25);
        }

        SUBCASE("get first half of forward no increment")
        {
            fifty_items_unknown_size_no_pre_increment_t unknown_size;
            static_assert(
                detail::is_multi_pass_range_v<decltype(unknown_size)>);
            auto half_view = unknown_size | take_at_most(25);
            static_assert(detail::is_multi_pass_range_v<decltype(half_view)>);

            size_t counter = 0;
            ok_foreach(auto&& i, half_view) { ++counter; }
            REQUIRE(counter == 25);
        }

        SUBCASE("get first half of bidirectional no increment/decrement")
        {
            fifty_items_bidir_no_pre_decrement_t bidir_nooperators;
            static_assert(
                detail::is_bidirectional_range_v<decltype(bidir_nooperators)>);
            auto half_view = bidir_nooperators | take_at_most(25);
            static_assert(
                detail::is_bidirectional_range_v<decltype(half_view)>);

            size_t counter = 0;
            ok_foreach(auto&& i, half_view) { ++counter; }
            REQUIRE(counter == 25);
        }

        SUBCASE("get first half of bidirectional")
        {
            example_range_bidirectional bidir;
            static_assert(detail::is_bidirectional_range_v<decltype(bidir)>);
            auto half_view = bidir | take_at_most(25);
            range_def_for<decltype(half_view)> test;
            static_assert(is_range_v<decltype(half_view)>);
            static_assert(
                detail::is_bidirectional_range_v<decltype(half_view)>);
            REQUIRE(half_view.size() == 25);
        }

        SUBCASE("get first half of vector of runtime known size")
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

        SUBCASE("take subset of indices")
        {
            size_t counter = 0;
            ok_foreach(auto i, indices | take_at_most(10))
            {
                REQUIRE(i == counter);
                ++counter;
            }
        }
    }
}
