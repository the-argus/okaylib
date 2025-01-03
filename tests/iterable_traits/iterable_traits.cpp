#include "test_header.h"
// test header must be first
#include "okay/iterable/traits.h"
#include "testing_types.h"
#include <array>
#include <vector>

static_assert(ok::detail::is_random_access_iterable<std::vector<int>, size_t>);

static_assert(
    ok::detail::is_random_access_iterable<example_iterable_cstyle, size_t>);

static_assert(!ok::detail::has_sentinel_type_meta_t<example_iterable_cstyle>{});
static_assert(std::is_same_v<
              size_t, ok::detail::sentinel_type_for_iterable_and_cursor_meta_t<
                          example_iterable_cstyle, size_t>::type>);
static_assert(ok::detail::is_potential_cursor<size_t>);
static_assert(ok::detail::is_potential_iterable<example_iterable_cstyle>);
static_assert(
    ok::detail::have_sentinel_meta_t<example_iterable_cstyle, size_t>::value);
static_assert(ok::detail::iterable_has_iter_get_ref_array_operator_t<
              example_iterable_cstyle, size_t>{});
static_assert(!ok::detail::iterable_has_iter_get_ref_member_t<
              example_iterable_cstyle, size_t>{});
static_assert(ok::detail::is_input_or_output_cursor_for_iterable<
              example_iterable_cstyle, size_t>);
// implicitly convertible indices should work (depends on compiler options
// though?)
static_assert(ok::detail::is_input_or_output_cursor_for_iterable<
              example_iterable_cstyle, int>);
std::vector<int>::value_type test = 1;
static_assert(std::is_same_v<decltype(test), int>);

static_assert(ok::detail::is_input_or_output_cursor_for_iterable<
              std::vector<int>, size_t>);

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

        SUBCASE("iter_get_ref multiple cursor types")
        {
            example_multiple_cursor_iterable iterable;
            auto iterator = decltype(iterable)::iterator{.actual = 0};
            REQUIRE(ok::iter_get_ref(iterable, iterator) ==
                    example_multiple_cursor_iterable::iterator_cursor_value);
            // multiple cursor iterable thing should give different values for
            // access with iterator and access with size_t
            REQUIRE(ok::iter_get_ref(iterable, iterator) !=
                    ok::iter_get_ref(iterable, 0));
            REQUIRE(
                ok::iter_get_ref(iterable, 0) ==
                example_multiple_cursor_iterable::initial_size_t_cursor_value);
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

        SUBCASE("iter_get_temporary_ref on vector")
        {
            std::vector<int> ints = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < ints.size(); ++i) {
                const int& tref = ok::iter_get_temporary_ref(ints, i);
                REQUIRE(tref == i);
                static_assert(
                    detail::iterable_can_get_const_ref<std::vector<int>,
                                                       size_t>);
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
                    detail::iterable_can_get_const_ref<std::vector<int>,
                                                       size_t>);
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
                static_assert(
                    detail::iterable_can_get_const_ref<std::vector<int>,
                                                       size_t>);
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
                REQUIRE(copied == ok::iter_get_const_ref(ints, i));
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
                REQUIRE(copied == ok::iter_get_const_ref(bytes, i));
                REQUIRE(copied == i);
            }
        }
    }
}
