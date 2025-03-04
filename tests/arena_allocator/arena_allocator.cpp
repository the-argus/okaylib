#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/arena.h"

using namespace ok;

TEST_SUITE("c_allocator")
{
    TEST_CASE("allocator tests")
    {
        ok::array_t<u8, 10000> bytes = {};
        arena_t arena(bytes);
        run_allocator_tests_static_and_dynamic_dispatch(arena);
    }
}
