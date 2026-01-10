#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/reserving_page_allocator.h"
#include "okay/allocators/slab_allocator.h"
#include "okay/containers/array.h"
#include "okay/containers/arraylist.h"
#include "okay/iterables/indices.h"

using namespace ok;

struct ooming_allocator_t : ok::allocator_t
{
    bool should_oom = true;
    ok::allocator_t* backing_actual = nullptr;

    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_reclaim;

    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT final
    {
        if (should_oom)
            return alloc::error::oom;

        return backing_actual->allocate(request);
    }

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        if (!backing_actual)
            return type_features;

        return backing_actual->features();
    }

    inline void impl_deallocate(void* memory,
                                size_t size_hint) OKAYLIB_NOEXCEPT final
    {
        if (backing_actual)
            backing_actual->deallocate(memory, size_hint);
    }

    [[nodiscard]] inline alloc::result_t<bytes_t> impl_reallocate(
        const alloc::reallocate_request_t& request) OKAYLIB_NOEXCEPT final
    {
        if (should_oom || !backing_actual)
            return alloc::error::oom;

        return backing_actual->reallocate(request);
    }
};

struct destruction_counting
{
    size_t* counter;
    destruction_counting() = delete;
    destruction_counting(size_t* c) : counter(c)
    {
        __ok_internal_assert(counter);
    }
    destruction_counting& operator=(destruction_counting&& other)
    {
        other.counter = nullptr;
        return *this;
    }
    destruction_counting(destruction_counting&& other)
    {
        other.counter = nullptr;
    }
    ~destruction_counting() { ++(*counter); }
};

struct trivial_with_failing_construction
{
    int* contents;

    struct failing_construction
    {
        static constexpr auto implemented_make_function =
            ok::implemented_make_function::make_into_uninit;

        template <typename...>
        using associated_type = trivial_with_failing_construction;

        constexpr alloc::error
        make_into_uninit(trivial_with_failing_construction& uninit,
                         ok::allocator_t& allocator, int initial_value) const
        {
            auto res = allocator.make_non_owning<int>(initial_value);
            if (res.is_success()) {
                uninit.contents = &res.unwrap();
                return alloc::error::success;
            }
            return res.status();
        }
    };
};

