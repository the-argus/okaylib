#include "test_header.h"
// test header must be first
#include "okay/allocators/allocator.h"
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/arena.h"

using namespace ok;

struct test
{
    int a;
    float b;

    test() = default;
    explicit test(int num) noexcept : a(num), b(num) {}
    ~test() { __ok_assert(false, "this should have been caught at compile time"); }
};

TEST_SUITE("abstract allocator")
{
    TEST_CASE("c allocator implements interface")
    {
        c_allocator_t allocator;
        auto& buf = ok::make<test[100]>(allocator).release();

        arena_t arena(reinterpret_as_bytes(slice_t(buf)));

        test& mytest = ok::make<test>(arena).release();
        test& test2 = ok::make<test>(arena, 1).release();

        ok::free(allocator, buf);
    }
}
