#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/block_allocator.h"
#include "okay/allocators/c_allocator.h"

using namespace ok;

TEST_SUITE("block allocator")
{
    TEST_CASE("allocator tests")
    {
        c_allocator_t backing;
        block_allocator_t blocks = block_allocator::alloc_initial_buf(
                                       backing, {.num_initial_spots = 1024,
                                                 .num_bytes_per_block = 64,
                                                 .minimum_alignment = 16})
                                       .release();
        run_allocator_tests_static_and_dynamic_dispatch(blocks);
    }
}
