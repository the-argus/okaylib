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
        arena_compat_wrapper_t arena_compat(arena);
        static_assert(ok::allocator_c<decltype(arena_compat)>);
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            auto out = linked_blockpool_allocator::start_with_one_pool(
                arena_compat, linked_blockpool_allocator::options_t{
                                  .num_bytes_per_block = 1024,
                                  .minimum_alignment = 64,
                                  .num_blocks_in_first_pool = 5000,
                              });
            return ok::opt<linked_blockpool_allocator_t<
                arena_compat_wrapper_t<arena_t<>>>>(std::move(out.unwrap()));
        });
        // run again but this time zero the contents before each run
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            ok::memfill(bytes.items(), 0);
            auto out = linked_blockpool_allocator::start_with_one_pool(
                arena_compat, linked_blockpool_allocator::options_t{
                                  .num_bytes_per_block = 1024,
                                  .minimum_alignment = 64,
                                  .num_blocks_in_first_pool = 5000,
                              });
            return ok::opt<linked_blockpool_allocator_t<
                arena_compat_wrapper_t<arena_t<>>>>(std::move(out.unwrap()));
        });
    }
}
