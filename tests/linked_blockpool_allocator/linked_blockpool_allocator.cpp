#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/linked_blockpool_allocator.h"

using namespace ok;

TEST_SUITE("c_allocator")
{
    TEST_CASE("allocator tests")
    {
        ok::array_t<u8, 64 * 5100> bytes = {};
        arena_t arena(bytes);
        defer arena_destroy([&] { arena.destroy(); });
        auto blockpool = linked_blockpool_allocator::start_with_one_pool
                             .make({
                                 .num_bytes_per_block = 64,
                                 .minimum_alignment = 64,
                                 .num_blocks_in_first_pool = 5000,
                                 .backing_allocator = arena,
                             })
                             .release();
        run_allocator_tests_static_and_dynamic_dispatch(blockpool);
    }
}
