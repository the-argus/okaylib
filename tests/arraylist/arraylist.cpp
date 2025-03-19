#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/reserving_page_allocator.h"
#include "okay/allocators/slab_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/arraylist.h"
#include "okay/macros/foreach.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/take_at_most.h"
#include "okay/ranges/views/zip.h"

using namespace ok;

struct ooming_allocator_t : ok::allocator_t
{
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        alloc::feature_flags::can_reclaim | alloc::feature_flags::is_threadsafe;

    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final
    {
        return alloc::error::oom;
    }

    inline void impl_clear() OKAYLIB_NOEXCEPT final {}

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final
    {
        __ok_abort("how did you get here");
    }

    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final
    {
        __ok_abort("how did you get here");
        return alloc::error::oom;
    }

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final
    {
        __ok_abort("how did you get here");
        return alloc::error::oom;
    }
};

template <typename backing_allocator_t>
auto make_slab(backing_allocator_t& allocator)
{
    return

        slab_allocator::with_blocks(
            allocator,
            slab_allocator::options_t<4>{
                .available_blocksizes =
                    {
                        slab_allocator::blocks_description_t{.blocksize = 64,
                                                             .alignment = 16},
                        slab_allocator::blocks_description_t{.blocksize = 256,
                                                             .alignment = 16},
                        slab_allocator::blocks_description_t{.blocksize = 1024,
                                                             .alignment = 16},
                        slab_allocator::blocks_description_t{
                            .blocksize = 100000, .alignment = 16},
                    },
                .num_initial_blocks_per_blocksize = 1,
            })
            .release();
}

