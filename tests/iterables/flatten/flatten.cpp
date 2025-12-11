#include "test_header.h"
// test header must be first
#include "okay/iterables/iterables.h"
#include "okay/slice.h"
#include <array>

using namespace ok;

TEST_SUITE("join")
{
    TEST_CASE("functionality")
    {
        SUBCASE("join three c style arrays")
        {
            int a[3] = {1, 2, 3};
            int b[3] = {1, 2, 3};
            int c[3] = {1, 2, 3};

            slice<int> arrays[] = {a, b, c};

            static_assert(arraylike_iterable_c<decltype(a)&>);
            static_assert(arraylike_iterable_c<decltype(arrays)&>);
            // TODO: fix this, a range should be able to declare that all of its
            // items are ranges which are of the same size
            static_assert(!arraylike_iterable_c<decltype(flatten(arrays))>);

            size_t counter = 0;
            auto&& rng = flatten(arrays);
            using T = decltype(flatten(arrays));
            using namespace detail;
            using U = int&;
            static_assert(iterable_c<T>);
            for (int i : rng) {
                switch (i) {
                case 1:
                    REQUIRE(counter % 3 == 0);
                    break;
                case 2:
                    REQUIRE(counter % 3 == 1);
                    break;
                case 3:
                    REQUIRE(counter % 3 == 2);
                    break;
                default:
                    REQUIRE(false); // unreachable
                }
                ++counter;
            }
        }

        SUBCASE("join some empty slices")
        {
            // TODO: c arrays of size zero don't convert to slices
            std::array<int, 0> a = {};
            std::array<int, 0> b = {};
            int c[] = {1, 2, 3};
            std::array<int, 0> d = {};

            slice<int> arrays[] = {a, b, c, d};

            size_t counter = 0;
            for (auto&& _ : flatten(arrays)) {
                ++counter;
            }
            REQUIRE(counter == 3);
        }

        SUBCASE("enumerate with empty slices")
        {
            std::array<int, 0> a = {};
            std::array<int, 0> b = {};
            int c[] = {1, 2, 3};
            std::array<int, 0> d = {};
            slice<int> arrays[] = {a, b, c, d};

            for (auto [i, index] : flatten(arrays).enumerate()) {
                REQUIRE(i - 1 == index);
            }
        }

        SUBCASE("create keep_if/filter by using transform and opt and join")
        {
            int myints[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

            auto evens_keep_if =
                keep_if(myints, [](int i) { return i % 2 == 0; });

            const auto empty_range_or_even_number = [](int i) -> opt<int> {
                if (i % 2 == 0)
                    return i;
                else
                    return {};
            };

            auto evens_opt_transform =
                transform(myints, empty_range_or_even_number).flatten();

            REQUIRE(iterators_equal(std::move(evens_keep_if),
                                    std::move(evens_opt_transform)));
        }
    }
}
