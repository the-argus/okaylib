#include "test_header.h"
// test header must be first
#include "allocator_tests.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/c_allocator.h"
#include "okay/allocators/reserving_page_allocator.h"
#include "okay/containers/arraylist.h"
#include "testing_types.h"

using namespace ok;

TEST_SUITE("arena allocator")
{
    TEST_CASE("allocator tests")
    {
        ok::zeroed_array_t<u8, 100000> bytes;
        run_allocator_tests_static_and_dynamic_dispatch(
            [&] { return ok::opt<arena_t>(ok::in_place, bytes); });
        run_allocator_tests_static_and_dynamic_dispatch([&] {
            ok::memfill(bytes.items(), 0);
            return ok::opt<arena_t>(ok::in_place, bytes);
        });

        SUBCASE(
            "allocator tests but arena has an allocator that can't reallocate")
        {
            c_allocator_t allocator;

            run_allocator_tests_static_and_dynamic_dispatch([&] {
                arena_t arena(allocator);
                // "prime" the arena
                // TODO: implement some
                // .ensure_additional_capacity() function for arenas
                bytes_t _ = arena.allocate({.num_bytes = 100000}).unwrap();
                arena.clear();
                return ok::opt<arena_t>(ok::in_place, stdc::move(arena));
            });
        }
    }

    TEST_CASE("arena with allocator and empty initial buffer")
    {
        reserving_page_allocator_t allocator{{}};
        run_allocator_tests_static_and_dynamic_dispatch(
            [&] { return ok::opt<arena_t>(ok::in_place, allocator); });
    }

    TEST_CASE("arena allocator with scopes and destructors")
    {
        const size_t expected_destructs = [] {
            ok::zeroed_array_t<u8, 10000> bytes;
            arena_t arena(bytes);

            auto counterslist = arraylist::empty<counter_type*>(arena);
            counterslist.append(&arena.make_non_owning<counter_type>().unwrap())
                .or_panic();
            counterslist.append(&arena.make_non_owning<counter_type>().unwrap())
                .or_panic();
            counterslist.append(&arena.make_non_owning<counter_type>().unwrap())
                .or_panic();
            counterslist.append(&arena.make_non_owning<counter_type>().unwrap())
                .or_panic();

            REQUIRE(counter_type::counters.destructs == 0);

            const size_t expected_destructs = [&] {
                auto&& _0 = arena.begin_scope();
                // include some empty scopes, that shouldn't cause problems
                auto&& _1 = arena.begin_scope();
                auto&& _2 = arena.begin_scope();

                auto inner_counterslist =
                    arraylist::empty<counter_type*>(arena);
                inner_counterslist
                    .append(&arena.make_non_owning<counter_type>().unwrap())
                    .or_panic();
                inner_counterslist
                    .append(&arena.make_non_owning<counter_type>().unwrap())
                    .or_panic();
                inner_counterslist
                    .append(&arena.make_non_owning<counter_type>().unwrap())
                    .or_panic();
                inner_counterslist
                    .append(&arena.make_non_owning<counter_type>().unwrap())
                    .or_panic();

                REQUIRE(counter_type::counters.destructs == 0);

                const size_t expected_destructs = [&] {
                    auto&& _ = arena.begin_scope();

                    auto innermost_counterslist =
                        arraylist::empty<counter_type*>(arena);
                    innermost_counterslist
                        .append(&arena.make_non_owning<counter_type>().unwrap())
                        .or_panic();
                    innermost_counterslist
                        .append(&arena.make_non_owning<counter_type>().unwrap())
                        .or_panic();
                    innermost_counterslist
                        .append(&arena.make_non_owning<counter_type>().unwrap())
                        .or_panic();
                    innermost_counterslist
                        .append(&arena.make_non_owning<counter_type>().unwrap())
                        .or_panic();

                    REQUIRE(counter_type::counters.destructs == 0);
                    return innermost_counterslist.size();
                }();

                REQUIRE(counter_type::counters.destructs == expected_destructs);

                return expected_destructs + inner_counterslist.size();
            }();

            REQUIRE(counter_type::counters.destructs == expected_destructs);
            return expected_destructs + counterslist.size();
        }();
        REQUIRE(counter_type::counters.destructs == expected_destructs);
    }
}
