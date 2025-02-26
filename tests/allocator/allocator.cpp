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

struct child
{
  private:
    struct childimpl
    {
        float a;
        float b;
        void* allocation;
    };
    childimpl impl;

  public:
    struct options
    {
        using out_error_type = status_t<alloc::error>;
        float a;
        float b;
        size_t size;
    };

    using impl_type = childimpl;

    child(const options& options, options::out_error_type& outerr)
    {
        void* malloced = malloc(options.size);
        if (!malloced) {
            outerr = alloc::error::oom;
            return;
        }

        this->impl = {
            .a = options.a,
            .b = options.b,
            .allocation = malloced,
        };

        outerr = alloc::error::okay;
    }

    child(const child&) = delete;
    child& operator=(const child&) = delete;
    child(child&&) = delete;
    child& operator=(child&&) = delete;

    ~child() { free(impl.allocation); }
};

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
void virtual_tests_1mb_a(allocator_t& ally)
{
    // alloc a bunch of stuff, every allocator should be capable of at least
    // this
    auto& array_on_heap =
        ally.make_using_factory(array_t<int, 500>::make::undefined).release();

    auto array_on_stack = array_t<int, 500>::make::undefined();

    auto int_array =
        ally.make_using_factory(array_t<int, 50>::make::default_all).release();

    ok::array_t deduction_test = array_t<int, 500>::make::default_all();

    if (ally.features() & alloc::feature_flags::can_only_alloc) {
    }
}

void virtual_tests_array_list(allocator_t& ally)
{
    ok::array_t array = {1, 2, 3, 4};

    ok::array_t zeroed = array_t<int, 5>::make::default_all();

    auto undefined = array_t<int, 5>::make::undefined();

    using T = arraylist_t<int>;

    {
        auto& array2d_on_heap =
            ally.make<arraylist_t<arraylist_t<int>>, spots_preallocated_tag>(
                    ally, 50UL)
                .release();

        ok::free(ally, array2d_on_heap);
    }

    arraylist_t empty_on_stack = ok::make<arraylist_t<int>, empty_tag>(ally);

    if (!empty_on_stack.append(1).okay()) {
        __ok_assert(false, "couldnt append");
        return;
    }

    auto arraylist =
        ok::make<arraylist_t<int>, copy_items_from_range_tag>(ally, array)
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
            allocator.make_using_factory(array_t<test, 100>::make::undefined)
                .release();

        arena_t arena(reinterpret_as_bytes(slice_t(buf)));

        test& mytest = arena.make<test>().release();
        test& test2 = arena.make<test>(1).release();
        test& test3 = arena.make<test>(mytest).release();

        arena.destroy();

        virtual_tests_array_list(allocator);

        ok::free(allocator, buf);
    }
}
