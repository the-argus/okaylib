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

            auto t = [](auto i) { return true; };
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
            auto skip_even = filter([](auto i) { return i % 2 != 1; });

            std::array<int, 50> ints;

            static_assert(
                !detail::range_definition_has_increment_v<decltype(ints)>);
            static_assert(
                detail::range_definition_has_increment_v<decltype(ints |
                                                                  skip_even)>);
            static_assert(detail::is_random_access_range_v<decltype(ints)>);
            static_assert(
                !detail::is_random_access_range_v<decltype(ints | skip_even)>);
            static_assert(
                std::is_same_v<cursor_type_for<decltype(ints)>,
                               cursor_type_for<decltype(ints | skip_even)>>);
            static_assert(
                std::is_same_v<value_type_for<decltype(ints)>,
                               value_type_for<decltype(ints | skip_even)>>);

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            ok_foreach(const int i, ints | skip_even) { REQUIRE(i % 2 == 1); }
        }

        SUBCASE("filter by index and then go back to not having index type")
        {
            auto skip_even = filter([](auto i) { return i.second % 2 != 1; });
            auto get_first = transform([](auto pair) { return pair.first; });

            std::array<int, 50> ints;
            memfill(slice_t(ints), 0);

            ok_foreach(ok_pair(i, index), enumerate(ints))
            {
                // start at 50 and count backwards
                i = ints.size() - index;
            }

            ok_foreach(const auto i, ints | enumerate | get_first)
            {
                REQUIRE(i % 2 == 1);
            }
        }

        SUBCASE("squared view with rvalue std::array")
        {
            auto squared = transform([](auto i) { return i * i; });

            std::array<int, 50> ints;

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            size_t c = 0;
            ok_foreach(const int i, std::move(ints) | squared)
            {
                REQUIRE(i == c * c);
                ++c;
            }
        }

        SUBCASE("squared view with c-style array")
        {
            auto squared = transform([](auto i) { return i * i; });

            int ints[50];

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            size_t c = 0;
            ok_foreach(const int i, ints | squared)
            {
                REQUIRE(i == c * c);
                ++c;
            }
        }

        SUBCASE("can still get the size of transformed things")
        {
            auto squared = transform([](auto i) { return i * i; });
            std::array<int, 50> stdarray;
            int carray[35];
            std::vector<int> vector;
            vector.resize(25);

            const size_t arraysize = ok::size(stdarray);
            const size_t carraysize = ok::size(carray);
            const size_t vectorsize = ok::size(vector);

            REQUIRE(ok::size(stdarray | squared) == arraysize);
            REQUIRE(ok::size(carray | squared) == carraysize);
            REQUIRE(ok::size(vector | squared) == vectorsize);
        }
    }
}
