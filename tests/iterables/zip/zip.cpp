#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
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

    TEST_CASE("different combinations of forward/arraylike/reference/by value")
    {
        SUBCASE("zip view with arraylike value types")
        {
            int ints[] = {0, 1, 2, 3, 4};

            for (auto [i, index] : ok::iter(ints).enumerate()) {
                REQUIRE(i == index);
            }

            for (auto [i, index] : enumerate(ints)) {
                REQUIRE(i == index);
            }

            REQUIRE(ok::iter(ints).zip(ok::indices()).size() == 5);
            REQUIRE(zip(ints, indices()).size() == 5);

            for (auto [i, index] : zip(ints, indices())) {
                REQUIRE(i == index);
            }

            for (auto [enumerated_normal, enumerated_zip] :
                 enumerate(ints).zip(zip(ints, ok::indices()))) {
                REQUIRE(enumerated_normal == enumerated_zip);
            }
        }

        SUBCASE("zip view with forward value types")
        {
            forward_iterable_size_test_t<size_mode::unknown_sized> iterable;

            for (auto [i, index] : iterable.iter().zip(indices())) {
                REQUIRE(i == index);
            }

            for (auto [enumerated_normal, enumerated_zip] :
                 enumerate(iterable).zip(zip(iterable, indices()))) {
                REQUIRE(enumerated_normal == enumerated_zip);
            }
        }

        SUBCASE("zip view with an lvalue reference and a value type")
        {
            int ints[] = {0, 1, 2, 3, 4};

            for (auto& [int_item, index] : ok::iter(ints).zip(indices())) {
                static_assert(same_as_c<decltype(int_item), int&>);
                static_assert(same_as_c<decltype(index), size_t>);
                REQUIRE(int_item == index);
            }

            arraylike_iterable_reftype_test_t iterable;

            for (auto& [iterable_item, int_item, index] :
                 iterable.iter().zip(ok::iter(ints), indices())) {
                static_assert(same_as_c<decltype(iterable_item), int&>);
                static_assert(same_as_c<decltype(int_item), int&>);
                static_assert(same_as_c<decltype(index), size_t>);
                REQUIRE(iterable_item == int_item);
            }
        }

        SUBCASE("zip view with only lvalue references")
        {
            int ints[] = {0, 1, 2, 3, 4, 5};
            arraylike_iterable_reftype_test_t iterable;

            for (auto& [iterable_item, int_item] :
                 zip(iterable, reverse(ints))) {
                static_assert(same_as_c<decltype(iterable_item), int&>);
                static_assert(same_as_c<decltype(int_item), int&>);

                REQUIRE(int_item != iterable_item);
                int_item = iterable_item; // swap the two ranges
            }

            REQUIRE(ok::iterators_equal(
                ints, maybe_undefined_array_t{5, 4, 3, 2, 1, 0}));
        }
    }
}
