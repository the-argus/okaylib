#include "test_header.h"
// test header must be first
#include "okay/iterable/enumerate.h"
#include "okay/macros/foreach.h"
#include "okay/slice.h"
#include <array>
#include <vector>

using namespace ok;

// make sure that enumerated pair includes mutable reference if possible,
// otherwise it includes const reference
using eview = detail::enumerated_view_t<std::array<int, 50>&>;
using ecursor = detail::enumerated_cursor_t<std::array<int, 50>&>;
static_assert(
    std::is_same_v<decltype(ok::iter_copyout(std::declval<const eview&>(),
                                             std::declval<const ecursor&>())
                                .first),
                   int&>);
using eview_const = detail::enumerated_view_t<const std::array<int, 50>&>;
using ecursor_const = detail::enumerated_cursor_t<const std::array<int, 50>&>;
static_assert(std::is_same_v<
              decltype(ok::iter_copyout(std::declval<const eview_const&>(),
                                        std::declval<const ecursor_const&>())
                           .first),
              const int&>);
// TODO: make sure otherwise it includes get() result by value

TEST_SUITE("enumerate")
{
    TEST_CASE("functionality")
    {
        SUBCASE("enumerate array")
        {
            std::array<int, 50> ints = {};
            std::fill(ints.begin(), ints.end(), 0);

            size_t i = 0;
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

        SUBCASE("can still get the size of enumerated things")
        {
            std::array<int, 50> stdarray;
            int carray[35];
            std::vector<int> vector;
            vector.resize(25);

            const size_t arraysize = ok::size(stdarray);
            const size_t carraysize = ok::size(carray);
            const size_t vectorsize = ok::size(vector);

            REQUIRE(ok::size(stdarray | enumerate) == arraysize);
            REQUIRE(ok::size(carray | enumerate) == carraysize);
            REQUIRE(ok::size(vector | enumerate) == vectorsize);
        }
    }
}
