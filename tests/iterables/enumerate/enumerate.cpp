#include "test_header.h"
// test header must be first
#include "okay/iterables/iterables.h"
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

            static_assert(iterable_c<decltype(enumerate(ints))>);

            size_t i = 0;
            for (auto [item, index] : enumerate(ints)) {
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
            for (auto [item, index] : enumerate(mem)) {
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
            for (auto [item, index] : enumerate(std::move(mem))) {
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
            slice<uint8_t> test(mem);
            for (auto [item, index] : enumerate(test)) {
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
            for (auto [item, index] : enumerate(memref)) {
                static_assert(std::is_same_v<decltype(item), const Test&>);
                REQUIRE(item.i == 0);
                REQUIRE(item.j == 0);
                REQUIRE(index == i);
                ++i;
            }
        }

        SUBCASE("can still get the size of enumerated things")
        {
            std::array<int, 50> stdarray;
            int carray[35];
            std::vector<int> vector;
            vector.resize(25);

            const size_t arraysize = ok::size(stdarray);
            const size_t carraysize = ok::size(carray);
            const size_t vectorsize = ok::size(vector);

            REQUIRE(ok::size(enumerate(stdarray)) == arraysize);
            REQUIRE(ok::size(enumerate(carray)) == carraysize);
            REQUIRE(ok::size(enumerate(vector)) == vectorsize);
        }
    }
}
