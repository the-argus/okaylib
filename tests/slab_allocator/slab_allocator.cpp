// making stacktraces every time an error is thrown is crazy slow, especially
// for this test
#define OKAYLIB_TESTING_BACKTRACE_DISABLE_FOR_RES_AND_STATUS
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
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            constexpr auto options = slab_allocator::options_t<3>{
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
            };
            return ok::opt<slab_allocator_t<c_allocator_t, 3>>(
                slab_allocator::with_blocks(backing, options).unwrap());
        });
    }
}
