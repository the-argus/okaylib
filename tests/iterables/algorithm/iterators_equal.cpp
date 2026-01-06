#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/opt.h"
#include "okay/ranges/algorithm.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/keep_if.h"

TEST_SUITE("ok::ranges_equal algorithm")
{
    constexpr auto is_even = [](size_t i) { return i % 2 == 0; };

    TEST_CASE("compare arraylike ranges (all known size)")
    {
        const ok::maybe_undefined_array_t test = {2, 4, 6, 8, 10};
        const ok::maybe_undefined_array_t test_diff_size = {2, 4, 6, 8, 10, 12};
        const ok::maybe_undefined_array_t test_diff_contents = {2, 4, 6, 8, 11};

        SUBCASE("compare against self is always true")
        {
            REQUIRE(ok::ranges_equal(test, test));
            REQUIRE(ok::ranges_equal(test_diff_size, test_diff_size));
            REQUIRE(ok::ranges_equal(test_diff_contents, test_diff_contents));
        }

        SUBCASE("differently sized things are always not the same")
        {
            REQUIRE(!ok::ranges_equal(test_diff_size, test));
            REQUIRE(!ok::ranges_equal(test, test_diff_size));
            REQUIRE(!ok::ranges_equal(test_diff_contents, test_diff_size));
        }

        SUBCASE("things with the same size but different contents are not same")
        {
            REQUIRE(!ok::ranges_equal(test, test_diff_contents));
            REQUIRE(!ok::ranges_equal(test_diff_contents, test));
        }

        SUBCASE("compare optionals")
        {
            ok::opt<int> i = 1;
            ok::opt<int> j = 2;
            ok::opt<int> k = {};

            REQUIRE(ok::ranges_equal(i, i));
            REQUIRE(ok::ranges_equal(j, j));
            // empty ranges are the same
            REQUIRE(ok::ranges_equal(k, k));

            REQUIRE(!ok::ranges_equal(i, j));
            REQUIRE(!ok::ranges_equal(j, i));
            REQUIRE(!ok::ranges_equal(j, k));
            REQUIRE(!ok::ranges_equal(k, j));
            REQUIRE(!ok::ranges_equal(i, k));
            REQUIRE(!ok::ranges_equal(k, i));
        }
    }

    TEST_CASE("compare combinations of infinite/finite/sized")
    {
        SUBCASE("infinite and sized")
        {
            const ok::maybe_undefined_array_t test = {0, 1, 2, 3, 4, 5};
            const ok::maybe_undefined_array_t non_indices_test = {-1, 0, 1, 2,
                                                                  3,  4, 5};

            REQUIRE(ok::ranges_equal(test, ok::indices));
            REQUIRE(ok::ranges_equal(ok::indices, test));
            REQUIRE(!ok::ranges_equal(non_indices_test, ok::indices));
            REQUIRE(!ok::ranges_equal(ok::indices, non_indices_test));
        }

        // TODO: this should be constexpr but ok::keep_if
        // range_adaptor_closure_t is non-literal type
        const auto keep_if_less_than_100 =
            ok::keep_if([](int i) { return i < 100; });

        constexpr ok::maybe_undefined_array_t finite_input = {0, 100, 1, 100,
                                                              2, 100, 3, 100};
        constexpr ok::maybe_undefined_array_t non_indices_finite_input = {
            0, 100, 1, 100, 2, 100, 3, 100, 5};

        const auto finite = finite_input | keep_if_less_than_100;
        const auto non_indices_finite =
            non_indices_finite_input | keep_if_less_than_100;

        SUBCASE("infinite and finite")
        {
            REQUIRE(ok::ranges_equal(finite, ok::indices));
            REQUIRE(ok::ranges_equal(ok::indices, finite));
            REQUIRE(!ok::ranges_equal(non_indices_finite, ok::indices));
            REQUIRE(!ok::ranges_equal(ok::indices, non_indices_finite));
        }

        SUBCASE("sized and finite")
        {
            constexpr ok::maybe_undefined_array_t sized_indices = {0, 1, 2, 3};
            REQUIRE(ok::ranges_equal(sized_indices, finite));
            REQUIRE(ok::ranges_equal(finite, sized_indices));
            REQUIRE(!ok::ranges_equal(sized_indices, non_indices_finite));
            REQUIRE(!ok::ranges_equal(non_indices_finite, sized_indices));
        }
    }
}
