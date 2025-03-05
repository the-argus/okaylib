#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/linked_block_allocator.h"

using namespace ok;

TEST_SUITE("c_allocator")
{
    TEST_CASE("allocator tests")
    {
        ok::array_t<u8, 10000> bytes = {};
        arena_t arena(bytes);
        auto blockpool = linked_blockpool_allocator::start_with_one_pool
                             .make({
                                 .num_bytes_per_block = 64,
                                 .minimum_alignment = 64,
                                 .num_blocks_in_first_pool = 100,
                                 .backing_allocator = arena,
                             })
                             .release();
        run_allocator_tests_static_and_dynamic_dispatch(blockpool);
    }
}
