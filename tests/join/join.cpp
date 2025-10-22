#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/join.h"
#include "okay/ranges/views/keep_if.h"
#include "okay/ranges/views/transform.h"
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

            static_assert(random_access_range_c<decltype(a)>);
            static_assert(random_access_range_c<decltype(arrays)>);
            // i wish... maybe if the sizes of all the arrays were statically
            // known
            static_assert(!random_access_range_c<decltype(arrays | join)>);

            size_t counter = 0;
            auto&& rng = arrays | join;
            using T = decltype(arrays | join);
            using namespace detail;
            using U = int&;
            static_assert(range_has_definition_c<T>);
            static_assert(!range_gets_by_value_c<T>);
            static_assert(range_c<T>);
            static_assert(detail::valid_range_value_type_c<
                          typename range_def_for<T>::value_type>);
            auto t = ok::begin(rng);
            ok_foreach(int i, rng)
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

            slice<int> arrays[] = {a, b, c, d};

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
            slice<int> arrays[] = {a, b, c, d};

            ok_foreach(ok_pair(i, index), arrays | join | enumerate)
            {
                REQUIRE(i - 1 == index);
            }
        }

        SUBCASE("create keep_if/filter by using transform and opt and join")
        {
            int myints[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

            auto evens_keep_if =
                myints | keep_if([](int i) { return i % 2 == 0; });

            const auto empty_range_or_even_number =
                transform([](int i) -> opt<int> {
                    if (i % 2 == 0)
                        return i;
                    else
                        return {};
                });

            auto evens_opt_transform =
                myints | empty_range_or_even_number | join;

            auto begin_keep_if = ok::begin(evens_keep_if);
            auto begin_transform = ok::begin(evens_opt_transform);

            while (ok::is_inbounds(evens_keep_if, begin_keep_if)) {
                __ok_assert(
                    ok::is_inbounds(evens_opt_transform, begin_transform),
                    "keep_if and transform -> opt | join are not equivalent");

                auto&& a = ok::range_get(evens_keep_if, begin_keep_if);
                auto&& b = ok::range_get(evens_opt_transform, begin_transform);

                REQUIRE(a == b);

                ok::increment(evens_keep_if, begin_keep_if);
                ok::increment(evens_opt_transform, begin_transform);
            }
        }
    }
}
