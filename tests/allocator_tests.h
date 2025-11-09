#ifndef __OKAYLIB_ALLOCATOR_TEST_H__
#define __OKAYLIB_ALLOCATOR_TEST_H__

#include "okay/allocators/allocator.h"
#include "okay/containers/array.h"
#include "okay/defer.h"
#include "okay/ranges/algorithm.h"
#include "okay/ranges/views/std_for.h"
#include "okay/short_arithmetic_types.h"
#include <doctest.h>
#include <random>
#include <vector>

static_assert(ok::memory_resource_c<ok::memory_resource_t>);
static_assert(ok::allocator_c<ok::allocator_t>);

enum class allocator_test_mode
{
    recreate_each_test,
    recreate_each_test_and_check_oom,
    keep_allocator_throughout,
};

template <ok::memory_resource_c allocator_impl_t = ok::memory_resource_t>
struct memory_resource_counter_wrapper_t : public ok::allocator_t
{
  public:
    memory_resource_counter_wrapper_t(allocator_impl_t& allocator,
                                      ok::opt<size_t> limit = {})
        : wrapped(allocator), bytes_limit(limit)
    {
    }

    allocator_impl_t& wrapped;
    size_t bytes_allocated = 0;
    ok::opt<size_t> bytes_limit;

  protected:
    [[nodiscard]] ok::alloc::result_t<ok::bytes_t>
    impl_allocate(const ok::alloc::request_t& request) OKAYLIB_NOEXCEPT final
    {
        if (bytes_limit &&
            bytes_allocated + request.num_bytes > bytes_limit.ref_or_panic())
            return ok::alloc::error::oom;

        ok::res out = wrapped.allocate(request);
        if (ok::is_success(out)) {
            bytes_allocated += out.unwrap_unchecked().size();
        }
        return out;
    }