template <typename backing_allocator_t>
auto make_slab(backing_allocator_t& allocator)
{
    constexpr auto options = slab_allocator::options_t<4>{
        .available_blocksizes =
            {
                slab_allocator::blocks_description_t{.blocksize = 64,
                                                     .alignment = 16},
                slab_allocator::blocks_description_t{.blocksize = 256,
                                                     .alignment = 16},
                slab_allocator::blocks_description_t{.blocksize = 1024,
                                                     .alignment = 16},
                slab_allocator::blocks_description_t{.blocksize = 100000,
                                                     .alignment = 16},
            },
        .num_initial_blocks_per_blocksize = 1,
    };

    return slab_allocator::with_blocks(allocator, options).unwrap();
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
                arraylist::spots_preallocated<int>(reserving, 50).unwrap();
            // tons of zeroed ints
            zeroed_array_t<int, 500> arr;
            arraylist_t k =
                arraylist::copy_items_from_iterator(reserving, iter(arr))
                    .unwrap();
        }

        SUBCASE("c allocator")
        {
            arraylist_t i = arraylist::empty<int>(malloc);
            arraylist_t j =
                arraylist::spots_preallocated<int>(malloc, 50).unwrap();
            // tons of zeroed ints
            zeroed_array_t<int, 500> arr;
            arraylist_t k =
                arraylist::copy_items_from_iterator(malloc, iter(arr)).unwrap();
        }

        SUBCASE("slab allocator")
        {
            arraylist_t i = arraylist::empty<int>(slab);
            arraylist_t j =
                arraylist::spots_preallocated<int>(slab, 50).unwrap();
            // tons of zeroed ints
            zeroed_array_t<int, 500> arr;
            arraylist_t k =
                arraylist::copy_items_from_iterator(slab, iter(arr)).unwrap();
        }
    }

    TEST_CASE("move semantics")
    {
        c_allocator_t backing;
        maybe_undefined_array_t example{1, 2, 3, 4, 5};

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
                arraylist::copy_items_from_iterator(backing, iter(example))
                    .unwrap();
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
                arraylist::copy_items_from_iterator(backing, iter(example))
                    .unwrap();
            arraylist_t j =
                arraylist::copy_items_from_iterator(backing, iter(example))
                    .unwrap();
            j = std::move(i);
        }
    }

    TEST_CASE("items() and size() and data()")
    {
        ok::c_allocator_t allocator;
        ok::maybe_undefined_array_t arr{1, 2, 3, 4, 5};
        auto listres =
            arraylist::copy_items_from_iterator(allocator, iter(arr));
        auto& list = listres.unwrap();

        SUBCASE("items matches direct iteration")
        {
            bool all_three_equal = iterators_equal(arr, list) &&
                                   iterators_equal(list, list.items());
            REQUIRE(all_three_equal);
        }

        SUBCASE("items size matches direct size")
        {
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

        SUBCASE(
            "can call size and items on empty arraylist regardless of const")
        {
            arraylist_t alist = arraylist::empty<int>(allocator);
            const auto& const_alist = alist;

            REQUIRE(alist.size() == 0);
            {
                auto _ = alist.items();
            }
            {
                auto _ = const_alist.items();
            }
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
                REQUIRE(result.is_success());
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
                REQUIRE(result.is_success());
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
                REQUIRE(result.is_success());
            }

            dup2 = std::move(dup);

            bool eql = iterators_equal(take_at_most(dup2, 4097),
                                       ok::indices().take_at_most(4097));
            REQUIRE(eql);
        }
    }

    TEST_CASE("append failing constructors")
    {
        SUBCASE("arraylist of arraylist, copy from range constructor")
        {
            c_allocator_t backing;

            maybe_undefined_array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, c_allocator_t>>(backing);

            auto result = alist.append(arraylist::copy_items_from_iterator,
                                       backing, iter(sub_array));
            REQUIRE(result.is_success());
            REQUIRE(alist.size() == 1);
            bool eql = iterators_equal(alist[0], sub_array);
            REQUIRE(eql);
        }

        SUBCASE(
            "arraylist of arraylist, with failing allocator on inner arraylist")
        {
            c_allocator_t working_allocator;

            ooming_allocator_t failing_allocator;

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, ooming_allocator_t>>(
                    working_allocator);

            auto result = alist.append(
                arraylist::copy_items_from_iterator, failing_allocator,
                iter(maybe_undefined_array_t{1, 2, 3, 4, 5, 6}));
#ifdef OKAYLIB_USE_FMT
            fmt::println("Tried to create a new array inside of `alist`, got "
                         "return code {}",
                         result);
            fmt::println("Size of `alist`: {}", alist.size());
#endif
        }
    }

    TEST_CASE("insert_at")
    {
        SUBCASE("insert_at succeeds and the size of the container is bigger")
        {
            c_allocator_t backing;
            arraylist_t nums = arraylist::copy_items_from_iterator(
                                   backing, indices().take_at_most(10))
                                   .unwrap();

            REQUIRE(nums.size() == 10);

            auto insert_begin_result = nums.insert_at(0, 0);
            REQUIRE(insert_begin_result.is_success());
            REQUIRE(nums.size() == 11);

            auto insert_middle_result = nums.insert_at(5, 0);
            REQUIRE(insert_middle_result.is_success());
            REQUIRE(nums.size() == 12);

            auto insert_end_result = nums.insert_at(nums.size(), 0);
            REQUIRE(insert_end_result.is_success());
            REQUIRE(nums.size() == 13);
        }

        SUBCASE("insert_at aborts if inserting out of bounds")
        {
            c_allocator_t backing;
            arraylist_t nums = arraylist::copy_items_from_iterator(
                                   backing, indices().take_at_most(10))
                                   .unwrap();

            REQUIREABORTS(auto&& _ = nums.insert_at(12, 1));
            REQUIREABORTS(auto&& _ = nums.insert_at(13, 1));
            REQUIREABORTS(auto&& _ = nums.insert_at(50, 1));
            auto res = nums.insert_at(10, 11);
            REQUIRE(res.is_success());
        }

        SUBCASE(
            "after being moved by insert_at, values of arraylist are preserved")
        {
            c_allocator_t backing;
            ok::maybe_undefined_array_t initial_state = {0, 2, 4, 6, 8};
            arraylist_t nums = arraylist::copy_items_from_iterator(
                                   backing, iter(initial_state))
                                   .unwrap();

            auto require_nums_is_equal_to = [&nums](const auto& new_range) {
                REQUIRE_RANGES_EQUAL(nums, new_range);
            };

            require_nums_is_equal_to(initial_state);

            {
                auto res = nums.insert_at(1, 1);
                REQUIRE(res.is_success());
            }

            require_nums_is_equal_to(
                ok::maybe_undefined_array_t{0, 1, 2, 4, 6, 8});

            REQUIRE(nums.insert_at(3, 3).is_success());

            require_nums_is_equal_to(
                ok::maybe_undefined_array_t{0, 1, 2, 3, 4, 6, 8});

            REQUIRE(nums.insert_at(5, 5).is_success());

            require_nums_is_equal_to(
                ok::maybe_undefined_array_t{0, 1, 2, 3, 4, 5, 6, 8});

            REQUIRE(nums.insert_at(7, 7).is_success());

            require_nums_is_equal_to(
                ok::maybe_undefined_array_t{0, 1, 2, 3, 4, 5, 6, 7, 8});

            REQUIRE(nums.insert_at(0, 42).is_success());

            require_nums_is_equal_to(
                ok::maybe_undefined_array_t{42, 0, 1, 2, 3, 4, 5, 6, 7, 8});
        }

        SUBCASE("insert_at with copy from range constructor")
        {
            c_allocator_t backing;

            maybe_undefined_array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, c_allocator_t>>(backing);

            auto result =
                alist.insert_at(0, arraylist::copy_items_from_iterator, backing,
                                iter(sub_array));
            REQUIRE(result.is_success());
            REQUIRE(alist.size() == 1);
            REQUIRE_RANGES_EQUAL(alist[0], sub_array);
        }

        SUBCASE(
            "arraylist of arraylist, with failing allocator on inner arraylist")
        {
            c_allocator_t main_backing;

            ooming_allocator_t failing;

            maybe_undefined_array_t sub_array{1, 2, 3, 4, 5, 6};

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, ooming_allocator_t>>(
                    main_backing);

            auto result =
                alist.insert_at(0, arraylist::copy_items_from_iterator, failing,
                                iter(sub_array));
            REQUIRE(!result.is_success());
            REQUIRE(alist.size() == 0);
        }

        SUBCASE("insert_at failing and having to restore the items")
        {
            c_allocator_t main_backing;

            ooming_allocator_t failing;
            failing.backing_actual = &main_backing;
            failing.should_oom = false;

            // empty with c allocator
            arraylist_t alist =
                arraylist::empty<arraylist_t<int, ooming_allocator_t>>(
                    main_backing);

            // insert some copies of sub_array
            maybe_undefined_array_t sub_array{1, 2, 3, 4, 5, 6};
            auto status =
                alist.insert_at(0, arraylist::copy_items_from_iterator, failing,
                                iter(sub_array));
            REQUIRE(status.is_success());
            REQUIRE(alist.size() == 1);

            for (size_t i = 0; i < 30; ++i) {
                status = alist.insert_at(0, arraylist::copy_items_from_iterator,
                                         failing, iter(sub_array));
                REQUIRE(status.is_success());
            }

            // make some items distinguishable so we can test if they moved
            // around properly
            alist[0] = arraylist::empty<int>(failing);
            REQUIRE(alist[0].is_empty());
            alist[0].append(0);
            alist[0].append(1);
            REQUIRE(!alist[0].is_empty());

            REQUIRE(alist.size() == 31);

            for (auto& innerlist : iter(alist).drop(1)) {
                REQUIRE(innerlist.size() == sub_array.size());
            }

            // now have a failing allocator call
            failing.should_oom = true;
            maybe_undefined_array_t different_sub_array{1, 2, 3};
            status = alist.insert_at(0, arraylist::copy_items_from_iterator,
                                     failing, iter(sub_array));
            REQUIRE(!status.is_success());
            REQUIRE(alist.size() == 31);

            // make sure all elements past 0 are the same as sub_array

            iter(alist).drop(1).for_each([&](auto& sub_arraylist) {
                REQUIRE_RANGES_EQUAL(sub_arraylist, sub_array);
            });

            REQUIRE(alist[0].size() == 2);
            REQUIRE(alist[0][0] == 0);
            REQUIRE(alist[0][1] == 1);
        }

        SUBCASE("insert_at failing and having to restore the items with a "
                "trivially copyable type")
        {
            static_assert(std::is_trivially_copyable_v<
                          trivial_with_failing_construction>);
            c_allocator_t main_backing;
            ooming_allocator_t failing;
            failing.backing_actual = &main_backing;
            failing.should_oom = false;

            arraylist_t alist =
                arraylist::empty<trivial_with_failing_construction>(
                    main_backing);

            REQUIRE(alist.is_empty());

            constexpr auto constructor =
                trivial_with_failing_construction::failing_construction{};
            for (size_t i = 0; i < 5; ++i) {
                auto result = alist.insert_at(0, constructor, failing, i);
                REQUIRE(result.is_success());
                REQUIRE(!alist.is_empty());
            }

            failing.should_oom = true;

            auto result = alist.insert_at(0, constructor, failing, 0);
            REQUIRE(!result.is_success());

            // everything still normal
            REQUIRE(*alist[0].contents == 4);
            REQUIRE(*alist[1].contents == 3);
            REQUIRE(*alist[2].contents == 2);
            REQUIRE(*alist[3].contents == 1);
            REQUIRE(*alist[4].contents == 0);

            // free all the ints that were validated
            for (const auto& intptr_wrapper : iter(alist)) {
                main_backing.deallocate(intptr_wrapper.contents);
            }
        }
    }

    TEST_CASE("capacity / reallocation operations")
    {
        SUBCASE("capacity() getter and evident 2x grow rate")
        {
            c_allocator_t backing;
            arraylist_t alist = arraylist::empty<int>(backing);
            REQUIRE(alist.is_empty());
            REQUIRE(alist.capacity() == 0);

            // some implementation details of the arraylist
            constexpr float grow_factor = 2;
            constexpr size_t initial_size = 4;

            for (size_t i = 0; i < initial_size; ++i) {
                REQUIRE(alist.size() == i);
                alist.append(i);
            }
            REQUIRE(alist.capacity() == initial_size);
            alist.append(int(initial_size));
            REQUIRE(alist.size() == initial_size + 1);
            REQUIRE(alist.capacity() == size_t(std::round(4 * grow_factor)));
        }
    }

    TEST_CASE("clear()")
    {
        SUBCASE("clearing decreases size to zero")
        {
            c_allocator_t backing;
            arraylist_t alist = arraylist::empty<int>(backing);

            // clearing on empty is fine
            {
                size_t cap_before = alist.capacity();
                REQUIRE(alist.size() == 0);
                alist.clear();
                REQUIRE(alist.size() == 0);
                REQUIRE(alist.capacity() == cap_before);
            }

            // push then clear
            auto _ = alist.append(0);
            _ = alist.append(1);
            _ = alist.append(2);
            _ = alist.append(3);
            REQUIRE(alist.size() == 4);
            size_t cap_before = alist.capacity();
            alist.clear();
            REQUIRE(alist.size() == 0);
            REQUIRE(alist.capacity() == cap_before);
        }

        SUBCASE("clearing calls destructors")
        {
            c_allocator_t backing;
            size_t counter = 0;
            arraylist_t alist = arraylist::empty<destruction_counting>(backing);

            auto _ = alist.append(&counter);
            _ = alist.append(&counter);
            _ = alist.append(&counter);
            _ = alist.append(&counter);

            REQUIRE(counter == 0);

            size_t num_items = alist.size();
            alist.clear();

            REQUIRE(counter == num_items);
        }
    }

    TEST_CASE("remove() and pop_last()")
    {
        SUBCASE("remove with trivial objects")
        {
            c_allocator_t backing;
            ok::maybe_undefined_array_t initial = {0, 1, 2, 2, 3, 4,
                                                   4, 5, 6, 7, 7, 8};
            arraylist_t alist =
                arraylist::copy_items_from_iterator(backing, iter(initial))
                    .unwrap();

            REQUIRE(!ok::iterators_equal(alist, ok::indices()));

            REQUIRE(alist.remove(2) == 2);
            REQUIRE(alist.remove(4) == 4);
            REQUIRE(alist.remove(7) == 7);

            REQUIRE_RANGES_EQUAL(alist, ok::indices());
        }

        SUBCASE("remove with non-trivial objects")
        {
            c_allocator_t backing;

            arraylist_t alist =
                arraylist::empty<arraylist_t<int, c_allocator_t>>(backing);

            constexpr maybe_undefined_array_t initial = {1, 2, 3};

            // append three arraylists, each a copy of `initial`
            for (size_t i = 0; i < 3; ++i) {
                REQUIRE(alist
                            .append(arraylist::copy_items_from_iterator,
                                    backing, iter(initial))
                            .is_success());
            }

            REQUIRE(alist[0].remove(0) == 1); // alist[0] = {2, 3}
            REQUIRE(alist[1].remove(1) == 2); // alist[0] = {1, 3}
            REQUIRE(alist[2].remove(2) == 3); // alist[0] = {1, 2}

            arraylist_t out = alist.remove(1);
            REQUIRE(alist.size() == 2);
            REQUIRE_RANGES_EQUAL(out, iter(maybe_undefined_array_t{1, 3}));
            REQUIRE_RANGES_EQUAL(alist[0], iter(maybe_undefined_array_t{2, 3}));
            REQUIRE_RANGES_EQUAL(alist[1], iter(maybe_undefined_array_t{1, 2}));
        }

        SUBCASE("pop_last with trivial objects")
        {
            c_allocator_t backing;
            arraylist_t alist =
                arraylist::copy_items_from_iterator(
                    backing, iter(maybe_undefined_array_t{0, 1, 2, 3, 4}))
                    .unwrap();

            REQUIRE_RANGES_EQUAL(alist,
                                 iter(maybe_undefined_array_t{0, 1, 2, 3, 4}));
            REQUIRE(alist.pop_last().ref_or_panic() == 4);
            REQUIRE_RANGES_EQUAL(alist,
                                 iter(maybe_undefined_array_t{0, 1, 2, 3}));
            REQUIRE(alist.pop_last().ref_or_panic() == 3);
            REQUIRE_RANGES_EQUAL(alist, iter(maybe_undefined_array_t{0, 1, 2}));
            REQUIRE(alist.pop_last().ref_or_panic() == 2);
            REQUIRE_RANGES_EQUAL(alist, iter(maybe_undefined_array_t{0, 1}));
        }

        SUBCASE("pop_last with nontrivial objects")
        {
            c_allocator_t backing;
            arraylist_t alist =
                arraylist::empty<arraylist_t<int, c_allocator_t>>(backing);

            // append three arraylists, each a copy of `initial`
            for (size_t i = 0; i < 3; ++i) {
                alist.append(arraylist::copy_items_from_iterator, backing,
                             iter(maybe_undefined_array_t{1, 2, 3}));
            }
            REQUIRE(alist.size() == 3);

            REQUIRE_RANGES_EQUAL(iter(maybe_undefined_array_t{1, 2, 3}),
                                 alist.pop_last().ref_or_panic());
            REQUIRE(alist[1].pop_last().ref_or_panic() == 3);
            REQUIRE_RANGES_EQUAL(iter(maybe_undefined_array_t{1, 2}),
                                 alist.pop_last().ref_or_panic());
            REQUIRE(alist[0].pop_last().ref_or_panic() == 3);
            REQUIRE(alist[0].pop_last().ref_or_panic() == 2);
            REQUIRE_RANGES_EQUAL(iter(maybe_undefined_array_t{1}),
                                 alist.pop_last().ref_or_panic());
        }
    }

    TEST_CASE("remove_and_swap_last()")
    {
        SUBCASE("correct ordering")
        {
            c_allocator_t backing;
            arraylist_t alist =
                arraylist::copy_items_from_iterator(
                    backing,
                    iter(maybe_undefined_array_t{0, 6, 7, 3, 4, 5, 1, 2}))
                    .unwrap();

            REQUIRE(!iterators_equal(alist, indices()));
            REQUIRE(alist.remove_and_swap_last(2) == 7);
            REQUIRE_RANGES_EQUAL(
                alist, iter(maybe_undefined_array_t{0, 6, 2, 3, 4, 5, 1}));
            REQUIRE(alist.remove_and_swap_last(1) == 6);
            REQUIRE_RANGES_EQUAL(
                alist, iter(maybe_undefined_array_t{0, 1, 2, 3, 4, 5}));
            REQUIRE(alist.remove_and_swap_last(0) == 0);
            REQUIRE(alist.remove_and_swap_last(0) == 5);
            REQUIRE(alist.remove_and_swap_last(0) == 4);
            REQUIRE(alist.remove_and_swap_last(0) == 3);
            REQUIRE(alist.remove_and_swap_last(0) == 2);
            REQUIRE(alist.remove_and_swap_last(0) == 1);
            REQUIREABORTS(alist.remove_and_swap_last(0));
        }

        SUBCASE("still works after reallocation")
        {
            maybe_undefined_array_t initial = {0, 6, 7, 3, 4, 5, 1, 2};
            c_allocator_t backing;
            arraylist_t alist =
                arraylist::copy_items_from_iterator(backing, iter(initial))
                    .unwrap();
            REQUIRE(alist.capacity() == initial.size());
            REQUIRE(alist.capacity() == alist.size());

            alist.remove_and_swap_last(2);
            // no reallocation yet
            REQUIRE(alist.capacity() == alist.size() + 1);
            // ordering is preserved
            REQUIRE_RANGES_EQUAL(
                alist, iter(maybe_undefined_array_t{0, 6, 2, 3, 4, 5, 1}));

            // okay now reallocate
            auto status = alist.increase_capacity_by_at_least(100);
            const bool good =
                status.is_success() &&
                iterators_equal(alist,
                                maybe_undefined_array_t{0, 6, 2, 3, 4, 5, 1});
            REQUIRE(good);
            alist.remove_and_swap_last(1);
            REQUIRE_RANGES_EQUAL(alist, indices());
        }
    }

    TEST_CASE("shrink_and_leak()")
    {
        c_allocator_t backing;
        arraylist_t alist = arraylist::copy_items_from_iterator(
                                backing, indices().take_at_most(100))
                                .unwrap();

        for (size_t i = 0; i < 50; ++i) {
            alist.pop_last();
        }

        REQUIRE(alist.size() == 50);

        slice<size_t> items = alist.shrink_and_leak();

        REQUIRE(items.size() == 50);

        REQUIRE(alist.size() == 0);
        REQUIRE(alist.is_empty());

        indices().take_at_most(100).for_each(
            [&](size_t i) { alist.append(i); });

        backing.deallocate(items.unchecked_address_of_first_item());

        REQUIRE_RANGES_EQUAL(alist, indices());
    }

    TEST_CASE("resize()")
    {
        SUBCASE("resize zeroes trivially constructible stuff")
        {
            c_allocator_t backing;
            arraylist_t alist = arraylist::empty<int>(backing);

            auto&& _ = alist.resize(100);

            for (int i : iter(alist)) {
                REQUIRE(i == 0);
            }
        }

        SUBCASE("resize default constructs everything")
        {
            c_allocator_t backing;

            struct test_constructor
            {};
            struct bad_constructor
            {};

            struct Thing
            {
                int i;
                Thing() { i = 42; }
                Thing(test_constructor) { i = 20; }
                Thing(bad_constructor) { REQUIRE(false); }
            };

            arraylist_t alist = arraylist::empty<Thing>(backing);

            auto _ = alist.resize(100);

            for (Thing& t : iter(alist)) {
                REQUIRE(t.i == 42);
            }
            // resizing smaller never calls constructor
            _ = alist.resize(50, bad_constructor{});
            _ = alist.resize(0, bad_constructor{});

            _ = alist.resize(50);
            _ = alist.resize(100, test_constructor{});
            for (Thing i : iter(alist).take_at_most(50)) {
                REQUIRE(i.i == 42);
            }
            REQUIRE(alist.size() == 100);
            REQUIRE(ok::size(alist) == 100);
            REQUIRE(ok::size(iter(alist).drop(50)) == 50);
            for (Thing i : iter(alist).drop(50)) {
                REQUIRE(i.i == 20);
            }
        }

        SUBCASE("resize can call constructor")
        {
            c_allocator_t backing;
            arraylist_t alist = arraylist::empty<int>(backing);
            auto&& _ = alist.resize(100, 42);
            for (int& t : iter(alist)) {
                REQUIRE(t == 42);
            }

            _ = alist.resize(150, 32);

            for (int i : iter(alist).take_at_most(100)) {
                REQUIRE(i == 42);
            }
            for (int i : iter(alist).drop(100)) {
                REQUIRE(i == 32);
            }
        }

        SUBCASE("resize calls destructors when shrinking")
        {
            size_t counter = 0;
            c_allocator_t backing;
            {
                arraylist_t alist =
                    arraylist::empty<destruction_counting>(backing);

                auto&& _ = alist.resize(100, &counter);

                REQUIRE(counter == 0);
                alist.clear();
                REQUIRE(counter == 100);

                _ = alist.resize(100, &counter);
                REQUIRE(counter == 100);
                _ = alist.resize(50, &counter);
                REQUIRE(counter == 150);
            }
            REQUIRE(counter == 200);
        }
    }

    TEST_CASE("first() and last()")
    {
        c_allocator_t backing;
        arraylist_t alist = arraylist::empty<int>(backing);
        REQUIREABORTS(auto&& _ = alist.items().first());
        REQUIREABORTS(auto&& _ = alist.items().last());
        REQUIREABORTS(
            auto&& _ =
                static_cast<const decltype(alist)&>(alist).items().first());
        REQUIREABORTS(
            auto&& _ =
                static_cast<const decltype(alist)&>(alist).items().last());
        iter(maybe_undefined_array_t{0, 1, 2, 3}).for_each([&alist](int i) {
            auto&& _ = alist.append(i);
        });
        REQUIRE(alist.items().first() == 0);
        REQUIRE(alist.items().last() == 3);

        alist.items().first() = 1;
        REQUIRE_RANGES_EQUAL(alist, iter(maybe_undefined_array_t{1, 1, 2, 3}));
        alist.items().last() = 2;
        REQUIRE_RANGES_EQUAL(alist, iter(maybe_undefined_array_t{1, 1, 2, 2}));
        alist.items().first() = 0;
        alist.items().last() = 3;
        REQUIRE_RANGES_EQUAL(alist, indices());
    }

    TEST_CASE("shrink_to_reclaim_unused_memory()")
    {
        // only allocator which can both reclaim and realloc in place
        reserving_page_allocator_t backing({.pages_reserved = 100});
        size_t page_size = mmap::get_page_size();
        // two pages in size
        arraylist_t alist =
            arraylist::copy_items_from_iterator(
                backing, indices().take_at_most(2 * page_size / sizeof(size_t)))
                .unwrap();

        REQUIRE(alist.items().size_bytes() == 2 * page_size);
        REQUIRE(alist.capacity() == alist.size());

        for (size_t i = 0; i < (page_size / sizeof(size_t)); ++i) {
            alist.pop_last();
        }

        REQUIRE(alist.items().size_bytes() == page_size);
        REQUIRE(alist.capacity() == 2 * alist.size());

        alist.shrink_to_reclaim_unused_memory();
        REQUIRE(alist.capacity() == alist.size());
        REQUIRE_RANGES_EQUAL(alist, indices());
    }

    TEST_CASE("append_range()")
    {
        SUBCASE("sized range")
        {
            c_allocator_t backing;
            auto alist = arraylist::empty<int>(backing);
            maybe_undefined_array_t initial = {0, 1, 2, 3};

            auto status = alist.append_iterator(
                iter(maybe_undefined_array_t{0, 1, 2, 3}));

            REQUIRE(status.is_success());
            REQUIRE_RANGES_EQUAL(alist, initial);

            status = alist.append_iterator(
                iter(maybe_undefined_array_t{4, 5, 6, 7, 8}));

            REQUIRE(status.is_success());
            REQUIRE_RANGES_EQUAL(alist, iter(maybe_undefined_array_t{
                                            0, 1, 2, 3, 4, 5, 6, 7, 8}));
        }

        SUBCASE("forward iterator")
        {
            c_allocator_t backing;
            auto alist = arraylist::empty<int>(backing);

            const maybe_undefined_array_t initial = {0, 1, 2, 3};
            const auto initial_forward =
                keep_if(initial, [](auto&&) { return true; });
            static_assert(!arraylike_iterator_c<decltype(initial_forward)>);

            auto status = alist.append_iterator(iter(initial_forward));

            REQUIRE(status.is_success());
            REQUIRE_RANGES_EQUAL(alist, initial);

            const auto second_forward =
                keep_if(maybe_undefined_array_t{4, 5, 6, 7, 8},
                        [](auto&&) { return true; });
            static_assert(!arraylike_iterator_c<decltype(second_forward)>);
            status = alist.append_iterator(iter(second_forward));

            REQUIRE(status.is_success());
            REQUIRE_RANGES_EQUAL(alist, iter(maybe_undefined_array_t{
                                            0, 1, 2, 3, 4, 5, 6, 7, 8}));
        }
    }

    TEST_CASE("increase_capacity_by()")
    {
        SUBCASE("reallocation")
        {
            c_allocator_t backing;
            arraylist_t alist = arraylist::copy_items_from_iterator(
                                    backing, indices().take_at_most(100))
                                    .unwrap();
            REQUIRE(alist.capacity() == 100);
            auto status = alist.increase_capacity_by_at_least(100);
            const bool good = status.is_success() && alist.capacity() >= 200;
            REQUIRE(good);
        }

        SUBCASE("initial allocation")
        {
            c_allocator_t backing;
            arraylist_t alist = arraylist::empty<size_t>(backing);
            auto status = alist.increase_capacity_by_at_least(100);
            const bool good = status.is_success() && alist.capacity() >= 100;
            REQUIRE(good);
        }
    }
}
