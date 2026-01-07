#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/iterables/algorithm.h"
#include "okay/iterables/indices.h"

#include <array>

using namespace ok;

TEST_SUITE("ok::ranges_copy and ok::ranges_copy_as_much_as_will_fit algorithms")
{
    constexpr auto is_even = [](size_t i) { return i % 2 == 0; };

    TEST_CASE("fill std::array withg indices")
    {
        std::array<int, 50> ints = {};
        iterators_copy_assign(ints, indices());
        REQUIRE(iterators_equal(ints, indices()));
    }

    TEST_CASE("copy from one array to another")
    {
        maybe_undefined_array_t a{1, 2, 3, 4, 5, 6};
        zeroed_array_t<int, 20> b;

        iterators_copy_assign(b, a);

        REQUIRE(iterators_equal(take_at_most(b, a.size()), a));
        bool all_zero =
            iter(b).drop(a.size()).all_satisfy([](auto i) { return i == 0; });
        REQUIRE(all_zero);
    }

    TEST_CASE("copy from infinite -> finite or sized")
    {
        SUBCASE("indices into sized (array)")
        {
            maybe_undefined_array_t<int, 5> array;
            iterators_copy_assign(array, indices());

            REQUIRE(
                iterators_equal(array, maybe_undefined_array_t{0, 1, 2, 3, 4}));
        }

        SUBCASE("indices into iterator of unknown size")
        {
            zeroed_array_t<int, 10> array;

            auto finite_view = iter(array)
                                   .enumerate()
                                   .keep_if([](auto pair) -> bool {
                                       auto [item, index] = pair;
                                       return index % 2 == 0;
                                   })
                                   .transform([](auto pair) -> int& {
                                       auto [item, index] = pair;
                                       return item;
                                   });
            static_assert(!detail::sized_iterator_c<decltype(finite_view)>);

            iterators_copy_assign(finite_view, indices());

            // only affect every other item
            REQUIRE(iterators_equal(
                array, maybe_undefined_array_t{0, 0, 1, 0, 2, 0, 3, 0, 4, 0}));

            for (auto& [lhs, rhs] : zip(finite_view, indices()))
                lhs = rhs;

            REQUIRE(iterators_equal(
                array, maybe_undefined_array_t{0, 0, 1, 0, 2, 0, 3, 0, 4, 0}));
        }
    }

    TEST_CASE("copy from finite to finite")
    {
        ok::maybe_undefined_array_t a = {0, 1, 2, 3, 4};
        auto finite_view = iter(a)
                               .enumerate()
                               .keep_if([](auto pair) -> bool {
                                   auto [item, index] = pair;
                                   return index % 2 == 0;
                               })
                               .transform([](auto pair) -> int& {
                                   auto [item, index] = pair;
                                   return item;
                               });

        auto finite_input = iter(a).keep_if([](int i) { return i % 2 == 1; });

        iterators_copy_assign(finite_view, finite_input);

        REQUIRE(iterators_equal(a, maybe_undefined_array_t{1, 1, 3, 3, 4}));
    }
}
