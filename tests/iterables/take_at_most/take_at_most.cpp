#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include "testing_types.h"
#include <array>

using namespace ok;

TEST_SUITE("take_at_most")
{
    TEST_CASE("take_at_most of array is still an array of known length")
    {
        std::array<int, 50> array;
        static_assert(arraylike_iterable_c<decltype(array)>);
        static_assert(arraylike_iterable_c<decltype(take_at_most(array, 25))>);
        REQUIRE(ok::size(take_at_most(array, 25)) == 25);
    }

    TEST_CASE("take_at_most size does not overflow, clamps at zero")
    {
        std::array<int, 50> array;
        REQUIRE(ok::size(take_at_most(array, 0)) == 0);
    }

    TEST_CASE("take_at_most size does not exceed container size")
    {
        std::array<int, 50> array;
        REQUIRE(
            ok::size(take_at_most(array, std::numeric_limits<size_t>::max())) ==
            array.size());
    }

    TEST_CASE("take_at_most of a forward iterable whose size is not known")
    {
        using container_t =
            forward_iterable_size_test_t<size_mode::unknown_sized>;
        container_t container;
        static_assert(iterable_c<decltype(container)>);
        auto half_iterator = container.iter().take_at_most(25);
        static_assert(iterable_c<decltype(half_iterator)>);

        size_t counter = 0;
        for (auto&& _ : half_iterator) {
            ++counter;
        }
        REQUIRE(counter == 25);
    }

    TEST_CASE("take_at_most of a forward iterable whose size *is* known")
    {
        using container_t =
            forward_iterable_size_test_t<size_mode::known_sized>;
        container_t container;
        static_assert(iterable_c<decltype(container)>);
        auto half_iterator = container.iter().take_at_most(25);
        static_assert(iterable_c<decltype(half_iterator)>);

        REQUIRE(ok::size(half_iterator) == 25);

        size_t counter = 0;
        for (auto&& _ : half_iterator) {
            ++counter;
        }
        REQUIRE(counter == 25);
    }

    TEST_CASE("take subset of indices")
    {
        size_t counter = 0;
        for (auto i : ok::indices().take_at_most(10)) {
            REQUIRE(i == counter);
            ++counter;
        }
    }
}
