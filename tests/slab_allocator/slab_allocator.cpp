#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/slab_allocator.h"

using namespace ok;

TEST_SUITE("block allocator")
{
    TEST_CASE("allocator tests")
    {
        c_allocator_t backing;
        slab_allocator_t slab = slab_allocator::with_blocks.make(
            backing, {
                         .available_blocksizes =
                             {
                                 {
                                     .blocksize = 64,
                                     .alignment = 16,
                                 },
                                 {
                                     .blocksize = 256,
                                     .alignment = 16,
                                 },
                                 {
                                     .blocksize = 1024,
                                     .alignment = 16,
                                 },
                             },
                         .num_initial_blocks_per_blocksize = 1024,
                     });
        run_allocator_tests_static_and_dynamic_dispatch(slab);
    }
}
