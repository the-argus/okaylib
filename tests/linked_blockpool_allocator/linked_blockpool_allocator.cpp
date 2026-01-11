// this test is incredibly slow, removing backtraces helps
#define OKAYLIB_TESTING_BACKTRACE_DISABLE_FOR_RES_AND_STATUS
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
        ok::zeroed_array_t<u8, 1024 * 5100> bytes;
        arena_t arena(bytes);
        static_assert(ok::allocator_c<arena_t<>>);
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            arena.clear();
            auto out = linked_blockpool_allocator::start_with_one_pool(
                arena, linked_blockpool_allocator::options_t{
                           .num_bytes_per_block = 1024,
                           .minimum_alignment = 64,
                           .num_blocks_in_first_pool = 5000,
                       });
            OKAYLIB_REQUIRE_RES_WITH_BACKTRACE(out);
            return ok::opt<linked_blockpool_allocator_t<arena_t<>>>(
                std::move(out.unwrap()));
        });
    }
}
