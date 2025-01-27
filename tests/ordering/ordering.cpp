#include "test_header.h"
// test header must be first
#include "okay/math/ordering.h"

struct int_wrapper
{
    int inner;
};

template <> struct ok::orderable_definition<int_wrapper>
{
    static constexpr ordering cmp(const int_wrapper& lhs,
                                  const int_wrapper& rhs) noexcept
    {
        if (lhs.inner == rhs.inner) {
            return ordering::equivalent;
        } else if (lhs.inner > rhs.inner) {
            return ordering::greater;
        } else {
            return ordering::less;
        }
    }
};

struct float_wrapper
{
    float inner;
};

template <> struct ok::partially_orderable_definition<float_wrapper>
{
    static constexpr partial_ordering
    partial_cmp(const float_wrapper& lhs, const float_wrapper& rhs) noexcept
    {
        if (lhs.inner == rhs.inner) {
            return partial_ordering::equivalent;
        } else if (lhs.inner > rhs.inner) {
            return partial_ordering::greater;
        } else if (lhs.inner < rhs.inner) {
            return partial_ordering::less;
        } else {
            return partial_ordering::unordered;
        }
    }
};

TEST_SUITE("ordering")
{
    TEST_CASE("type behavior")
    {
        SUBCASE(
            "conversion from ordering to partial ordering, not the other way")
        {
            ok::partial_ordering test = ok::ordering::greater;

            // this shouldnt do conversion bc a partial_ordering -> ordering
            // operator== is defined, and vice versa
            REQUIRE(test == ok::ordering::greater);
            REQUIRE(ok::ordering::greater == test);

            static_assert(!std::is_convertible_v<
                          decltype(ok::partial_ordering::unordered),
                          decltype(ok::ordering::less)>);
        }

        SUBCASE("cmp and partial_cmp template deducation")
        {
            REQUIRE(ok::partial_cmp(100.0f, 100.0f) ==
                    ok::ordering::equivalent);
            REQUIRE(ok::partial_cmp(100.0f, 100) == ok::ordering::equivalent);
            REQUIRE(ok::cmp(100, 100.f) == ok::ordering::equivalent);
            REQUIRE(ok::partial_cmp(0.0f / 0.0f, 0.0f / 0.0f) ==
                    ok::partial_ordering::unordered);
            REQUIRE(ok::cmp(0, -0) == ok::ordering::equivalent);
            REQUIRE(ok::cmp(1, 2) == ok::ordering::less);
            REQUIRE(ok::cmp(-13, 2) == ok::ordering::less);
            REQUIRE(ok::cmp(432, -942) == ok::ordering::greater);
        }

        SUBCASE("compare int_wrappers")
        {
            int_wrapper a{0};
            int_wrapper b{1};
            int_wrapper c{1};

            REQUIRE(ok::cmp(a, b) == ok::ordering::less);
            REQUIRE(ok::cmp(b, a) == ok::ordering::greater);
            REQUIRE(ok::cmp(c, b) == ok::ordering::equivalent);
            REQUIRE(ok::equal(c, b));
        }

        SUBCASE("partial compare fully comparable type")
        {
            int_wrapper a{0};
            int_wrapper b{1};
            int_wrapper c{1};

            REQUIRE(ok::partial_cmp(a, b) == ok::ordering::less);
            REQUIRE(ok::partial_cmp(b, a) == ok::ordering::greater);
            REQUIRE(ok::partial_cmp(c, b) == ok::ordering::equivalent);
        }

        SUBCASE("partial compare float_wrappers")
        {
            float_wrapper a{1};
            float_wrapper b{-123};
            float_wrapper c{234};
            float_wrapper d{1};
            float_wrapper f{0.0f / 0.f};
            float_wrapper g{0.0f / 0.f};

            REQUIRE(ok::partial_cmp(f, g) == ok::partial_ordering::unordered);
            REQUIRE(!ok::partial_equal(f, g));
            REQUIRE(!ok::partial_equal(g, g));
            REQUIRE(ok::partial_equal(d, d));
            REQUIRE(ok::partial_cmp(a, b) == ok::partial_ordering::greater);
            REQUIRE(ok::partial_cmp(d, f) == ok::partial_ordering::unordered);
            REQUIRE(ok::partial_cmp(c, g) == ok::partial_ordering::unordered);
            REQUIRE(ok::partial_cmp(d, c) == ok::partial_ordering::less);
        }
    }

#ifdef OKAYLIB_USE_FMT
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
