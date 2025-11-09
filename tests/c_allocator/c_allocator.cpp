#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/c_allocator.h"

using namespace ok;

TEST_SUITE("c_allocator")
{
    TEST_CASE("allocator tests")
    {
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            auto out = ok::opt<c_allocator_t>();
            out.emplace();
            return out;
        });
    }
}
