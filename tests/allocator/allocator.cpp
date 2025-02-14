#include "test_header.h"
// test header must be first
#include "okay/allocators/allocator.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/c_allocator.h"

using namespace ok;

struct test
{
    int a;
    float b;

    test() = default;
    explicit test(int num) noexcept : a(num), b(num) {}
};

TEST_SUITE("abstract allocator")
{
    TEST_CASE("c allocator implements interface")
    {
        c_allocator_t allocator;
        auto& buf = ok::make<test[100]>(allocator).release();

        arena_t arena(reinterpret_as_bytes(slice_t(buf)));

        auto mytest = ok::make<test>(arena);
        auto test2 = ok::make<test>(arena, 1);

        arena.destroy();

        ok::free(allocator, buf);
    }
}
