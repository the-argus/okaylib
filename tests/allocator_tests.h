#ifndef __OKAYLIB_ALLOCATOR_TEST_H__
#define __OKAYLIB_ALLOCATOR_TEST_H__

#include "okay/allocators/allocator.h"
#include "okay/containers/array.h"
#include "okay/short_arithmetic_types.h"
#include <doctest.h>
#include <random>
#include <vector>

template <typename allocator_t> struct allocator_tests
{
    inline static void alloc_1mb_andfree(allocator_t& ally)
    {
        using namespace ok;

        // alloc::owned destroyed as lvalue
        {
            auto result = ally.make(array::undefined<u8, 1024>);
            if (result.okay()) {
                alloc::owned mb = result.release();
            } else {
                // early return because this isnt going to be able to allocate
                // at all, but thats fine, erroring is not bad behavior,
                // hopefully any problems will be caught in other tests
                return;
            }
        }

        // alloc::owned destroyed as rvalue
        {
            auto&& _ = ally.make(array::undefined<u8, 1024>).release();
        }

        // alloc::owned destroyed within result
        {
            auto&& _ = ally.make(array::undefined<u8, 1024>);
        }

        // manual free
        if (ally.features() & alloc::feature_flags::can_clear) {
            array_t<u8, 1024>& array =
                ally.make_non_owning(array::undefined<u8, 1024>).release();

            ally.free(array);
        }
    }

    inline static void alloc_1k_induvidual_bytes(allocator_t& ally)
    {
        using namespace ok;
        std::vector<alloc::owned<u8>> bytes;
        bytes.reserve(1024);

        for (u64 i = 0; i < 1024; ++i) {
            bytes.push_back(ally.template make<u8>(0).release());
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
            if (!result.okay())
                continue;

            bytes_t bytes = result.release().as_bytes();

            REQUIRE(bytes.size() >= num_bytes);
            REQUIRE(uintptr_t(bytes.data()) % 16 == 0);

            for (u64 i = 0; i < bytes.size(); ++i) {
                // should be all zeroed
                REQUIRE(bytes[i] == 0);
            }

            if (ally.features() & alloc::feature_flags::can_clear) {
                ally.clear();
            } else {
                ally.deallocate(bytes);
            }
        }
    }

    inline static void allocate_and_clear_repeatedly(allocator_t& ally)
    {
        using namespace ok;
        if (!(ally & alloc::feature_flags::can_clear))
            return;

        // if we cant allocate a megabyte at all, quit because this is a block
        // allocator or something
        auto mb_allocation =
            ally.allocate(alloc::request_t{.num_bytes = 1024, .alignment = 16});
        if (!mb_allocation.okay()) {
            return;
        }
        ally.clear();

        for (u64 i = 0; i < 100000000; ++i) {
            bytes_t allocated =
                ally.allocate(
                        alloc::request_t{.num_bytes = 1024, .alignment = 16})
                    .release()
                    .as_bytes();
            ally.clear();
        }
    }

    inline static void inplace_feature_flag(allocator_t& ally)
    {
        using namespace ok;
        if (ally.features() &
            alloc::feature_flags::can_predictably_realloc_in_place) {
            return;
        }

        bytes_t allocation =
            ally.allocate(alloc::request_t{.num_bytes = 1, .alignment = 1})
                .release()
                .as_bytes();

        auto reallocation = ally.reallocate(alloc::reallocate_request_t{
            .memory = allocation,
            .new_size_bytes = 1,
            .flags =
                alloc::flags::expand_back | alloc::flags::in_place_orelse_fail,
        });

        REQUIRE(reallocation.err() == alloc::error::unsupported);

        ally.deallocate(allocation);
        if (ally.features() & alloc::feature_flags::can_clear) {
            ally.clear();
        }
    }

    inline void run_all_fuzzed(allocator_t& ally)
    {
        constexpr ok::array_t test_functions{
            alloc_1mb_andfree,
            alloc_1k_induvidual_bytes,
            allocations_are_correctly_sized_aligned_and_zeroed,
            allocate_and_clear_repeatedly,
            inplace_feature_flag,
        };

        auto visited =
            ok::array::defaulted_or_zeroed<bool, test_functions.size()>();

        constexpr auto seed = 1;
        std::default_random_engine engine(seed);
        std::uniform_int_distribution<u64> distribution(0,
                                                        test_functions.size());

        auto all_visited = [&visited] {
            for (u64 i = 0; i < visited.size(); ++i) {
                if (!visited[i]) {
                    return false;
                }
            }
            return true;
        };

        while (!all_visited()) {
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
    allocator_tests<ok::allocator_t> dynamic_dispatch;
    dynamic_dispatch.run_all_fuzzed(allocator);
}

#endif
