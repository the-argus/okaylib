#include "test_header.h"
// test header must be first
#include "okay/math/ordering.h"

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
    }
}
