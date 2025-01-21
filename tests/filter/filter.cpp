#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/filter.h"
#include "okay/ranges/views/transform.h"
#include "okay/slice.h"
#include "okay/stdmem.h"
#include <array>

using namespace ok;

TEST_SUITE("enumerate")
{
    TEST_CASE("functionality")
    {
        SUBCASE("identity filter")
        {
            std::array<int, 50> ints = {};
            std::fill(ints.begin(), ints.end(), 0);

            for (auto c = ok::begin(ints); ok::is_inbounds(ints, c);
                 ok::increment(ints, c)) {
                int& item = ok::iter_get_ref(ints, c);
                item = c;
            }

            auto identity = ints | filter([](auto i) { return true; });
            for (auto c = ok::begin(identity); ok::is_inbounds(identity, c);
                 ok::increment(identity, c)) {
                int& item = ok::iter_get_ref(identity, c);
                REQUIRE(item == c);
            }
        }

        SUBCASE("identity filter with macros")
        {
            std::array<int, 50> ints = {};
            memfill(slice_t(ints), 0);

            size_t c = 0;
            ok_foreach(auto& item, ints)
            {
                item = c;
                ++c;
            }

            auto identity = ints | filter([](auto) { return true; });
            c = 0;
            ok_foreach(const auto& item, identity)
            {
                REQUIRE(item == c);
                ++c;
            }
        }

        SUBCASE("skip even numbers with std::array")
        {
            auto is_even = [](auto i) { return i % 2 == 0; };

            std::array<int, 50> ints;

            static_assert(
                !detail::range_definition_has_increment_v<decltype(ints)>);
            static_assert(detail::range_definition_has_increment_v<
                          decltype(ints | filter(is_even))>);
            static_assert(detail::is_random_access_range_v<decltype(ints)>);
            static_assert(
                !detail::is_random_access_range_v<decltype(ints |
                                                           filter(is_even))>);
            static_assert(std::is_same_v<
                          cursor_type_for<decltype(ints)>,
                          cursor_type_for<decltype(ints | filter(is_even))>>);
            static_assert(std::is_same_v<
                          value_type_for<decltype(ints)>,
                          value_type_for<decltype(ints | filter(is_even))>>);

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            auto&& items = ints | filter(is_even);
            auto begin = ok::begin(items);
            REQUIRE(ok::iter_get_temporary_ref(items, begin) == 0);
            for (auto i = ok::begin(items); ok::is_inbounds(items, i);
                 ok::increment(items, i)) {
                REQUIRE(ok::iter_get_temporary_ref(items, i) % 2 == 0);
            }

            // or, with macros
            ok_foreach(const int i, ints | filter(is_even))
            {
                REQUIRE(i % 2 == 0);
            }
        }

        SUBCASE("ok::begin() filters")
        {
            auto is_odd = [](auto i) { return i % 2 == 1; };
            int myints[100];

            ok_foreach(ok_pair(i, index), myints | enumerate) { i = index; }

            auto filtered = myints | filter(is_odd);
            // starts at 1, skipping zero because it is not odd
            REQUIRE(iter_get_temporary_ref(filtered, ok::begin(filtered)) == 1);
        }

        SUBCASE("filter by index and then go back to not having index type")
        {
            auto skip_even = filter([](auto i) { return i.second % 2 == 1; });
            auto get_first = transform([](auto pair) { return pair.first; });

            std::array<int, 50> ints;
            memfill(slice_t(ints), 0);

            ok_foreach(ok_pair(i, index), enumerate(ints))
            {
                // start at 50 and count backwards
                i = ints.size() - index;
            }

            ok_foreach(const int i, ints | enumerate | skip_even | get_first)
            {
                REQUIRE(i % 2 == 1);
            }
        }

        SUBCASE("filter with no matches never runs in loop")
        {
            auto null_filter = filter([](auto) { return false; });

            std::array<int, 50> array;
            memfill(slice_t(array), 0);

            ok_foreach(const int i, array | null_filter)
            {
                // should be unreachable
                REQUIRE(false);
            }
        }

        SUBCASE("filter with over empty array never runs")
        {
            auto identity_filter = filter([](auto) { return true; });

            std::array<int, 0> array;

            ok_foreach(const int i, array | identity_filter)
            {
                // should be unreachable
                REQUIRE(false);
            }
        }
    }
}
