#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/arena.h"

using namespace ok;

TEST_SUITE("arena allocator")
{
    TEST_CASE("allocator tests")
    {
        ok::zeroed_array_t<u8, 10000> bytes;
        run_allocator_tests_static_and_dynamic_dispatch(
            [&] { return ok::opt<arena_t<>>(ok::in_place, bytes); });
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            ok::memfill(bytes.items(), 0);
            return ok::opt<arena_t<>>(ok::in_place, bytes);
        });
    }
}
