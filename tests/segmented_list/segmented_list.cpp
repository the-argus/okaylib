#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/segmented_list.h"

using namespace ok;

TEST_SUITE("segmented list")
{
    TEST_CASE("initialization with trivial type (ints)")
    {
        SUBCASE("cannot init with zero starting spots")
        {
            c_allocator_t c_allocator;
            REQUIREABORTS(
                auto&& guy = segmented_list::empty<int>(
                    c_allocator, segmented_list::empty_options_t{
                                     .num_initial_contiguous_spots = 0,
                                 }));
        }

        SUBCASE("init with one starting spot") {}
    }
}
