#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/arraylist.h"
#include "okay/ranges/algorithm.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/all.h"
#include "okay/ranges/views/drop.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/transform.h"

using namespace ok;

TEST_SUITE("ok::ranges_copy and ok::ranges_copy_as_much_as_will_fit algorithms")
{
    constexpr auto is_even = [](size_t i) { return i % 2 == 0; };

    TEST_CASE("copy from one array to another")
    {
        array_t a{1, 2, 3, 4, 5, 6};
        array_t b = array::defaulted_or_zeroed<int, 20>();

        ok::ranges_copy(ok::dest(b), ok::source(a));

        REQUIRE(ranges_equal(b | take_at_most(a.size()), a));
        bool all_zero = b | drop(a.size()) | all([](auto i) { return i == 0; });
        REQUIRE(all_zero);
    }

    TEST_CASE("copy from infinite -> finite or sized")
    {
        SUBCASE("indices into sized (array)")
        {
            array_t array = array::undefined<int, 5>();
            ranges_copy(dest(array), source(indices));

            REQUIRE(ranges_equal(array, array_t{0, 1, 2, 3, 4}));
        }

        SUBCASE("indices into finite")
        {
            array_t array = array::defaulted_or_zeroed<int, 10>();

            auto finite_view = array | enumerate |
                               keep_if([](auto pair) -> bool {
                                   auto [item, index] = pair;
                                   return index % 2 == 0;
                               }) |
                               transform([](auto pair) -> int& {
                                   auto [item, index] = pair;
                                   return item;
                               });
            static_assert(detail::range_marked_finite_v<decltype(finite_view)>);

            auto c = ok::begin(finite_view);

            ranges_copy(dest(finite_view), source(indices));

            // only affect every other item
            REQUIRE(ranges_equal(array, array_t{0, 0, 1, 0, 2, 0, 3, 0, 4, 0}));
        }
    }

    TEST_CASE("copy from finite or sized -> infinite")
    {
        c_allocator_t backing;
        arraylist_t alist = arraylist::empty<int>(backing);

        auto appender = arraylist::appender_t(alist);

        ranges_copy(dest(appender), source(array_t{0, 1, 2}));

        REQUIRE(ranges_equal(alist, array_t{0, 1, 2}));
    }

    TEST_CASE("copy from finite to finite")
    {
        ok::array_t a = {0, 1, 2, 3, 4};
        auto finite_view = a | enumerate | keep_if([](auto pair) -> bool {
                               auto [item, index] = pair;
                               return index % 2 == 0;
                           }) |
                           transform([](auto pair) -> int& {
                               auto [item, index] = pair;
                               return item;
                           });

        auto finite_input = a | keep_if([](int i) { return i % 2 == 1; });

        ranges_copy(dest(finite_view), source(finite_input));

        REQUIRE(ranges_equal(a, array_t{1, 1, 3, 3, 4}));
    }
}
