#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/transform.h"
#include "okay/slice.h"
#include "okay/stdmem.h"
#include <array>

using namespace ok;

TEST_SUITE("transform")
{
    TEST_CASE("functionality")
    {
        SUBCASE("identity transform")
        {
            std::array<int, 50> ints = {};
            std::fill(ints.begin(), ints.end(), 0);

            for (auto c = ok::begin(ints); ok::is_inbounds(ints, c);
                 ok::increment(ints, c)) {
                int& item = ok::range_get_ref(ints, c);
                item = c;
            }

            auto identity = ints | transform([](auto i) { return i; });

            for (auto c = ok::begin(identity); ok::is_inbounds(identity, c);
                 ok::increment(identity, c)) {
                int item = ok::range_get(identity, c);
                REQUIRE(item == c);
            }
        }

        SUBCASE("identity transform with macros")
        {
            std::array<int, 50> ints = {};
            memfill(slice(ints), 0);

            size_t c = 0;
            ok_foreach(auto& item, ints)
            {
                item = c;
                ++c;
            }

            auto identity = ints | transform([](auto i) { return i; });
            c = 0;
            ok_foreach(const auto& item, identity)
            {
                REQUIRE(item == c);
                ++c;
            }
        }

        SUBCASE("squared view with std::array")
        {
            auto squared = transform([](auto i) { return i * i; });

            std::array<int, 50> ints;

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            size_t c = 0;
            ok_foreach(const int i, ints | squared)
            {
                REQUIRE(i == c * c);
                ++c;
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
