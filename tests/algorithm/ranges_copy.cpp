#include "okay/ranges/views/take_at_most.h"
#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/containers/arraylist.h"
#include "okay/opt.h"
#include "okay/ranges/algorithm.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/keep_if.h"

using namespace ok;

TEST_SUITE("ok::ranges_copy and ok::ranges_copy_as_much_as_will_fit algorithms")
{
    constexpr auto is_even = [](size_t i) { return i % 2 == 0; };

    TEST_CASE("copy from one array to another")
    {
        array_t a{1, 2, 3, 4, 5, 6};
        array_t b = array::defaulted_or_zeroed<int, 20>();

        ranges_copy(ranges_copy_options_t<decltype(b), decltype(a)>{
            .dest = b,
            .source = a,
        });

        REQUIRE(ranges_equal(b | take_at_most(a.size()), a));
    }

    TEST_CASE("copy from infinite -> finite or sized") {}

    TEST_CASE("copy from finite or sized -> infinite") {}

    TEST_CASE("copy from finite to finite") {}
}
