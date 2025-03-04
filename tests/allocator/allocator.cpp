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

  private:
    struct default_construct_t
    {
        template <typename...> using associated_type = test;
        constexpr auto operator()() const noexcept { return test{}; }
    };

  public:
    inline static constexpr default_construct_t default_construct;

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
void virtual_tests_1mb_a(allocator_t& ally)
{
    // alloc a bunch of stuff, every allocator should be capable of at least
    // this
    alloc::owned array_on_heap =
        ally.make(array::undefined<int, 500>).release();

    auto array_on_stack = array::undefined<int, 500>();

    auto int_array = ally.make(array::defaulted_or_zeroed<int, 50>).release();

    ok::array_t deduction_test = array::defaulted_or_zeroed<int, 500>();

    if (ally.features() & alloc::feature_flags::can_only_alloc) {
    }
}

void virtual_tests_array_list(allocator_t& ally)
{
    ok::array_t array = {1, 2, 3, 4};

    ok::arraylist_t arraylist =
        ok::make(ok::arraylist::copy_items_from_range, ally, array).release();
    ok::arraylist_t arraylist2 =
        ok::arraylist::copy_items_from_range.make(ally, array).release();

    ok::array_t zeroed = ok::array::defaulted_or_zeroed<int, 5>();
    zeroed = ok::make(ok::array::defaulted_or_zeroed<int, 5>);

    ok::array_t undefined = ok::array::undefined<int, 5>();
    undefined = ok::make(ok::array::undefined<int, 5>);

    auto alist1 = ok::arraylist::empty<int>(ally);

    arraylist_t alist = ok::make(ok::arraylist::empty<int>, ally);

    auto test = ok::arraylist::spots_preallocated<int>.make(ally, 5);
    arraylist_t alist2 =
        ok::make(ok::arraylist::spots_preallocated<int>, ally, 5).release();

    static_assert(is_fallible_constructible_v<
                  ok::arraylist_t<int, ok::allocator_t>,
                  decltype(ok::arraylist::spots_preallocated<int>),
                  decltype(ally), size_t>);
    {
        alloc::owned array2d_on_heap =
            ally.make(ok::arraylist::spots_preallocated<int>, ally, 50UL)
                .release();
    }

    arraylist_t empty_on_stack = arraylist::empty<int>(ally);

    if (!empty_on_stack.append(1).okay()) {
        __ok_assert(false, "couldnt append");
        return;
    }

    auto arraylist3 =
        ok::make(arraylist::copy_items_from_range, ally, array).release();

    ok_foreach(ok_pair(a, al), zip(array, arraylist)) { REQUIRE(a == al); }
}

TEST_SUITE("abstract allocator")
{
    TEST_CASE("c allocator implements interface")
    {
        c_allocator_t allocator;
        // TODO: support allocating c style arrays with allocator_t::make()
        // auto& buf = allocator.make<test[100]>().release();
        auto& buf = allocator.make(array::undefined<test, 100>)
                        .release()
                        .into_non_owning_ref();

        arena_t arena(reinterpret_as_bytes(slice_t(buf)));

        alloc::owned mytest = arena.make(test::default_construct).release();
        test& mytest2 =
            arena.make(test::default_construct).release().into_non_owning_ref();
        test& mytest3 = arena.make_non_owning<test>(1).release();
        test& mytest4 = arena.make_non_owning<test>(1).release();
        test& test2 = arena.make_non_owning<test>(1).release();
        test& test3 = arena.make_non_owning<test>(test2).release();

        arena.destroy();

        virtual_tests_array_list(allocator);

        ok::free(allocator, buf);
    }
}
