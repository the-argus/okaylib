#include "test_header.h"
// test header must be first
#include "okay/allocators/allocator.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/wrappers.h"
#include "okay/containers/array.h"
#include "okay/containers/arraylist.h"
#include "okay/macros/foreach.h"
#include "okay/ranges/views/zip.h"

using namespace ok;

struct test
{
    int a;
    float b;

    struct default_construct
    {
        using associated_type = test;
    };

    test() = default;
    static constexpr test construct(const default_construct&) { return test(); }
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
//     alloc::owned array_on_heap =
//         ally.make(array_t<int, 500>::undefined{}).release();

//     auto array_on_stack = array_t<int, 500>::construct({});

//     auto int_array =
//         ally.make(array_t<int, 50>::defaulted_or_zeroed{}).release();

//     ok::array_t deduction_test =
//         ok::make(array_t<int, 500>::defaulted_or_zeroed{});

//     if (ally.features() & alloc::feature_flags::can_only_alloc) {
//     }
// }

void virtual_tests_array_list(allocator_t& ally)
{
    ok::array_t array = {1, 2, 3, 4};

    ok::array_t zeroed = array::defaulted_or_zeroed<int, 5>();
    zeroed = ok::make(array::defaulted_or_zeroed<int, 5>);

    ok::array_t undefined = array::undefined<int, 5>();
    undefined = ok::make(ok::array::undefined<int, 5>);

    auto alist1 = arraylist::empty<int>(ally);

    arraylist_t alist = ok::make(arraylist_t<int>::empty{.allocator = ally});

    auto arraylist =
        ok::make(ok::arraylist_t<int>::copy_items_from_range{ally, array})
            .release();

    {
        alloc::owned array2d_on_heap =
            ally.make(arraylist_t<arraylist_t<int>>::spots_preallocated{
                          .allocator = ally,
                          .num_spots_preallocated = 50UL,
                      })
                .release();
    }

    arraylist_t empty_on_stack = ok::make(arraylist_t<int>::empty{ally});

    if (!empty_on_stack.append(1).okay()) {
        __ok_assert(false, "couldnt append");
        return;
    }

    auto arraylist2 =
        ok::make(arraylist_t<int>::copy_items_from_range(ally, array))
            .release();

    ok_foreach(ok_pair(a, al), zip(array, arraylist)) { REQUIRE(a == al); }
}

TEST_SUITE("abstract allocator")
{
    TEST_CASE("c allocator implements interface")
    {
        c_allocator_t allocator;
        // TODO: support allocating c style arrays with allocator_t::make()
        // auto& buf = allocator.make<test[100]>().release();
        auto& buf =
            allocator.make(array_t<test, 100>::undefined{}).release().release();

        arena_t arena(reinterpret_as_bytes(slice_t(buf)));

        alloc::owned mytest = arena.make(test::default_construct{}).release();
        test& mytest2 =
            arena.make(test::default_construct{}).release().release();
        alloc::owned mytest3 = arena.make_trivial<test>(1).release();
        test& mytest4 = arena.make_trivial<test>(1).release().release();
        test& test2 = arena.malloc<test>(1).release();
        test& test3 = arena.malloc<test>(test2).release();

        arena.destroy();

        virtual_tests_array_list(allocator);

        ok::free(allocator, buf);
    }
}
