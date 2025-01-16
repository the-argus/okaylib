#include "test_header.h"
// test header must be first
#include "okay/iterable/enumerate.h"
#include "okay/iterable/transform.h"
#include "okay/macros/foreach.h"
#include "okay/slice.h"
#include <array>
#include <vector>

using namespace ok;

TEST_SUITE("enumerate")
{
    TEST_CASE("functionality")
    {
        SUBCASE("enumerate array")
        {
            std::array<int, 50> ints = {};
            std::fill(ints.begin(), ints.end(), 0);

            // auto trfrm = [](auto pair) { return std::make_pair(pair,
            // ok::detail::empty_t{}); };

            size_t i = 0;
            // ok_foreach(ok_pair(item, index), transform(ints | enumerate,
            // trfrm))
            ok_foreach(ok_pair(item, index), ints | enumerate)
            {
                static_assert(std::is_same_v<decltype(item), int&>);
                static_assert(std::is_same_v<decltype(index), const size_t>);

                REQUIRE(item == 0);
                REQUIRE(index == i);

                ++i;
            }
        }

        SUBCASE("enumerate vector")
        {
            std::vector<uint8_t> mem;
            mem.reserve(500);
            for (size_t i = 0; i < 500; ++i) {
                mem.push_back(0);
            }

            size_t i = 0;
            ok_foreach(ok_pair(item, index), mem | enumerate)
            {
                REQUIRE(item == 0);
                REQUIRE(index == i);
                ++i;
            }
        }

        SUBCASE("enumerate moved vector")
        {
            std::vector<uint8_t> mem;
            mem.reserve(500);
            for (size_t i = 0; i < 500; ++i) {
                mem.push_back(0);
            }

            size_t i = 0;
            ok_foreach(ok_pair(item, index), std::move(mem) | enumerate)
            {
                REQUIRE(item == 0);
                REQUIRE(index == i);
                ++i;
            }
        }

        SUBCASE("enumerate slice")
        {
            std::vector<uint8_t> mem;
            mem.reserve(500);
            for (size_t i = 0; i < 500; ++i) {
                mem.push_back(0);
            }

            size_t i = 0;
            slice_t<uint8_t> test(mem);
            ok_foreach(ok_pair(item, index), test | enumerate)
            {
                REQUIRE(item == 0);
                REQUIRE(index == i);
                ++i;
            }
        }

        SUBCASE("enumerate const vector of large type")
        {
            struct Test
            {
                int i;
                size_t j;
            };
            std::vector<Test> mem;
            mem.reserve(500);
            for (size_t i = 0; i < 500; ++i) {
                mem.push_back({});
            }

            const std::vector<Test>& memref = mem;

            size_t i = 0;
            ok_foreach(ok_pair(item, index), memref | enumerate)
            {
                static_assert(std::is_same_v<decltype(item), const Test&>);
                REQUIRE(item.i == 0);
                REQUIRE(item.j == 0);
                REQUIRE(index == i);
                ++i;
            }
        }
    }
}
