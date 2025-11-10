// making stacktraces every time an error is thrown is crazy slow, especially
// for this test which overallocates
#define OKAYLIB_TESTING_BACKTRACE_DISABLE_FOR_RES_AND_STATUS
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
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            auto block = block_allocator::alloc_initial_buf(
                backing, {
                             .num_initial_spots = 1024,
                             .num_bytes_per_block = 1024,
                             .minimum_alignment = 16,
                         });
            return ok::opt<block_allocator_t<ok::c_allocator_t>>(
                std::move(block.unwrap()));
        });
    }
}
