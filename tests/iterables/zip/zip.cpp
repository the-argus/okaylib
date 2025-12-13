#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include "okay/stdmem.h"
#include "testing_types.h"
#include <array>

using namespace ok;
static_assert(arraylike_iterable_c<example_range_cstyle>);
static_assert(iterable_c<example_range_cstyle>);
static_assert(arraylike_iterable_c<
              decltype(ok::zip(stdc::declval<example_range_cstyle>(),
                               stdc::declval<example_range_cstyle>()))>);
static_assert(is_iterable_infinite<decltype(zip(indices(), indices()))>);

TEST_SUITE("zip")
{
    TEST_CASE("zip three c style arrays")
    {
        int a1[] = {1, 2, 3};
        int a2[] = {1, 2, 3};
        int a3[] = {1, 2, 3};

        for (auto [i1, i2, i3] : zip(a1, a2, a3)) {
            REQUIRE(i1 == i2);
            REQUIRE(i2 == i3);
        }
    }

    TEST_CASE("zipping takes on the size of the smaller item, if it is first")
    {
        int a1[] = {1};
        int a2[] = {1, 2};

        static_assert(arraylike_iterable_c<decltype(a1)&>);
        static_assert(arraylike_iterable_c<decltype(zip(a1, a2))>);

        REQUIRE(ok::size(zip(a1, a2)) == ok::size(a1));

        std::tuple<int> test;

        static_assert(std::is_constructible_v<std::tuple<int, int>, int, int>);
        static_assert(std::is_constructible_v<ok::tuple<int, int>, int, int>);
        static_assert(
            !std::is_constructible_v<ok::tuple<int(&)[2], int>, float, float>);
        static_assert(
            !std::is_constructible_v<std::tuple<int(&)[2], int>, float, float>);

        using T = decltype(zip(a2, indices()));

        REQUIRE(ok::size(zip(a2, indices())) == ok::size(a2));

        example_range_cstyle example;

        // random-access-ness is propagated
        static_assert(arraylike_iterable_c<example_range_cstyle>);
        static_assert(arraylike_iterable_c<decltype(a2)&>);
        static_assert(arraylike_iterable_c<decltype(zip(a2, example))>);

        REQUIRE(ok::size(zip(a2, example)) == ok::size(a2));

        std::array<int, 20> small;

        REQUIRE(ok::size(zip(small, example)) == ok::size(small));

        forward_iterable_size_test_t<size_mode::unknown_sized> finite;

        static_assert(!is_iterable_sized<decltype(zip(small, finite))>);
    }

    TEST_CASE("zip with zero sized range makes empty range")
    {
        std::array<int, 0> zero = {};
        int a[] = {1, 2, 3, 4};

        for (auto [z, a] : zip(zero, a)) {
            REQUIRE(false);
        }
    }

    TEST_CASE("check that size of equally sized things zipped is the same size")
    {
        int a1[] = {1, 2};
        int a2[] = {1, 2};
        auto zipped = zip(a1, a2);
        REQUIRE(ok::size(zipped) == ok::size(a1));
    }

    TEST_CASE("zip then enumerate")
    {
        std::array a1 = {0, 1, 2};
        std::array a2 = {3, 4, 5};

        using T = decltype(zip(a1, a2));

        static_assert(arraylike_iterable_c<T>);

        for (auto [tuple, index] : zip(a1, a2).enumerate()) {
            REQUIRE(ok::get<0>(tuple) % 3 == index);
        }
    }

    TEST_CASE("infinite range zip")
    {
        std::array<size_t, 100> array = {};

        for (auto [s, i] : enumerate(array)) {
            s = i;
        }
        REQUIRE(iterators_equal(array, indices()));

        for (auto [s, i] : zip(array, indices().take_at_most(100))) {
            REQUIRE(s == i);
        }

        for (auto [s, i] : zip(array, indices())) {
            REQUIRE(s == i);
        }
    }
}
