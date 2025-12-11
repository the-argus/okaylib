#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/iterables/algorithm.h"
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
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

            for (auto& [integer, index] : enumerate(ints))
                integer = index;

            constexpr auto identity = [](auto i) { return true; };

            REQUIRE(iterators_equal(keep_if(ints, identity), ints));
        }

        SUBCASE("skip even numbers with std::array")
        {
            auto is_even = [](auto i) { return i % 2 == 0; };

            std::array<int, 50> ints;

            for (auto& [integer, index] : enumerate(ints))
                integer = index;

            // starts at zero
            REQUIRE(keep_if(ints, is_even).next().deep_compare_with(0));

            auto&& items = keep_if(ints, is_even);

            REQUIRE(std::move(items).all_satisfy(is_even));
        }

        SUBCASE("ok::begin() skips until first item that should be kept")
        {
            auto is_odd = [](auto i) { return i % 2 == 1; };
            int myints[100];

            iterators_copy_assign(iter(myints), indices());
            // for (auto& [integer, index] : enumerate(myints))
            //     integer = index;

            auto filtered = keep_if(myints, is_odd);
            REQUIRE(std::move(filtered).next().deep_compare_with(1));
        }

        SUBCASE("keep_if by index and then go back to not having index type")
        {
            auto skip_odd_indices = [](auto i) -> bool {
                auto out = ok::get<1>(i) % 2 == 1;
                return out;
            };
            auto remove_index = [](ok::tuple<int&, const size_t> pair) -> int& {
                return ok::get<0>(pair);
            };

            std::array<int, 50> ints{};

            for (auto& [integer, index] : enumerate(ints))
                // start at 50 and count backwards
                integer = ints.size() - index;

            using T = decltype(enumerate(ints).keep_if(skip_odd_indices));
            static_assert(std::is_same_v<ok::tuple<int&, const size_t>,
                                         value_type_for<T>>);
            static_assert(
                std::is_same_v<
                    ok::tuple<int&, const size_t>,
                    stdc::remove_cvref_t<
                        decltype(stdc::declval<T>().next().ref_unchecked())>>);

            for (const int i : enumerate(ints)
                                   .keep_if(skip_odd_indices)
                                   .transform(remove_index))
                REQUIRE(i % 2 == 1);
        }

        SUBCASE("keep_if of const ref to array is a range")
        {
            const auto less_than_100 = [](int i) -> bool { return i < 100; };

            constexpr ok::maybe_undefined_array_t nums = {
                0, 100, 1, 100, 2, 100, 3, 100,
            };

            int counter = 0;
            for (int i : keep_if(nums, less_than_100)) {
                REQUIRE(i == counter);
                ++counter;
            }
        }

        SUBCASE("keep_if with no matches never runs in loop")
        {
            auto none = [](auto) { return false; };

            std::array<int, 50> array;
            memfill(slice(array), 0);

            for (const int i : keep_if(array, none))
                // should be unreachable
                REQUIRE(false);
        }

        SUBCASE("filter with over empty array never runs")
        {
            auto all = [](auto) { return true; };

            std::array<int, 0> array;

            for (const int i : keep_if(array, all))
                // should be unreachable
                REQUIRE(false);
        }
    }
}
