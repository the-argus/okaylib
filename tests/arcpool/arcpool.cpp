#include "test_header.h"
// test header must be first
#include "okay/containers/arcpool.h"
#include "okay/allocators/c_allocator.h"

using namespace ok;

TEST_SUITE("arcpool")
{
    TEST_CASE("construction")
    {
        c_allocator_t allocator;
        arcpool_t<int, c_allocator_t> pool(allocator);
    }

    TEST_CASE("single threaded ownership")
    {
    }
}
