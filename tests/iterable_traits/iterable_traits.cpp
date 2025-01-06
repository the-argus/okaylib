#include "test_header.h"
// test header must be first
#include "okay/iterable/iterable.h"
#include "testing_types.h"
#include <array>
#include <vector>

static_assert(ok::detail::is_output_iterable_v<example_iterable_cstyle>);
static_assert(ok::detail::is_output_iterable_v<example_iterable_cstyle_child>);
static_assert(ok::detail::is_random_access_iterable_v<example_iterable_cstyle>);
static_assert(ok::detail::is_valid_cursor_v<size_t>);
static_assert(ok::is_iterable_v<example_iterable_cstyle>);
static_assert(ok::detail::is_valid_sentinel_v<size_t, size_t>);
static_assert(ok::detail::iterable_has_get_ref_v<example_iterable_cstyle>);
static_assert(
    ok::detail::iterable_has_get_ref_const_v<example_iterable_cstyle>);

static_assert(ok::detail::is_random_access_iterable_v<std::vector<int>>);
static_assert(
    std::is_same_v<std::vector<int>::value_type,
                   ok::iterable_definition<std::vector<int>>::value_type>);

using namespace ok;

TEST_SUITE("iterable traits")
{
    TEST_CASE("functionality")
    {
        SUBCASE("iter_get_ref vector")
        {
            std::vector<int> ints = {0, 1, 2, 3, 4};
            for (size_t i = 0; i < ints.size(); ++i) {
                REQUIRE(ok::iter_get_ref(ints, i) == i);
                static_assert(
                    std::is_same_v<decltype(ok::iter_get_ref(ints, i)), int&>);
                ok::iter_get_ref(ints, i) = 0;
                REQUIRE(ints.at(i) == 0);
            }
        }

        SUBCASE("iter_get_ref example iterable")
        {
            example_iterable_cstyle bytes;
            REQUIRE(bytes.size() < 256); // no overflow, store as bits
            for (size_t i = 0; i < bytes.size(); ++i) {
                // initialized to zeroes
                REQUIRE(ok::iter_get_ref(bytes, i) == 0);
                static_assert(
                    std::is_same_v<decltype(ok::iter_get_ref(bytes, i)),
                                   uint8_t&>);
                ok::iter_get_ref(bytes, i) = i;
                REQUIRE(bytes[i] == i);
            }
        }

        SUBCASE("iter_get_ref array")
        {
            std::array<int, 5> arr = {0, 1, 2, 3, 4};
            for (size_t i = 0; i < arr.size(); ++i) {
                REQUIRE(ok::iter_get_ref(arr, i) == i);
                static_assert(
                    std::is_same_v<decltype(ok::iter_get_ref(arr, i)), int&>);
                ok::iter_get_ref(arr, i) = 0;
                REQUIRE(arr.at(i) == 0);
            }
        }

        SUBCASE("iter_get_ref c style array")
        {
            int arr[5] = {0, 1, 2, 3, 4};

            const int arr2[5] = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); ++i) {
                REQUIRE(ok::iter_get_ref(arr, i) == i);
                static_assert(
                    std::is_same_v<decltype(ok::iter_get_ref(arr, i)), int&>);
                ok::iter_get_ref(arr, i) = 0;
                REQUIRE(arr[i] == 0);
            }

            for (size_t i = 0; i < sizeof(arr2) / sizeof(arr2[0]); ++i) {
                const int& iter = ok::iter_get_ref(arr2, i);
                REQUIRE(ok::iter_get_ref(arr2, i) == i);
                REQUIRE(ok::iter_get_ref(arr2, i) == iter);
            }
        }

        SUBCASE("iter_set vector")
        {
            std::vector<int> ints;
            ints.reserve(50);
            for (size_t i = 0; i < 50; ++i) {
                // TODO: requires default construction here before we can set.
                // how does std::back_inserter work? does it avoid this? maybe
                // have some trait for things with emplace functions?
                ints.emplace_back();
                ok::iter_set(ints, i, i);
                REQUIRE(ints.at(i) == i);
            }
            // is iota
            for (size_t i = 0; i < ints.size(); ++i) {
                REQUIRE(ints.at(i) == i);
            }
        }

        SUBCASE("iter_set example iterable")
        {
            example_iterable_cstyle bytes;
            for (size_t i = 0; i < bytes.size(); ++i) {
                ok::iter_set(bytes, i, i);
                REQUIRE(bytes[i] == i);
            }
            // is iota
            for (size_t i = 0; i < bytes.size(); ++i) {
                REQUIRE(bytes[i] == i);
            }
        }

        SUBCASE("iter_set array")
        {
            std::array<int, 50> arr = {};
            for (size_t i = 0; i < arr.size(); ++i) {
                ok::iter_set(arr, i, i);
                REQUIRE(arr.at(i) == i);
            }
            // is iota
            for (size_t i = 0; i < arr.size(); ++i) {
                REQUIRE(arr.at(i) == i);
            }
        }

        SUBCASE("iter_set c-style array")
        {
            int arr[50];
            for (size_t i = 0; i < 50; ++i) {
                ok::iter_set(arr, i, i);
                REQUIRE(arr[i] == i);
            }
            // is iota
            for (size_t i = 0; i < 50; ++i) {
                REQUIRE(arr[i] == i);
            }
        }

        SUBCASE("iter_get_temporary_ref on vector")
        {
            std::vector<int> ints = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < ints.size(); ++i) {
                const int& tref = ok::iter_get_temporary_ref(ints, i);
                REQUIRE(tref == i);
                static_assert(
                    detail::iterable_has_get_ref_const_v<std::vector<int>>);
                static_assert(std::is_same_v<
                                  decltype(ok::iter_get_temporary_ref(ints, i)),
                                  const int&>,
                              "std::vector not giving a const int& for "
                              "temporary reference, specialization is broken?");
            }
        }

        SUBCASE("iter_get_temporary_ref on example iterable")
        {
            example_iterable_cstyle bytes;

            for (size_t i = 0; i < bytes.size(); ++i) {
                const int& tref = ok::iter_get_temporary_ref(bytes, i);
                REQUIRE(tref == 0); // example iterable inits to 0
                static_assert(
                    std::is_same_v<decltype(ok::iter_get_temporary_ref(bytes,
                                                                       i)),
                                   const uint8_t&>,
                    "example iterable not giving a const uint8_t& for "
                    "temporary reference, specialization is broken?");
            }
        }

        SUBCASE("iter_get_temporary_ref on array")
        {
            std::array<int, 5> ints = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < ints.size(); ++i) {
                const int& tref = ok::iter_get_temporary_ref(ints, i);
                REQUIRE(tref == i);
                static_assert(std::is_same_v<
                                  decltype(ok::iter_get_temporary_ref(ints, i)),
                                  const int&>,
                              "std::array not giving a const int& for "
                              "temporary reference, specialization is broken?");
            }
        }

        SUBCASE("iter_copyout array")
        {
            std::array<int, 100> ints;
            for (int i = 0; i < ints.size(); ++i) {
                ints[i] = i;
            }

            for (size_t i = 0; i < ints.size(); ++i) {
                int copied = ok::iter_copyout(ints, i);
                REQUIRE(copied == ok::iter_get_ref(ints, i));
                REQUIRE(copied == i);
            }
        }

        SUBCASE("iter_copyout example iterable")
        {
            example_iterable_cstyle bytes;
            for (int i = 0; i < bytes.size(); ++i) {
                bytes[i] = i;
            }

            for (size_t i = 0; i < bytes.size(); ++i) {
                int copied = ok::iter_copyout(bytes, i);
                REQUIRE(copied == ok::iter_get_ref(bytes, i));
                REQUIRE(copied == i);
            }
        }
    }
}
