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
        using blocks_desc = slab_allocator::blocks_description_t;
        slab_allocator_t slab =
            slab_allocator::with_blocks(
                backing,
                slab_allocator::options_t<3>{
                    .available_blocksizes =
                        {
                            blocks_desc{
                                .blocksize = 64,
                                .alignment = 16,
                            },
                            blocks_desc{
                                .blocksize = 256,
                                .alignment = 16,
                            },
                            blocks_desc{
                                .blocksize = 1024,
                                .alignment = 16,
                            },
                        },
                    .num_initial_blocks_per_blocksize = 1024,
                })
                .release();
        run_allocator_tests_static_and_dynamic_dispatch(slab);
    }
}
