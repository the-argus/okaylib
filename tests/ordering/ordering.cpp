#include "test_header.h"
// test header must be first
#include "okay/math/ordering.h"

struct int_wrapper
{
    int inner;

    constexpr auto operator<=>(const int_wrapper&) const = default;
};

struct float_wrapper
{
    float inner;

    constexpr auto operator<=>(const float_wrapper&) const = default;
};

struct int_wrapper_okaylib
{
    int inner;

    constexpr ok::ordering operator<=>(const int_wrapper_okaylib& other) const
    {
        if (inner == other.inner) {
            return ok::ordering::equivalent;
        } else if (inner < other.inner) {
            return ok::ordering::less;
        } else {
            return ok::ordering::greater;
        }
    }
};

struct float_wrapper_okaylib
{
    float inner;

    constexpr ok::partial_ordering
    operator<=>(const float_wrapper_okaylib& other) const
    {
        if (inner == other.inner) {
            return ok::partial_ordering::equivalent;
        } else if (inner < other.inner) {
            return ok::partial_ordering::less;
        } else if (inner > other.inner) {
            return ok::partial_ordering::greater;
        } else {
            return ok::partial_ordering::unordered;
        }
    }
};

static_assert(ok::strongly_equality_comparable_c<int>);
static_assert(ok::strongly_equality_comparable_c<int_wrapper>);
static_assert(ok::strongly_equality_comparable_c<int_wrapper_okaylib>);
static_assert(!ok::strongly_equality_comparable_c<float>);
static_assert(!ok::strongly_equality_comparable_c<float_wrapper>);
static_assert(!ok::strongly_equality_comparable_c<float_wrapper_okaylib>);
static_assert(ok::partially_equality_comparable_c<float>);
static_assert(ok::partially_equality_comparable_c<float_wrapper>);
static_assert(ok::partially_equality_comparable_c<float_wrapper_okaylib>);
static_assert(ok::partially_equality_comparable_c<int>);
static_assert(ok::partially_equality_comparable_c<int_wrapper>);
static_assert(ok::partially_equality_comparable_c<int_wrapper_okaylib>);

static_assert(ok::orderable_c<int>);
static_assert(ok::orderable_c<int_wrapper>);
static_assert(ok::orderable_c<int_wrapper_okaylib>);
static_assert(!ok::orderable_c<float>);
static_assert(!ok::orderable_c<float_wrapper_okaylib>);
static_assert(ok::partially_orderable_c<int>);
static_assert(ok::partially_orderable_c<int_wrapper>);
static_assert(ok::partially_orderable_c<int_wrapper_okaylib>);

