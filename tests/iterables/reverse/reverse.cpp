#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include <array>

using namespace ok;

TEST_SUITE("reverse")
{
    TEST_CASE("functionality")
    {
        SUBCASE("reverse c style array")
        {
            int forward[3] = {1, 2, 3};

            static_assert(iterable_c<decltype(forward)>);
            static_assert(arraylike_iterable_c<decltype(reverse(forward))>);

            auto reversed = reverse(forward);
            REQUIRE(ok::size(reversed) == ok::size(forward));

            int prev = 4;
            for (int i : reversed) {
                REQUIRE(i < prev);
                REQUIRE(prev - 1 == i);
                prev = i;
            }
        }

        SUBCASE("reversed c style array, checked with enumeration")
        {
            int forward[] = {5, 4, 3, 2, 1, 0};

            auto size_minus = [&](int i) { return ok::size(forward) - 1 - i; };

            for (auto [value, idx] :
                 transform(forward, size_minus).enumerate()) {
                REQUIRE(value == idx);
                const char* sep = idx == ok::size(forward) - 1 ? "\n" : " -> ";
            }

            for (auto [value, idx] : reverse(forward).enumerate()) {
                assert(value == idx);
                const char* sep = idx == ok::size(forward) - 1 ? "\n" : " -> ";
            }
        }

        SUBCASE("reverse 1-sized array")
        {
            std::array<int, 1> forward = {42};

            REQUIRE(ok::size(forward) == ok::size(reverse(forward)));

            size_t counter = 0;
            for (int item : reverse(forward)) {
                REQUIRE(item == 42);
                REQUIRE(counter == 0);
                ++counter;
            }
        }

        SUBCASE("reverse 0-sized array")
        {
            std::array<int, 0> null = {};

            REQUIRE(ok::size(null) == ok::size(reverse(null)));

            for (int item : reverse(null)) {
                REQUIRE(false);
            }
        }

        SUBCASE("take and reverse indices to count backwards")
        {
            auto count_backwards_from_ten =
                indices().take_at_most(10).reverse();

            static_assert(
                arraylike_iterable_c<decltype(count_backwards_from_ten)>);

            for (auto [item, index] : enumerate(count_backwards_from_ten)) {
                REQUIRE(9 - item == index);
            }
        }
    }
}
