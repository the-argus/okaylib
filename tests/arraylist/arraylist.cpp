#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/reserving_page_allocator.h"
#include "okay/allocators/slab_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/arraylist.h"
#include "okay/macros/foreach.h"
#include "okay/ranges/views/zip.h"

using namespace ok;

TEST_SUITE("arraylist")
{
    TEST_CASE("initialization with different allocators")
    {
        c_allocator_t malloc;

        reserving_page_allocator_t reserving({.pages_reserved = 100000000});

        slab_allocator_t slab =
            slab_allocator::with_blocks
                .make(reserving,
                      slab_allocator::options_t<4>{
                          .available_blocksizes =
                              {
                                  slab_allocator::blocks_description_t{
                                      .blocksize = 64, .alignment = 16},
                                  slab_allocator::blocks_description_t{
                                      .blocksize = 256, .alignment = 16},
                                  slab_allocator::blocks_description_t{
                                      .blocksize = 1024, .alignment = 16},
                                  slab_allocator::blocks_description_t{
                                      .blocksize = 100000, .alignment = 16},
                              },
                          .num_initial_blocks_per_blocksize = 1,
                      })
                .release();

        SUBCASE("reserved buffer")
        {
            arraylist_t i = arraylist::empty<int>(reserving);
            arraylist_t j =
                arraylist::spots_preallocated<int>.make(reserving, 50).release();
            // tons of zeroed ints
            array_t arr = array::defaulted_or_zeroed<int, 500>();
            arraylist_t k =
                arraylist::copy_items_from_range.make(reserving, arr).release();
        }

        SUBCASE("c allocator")
        {
            arraylist_t i = arraylist::empty<int>(malloc);
            arraylist_t j =
                arraylist::spots_preallocated<int>.make(malloc, 50).release();
            // tons of zeroed ints
            array_t arr = array::defaulted_or_zeroed<int, 500>();
            arraylist_t k =
                arraylist::copy_items_from_range.make(malloc, arr).release();
        }

        SUBCASE("slab allocator")
        {
            arraylist_t i = arraylist::empty<int>(slab);
            arraylist_t j =
                arraylist::spots_preallocated<int>.make(slab, 50).release();
            // tons of zeroed ints
            array_t arr = array::defaulted_or_zeroed<int, 500>();
            arraylist_t k =
                arraylist::copy_items_from_range.make(slab, arr).release();
        }
    }

    TEST_CASE("move semantics")
    {
        c_allocator_t backing;
        array_t example{1, 2, 3, 4, 5};

        SUBCASE(
            "move construction causes right number of destructions with empty")
        {
            arraylist_t i = arraylist::empty<int>(backing);
            arraylist_t j = std::move(i);
        }

        SUBCASE(
            "move construction causes right number of destructions with full")
        {
            arraylist_t i =
                arraylist::copy_items_from_range.make(backing, example)
                    .release();
            arraylist_t j = std::move(i);
        }

        SUBCASE(
            "move assignment causes right number of destructions with empty")
        {
            arraylist_t i = arraylist::empty<int>(backing);
            arraylist_t j = arraylist::empty<int>(backing);
            j = std::move(i);
        }

        SUBCASE("move assignment causes right number of destructions with full")
        {
            arraylist_t i =
                arraylist::copy_items_from_range.make(backing, example)
                    .release();
            arraylist_t j =
                arraylist::copy_items_from_range.make(backing, example)
                    .release();
            j = std::move(i);
        }
    }

    TEST_CASE("items() and size() and data()")
    {
        ok::c_allocator_t allocator;
        ok::array_t arr{1, 2, 3, 4, 5};
        auto listres = arraylist::copy_items_from_range.make(allocator, arr);
        auto& list = listres.release_ref();

        SUBCASE("items matches direct iteration")
        {
            ok_foreach(ok_decompose(original, direct, itemsiter),
                       zip(arr, list, list.items()))
            {
                REQUIRE(original == direct);
                REQUIRE(direct == itemsiter);
            }
        }

        SUBCASE("items size and data matches direct size and data")
        {
            REQUIRE(list.data() == list.items().data());
            static_assert(std::is_same_v<decltype(list.data()),
                                         decltype(list.items().data())>);
            REQUIRE(list.size() == list.items().size());
        }

        SUBCASE("items size matches direct size and original size")
        {
            auto original_size = arr.size();
            auto direct_size = list.size();
            auto items_size = list.size();

            REQUIRE(original_size == direct_size);
            REQUIRE(direct_size == items_size);
        }

        SUBCASE("can call size on empty arraylist but not data or items")
        {
            arraylist_t alist = arraylist::empty<int>(allocator);
            const auto& const_alist = alist;

            REQUIRE(alist.size() == 0);
            REQUIREABORTS(alist.data());
            REQUIREABORTS(const_alist.data());
            REQUIREABORTS(alist.items());
            REQUIREABORTS(const_alist.items());
        }
    }
}
