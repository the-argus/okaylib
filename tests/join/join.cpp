#include "okay/slice.h"
#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/join.h"
// #include "okay/opt.h"
// #include "okay/ranges/views/keep_if.h"
// #include "okay/ranges/views/transform.h"

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

            slice_t<int> arrays[] = {a, b, c};

            static_assert(detail::is_random_access_range_v<decltype(a)>);
            static_assert(detail::is_random_access_range_v<decltype(arrays)>);
            static_assert(
                !detail::is_random_access_range_v<decltype(arrays | join)>);

            size_t counter = 0;
            ok_foreach(int i, arrays | join)
            {
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

            slice_t<int> arrays[] = {a, b, c, d};

            size_t counter = 0;
            ok_foreach(auto&& _, arrays | join) { ++counter; }
            REQUIRE(counter == 3);
        }

        SUBCASE("enumerate with empty slices")
        {
            std::array<int, 0> a = {};
            std::array<int, 0> b = {};
            int c[] = {1, 2, 3};
            std::array<int, 0> d = {};
            slice_t<int> arrays[] = {a, b, c, d};

            ok_foreach(ok_pair(i, index), arrays | join | enumerate)
            {
                REQUIRE(i - 1 == index);
            }
        }

        // SUBCASE("create keep_if/filter by using transform and opt and join")
        // {
        //     int myints[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        //     14};

        //     auto evens_keep_if =
        //         myints | keep_if([](int i) { return i % 2 == 0; });

        //     auto evens_opt_transform = myints |
        //                                transform([](int i) -> opt_t<int> {
        //                                    if (i % 2 == 0)
        //                                        return i;
        //                                    else
        //                                        return {};
        //                                }) |
        //                                join;
        // }
    }
}