TEST_SUITE("ordering")
{
    TEST_CASE("conversion from ordering to partial ordering, all explicit")
    {
        ok::partial_ordering test = ok::ordering::greater.as_partial();

        // this shouldnt do conversion bc a partial_ordering -> ordering
        // operator== is defined, and vice versa
        REQUIRE(test == ok::ordering::greater);
        REQUIRE(ok::ordering::greater == test);

        static_assert(!ok::stdc::is_convertible_v<
                      decltype(ok::partial_ordering::unordered),
                      decltype(ok::ordering::less)>);
        static_assert(!ok::stdc::is_convertible_v<
                      decltype(ok::ordering::less),
                      decltype(ok::partial_ordering::unordered)>);
    }

    TEST_CASE("cmp and partial_cmp template deducation")
    {
        REQUIRE(ok::partial_cmp(100.0f, 100.0f) == ok::ordering::equivalent);
        REQUIRE(ok::partial_cmp(100.0f, 100.0f) == ok::ordering::equivalent);
        REQUIRE(ok::partial_cmp(100.0f, 100.0f) == ok::ordering::equivalent);
        REQUIRE(ok::partial_cmp(0.0f / 0.0f, 0.0f / 0.0f) ==
                ok::partial_ordering::unordered);
        REQUIRE(ok::cmp(0, -0) == ok::ordering::equivalent);
        REQUIRE(ok::cmp(1, 2) == ok::ordering::less);
        REQUIRE(ok::cmp(-13, 2) == ok::ordering::less);
        REQUIRE(ok::cmp(432, -942) == ok::ordering::greater);
    }

    TEST_CASE("compare int_wrappers")
    {
        int_wrapper a{0};
        int_wrapper b{1};
        int_wrapper c{1};

        REQUIRE(ok::cmp(a, b) == ok::ordering::less);
        REQUIRE(ok::cmp(b, a) == ok::ordering::greater);
        REQUIRE(ok::cmp(c, b) == ok::ordering::equivalent);
        REQUIRE(ok::is_equal(c, b));
    }

    TEST_CASE("partial compare fully comparable type")
    {
        int_wrapper a{0};
        int_wrapper b{1};
        int_wrapper c{1};

        REQUIRE(ok::partial_cmp(a, b) == ok::ordering::less);
        REQUIRE(ok::partial_cmp(b, a) == ok::ordering::greater);
        REQUIRE(ok::partial_cmp(c, b) == ok::ordering::equivalent);
    }

    TEST_CASE("partial compare float_wrappers")
    {
        float_wrapper a{1};
        float_wrapper b{-123};
        float_wrapper c{234};
        float_wrapper d{1};
        float_wrapper f{0.0f / 0.f};
        float_wrapper g{0.0f / 0.f};

        REQUIRE(ok::partial_cmp(f, g) == ok::partial_ordering::unordered);
        REQUIRE(!ok::is_partial_equal(f, g));
        REQUIRE(!ok::is_partial_equal(g, g));
        REQUIRE(ok::is_partial_equal(d, d));
        REQUIRE(ok::partial_cmp(a, b) == ok::partial_ordering::greater);
        REQUIRE(ok::partial_cmp(d, f) == ok::partial_ordering::unordered);
        REQUIRE(ok::partial_cmp(c, g) == ok::partial_ordering::unordered);
        REQUIRE(ok::partial_cmp(d, c) == ok::partial_ordering::less);
    }

    TEST_CASE("equal comparison volatile int")
    {
        volatile int i = 1;
        volatile int j = 1;
        // make sure this compiles is all
        REQUIRE(ok::is_equal(i, j));
    }

    TEST_CASE("mins and maxs of ints")
    {
        REQUIRE(ok::min(1, 2) == 1);
        REQUIRE(ok::min(1UL, 2UL) == 1UL);
        REQUIRE(ok::min(uint8_t(1), uint8_t(2)) == uint8_t(1));
        REQUIRE(ok::min(int8_t(1), int8_t(2)) == int8_t(1));
        REQUIRE(ok::min(uint16_t(1), uint16_t(2)) == uint16_t(1));
        REQUIRE(ok::min(int16_t(1), int16_t(2)) == int16_t(1));
        REQUIRE(ok::min(uint32_t(1), uint32_t(2)) == uint32_t(1));
        REQUIRE(ok::min(int32_t(1), int32_t(2)) == int32_t(1));
        REQUIRE(ok::min(uint64_t(1), uint64_t(2)) == uint64_t(1));
        REQUIRE(ok::min(int64_t(1), int64_t(2)) == int64_t(1));
        REQUIRE(ok::max(1, 2) == 2);
        REQUIRE(ok::max(1UL, 2UL) == 2UL);
        REQUIRE(ok::max(uint8_t(1), uint8_t(2)) == uint8_t(2));
        REQUIRE(ok::max(int8_t(1), int8_t(2)) == int8_t(2));
        REQUIRE(ok::max(uint16_t(1), uint16_t(2)) == uint16_t(2));
        REQUIRE(ok::max(int16_t(1), int16_t(2)) == int16_t(2));
        REQUIRE(ok::max(uint32_t(1), uint32_t(2)) == uint32_t(2));
        REQUIRE(ok::max(int32_t(1), int32_t(2)) == int32_t(2));
        REQUIRE(ok::max(uint64_t(1), uint64_t(2)) == uint64_t(2));
        REQUIRE(ok::max(int64_t(1), int64_t(2)) == int64_t(2));
        REQUIRE(ok::partial_min(1.f, 2.f).ref_or_panic() == 1.f);
        REQUIRE(ok::partial_max(1.f, 2.f).ref_or_panic() == 2.f);
    }

    TEST_CASE("partial max returns nullopt on undefined comparison")
    {
        bool a = !ok::partial_max(0.f / 0.0f, 10.f).has_value();
        REQUIRE(a);
        bool b = ok::partial_max(0.f, 10.f) == 10.f;
        REQUIRE(b);
        bool c = ok::partial_max(0.f, 10.f).has_value();
        REQUIRE(c);
    }

    TEST_CASE("partial min returns nullopt on undefined comparison")
    {
        REQUIRE(!ok::partial_min(0.f / 0.0f, 10.f).has_value());
        REQUIRE(ok::partial_min(0.f, 10.f) == 0.f);
        REQUIRE(ok::partial_min(0.f, 10.f).has_value());
    }

    TEST_CASE("clamp ints")
    {
        // clamp up
        REQUIRE(ok::clamp(uint8_t(1), {.min = 2, .max = 20}) == uint8_t(2));
        REQUIRE(ok::clamp(int8_t(1), {.min = 2, .max = 20}) == int8_t(2));
        REQUIRE(ok::clamp(uint16_t(1), {.min = 2, .max = 20}) == uint16_t(2));
        REQUIRE(ok::clamp(int16_t(1), {.min = 2, .max = 20}) == int16_t(2));
        REQUIRE(ok::clamp(uint32_t(1), {.min = 2, .max = 20}) == uint32_t(2));
        REQUIRE(ok::clamp(int32_t(1), {.min = 2, .max = 20}) == int32_t(2));
        REQUIRE(ok::clamp(uint64_t(1), {.min = 2, .max = 20}) == uint64_t(2));
        REQUIRE(ok::clamp(int64_t(1), {.min = 2, .max = 20}) == int64_t(2));

        // in range
        REQUIRE(ok::clamp(uint8_t(10), {.min = 2, .max = 20}) == uint8_t(10));
        REQUIRE(ok::clamp(int8_t(10), {.min = 2, .max = 20}) == int8_t(10));
        REQUIRE(ok::clamp(uint16_t(10), {.min = 2, .max = 20}) == uint16_t(10));
        REQUIRE(ok::clamp(int16_t(10), {.min = 2, .max = 20}) == int16_t(10));
        REQUIRE(ok::clamp(uint32_t(10), {.min = 2, .max = 20}) == uint32_t(10));
        REQUIRE(ok::clamp(int32_t(10), {.min = 2, .max = 20}) == int32_t(10));
        REQUIRE(ok::clamp(uint64_t(10), {.min = 2, .max = 20}) == uint64_t(10));
        REQUIRE(ok::clamp(int64_t(10), {.min = 2, .max = 20}) == int64_t(10));

        // clamp down
        REQUIRE(ok::clamp(uint8_t(40), {.min = 2, .max = 20}) == uint8_t(20));
        REQUIRE(ok::clamp(int8_t(40), {.min = 2, .max = 20}) == int8_t(20));
        REQUIRE(ok::clamp(uint16_t(40), {.min = 2, .max = 20}) == uint16_t(20));
        REQUIRE(ok::clamp(int16_t(40), {.min = 2, .max = 20}) == int16_t(20));
        REQUIRE(ok::clamp(uint32_t(40), {.min = 2, .max = 20}) == uint32_t(20));
        REQUIRE(ok::clamp(int32_t(40), {.min = 2, .max = 20}) == int32_t(20));
        REQUIRE(ok::clamp(uint64_t(40), {.min = 2, .max = 20}) == uint64_t(20));
        REQUIRE(ok::clamp(int64_t(40), {.min = 2, .max = 20}) == int64_t(20));
    }

    TEST_CASE("cant clamp invalid direction in debug mode")
    {
        REQUIREABORTS(ok::clamp(10, {.min = -30, .max = -40}));
    }

    TEST_CASE("partial clamp floats")
    {
        REQUIRE(ok::partial_clamp(10.f, {.min = 20.f, .max = 30.f}) == 20.f);
        REQUIRE(ok::partial_clamp(40.f, {.min = 20.f, .max = 30.f}) == 30.f);
        REQUIRE(ok::partial_clamp(25.f, {.min = 20.f, .max = 30.f}) == 25.f);
        REQUIRE(!ok::partial_clamp(0.0f / 0.0f, {.min = 20.f, .max = 30.f})
                     .has_value());
        REQUIRE(!ok::partial_clamp(1.f, {.min = 0.0f / 0.0f, .max = 30.f})
                     .has_value());
        REQUIRE(!ok::partial_clamp(1.f, {.min = 30.f, .max = 0.0f / 0.0f})
                     .has_value());
    }

    TEST_CASE("partial clamp doubles")
    {
        REQUIRE(ok::partial_clamp(10., {.min = 20., .max = 30.}) == 20.);
        REQUIRE(ok::partial_clamp(40., {.min = 20., .max = 30.}) == 30.);
        REQUIRE(ok::partial_clamp(25., {.min = 20., .max = 30.}) == 25.);
        REQUIREABORTS(ok::partial_clamp(0.0 / 0.0, {.min = 20., .max = 30.}));
        REQUIREABORTS(ok::partial_clamp(1., {.min = 0.0 / 0.0, .max = 30.}));
        REQUIREABORTS(ok::partial_clamp(1., {.min = 30., .max = 0.0 / 0.0}));
    }

#if defined(OKAYLIB_USE_FMT)
    TEST_CASE("formatting")
    {
        SUBCASE("print ordering")
        {
            fmt::println("{}", ok::ordering::equivalent);
            fmt::println("{}", ok::ordering::less);
            fmt::println("{}", ok::ordering::greater);
        }
        SUBCASE("print partial ordering")
        {
            fmt::println("{}", ok::partial_ordering::equivalent);
            fmt::println("{}", ok::partial_ordering::less);
            fmt::println("{}", ok::partial_ordering::greater);
            fmt::println("{}", ok::partial_ordering::unordered);
        }
    }
#endif
}
