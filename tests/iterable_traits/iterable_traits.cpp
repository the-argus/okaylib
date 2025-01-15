#include "okay/iterable/std_for.h"
#include "test_header.h"
// test header must be first
#include "okay/iterable/iterable.h"
#include "okay/macros/foreach.h"
#include "testing_types.h"
#include <array>
#include <vector>

#include "okay/iterable/enumerate.h"
#include "okay/slice.h"

static_assert(ok::detail::is_random_access_iterable_v<ok::slice_t<int>>);
static_assert(ok::detail::is_input_iterable_v<ok::slice_t<int>>);
static_assert(ok::detail::is_input_iterable_v<ok::slice_t<const int>>);
static_assert(ok::detail::is_output_iterable_v<ok::slice_t<int>>);
static_assert(!ok::detail::is_output_iterable_v<ok::slice_t<const int>>);

static_assert(ok::detail::is_output_iterable_v<example_iterable_cstyle>);
static_assert(ok::detail::is_output_iterable_v<example_iterable_bidirectional>);
static_assert(ok::detail::is_input_iterable_v<example_iterable_bidirectional>);
static_assert(
    ok::detail::is_bidirectional_iterable_v<example_iterable_bidirectional>);
static_assert(
    !ok::detail::is_random_access_iterable_v<example_iterable_bidirectional>);
static_assert(ok::detail::is_output_iterable_v<example_iterable_cstyle_child>);
static_assert(ok::detail::is_random_access_iterable_v<example_iterable_cstyle>);
static_assert(ok::detail::is_valid_cursor_v<size_t>);
static_assert(ok::is_iterable_v<example_iterable_cstyle>);
static_assert(ok::detail::iterable_has_get_ref_v<example_iterable_cstyle>);
static_assert(
    ok::detail::iterable_has_get_ref_const_v<example_iterable_cstyle>);

static_assert(ok::detail::has_baseline_functions_v<std::vector<int>>);
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

    TEST_CASE("begin and end")
    {
        SUBCASE("ok::begin() on array")
        {
            int cstyle_array[500];
            // array's cursor type is size_t
            static_assert(
                std::is_same_v<ok::cursor_type_for<decltype(cstyle_array)>,
                               size_t>);

            // begin for arrays always returns 0 for the index of first elem
            static_assert(ok::begin(cstyle_array) == 0);
            size_t begin = ok::begin(cstyle_array);
            REQUIRE(begin == 0);
        }

        SUBCASE("ok::begin() on user defined type with begin() definition")
        {
            example_iterable_cstyle_child begin_able;
            size_t begin = ok::begin(begin_able);
        }

        SUBCASE("ok::begin() on example iterable with free function begin()")
        {
            using cursor_t = size_t;
            using iterable_t = example_iterable_cstyle;
            iterable_t iterable;
            cursor_t begin = ok::begin(iterable);
            static_assert(ok::begin(iterable) == 0);
            REQUIRE(begin == 0);
        }

        SUBCASE("ok::begin() and ok::end() on c style array")
        {
            int myints[500];
            static_assert(ok::is_inbounds(myints, 499));
            static_assert(!ok::is_inbounds(myints, 500));
            static_assert(ok::begin(myints) == 0);

            for (size_t i = ok::begin(myints); ok::is_inbounds(myints, i);
                 ++i) {
                REQUIRE((i >= 0 && i < 500));
                myints[i] = i;
            }
        }

        SUBCASE("ok::begin() and ok::end() on simple iterable")
        {
            example_iterable_cstyle iterable;
            REQUIRE(!ok::is_inbounds(iterable, iterable.size()));
            REQUIRE(ok::begin(iterable) == 0);

            for (size_t i = ok::begin(iterable); ok::is_inbounds(iterable, i);
                 ++i) {
                REQUIRE((i >= 0 && i < 100)); // NOTE: 100 is size always
                iterable[i] = i;
            }
            // sanity check :)
            REQUIRE(iterable[50] == 50);
        }
    }

    TEST_CASE("foreach loop")
    {
        SUBCASE("foreach loop c array no macro")
        {
            int myints[500];

            for (size_t i = ok::begin(myints); ok::is_inbounds(myints, i);
                 ++i) {
                int& iter = myints[i];

                iter = i;
            }

            for (size_t i = 0; i < sizeof(myints) / sizeof(int); ++i) {
                REQUIRE(myints[i] == i);
            }
        }

        SUBCASE("foreach loop c array no macro, prefer_after")
        {
            int myints[500];

            for (size_t i = ok::begin(myints);
                 ok::is_inbounds(myints, i, ok::prefer_after_bounds_check_t{});
                 ++i) {
                int& iter = myints[i];

                iter = i;
            }

            for (size_t i = 0; i < sizeof(myints) / sizeof(int); ++i) {
                REQUIRE(myints[i] == i);
            }
        }

        SUBCASE("foreach loop is_before_bounds and is_after_bounds type, no "
                "macro, prefer_after")
        {
            example_iterable_bidirectional bytes;

            for (auto i = ok::begin(bytes);
                 ok::is_inbounds(bytes, i, ok::prefer_after_bounds_check_t{});
                 ++i) {
                uint8_t& iter = bytes.get(i);

                iter = i.inner();
            }

            for (auto i = ok::begin(bytes); ok::is_inbounds(bytes, i); ++i) {
                uint8_t& iter = bytes.get(i);

                REQUIRE(iter == i.inner());
            }
        }

        SUBCASE("foreach loop c array with macro")
        {
            int myints[500];

            const int& ref = ok::iter_get_ref(myints, 0UL);

            ok_foreach(int& i, myints) i = 20;

            ok_foreach(const int& i, myints) { REQUIRE(i == 20); }

            auto check_in_lambda = [](const int(&array)[500]) {
                ok_foreach(const int& i, array) { REQUIRE(i == 20); }
            };

            check_in_lambda(myints);
        }

        SUBCASE("foreach loop user defined type with wrapper")
        {
            example_iterable_cstyle bytes;

            for (uint8_t& i : ok::std_for(bytes))
                i = 20;

            for (const uint8_t& i : ok::std_for(bytes)) {
                REQUIRE(i == 20);
            }
        }

        SUBCASE("enumerated foreach loop")
        {
            example_iterable_cstyle bytes;

            for (uint8_t& i : ok::std_for(bytes))
                i = 20;

            // instantiated twice to make sure duplicate labels dont happen
            ok_foreach(ok_pair(byte, index), ok::enumerate(bytes))
            {
                REQUIRE(byte == 20);
            }
            ok_foreach(ok_pair(byte, index), ok::enumerate(bytes))
            {
                REQUIRE(byte == 20);
            }
        }
    }
}
