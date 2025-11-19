#include "test_header.h"
// test header must be first
#include "okay/tuple.h"
#include "testing_types.h"
#include <tuple>
#include <type_traits>

struct undestructible_t
{
    ~undestructible_t() = delete;
};

struct noncopy_t
{
    noncopy_t(const noncopy_t&) = delete;
    noncopy_t& operator=(const noncopy_t&) = delete;
    noncopy_t(noncopy_t&&) noexcept {}
    noncopy_t& operator=(noncopy_t&&) noexcept { return *this; }
    ~noncopy_t() noexcept {};
};

static_assert(std::is_constructible_v<std::tuple<int, int>, int, int> ==
              std::is_constructible_v<ok::tuple<int, int>, int, int>);
static_assert(std::is_convertible_v<std::tuple<int>, int> ==
              std::is_convertible_v<ok::tuple<int>, int>);
static_assert(std::is_destructible_v<std::tuple<undestructible_t>> ==
              std::is_destructible_v<ok::tuple<undestructible_t>>);
static_assert(std::is_move_assignable_v<std::tuple<noncopy_t>> ==
              std::is_move_assignable_v<ok::tuple<noncopy_t>>);
static_assert(std::is_move_assignable_v<std::tuple<noncopy_t, int>> ==
              std::is_move_assignable_v<ok::tuple<noncopy_t, int>>);
static_assert(std::is_move_constructible_v<std::tuple<noncopy_t>> ==
              std::is_move_constructible_v<ok::tuple<noncopy_t>>);
static_assert(std::is_move_constructible_v<std::tuple<noncopy_t, int>> ==
              std::is_move_constructible_v<ok::tuple<noncopy_t, int>>);
static_assert(std::is_copy_assignable_v<std::tuple<noncopy_t>> ==
              std::is_copy_assignable_v<ok::tuple<noncopy_t>>);
static_assert(std::is_copy_assignable_v<std::tuple<noncopy_t, int>> ==
              std::is_copy_assignable_v<ok::tuple<noncopy_t, int>>);
static_assert(std::is_copy_constructible_v<std::tuple<noncopy_t>> ==
              std::is_copy_constructible_v<ok::tuple<noncopy_t>>);
static_assert(std::is_copy_constructible_v<std::tuple<noncopy_t, int>> ==
              std::is_copy_constructible_v<ok::tuple<noncopy_t, int>>);
static_assert(std::is_trivially_destructible_v<ok::tuple<int, int, int>> ==
              std::is_trivially_destructible_v<std::tuple<int, int, int>>);
static_assert(
    std::is_trivially_destructible_v<ok::tuple<noncopy_t, int, int>> ==
    std::is_trivially_destructible_v<std::tuple<noncopy_t, int, int>>);
static_assert(std::is_trivially_destructible_v<ok::tuple<int, int, int>> ==
              std::is_trivially_destructible_v<std::tuple<int, int, int>>);
static_assert(
    std::is_trivially_destructible_v<ok::tuple<undestructible_t, int, int>> ==
    std::is_trivially_destructible_v<std::tuple<undestructible_t, int, int>>);
static_assert(
    std::is_trivially_destructible_v<ok::tuple<noncopy_t, int, int>> ==
    std::is_trivially_destructible_v<std::tuple<noncopy_t, int, int>>);

/// std::tuple doesn't make this guarantee, but we do
static_assert(std::is_trivially_move_constructible_v<ok::tuple<int, int, int>>);
static_assert(std::is_trivially_move_assignable_v<ok::tuple<int, int, int>>);
static_assert(std::is_trivially_copy_constructible_v<ok::tuple<int, int, int>>);
static_assert(std::is_trivially_copy_assignable_v<ok::tuple<int, int, int>>);

// noncopy_t is not trivially movable, so tuples containing it should not be
// either
static_assert(
    !std::is_trivially_move_constructible_v<ok::tuple<noncopy_t, int, int>>);
static_assert(
    !std::is_trivially_move_assignable_v<ok::tuple<noncopy_t, int, int>>);

struct eql_counter_type
{
    static size_t comparisons;
    int item;

    inline eql_counter_type(int i) : item(i) {}

    inline bool operator==(const eql_counter_type& other) const
    {
        comparisons += 1;
        return other.item == item;
    }
    inline bool operator!=(const eql_counter_type& other) const
    {
        comparisons += 1;
        return other.item != item;
    }
};
size_t eql_counter_type::comparisons = 0;