TEST_SUITE("arraylist")
{
    TEST_CASE("initialization with different allocators")
    {
        c_allocator_t malloc;

        reserving_page_allocator_t reserving({.pages_reserved = 100000000});

        auto slab = make_slab(reserving);

        SUBCASE("reserved buffer")
        {
            arraylist_t i = arraylist::empty<int>(reserving);
            arraylist_t j =
                arraylist::spots_preallocated<int>(reserving, 50).release();
            // tons of zeroed ints
            array_t arr = array::defaulted_or_zeroed<int, 500>();
            arraylist_t k =
                arraylist::copy_items_from_range(reserving, arr).release();
        }

        SUBCASE("c allocator")
        {
            arraylist_t i = arraylist::empty<int>(malloc);
            arraylist_t j =
                arraylist::spots_preallocated<int>(malloc, 50).release();
            // tons of zeroed ints
            array_t arr = array::defaulted_or_zeroed<int, 500>();
            arraylist_t k =
                arraylist::copy_items_from_range(malloc, arr).release();
        }

        SUBCASE("slab allocator")
        {
            arraylist_t i = arraylist::empty<int>(slab);
            arraylist_t j =
                arraylist::spots_preallocated<int>(slab, 50).release();
            // tons of zeroed ints
            array_t arr = array::defaulted_or_zeroed<int, 500>();
            arraylist_t k =
                arraylist::copy_items_from_range(slab, arr).release();
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
                arraylist::copy_items_from_range(backing, example).release();
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
                arraylist::copy_items_from_range(backing, example).release();
            arraylist_t j =
                arraylist::copy_items_from_range(backing, example).release();
            j = std::move(i);
        }
    }

    TEST_CASE("items() and size() and data()")
    {
        ok::c_allocator_t allocator;
        ok::array_t arr{1, 2, 3, 4, 5};
        auto listres = arraylist::copy_items_from_range(allocator, arr);
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

    TEST_CASE(
        "basic nonfailing append usage to test reallocation, many allocators")
    {
        c_allocator_t backing;
        SUBCASE("c allocator")
        {
            arraylist_t dup = arraylist::empty<int>(backing);
            arraylist_t dup2 = arraylist::empty<int>(backing);

            for (size_t i = 0; i < 4097; ++i) {
                auto result = dup.append(i);
                REQUIRE(result.okay());
            }

            // throw in move assignment for good measure, maybe reallocation
            // broke some other invariants
            dup2 = std::move(dup);

            for (size_t i = 0; i < 4097; ++i) {
                REQUIRE(dup2[i] == i);
            }
        }

        SUBCASE("slab allocator")
        {
            auto slab = make_slab(backing);
            arraylist_t dup = arraylist::empty<int>(slab);
            arraylist_t dup2 = arraylist::empty<int>(slab);

            for (size_t i = 0; i < 4097; ++i) {
                auto result = dup.append(i);
                REQUIRE(result.okay());
            }

            dup2 = std::move(dup);

            for (size_t i = 0; i < 4097; ++i) {
                REQUIRE(dup2[i] == i);
            }
        }

        SUBCASE("reserved buffer")
        {
            reserving_page_allocator_t reserving({.pages_reserved = 8});
            arraylist_t dup = arraylist::empty<int>(reserving);
            arraylist_t dup2 = arraylist::empty<int>(reserving);

            for (size_t i = 0; i < 4097; ++i) {
                auto result = dup.append(i);
                REQUIRE(result.okay());
            }

            dup2 = std::move(dup);

            for (size_t i = 0; i < 4097; ++i) {
                REQUIRE(dup2[i] == i);
            }
        }
    }

    TEST_CASE("append failing constructors")
    {
        SUBCASE("arraylist of arraylist, copy from range constructor")
        {
            c_allocator_t backing;

            array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, c_allocator_t>>(backing);

            auto result = alist.append(arraylist::copy_items_from_range,
                                       backing, sub_array);
            REQUIRE(result.okay());
            REQUIRE(alist.size() == 1);

            ok_foreach(ok_pair(lhs, rhs), zip(alist[0], sub_array))
            {
                REQUIRE(lhs == rhs);
            }
        }

        SUBCASE(
            "arraylist of arraylist, with failing allocator on inner arraylist")
        {
            c_allocator_t main_backing;

            ooming_allocator_t failing;

            array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, ooming_allocator_t>>(
                    main_backing);

            auto result = alist.append(arraylist::copy_items_from_range,
                                       failing, sub_array);
            REQUIRE(!result.okay());
            REQUIRE(alist.size() == 0);
        }
    }

    TEST_CASE("insert_at")
    {
        SUBCASE("insert_at succeeds and the size of the container is bigger")
        {
            c_allocator_t backing;
            arraylist_t nums = arraylist::copy_items_from_range(
                                   backing, indices | take_at_most(10))
                                   .release();

            REQUIRE(nums.size() == 10);

            auto insert_begin_result = nums.insert_at(0, 0);
            REQUIRE(insert_begin_result.okay());
            REQUIRE(nums.size() == 11);

            auto insert_middle_result = nums.insert_at(5, 0);
            REQUIRE(insert_middle_result.okay());
            REQUIRE(nums.size() == 12);

            auto insert_end_result = nums.insert_at(nums.size(), 0);
            REQUIRE(insert_end_result.okay());
            REQUIRE(nums.size() == 13);
        }

        SUBCASE("insert_at aborts if inserting out of bounds")
        {
            c_allocator_t backing;
            arraylist_t nums = arraylist::copy_items_from_range(
                                   backing, indices | take_at_most(10))
                                   .release();

            REQUIREABORTS(auto&& _ = nums.insert_at(50, 1));
            REQUIREABORTS(auto&& _ = nums.insert_at(11, 1));
            auto res = nums.insert_at(10, 11);
            REQUIRE(res.okay());
        }

        SUBCASE(
            "after being moved by insert_at, values of arraylist are preserved")
        {
            c_allocator_t backing;
            ok::array_t initial_state = {0, 2, 4, 6, 8};
            arraylist_t nums =
                arraylist::copy_items_from_range(backing, initial_state)
                    .release();

            auto require_nums_is_equal_to = [&nums](const auto& new_range) {
                // require that every item in initial is equal to nums
                ok_foreach(ok_pair(other, num), zip(new_range, nums))
                {
                    REQUIRE(other == num);
                }
            };

            require_nums_is_equal_to(initial_state);

            {
                auto res = nums.insert_at(1, 1);
                REQUIRE(res.okay());
            }

            require_nums_is_equal_to(ok::array_t{0, 1, 2, 4, 6, 8});

            REQUIRE(nums.insert_at(3, 3).okay());

            require_nums_is_equal_to(ok::array_t{0, 1, 2, 3, 4, 6, 8});

            REQUIRE(nums.insert_at(5, 5).okay());

            require_nums_is_equal_to(ok::array_t{0, 1, 2, 3, 4, 5, 6, 8});

            REQUIRE(nums.insert_at(7, 7).okay());

            require_nums_is_equal_to(ok::array_t{0, 1, 2, 3, 4, 5, 6, 8});

            REQUIRE(nums.insert_at(0, 42).okay());

            require_nums_is_equal_to(ok::array_t{42, 0, 1, 2, 3, 4, 5, 6, 8});
        }

        SUBCASE("insert_at with copy from range constructor")
        {
            c_allocator_t backing;

            array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, c_allocator_t>>(backing);

            auto result = alist.insert_at(0, arraylist::copy_items_from_range,
                                          backing, sub_array);
            REQUIRE(result.okay());
            REQUIRE(alist.size() == 1);

            ok_foreach(ok_pair(lhs, rhs), zip(alist[0], sub_array))
            {
                REQUIRE(lhs == rhs);
            }
        }

        SUBCASE(
            "arraylist of arraylist, with failing allocator on inner arraylist")
        {
            c_allocator_t main_backing;

            ooming_allocator_t failing;

            array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, ooming_allocator_t>>(
                    main_backing);

            auto result = alist.insert_at(0, arraylist::copy_items_from_range,
                                          failing, sub_array);
            REQUIRE(!result.okay());
            REQUIRE(alist.size() == 0);
        }
    }
}
