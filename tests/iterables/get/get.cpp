#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include <array>

using namespace ok;

TEST_SUITE("get")
{
    TEST_CASE("enumerate then remove indices")
    {
        std::array<int, 50> ints = {};
        iterators_copy_assign(ints, indices());
        REQUIRE(iterators_equal(ints, indices()));

        for (auto& [i1, i2] : enumerate(ints).get_tuple_elem<0>().zip(ints)) {
            REQUIRE(i1 == i2);
            REQUIRE(addressof(i1) == addressof(i2));
        }
    }

    TEST_CASE("enumerate, keep_if, remove indices")
    {
        int nums[] = {0, 1, 2, 3, 4};

        constexpr auto is_even_index = [](auto pair) -> bool {
            return ok::get<1>(pair) % 2 == 0;
        };

        for (int& even_int :
             enumerate(nums).keep_if(is_even_index).get_tuple_elem<0>()) {
            REQUIRE(even_int % 2 == 0);
        }
    }
}
