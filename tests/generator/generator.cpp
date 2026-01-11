#include "test_header.h"
// test header must be first
#include "okay/async/generator.h"

using namespace ok;

auto mkcoro() -> generator_t<int>
{
    for (int i = 0; i < 10; ++i)
        co_yield i;
}

TEST_SUITE("generator")
{
    TEST_CASE("ints")
    {
        generator_t ints = mkcoro();

        for (int i = 0; i < 10; ++i) {
            REQUIRE((ints.next() == i));
        }

        REQUIRE(!ints.next());
    }
}