TEST_SUITE("tuple")
{
    TEST_CASE("construct tuple")
    {
        std::tuple<int> test = 1;
        ok::tuple<int> test2 = 1;
        std::tuple<int, int> test3 = {1, 1};
        ok::tuple<int, int> test4 = {1, 1};
        std::tuple<int, int> test5(1, 1);
        ok::tuple<int, int> test6(1, 1);
        REQUIRE(test6 == test4);
        REQUIRE(test3 == test5);
    }

    TEST_CASE("structured bindings")
    {
        auto [i, j] = ok::tuple<int, int>{1, 2};
        REQUIRE(i == 1);
        REQUIRE(j == 2);

        auto [k, m] = std::tuple<int, int>{1, 2};
        REQUIRE(k == 1);
        REQUIRE(m == 2);
    }

    TEST_CASE("tuple of reference types")
    {
        using T = ok::tuple<int&, float&, bool&>;
        int i{};
        float f{};
        bool b{};
        T refs(i, f, b);

        ok::get<int&>(refs) = 1;
        ok::get<float&>(refs) = 2.0;
        ok::get<bool&>(refs) = true;

        REQUIRE(i == 1);
        REQUIRE(f == 2.0);
        REQUIRE(b == true);
    }

    TEST_CASE("tuple containing cstyle array references")
    {
        int array[] = {1, 1};
        ok::tuple<int(&)[2], int> mytuple(array, 1);
        ok::tuple<const int(&)[2], int> mytuple_const(array, 2);

        REQUIRE(std::addressof(array[0]) ==
                std::addressof(ok::get<0>(mytuple)[0]));
        REQUIRE(std::addressof(array[1]) ==
                std::addressof(ok::get<0>(mytuple_const)[1]));
        REQUIRE(ok::get<int>(mytuple) == 1);
        REQUIRE(ok::get<int>(mytuple_const) == 2);

        ok::get<0>(mytuple)[0] = 2;
        // definitely by reference...
        REQUIRE(std::addressof(array[0]) ==
                std::addressof(ok::get<0>(mytuple)[0]));
    }

    TEST_CASE("tuple elements special member functions")
    {
        std::tuple<int, std::vector<int>, counter_type, int> stdtuple;
        ok::tuple<int, std::vector<int>, counter_type, int> oktuple;

        struct owning_object
        {
            decltype(oktuple) oktuple;
            decltype(stdtuple) stdtuple;
        };

        REQUIRE(counter_type::counters ==
                special_member_counters_t{.default_constructs = 2});
        counter_type::reset_counters();

        {
            {
                std::make_unique<owning_object>(oktuple, stdtuple);
            }
            REQUIRE(counter_type::counters == special_member_counters_t{
                                                  .copy_constructs = 2,
                                                  .destructs = 2,
                                              });
            counter_type::reset_counters();
        }

        {
            REQUIRE(counter_type::counters == special_member_counters_t{});
            {
                std::make_unique<owning_object>(std::move(oktuple),
                                                std::move(stdtuple));
            }
            REQUIRE(counter_type::counters == special_member_counters_t{
                                                  .move_constructs = 2,
                                                  .destructs = 2,
                                              });
            counter_type::reset_counters();
        }

        std::tuple<int, std::vector<int>, counter_type, int> stdtuple2 = {
            0, std::vector<int>{0, 1, 2}, counter_type{}, 0};
        ok::tuple<int, std::vector<int>, counter_type, int> oktuple2 = {
            0, std::vector<int>{0, 1, 2}, counter_type{}, 0};
        // unfortunately, this is not inplace aggregate construction, it is
        // calling a constructor with a bunch of tempopraries that get moved
        // into place. theres not way to elide the move here afaik- you need
        // inheritance to implement tuple and aggregate construction is disabled
        // for fields of inherited structs
        REQUIRE(counter_type::counters == special_member_counters_t{
                                              .move_constructs = 2,
                                              .default_constructs = 2,
                                              .destructs = 2,
                                          });
        counter_type::reset_counters();
        {
            REQUIRE(counter_type::counters == special_member_counters_t{});

            stdtuple = stdtuple2;
            oktuple = oktuple2;

            REQUIRE(counter_type::counters == special_member_counters_t{
                                                  .copy_assigns = 2,
                                              });
        }

        counter_type::reset_counters();
        {
            REQUIRE(counter_type::counters == special_member_counters_t{});

            stdtuple = {};
            oktuple = {};
            REQUIRE(counter_type::counters ==
                    special_member_counters_t{
                        .default_constructs = 2,
                        .destructs =
                            2, // the temporary, default constructed objects
                        .move_assigns = 2,
                    });
            counter_type::reset_counters();
            stdtuple = std::move(stdtuple2);
            oktuple = std::move(oktuple2);

            REQUIRE(counter_type::counters == special_member_counters_t{
                                                  .move_assigns = 2,
                                              });
        }
        counter_type::reset_counters();
    }

    // NOTE: no three way comparison for tuples, I think that's too confusing :)
    TEST_CASE("equality and inequality operators and short circuiting")
    {
        const std::tuple<eql_counter_type, eql_counter_type, eql_counter_type>
            test_std_src = {1, 2, 3};
        const ok::tuple<eql_counter_type, eql_counter_type, eql_counter_type>
            test_ok_src = {1, 2, 3};

        REQUIRE(eql_counter_type::comparisons == 0);
        REQUIRE(test_std_src == decltype(test_std_src){1, 2, 3});
        REQUIRE(test_ok_src == decltype(test_ok_src){1, 2, 3});
        REQUIRE(eql_counter_type::comparisons == 6);
        eql_counter_type::comparisons = 0;

        REQUIRE(test_std_src != decltype(test_std_src){1, 3, 3});
        REQUIRE(test_ok_src != decltype(test_ok_src){1, 3, 3});
        REQUIRE(eql_counter_type::comparisons == 4);
        eql_counter_type::comparisons = 0;

        const bool a = test_std_src != decltype(test_std_src){3, 3, 3};
        const bool b = test_ok_src != decltype(test_ok_src){3, 3, 3};
        REQUIRE(a);
        REQUIRE(b);
        REQUIRE(eql_counter_type::comparisons == 2);
        eql_counter_type::comparisons = 0;
    }
}
