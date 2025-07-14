#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/zip.h"
#include "okay/stdmem.h"
#include "testing_types.h"
#include <array>

using namespace ok;
// zipped view should inherit random accessibility from zipped ranges
static_assert(random_access_range_c<example_range_cstyle&>);
static_assert(range_c<example_range_cstyle&>);
static_assert(random_access_range_c<ok::detail::zipped_view_t<
                  example_range_cstyle&, example_range_cstyle&>>);
// static_assert(detail::range_marked_infinite_c<decltype(zip(indices, indices))>);

TEST_SUITE("zip")
{
    TEST_CASE("zip three c style arrays")
    {
        int a1[] = {1, 2, 3};
        int a2[] = {1, 2, 3};
        int a3[] = {1, 2, 3};

        // since theyre all arraylike, zip is also arraylike
        static_assert(
            std::is_same_v<cursor_type_for<decltype(zip(a1, a2, a3))>, size_t>);

        auto [a, b, c] = ok::make_tuple(0, 1, 2);

        ok_foreach(ok_decompose(i1, i2, i3), zip(a1, a2, a3))
        {
            REQUIRE(i1 == i2);
            REQUIRE(i2 == i3);
        }
    }

    TEST_CASE("zipping takes on the size of the smaller item, if it is first")
    {
        int a1[] = {1};
        int a2[] = {1, 2};

        static_assert(detail::range_marked_arraylike_c<decltype(a1)>);
        static_assert(detail::range_marked_arraylike_c<decltype(zip(a1, a2))>);

        REQUIRE(ok::size(zip(a1, a2)) == ok::size(a1));

        std::tuple<int> test;

        static_assert(std::is_constructible_v<std::tuple<int, int>, int, int>);
        static_assert(std::is_constructible_v<ok::tuple<int, int>, int, int>);
        static_assert(!std::is_constructible_v<ok::tuple<int(&)[2], int>, float, float>);
        static_assert(!std::is_constructible_v<std::tuple<int(&)[2], int>, float, float>);

        using T = decltype(zip(a2, indices));

        REQUIRE(ok::size(zip(a2, indices)) == ok::size(a2));

        example_range_cstyle example;

        // random-access-ness is propagated
        static_assert(detail::random_access_range_c<example_range_cstyle>);
        static_assert(detail::random_access_range_c<decltype(a2)>);
        static_assert(
            detail::random_access_range_c<decltype(zip(a2, example))>);

        REQUIRE(ok::size(zip(a2, example)) == ok::size(a2));

        std::array<int, 20> small;

        REQUIRE(ok::size(zip(small, example)) == ok::size(small));

        fifty_items_unknown_size_t finite_range;

        // zip assumes it is the size of the first item
        REQUIRE(ok::size(zip(small, finite_range)) == ok::size(small));
    }

    TEST_CASE("bidirectionality propagated")
    {
        example_range_bidirectional bidir;
        int arr[50] = {};
        memfill(slice(arr), 0);

        static_assert(
            detail::bidirectional_range_c<example_range_bidirectional>);
        static_assert(detail::bidirectional_range_c<decltype(arr)>);
        static_assert(
            !detail::random_access_range_c<example_range_bidirectional>);

        using Z = decltype(zip(arr, bidir));

        static_assert(detail::bidirectional_range_c<Z>);
        static_assert(!detail::random_access_range_c<Z>);
    }

    TEST_CASE("zip with zero sized range makes empty range")
    {
        std::array<int, 0> zero = {};
        int a[] = {1, 2, 3, 4};

        ok_foreach(ok_pair(z, a), zip(zero, a)) { REQUIRE(false); }
    }

    TEST_CASE("zipping aborts on creation if later range is shorter")
    {
        int a1[] = {283, 24, 6, 5, 5};
        int a2[] = {};
        example_range_cstyle range;

        REQUIREABORTS(zip(range, a1));
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
            REQUIRE(ok::get<0>(tuple) % 3 == index);
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

        ok_foreach(ok_pair(s, i), zip(array, indices)) { REQUIRE(s == i); }
    }
}
