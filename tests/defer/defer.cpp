#include "test_header.h"
// test header must be first
#include "okay/defer.h"
#include "okay/macros/defer.h"
#include "okay/opt.h"
#include <array>
#include <unordered_set>

using namespace ok;

struct type_stats
{
    size_t times_nonspecial_constructed = 0;
    size_t times_copy_constructed = 0;
    size_t times_copy_assigned = 0;
    size_t times_move_constructed = 0;
    size_t times_move_assigned = 0;
    size_t times_destructed = 0;

    constexpr size_t total_operations()
    {
        return times_nonspecial_constructed + times_copy_constructed +
               times_copy_assigned + times_move_constructed +
               times_move_assigned + times_destructed;
    }

    constexpr size_t times_constructed()
    {
        return times_copy_constructed + times_nonspecial_constructed +
               times_move_constructed;
    }

    constexpr size_t times_assigned()
    {
        return times_copy_assigned + times_move_assigned;
    }
};

struct tracker
{
    type_stats& stats;
    std::vector<int> member;

    explicit tracker(type_stats& stats)
        : stats(stats), member({0, 1, 2, 3, 4, 5, 6, 7})
    {
        stats.times_nonspecial_constructed += 1;
    }
    tracker(const tracker& other) : stats(other.stats), member(other.member)
    {
        stats.times_copy_constructed += 1;
    }
    tracker(tracker&& other)
        : stats(other.stats), member(std::move(other.member))
    {
        stats.times_move_constructed += 1;
    }
    tracker& operator=(const tracker& other)
    {
        __ok_assert(&stats == &other.stats, "different stats objects?");
        member = other.member;
        return *this;
    }
    tracker& operator=(tracker&& other)
    {
        __ok_assert(&stats == &other.stats, "different stats objects?");
        member = std::move(other.member);
        return *this;
    }

    ~tracker() { stats.times_destructed += 1; }
};

TEST_SUITE("defer")
{
    TEST_CASE("functionality")
    {
        SUBCASE("defer that does nothing") { ok_defer{}; }

        SUBCASE("defer that adds to number")
        {
            int counter = 0;

            {
                ok_defer { counter = 0; };
                for (size_t i = 0; i < 10; ++i) {
                    ok_defer { counter++; };
                    REQUIRE(counter == i);
                }
            }

            REQUIRE(counter == 0);
        }

        SUBCASE("conditionally cancel defer")
        {
            std::unordered_set<void*> malloced_stuff;
            auto fakemalloc = [&malloced_stuff](size_t bytes) -> void* {
                void* mem = malloc(bytes);
                REQUIRE(mem);
                malloced_stuff.insert(mem);
                return mem;
            };

            auto fakefree = [&malloced_stuff](void* mem) {
                malloced_stuff.erase(mem);
                free(mem);
            };

            // make sure fakemalloc and fakefree work
            REQUIRE(malloced_stuff.size() == 0);
            void* mem = fakemalloc(100);
            REQUIRE(malloced_stuff.size() == 1);
            fakefree(mem);
            REQUIRE(malloced_stuff.size() == 0);

            auto getmems = [fakemalloc, fakefree](
                               bool fail_halfway) -> opt<std::array<void*, 3>> {
                void* first_mem = fakemalloc(100);
                if (!first_mem)
                    return {};

                ok_named_defer_ex(free_first_mem, ([first_mem, fakefree] {
                                      fakefree(first_mem);
                                  }));

                void* second_mem = fakemalloc(100);
                if (!second_mem)
                    return {};
                ok_named_defer_ex(free_second_mem, ([second_mem, fakefree] {
                                      fakefree(second_mem);
                                  }));

                if (fail_halfway)
                    return {};

                void* third_mem = fakemalloc(100);
                if (!third_mem)
                    return {};

                // okay, all initialization is good, dont free anything
                free_first_mem.cancel();
                free_second_mem.cancel();

                return std::array<void*, 3>{first_mem, second_mem, third_mem};
            };

            auto maybe_mems = getmems(false);
            if (maybe_mems.has_value()) {
                REQUIRE(malloced_stuff.size() == 3);
                for (void* mem : maybe_mems.ref_or_panic()) {
                    fakefree(mem);
                }
            }
            REQUIRE(malloced_stuff.size() == 0);
            maybe_mems = getmems(true);
            // all stuff should be cleaned up if we fail halfway through init
            REQUIRE(malloced_stuff.size() == 0);
        }
    }

    TEST_CASE("copying / moving")
    {
        SUBCASE("no observed copies or moves from deferring stuff")
        {
            type_stats stats;
            {
                tracker test(stats);

                ok_defer
                {
                    printf("first item in test: %d\n", test.member[0]);
                };

                // make sure the test was only constructed
                REQUIRE(stats.times_nonspecial_constructed == 1);
                REQUIRE(stats.total_operations() == 1);
            }

            // make sure the test was only constructed and destructed
            REQUIRE(stats.times_nonspecial_constructed == 1);
            REQUIRE(stats.times_destructed == 1);
            REQUIRE(stats.total_operations() == 2);
        }

        SUBCASE("no observed copies or moves from deferring stuff and then "
                "cancelling it")
        {
            type_stats stats;
            {
                tracker test(stats);

                ok_named_defer(printer)
                {
                    printf("first item in test: %d\n", test.member[0]);
                };

                // make sure the test was only constructed
                REQUIRE(stats.times_nonspecial_constructed == 1);
                REQUIRE(stats.total_operations() == 1);

                printer.cancel();
            }

            // make sure the test was only constructed and destructed
            REQUIRE(stats.times_nonspecial_constructed == 1);
            REQUIRE(stats.times_destructed == 1);
            REQUIRE(stats.total_operations() == 2);
        }

        SUBCASE("no move observed when copying value into capture")
        {
            type_stats stats;
            {
                tracker test(stats);

                ok_named_defer_ex(printer, [test]() {
                    printf("first item in test: %d\n", test.member[0]);
                });

                // make sure the test was only constructed
                REQUIRE(stats.times_nonspecial_constructed == 1);
                REQUIRE(stats.times_copy_constructed == 1);
                REQUIRE(stats.total_operations() == 2);

                printer.cancel();
            }

            // make sure the test was only constructed and destructed
            REQUIRE(stats.times_constructed() == 2);
            REQUIRE(stats.times_destructed == 2);
            REQUIRE(stats.total_operations() == 4);
        }
    }
}
