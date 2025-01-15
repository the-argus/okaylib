#include "test_header.h"
// test header must be first
#include "okay/iterable/enumerate.h"
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

            size_t i = 0;
            ok_foreach(ok_pair(item, index), enumerate(ints))
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
            ok_foreach(ok_pair(item, index), enumerate(mem))
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
            ok_foreach(ok_pair(item, index), enumerate(std::move(mem)))
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
            ok_foreach(ok_pair(item, index), enumerate(test))
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
            ok_foreach(ok_pair(item, index), enumerate(memref))
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
