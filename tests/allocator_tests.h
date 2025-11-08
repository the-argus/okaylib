#ifndef __OKAYLIB_ALLOCATOR_TEST_H__
#define __OKAYLIB_ALLOCATOR_TEST_H__

#include "okay/allocators/allocator.h"
#include "okay/containers/array.h"
#include "okay/defer.h"
#include "okay/ranges/algorithm.h"
#include "okay/short_arithmetic_types.h"
#include <doctest.h>
#include <random>
#include <vector>

static_assert(ok::memory_resource_c<ok::memory_resource_t>);
static_assert(ok::allocator_c<ok::allocator_t>);

template <ok::memory_resource_c allocator_t> struct allocator_tests
{
    constexpr static bool has_clear = requires(allocator_t& a) {
        { a.clear() } -> ok::same_as_c<void>;
    };

    constexpr static bool has_deallocate = ok::allocator_c<allocator_t>;

    constexpr static bool has_make = ok::allocator_c<allocator_t>;

    inline static void alloc_1mb_andfree(allocator_t& ally)
    {
        using namespace ok;
        if constexpr (has_make) {

            // alloc::owned destroyed as lvalue
            {
                ok::res result = ally.template make<zeroed_array_t<u8, 1024>>();
                if (result.is_success()) {
                    alloc::owned mb = std::move(result.unwrap());
                } else {
                    // early return because this isnt going to be able to
                    // allocate at all, but thats fine, erroring is not bad
                    // behavior, hopefully any problems will be caught in other
                    // tests
                    return;
                }
            }

            // alloc::owned destroyed as rvalue
            {
                auto&& _ =
                    ally.template make<zeroed_array_t<u8, 1024>>().unwrap();
            }

            // alloc::owned destroyed within result
            {
                ok::res result = ally.template make<zeroed_array_t<u8, 1024>>();
            }
        }

        // manual free
        // if statement is here to prevent assert in make_non_owning from firing
        auto& array =
            ally.template make_non_owning<zeroed_array_t<u8, 1024>>().unwrap();

        if constexpr (has_make) {
            ok::destroy_and_free(ally, array);
        } else if constexpr (has_clear) {
            ally.clear();
        }
    }

    inline static void alloc_1k_induvidual_bytes(allocator_t& ally)
    {
        if constexpr (has_make) {
            using namespace ok;
            std::vector<alloc::owned<u8>> bytes;
            bytes.reserve(1024);

            for (u64 i = 0; i < 1024; ++i) {
                bytes.push_back(ally.template make<u8>(0).unwrap());
            }
        } else if constexpr (has_clear) {
            using namespace ok;
            std::vector<u8*> bytes;
            bytes.reserve(1024);

            for (u64 i = 0; i < 1024; ++i) {
                bytes.push_back(ok::addressof(
                    ally.template make_non_owning<u8>(0).unwrap()));
            }

            ally.clear();
        }
    }

    inline static void
    allocations_are_correctly_sized_aligned_and_zeroed(allocator_t& ally)
    {
        using namespace ok;

        // allocate and then free random amounts of bytes between 1-100, 100
        // times
        for (u64 iteration = 0; iteration < 100; ++iteration) {
            std::default_random_engine engine(iteration);
            std::uniform_int_distribution<u64> distribution(1, 100);
            const u64 num_bytes = distribution(engine);
            auto result = ally.allocate(
                alloc::request_t{.num_bytes = num_bytes, .alignment = 16});

            // its okay for the allocator to fail, just not return bad memory
            if (!result.is_success())
                continue;

            bytes_t bytes = result.unwrap();

            REQUIRE(bytes.size() >= num_bytes);
            REQUIRE(uintptr_t(bytes.address_of_first()) % 16 == 0);

            for (u64 i = 0; i < bytes.size(); ++i) {
                // should be all zeroed
                REQUIRE(bytes[i] == 0);
            }

            if constexpr (has_clear) {
                ally.clear();
            } else if constexpr (has_deallocate) {
                ally.deallocate(bytes.address_of_first());
            }
        }
    }

    inline static void
    allocating_zero_bytes_returns_unsupported(allocator_t& ally)
    {
        REQUIRE(ally.allocate(ok::alloc::request_t{
                                  .num_bytes = 0,
                                  .alignment = 16,
                              })
                    .status() == ok::alloc::error::unsupported);
    }

    inline static void allocate_and_clear_repeatedly(allocator_t& ally)
    {
        if constexpr (has_clear) {
            using namespace ok;
            // if we cant allocate a megabyte at all, quit because this is a
            // block allocator or something
            auto mb_allocation = ally.allocate(
                alloc::request_t{.num_bytes = 1024, .alignment = 16});
            if (!mb_allocation.is_success()) {
                return;
            }
            ally.clear();

            for (u64 i = 0; i < 10000; ++i) {
                bytes_t allocated =
                    ally.allocate(alloc::request_t{.num_bytes = 1024,
                                                   .alignment = 16})
                        .unwrap();
                ally.clear();
            }
        }
    }

    inline static void inplace_feature_flag(allocator_t& ally)
    {
        using namespace ok;
        if constexpr (has_make) {
            if (ally.features() &
                alloc::feature_flags::can_predictably_realloc_in_place) {
                return;
            }

            bytes_t allocation =
                ally.allocate(alloc::request_t{.num_bytes = 1, .alignment = 1})
                    .unwrap();

            defer d([&] {
                if constexpr (has_deallocate) {
                    ally.deallocate(allocation.address_of_first());
                } else if constexpr (has_clear) {
                    ally.clear();
                }
            });

            auto reallocation = ally.reallocate(alloc::reallocate_request_t{
                .memory = allocation,
                .new_size_bytes = 1,
                .flags = alloc::realloc_flags::expand_back |
                         alloc::realloc_flags::in_place_orelse_fail,
            });

            REQUIRE(reallocation.status() == alloc::error::unsupported);
        }
    }

    inline void run_all_fuzzed(allocator_t& ally)
    {
        constexpr ok::maybe_undefined_array_t test_functions{
            alloc_1mb_andfree,
            alloc_1k_induvidual_bytes,
            allocations_are_correctly_sized_aligned_and_zeroed,
            allocate_and_clear_repeatedly,
            inplace_feature_flag,
            allocating_zero_bytes_returns_unsupported,
        };

        ok::zeroed_array_t<bool, test_functions.size()> visited;

        constexpr auto seed = 1;
        std::default_random_engine engine(seed);
        std::uniform_int_distribution<u64> distribution(
            0, test_functions.size() - 1);

        while (!ok::all_of(visited)) {
            u64 idx = distribution(engine);
            while (visited[idx]) {
                idx = distribution(engine);
            }
            visited[idx] = true;
            test_functions[idx](ally);
        }
    }
};

template <typename allocator_t>
inline void
run_allocator_tests_static_and_dynamic_dispatch(allocator_t& allocator)
{
    allocator_tests<allocator_t> static_dispatch;
    static_dispatch.run_all_fuzzed(allocator);

    if constexpr (ok::detail::is_derived_from_c<allocator_t, ok::allocator_t>) {
        allocator_tests<ok::allocator_t> dynamic_dispatch;
        dynamic_dispatch.run_all_fuzzed(allocator);
    } else {
        allocator_tests<ok::memory_resource_t> dynamic_dispatch;
        dynamic_dispatch.run_all_fuzzed(allocator);
    }
}

#endif
