#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include <array>

using namespace ok;

TEST_SUITE("transform")
{
    TEST_CASE("functionality")
    {
        SUBCASE("identity transform")
        {
            std::array<int, 50> ints = {};

            iterators_copy_assign(ints, indices());

            auto identity = [](int i) { return i; };

            REQUIRE(iterators_equal(ints, transform(ints, identity)));
        }

        SUBCASE("squared view with std::array")
        {
            constexpr auto squared = [](auto i) { return i * i; };

            std::array<int, 50> ints;
            iterators_copy_assign(ints, indices());

            size_t c = 0;
            for (const int i : transform(ints, squared)) {
                REQUIRE(i == c * c);
                ++c;
            }
            REQUIRE(c == 50);
        }

        SUBCASE("squared view with rvalue std::array")
        {
            constexpr auto squared = [](auto i) { return i * i; };
            std::array<int, 50> ints;
            iterators_copy_assign(ints, indices());

            size_t c = 0;
            for (const int i : transform(std::move(ints), squared)) {
                REQUIRE(i == c * c);
                ++c;
            }
            REQUIRE(c == 50);
        }

        SUBCASE("squared view with c-style array")
        {
            constexpr auto squared = [](auto i) { return i * i; };

            int ints[50];
            iterators_copy_assign(ints, indices());

            size_t c = 0;
            for (const int i : transform(ints, squared)) {
                REQUIRE(i == c * c);
                ++c;
            }
            REQUIRE(c == 50);
        }

        SUBCASE("can still get the size of transformed things")
        {
            constexpr auto squared = [](auto i) { return i * i; };
            std::array<int, 50> stdarray;
            int carray[35];
            std::vector<int> vector;
            vector.resize(25);

            const size_t arraysize = ok::size(stdarray);
            const size_t carraysize = ok::size(carray);
            const size_t vectorsize = ok::size(vector);

            REQUIRE(ok::size(transform(stdarray, squared)) == arraysize);
            REQUIRE(ok::size(transform(carray, squared)) == carraysize);
            REQUIRE(ok::size(transform(vector, squared)) == vectorsize);
        }
    }
}
