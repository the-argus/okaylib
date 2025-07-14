#include "test_header.h"
// test header must be first
#include "okay/tuple.h"
#include <tuple>

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
static_assert(std::is_trivially_copyable_v<ok::tuple<int, int, int>>);
static_assert(std::is_trivially_destructible_v<ok::tuple<int, int, int>> ==
              std::is_trivially_destructible_v<std::tuple<int, int, int>>);
static_assert(
    std::is_trivially_destructible_v<ok::tuple<undestructible_t, int, int>> ==
    std::is_trivially_destructible_v<std::tuple<undestructible_t, int, int>>);
static_assert(
    std::is_trivially_destructible_v<ok::tuple<noncopy_t, int, int>> ==
    std::is_trivially_destructible_v<std::tuple<noncopy_t, int, int>>);

/// std::tuple doesn't make this guarantee, but we do
static_assert(std::is_trivially_copyable_v<ok::tuple<int, int, int>>);

TEST_SUITE("tuple")
{
    TEST_CASE("construct tuple")
    {
        std::tuple<int> test = 1;
        ok::tuple<int> test2 = 1;
        std::tuple<int, int> test3 = {1, 1};
        ok::tuple<int, int> test4 = {1, 1};
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
}
