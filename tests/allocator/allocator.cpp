#include "test_header.h"
// test header must be first
#include "okay/allocators/allocator.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/wrappers.h"
#include "okay/containers/array.h"
#include "okay/containers/array_list.h"

#include <array>

using namespace ok;

struct test
{
    int a;
    float b;

    test() = default;
    explicit test(int num) noexcept : a(num), b(num) {}
};

struct empty_destructor
{
    int i = 0;
    ~empty_destructor() {}
};

// test an allocator through its vtable. expects at least 1mb of memory to be
// accessible to the allocator, although it can start with any amount
// void virtual_tests_1mb_a(allocator_t& ally)
// {
//     // alloc a bunch of stuff, every allocator should be capable of at least
//     // this

//     auto make_attempt = ok::try_make<array_t<int, 500>::make_factory>(ally);

//     auto& int_array = ok::make<array_t<int, 500>>(ally);

//     auto ints = array_t<int, 500>::make();

//     ok::array_t test = array_t<int, 500>::make();

//     if (ally.features() & alloc::feature_flags::can_only_alloc) {
//     }
// }

void virtual_tests_array_list(allocator_t& ally)
{
    ok::array_t array = {1, 2, 3, 4};

    array_list_t array_list =
        array_list_t<int>::try_make_by_copying(ally, array).release();
}

TEST_SUITE("abstract allocator")
{
    TEST_CASE("c allocator implements interface")
    {
        c_allocator_t allocator;
        auto& buf = ok::make<test[100]>(allocator).release();

        arena_t arena(reinterpret_as_bytes(slice_t(buf)));

        test& mytest = ok::make<test>(arena).release();
        test& test2 = ok::make<test>(arena, 1).release();
        test& test3 = ok::make<test>(arena, mytest).release();

        arena.destroy();

        virtual_tests_array_list(allocator);

        ok::free(allocator, buf);
    }
}
