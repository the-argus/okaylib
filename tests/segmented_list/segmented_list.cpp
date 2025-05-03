#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/bit_array.h"
#include "okay/containers/segmented_list.h"
#include "okay/ranges/views/transform.h"

using namespace ok;

TEST_SUITE("segmented list")
{
    TEST_CASE("initialization with trivial type (ints)")
    {
        c_allocator_t c_allocator;
        SUBCASE("empty constructor")
        {
            segmented_list_t a = segmented_list::empty<int>(
                                     c_allocator,
                                     {
                                         .num_initial_contiguous_spots = 0,
                                     })
                                     .release();

            segmented_list_t b = segmented_list::empty<int>(
                                     c_allocator,
                                     {
                                         .num_initial_contiguous_spots = 1,
                                     })
                                     .release();

            segmented_list_t c = segmented_list::empty<int>(
                                     c_allocator,
                                     {
                                         .num_initial_contiguous_spots = 21384,
                                     })
                                     .release();
        }

        SUBCASE("copy items from range constructor")
        {
            constexpr auto rng = bit_array::bit_string("10101");
            segmented_list_t bools =
                segmented_list::copy_items_from_range(
                    c_allocator,
                    rng | transform([](ok::bit b) { return bool(b); }), {})
                    .release();
        }
    }
}
