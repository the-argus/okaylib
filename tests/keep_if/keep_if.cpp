#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/macros/foreach.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/transform.h"
#include "okay/slice.h"
#include "okay/stdmem.h"
#include <array>

using namespace ok;

TEST_SUITE("keep_if")
{
    TEST_CASE("functionality")
    {
        SUBCASE("identity keep_if")
        {
            std::array<int, 50> ints = {};
            std::fill(ints.begin(), ints.end(), 0);

            for (auto c = ok::begin(ints); ok::is_inbounds(ints, c);
                 ok::increment(ints, c)) {
                int& item = ok::iter_get_ref(ints, c);
                item = c;
            }

            auto identity = ints | keep_if([](auto i) { return true; });
            for (auto c = ok::begin(identity); ok::is_inbounds(identity, c);
                 ok::increment(identity, c)) {
                int& item = ok::iter_get_ref(identity, c);
                REQUIRE(item == c);
            }
        }

        SUBCASE("identity keep_if with macros")
        {
            std::array<int, 50> ints = {};
            memfill(slice(ints), 0);

            size_t c = 0;
            ok_foreach(auto& item, ints)
            {
                item = c;
                ++c;
            }

            auto identity = ints | keep_if([](auto) { return true; });
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

            static_assert(!detail::range_impls_increment_v<decltype(ints)>);
            static_assert(
                detail::range_impls_increment_v<decltype(ints |
                                                         keep_if(is_even))>);
            static_assert(random_access_range_c<decltype(ints)>);
            static_assert(
                detail::is_bidirectional_range_v<decltype(ints |
                                                          keep_if(is_even))>);
            static_assert(
                !detail::range_can_offset_v<decltype(ints | keep_if(is_even))>);
            static_assert(
                !random_access_range_c<decltype(ints |
                                                           keep_if(is_even))>);
            static_assert(std::is_same_v<
                          cursor_type_for<decltype(ints)>,
                          cursor_type_for<decltype(ints | keep_if(is_even))>>);
            static_assert(std::is_same_v<
                          value_type_for<decltype(ints)>,
                          value_type_for<decltype(ints | keep_if(is_even))>>);

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            auto&& items = ints | keep_if(is_even);
            auto begin = ok::begin(items);
            REQUIRE(ok::iter_get_temporary_ref(items, begin) == 0);
            for (auto i = ok::begin(items); ok::is_inbounds(items, i);
                 ok::increment(items, i)) {
                REQUIRE(ok::iter_get_temporary_ref(items, i) % 2 == 0);
            }

            // or, with macros
            ok_foreach(const int i, ints | keep_if(is_even))
            {
                REQUIRE(i % 2 == 0);
            }
        }

        SUBCASE("ok::begin() skips until first item that should be kept")
        {
            auto is_odd = [](auto i) { return i % 2 == 1; };
            int myints[100];

            ok_foreach(ok_pair(i, index), myints | enumerate) { i = index; }

            auto filtered = myints | keep_if(is_odd);
            // starts at 1, skipping zero because it is not odd
            REQUIRE(iter_get_temporary_ref(filtered, ok::begin(filtered)) == 1);
        }

        SUBCASE("keep_if by index and then go back to not having index type")
        {
            auto skip_even = keep_if([](auto i) { return i.second % 2 == 1; });
            auto get_first = transform([](auto pair) { return pair.first; });

            std::array<int, 50> ints;
            memfill(slice(ints), 0);

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

        SUBCASE("keep_if of const ref to array is a range")
        {
            const auto keep_if_less_than_100 =
                ok::keep_if([](int i) -> bool { return i < 100; });

            constexpr ok::array_t nums = {0, 100, 1, 100, 2, 100, 3, 100};

            int counter = 0;
            ok_foreach(int i, nums | keep_if_less_than_100)
            {
                REQUIRE(i == counter);
                ++counter;
            }
        }

        SUBCASE("keep_if with no matches never runs in loop")
        {
            auto keep_none = keep_if([](auto) { return false; });

            std::array<int, 50> array;
            memfill(slice(array), 0);

            ok_foreach(const int i, array | keep_none)
            {
                // should be unreachable
                REQUIRE(false);
            }
        }

        SUBCASE("filter with over empty array never runs")
        {
            auto keep_all = keep_if([](auto) { return true; });

            std::array<int, 0> array;

            ok_foreach(const int i, array | keep_all)
            {
                // should be unreachable
                REQUIRE(false);
            }
        }
    }
}