    [[nodiscard]] ok::alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        if constexpr (ok::allocator_c<allocator_impl_t>) {
            return wrapped.features();
        } else {
            return {};
        }
    }

    /// NOTE: the implementation of this is not required to check for nullptr,
    /// that should be handled by the deallocate() wrapper that users actually
    /// call
    void impl_deallocate(void* memory) OKAYLIB_NOEXCEPT final
    {
        if constexpr (ok::allocator_c<allocator_impl_t>) {
            wrapped.deallocate(memory);
        }
    }

    [[nodiscard]] ok::alloc::result_t<ok::bytes_t> impl_reallocate(
        const ok::alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT final
    {
        if constexpr (ok::allocator_c<allocator_impl_t>) {
            return wrapped.reallocate(options);
        } else {
            return ok::alloc::error::unsupported;
        }
    }

    [[nodiscard]] ok::alloc::result_t<ok::alloc::reallocation_extended_t>
    impl_reallocate_extended(const ok::alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final
    {
        if constexpr (ok::allocator_c<allocator_impl_t>) {
            return wrapped.reallocate_extended(options);
        } else {
            return ok::alloc::error::unsupported;
        }
    }
};

template <ok::memory_resource_c allocator_t> struct allocator_tests
{
    constexpr static bool has_clear = requires(allocator_t& a) {
        { a.clear() } -> ok::same_as_c<void>;
    };

    constexpr static bool has_deallocate = ok::allocator_c<allocator_t>;

    constexpr static bool has_make = ok::allocator_c<allocator_t>;

    [[nodiscard]] static ok::status<ok::alloc::error>
    alloc_1mb_andfree(allocator_t& ally)
    {
        using namespace ok;
        if constexpr (has_make) {

            // alloc::owned destroyed as lvalue
            {
                ok::res result = ally.template make<zeroed_array_t<u8, 1024>>();
                if (ok::is_success(result)) {
                    alloc::owned mb = std::move(result.unwrap());
                } else {
                    return result.status();
                }
            }

            // alloc::owned destroyed as rvalue
            {
                auto res = ally.template make<zeroed_array_t<u8, 1024>>();
                if (!ok::is_success(res))
                    return res.status();
                auto&& _ = res.unwrap();
            }

            // alloc::owned destroyed within result
            {
                ok::res result = ally.template make<zeroed_array_t<u8, 1024>>();
                if (!ok::is_success(result))
                    return result.status();
            }
        }

        // manual free
        // if statement is here to prevent assert in make_non_owning from firing
        auto array_result =
            ally.template make_non_owning<zeroed_array_t<u8, 1024>>();
        if (!ok::is_success(array_result))
            return array_result.status();
        auto& array = array_result.unwrap();

        if constexpr (has_make) {
            ok::destroy_and_free(ally, array);
        } else if constexpr (has_clear) {
            ally.clear();
        }

        return ok::make_success<ok::alloc::error>();
    }

    [[nodiscard]] static ok::status<ok::alloc::error>
    alloc_1k_induvidual_bytes(allocator_t& ally)
    {
        if constexpr (has_make) {
            using namespace ok;
            std::vector<alloc::owned<u8>> bytes;
            bytes.reserve(1024);

            for (u64 i = 0; i < 1024; ++i) {
                auto byte_res = ally.template make<u8>(0);
                if (!ok::is_success(byte_res))
                    return byte_res.status();
                bytes.push_back(std::move(byte_res.unwrap()));
            }
        } else if constexpr (has_clear) {
            using namespace ok;
            std::vector<u8*> bytes;
            bytes.reserve(1024);

            for (u64 i = 0; i < 1024; ++i) {
                auto byte_res = ally.template make_non_owning<u8>(0);
                if (!ok::is_success(byte_res))
                    return byte_res.status();
                bytes.push_back(ok::addressof(byte_res.unwrap()));
            }

            ally.clear();
        }
        return ok::make_success<ok::alloc::error>();
    }

    [[nodiscard]] static ok::status<ok::alloc::error>
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
        return ok::make_success<ok::alloc::error>();
    }

    [[nodiscard]] static ok::status<ok::alloc::error>
    allocating_zero_bytes_returns_unsupported(allocator_t& ally)
    {
        REQUIRE(ally.allocate(ok::alloc::request_t{
                                  .num_bytes = 0,
                                  .alignment = 16,
                              })
                    .status() == ok::alloc::error::unsupported);
        return ok::make_success<ok::alloc::error>();
    }

    [[nodiscard]] static ok::status<ok::alloc::error>
    allocate_and_clear_repeatedly(allocator_t& ally)
    {
        if constexpr (has_clear) {
            using namespace ok;
            // if we cant allocate a megabyte at all, quit because this is a
            // block allocator or something
            auto mb_allocation = ally.allocate(
                alloc::request_t{.num_bytes = 1024, .alignment = 16});
            if (!mb_allocation.is_success()) {
                return mb_allocation.status();
            }
            ally.clear();

            for (u64 i = 0; i < 10000; ++i) {
                auto res = ally.allocate(
                    alloc::request_t{.num_bytes = 1024, .alignment = 16});
                if (!ok::is_success(res))
                    return res.status();
                bytes_t allocated = res.unwrap();
                ally.clear();
            }
        }
        return ok::make_success<ok::alloc::error>();
    }

    [[nodiscard]] static ok::status<ok::alloc::error>
    inplace_feature_flag(allocator_t& ally)
    {
        using namespace ok;
        if constexpr (has_make) {
            if (ally.features() &
                alloc::feature_flags::can_predictably_realloc_in_place) {
                return make_success<alloc::error>();
            }

            auto allocation_result =
                ally.allocate(alloc::request_t{.num_bytes = 1, .alignment = 1});
            if (!ok::is_success(allocation_result))
                return allocation_result.status();
            bytes_t allocation = allocation_result.unwrap();

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
        return ok::make_success<ok::alloc::error>();
    }

    constexpr static ok::maybe_undefined_array_t test_functions{
        alloc_1mb_andfree,
        alloc_1k_induvidual_bytes,
        allocations_are_correctly_sized_aligned_and_zeroed,
        allocate_and_clear_repeatedly,
        inplace_feature_flag,
        allocating_zero_bytes_returns_unsupported,
    };

    template <typename factory_callable_t>
    inline void run_all_fuzzed_all_modes(const factory_callable_t& factory)
    {
        static constexpr ok::maybe_undefined_array_t modes = {
            allocator_test_mode::keep_allocator_throughout,
            allocator_test_mode::recreate_each_test,
            allocator_test_mode::recreate_each_test_and_check_oom,
        };

        for (auto mode : modes | ok::std_for) {
            run_all_fuzzed(mode, factory);
        }
    }

    template <typename factory_callable_t>
    inline void run_all_fuzzed(allocator_test_mode mode,
                               const factory_callable_t& factory)
    {
        ok::zeroed_array_t<bool, test_functions.size()> visited;

        constexpr auto seed = 1;
        std::default_random_engine engine(seed);
        std::uniform_int_distribution<u64> distribution(
            0, test_functions.size() - 1);

        ok::opt ally = factory();

        while (!ok::all_of(visited)) {
            u64 idx = distribution(engine);
            while (visited[idx]) {
                idx = distribution(engine);
            }
            visited[idx] = true;

            // static dispatch
            auto static_status = test_functions[idx](ally.ref_or_panic());
            REQUIRE(ok::is_success(static_status));

            // dynamic dispatch, depends on mode
            switch (mode) {
            case allocator_test_mode::recreate_each_test:
                [[fallthrough]];
            case allocator_test_mode::keep_allocator_throughout:
                // only do dynamic dispatch if type seems like it supports it
                if constexpr (ok::detail::is_derived_from_c<
                                  allocator_t, ok::memory_resource_t>) {
                    auto status = allocator_tests<ok::memory_resource_t>::
                        test_functions[idx](ally.ref_or_panic());
                    REQUIRE(ok::is_success(status));
                } else if constexpr (ok::detail::is_derived_from_c<
                                         allocator_t, ok::allocator_t>) {
                    auto status =
                        allocator_tests<ok::allocator_t>::test_functions[idx](
                            ally.ref_or_panic());
                    REQUIRE(ok::is_success(status));
                }
                break;
            case allocator_test_mode::recreate_each_test_and_check_oom: {
                constexpr static auto polymorphic_tests =
                    allocator_tests<ok::allocator_t>::test_functions;
                // oom-checking is special, we always put the type in a wrapper
                // so it always supports polymorphism.
                memory_resource_counter_wrapper_t wrapper(ally.ref_or_panic());
                auto status = polymorphic_tests[idx](wrapper);
                REQUIRE(ok::is_success(status));

                // loop condition uses overflow, it is defined behavior
                for (size_t i = wrapper.bytes_allocated - 1;
                     i < wrapper.bytes_allocated; i -= 16) {
                    memory_resource_counter_wrapper_t limiter(
                        ally.ref_or_panic(), i);
                    auto status = polymorphic_tests[idx](wrapper);

                    // as we decrease the amount of bytes that can be allocated,
                    // it is okay to OOM as long as there is a graceful return.
                    // tests the failstate
                    const bool only_ooms =
                        ok::is_success(status) ||
                        status.as_enum() == ok::alloc::error::oom;
                    REQUIRE(only_ooms);
                    REQUIRE(limiter.bytes_allocated <= i);
                }

                break;
            }
            }

            // recreate the allocator afterwards, for modes that require it
            if (mode != allocator_test_mode::keep_allocator_throughout) {
                ally = ok::nullopt;
                ally = factory();
            }
        }
    }
};

template <typename factory_t>
inline void
run_allocator_tests_static_and_dynamic_dispatch(const factory_t& factory)
{
    // factory should return an ok::opt containing the allocator
    using allocator_t = typename decltype(factory())::value_type;
    allocator_tests<allocator_t> static_dispatch;
    static_dispatch.run_all_fuzzed_all_modes(factory);
}

#endif
