#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/zip.h"

using namespace ok;

TEST_SUITE("zip")
{
    TEST_CASE("zip three c style arrays")
    {
        int a1[] = {1, 2, 3};
        int a2[] = {1, 2, 3};
        int a3[] = {1, 2, 3};

        ok_foreach(ok_decompose(i1, i2, i3), zip(a1, a2, a3))
        {
            REQUIRE(i1 == i2);
            REQUIRE(i2 == i3);
        }
    }

    TEST_CASE("check that zipping differently sized things causes an abort")
    {
        int a1[] = {1};
        int a2[] = {1, 2};
        REQUIREABORTS(zip(a1, a2));
    }

    TEST_CASE("check that size of equally sized things zipped is the same size")
    {
        int a1[] = {1, 2};
        int a2[] = {1, 2};
        auto zipped = zip(a1, a2);
        REQUIRE(ok::size(zipped) == ok::size(a1));
    }

    TEST_CASE("iterating over forward ranges causes abort when iterating")
    {
        // keep_if turns views into forward views
        auto identity = keep_if([](auto i) { return true; });
        int a1[] = {1, 2};
        int a2[] = {1};

        // zipped not able to detect different lengths bc theyre not sized
        auto zipped = zip(a1 | identity, a2 | identity);

        REQUIREABORTS(
            ok_foreach(ok_pair(i1, i2), zipped) { REQUIRE(i1 == i2); })
    }

    TEST_CASE("zip then enumerate")
    {
        std::array a1 = {0, 1, 2};
        std::array a2 = {3, 4, 5};

        ok_foreach(ok_pair(tuple, index), zip(a1, a2) | enumerate)
        {
            REQUIRE(std::get<0>(tuple) % 3 == index);
        }
    }

    TEST_CASE("infinite range zip")
    {
        std::array<size_t, 100> array = {};

        ok_foreach(ok_pair(s, i), enumerate(array)) { s = i; }

        ok_foreach(ok_pair(s, i), zip(array, indices | take_at_most(100)))
        {
            REQUIRE(s == i);
        }
    }
}
