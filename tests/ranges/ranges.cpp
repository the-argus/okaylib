#include "test_header.h"
// test header must be first
#include "okay/detail/type_traits.h"
#include "okay/macros/foreach.h"
#include "okay/ranges/ranges.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/std_for.h"
#include "okay/slice.h"
#include "testing_types.h"
#include <array>
#include <functional>
#include <vector>

struct MemberPointerTester
{
    int i;
    float f;

    void one();
    float two();
};

// tests to make sure ok::invoke matches std::invoke
static_assert(std::is_same_v<
              decltype(std::invoke(ok::detail::range_get_ref_fn_t{},
                                   std::declval<int (&)[500]>(), size_t{})),
              decltype(ok::invoke(ok::detail::range_get_ref_fn_t{},
                                  std::declval<int (&)[500]>(), size_t{}))>);
static_assert(
    std::is_same_v<
        decltype(std::invoke(ok::detail::range_get_ref_fn_t{},
                             std::declval<const int (&)[500]>(), size_t{})),
        decltype(ok::invoke(ok::detail::range_get_ref_fn_t{},
                            std::declval<const int (&)[500]>(), size_t{}))>);
static_assert(std::is_same_v<
              decltype(std::invoke(&MemberPointerTester::one,
                                   std::declval<MemberPointerTester&>())),
              decltype(std::invoke(&MemberPointerTester::one,
                                   std::declval<MemberPointerTester&>()))>);

static_assert(std::is_same_v<
              decltype(std::invoke(&MemberPointerTester::two,
                                   std::declval<MemberPointerTester&>())),
              decltype(std::invoke(&MemberPointerTester::two,
                                   std::declval<MemberPointerTester&>()))>);

static_assert(std::is_same_v<
              decltype(std::invoke(&MemberPointerTester::i,
                                   std::declval<MemberPointerTester&>())),
              decltype(std::invoke(&MemberPointerTester::i,
                                   std::declval<MemberPointerTester&>()))>);

static_assert(std::is_same_v<
              decltype(std::invoke(&MemberPointerTester::f,
                                   std::declval<MemberPointerTester&>())),
              decltype(std::invoke(&MemberPointerTester::f,
                                   std::declval<MemberPointerTester&>()))>);

using namespace ok::detail;

ok::range_def_for<const int[500]> array_instantiation;
static_assert(std::is_same_v<ok::value_type_for<const int[500]>, int>);
static_assert(
    std::is_same_v<decltype(ok::range_get(std::declval<const int[500]>(), 0)),
                   const int&>);
static_assert(ok::detail::range_impls_const_get_c<const int[500]>);

static_assert(ok::random_access_range_c<ok::slice<int>>);
static_assert(ok::detail::producing_range_c<ok::slice<int>>);

static_assert(ok::detail::producing_range_c<ok::slice<const int>>);
static_assert(ok::detail::consuming_range_c<ok::slice<int>>);
static_assert(!ok::detail::range_impls_construction_set_c<ok::slice<const int>,
                                                          const int&>);
static_assert(ok::is_const_c<ok::slice<const int>::viewed_type>);
static_assert(
    std::is_same_v<decltype(ok::range_get(
                       ok::stdc::declval<const ok::slice<const int>&>(), 0)),
                   const int&>);
static_assert(
    std::is_same_v<
        decltype(ok::range_get(ok::stdc::declval<ok::slice<const int>&>(), 0)),
        const int&>);
static_assert(
    std::is_same_v<ok::value_type_for<ok::slice<const int>>, const int>);
// get ref is explicitly for nonconst overload
static_assert(
    ok::detail::range_gets_const_from_nonconst_c<std::array<const int, 1>>);
static_assert(
    ok::detail::range_gets_const_from_nonconst_c<ok::slice<const int>>);
static_assert(!ok::detail::range_impls_construction_set_c<ok::slice<const int>,
                                                          const int&>);
static_assert(!ok::detail::consuming_range_c<ok::slice<const int>>);

static_assert(ok::detail::consuming_range_c<example_range_cstyle>);
static_assert(range_marked_finite_c<example_range_bidirectional>);
static_assert(
    ok::detail::common_range_requirements_c<example_range_bidirectional>);
static_assert(ok::range_c<example_range_bidirectional>);
static_assert(ok::detail::consuming_range_c<example_range_bidirectional>);
static_assert(ok::detail::producing_range_c<example_range_bidirectional>);
static_assert(ok::bidirectional_range_c<example_range_bidirectional>);
static_assert(!ok::random_access_range_c<example_range_bidirectional>);
static_assert(ok::detail::consuming_range_c<example_range_cstyle_child>);
static_assert(ok::random_access_range_c<example_range_cstyle>);
static_assert(ok::range_c<example_range_cstyle>);
static_assert(!ok::detail::range_gets_by_value_c<example_range_cstyle>);
static_assert(
    !ok::detail::range_gets_const_from_nonconst_c<example_range_cstyle>);
static_assert(ok::detail::common_range_requirements_c<std::vector<int>>);
static_assert(ok::random_access_range_c<std::vector<int>>);
static_assert(
    ok::same_as_c<std::vector<int>::value_type,
                  ok::range_definition<std::vector<int>>::value_type>);

using namespace ok;

TEST_SUITE("range traits")
{
    TEST_CASE("functionality")
    {
        SUBCASE("range_get_ref vector")
        {
            std::vector<int> ints = {0, 1, 2, 3, 4};
            for (size_t i = 0; i < ints.size(); ++i) {
                REQUIRE(ok::range_get_ref(ints, i) == i);
                static_assert(
                    ok::same_as_c<decltype(ok::range_get_ref(ints, i)), int&>);
                ok::range_get_ref(ints, i) = 0;
                REQUIRE(ints.at(i) == 0);
            }
        }

        SUBCASE("range_get_ref aborts when out of bounds")
        {
            std::vector<int> ints = {0, 1, 2, 3, 4};
            int c_ints[] = {0, 1, 2, 3, 4};

            REQUIREABORTS(auto& ref = ok::range_get_ref(ints, 10));
            REQUIREABORTS(auto& ref = ok::range_get_ref(c_ints, 10));
        }

        SUBCASE("range_get_ref example range")
        {
            example_range_cstyle bytes;
            REQUIRE(bytes.size() < 256); // no overflow, store as bits
            for (size_t i = 0; i < bytes.size(); ++i) {
                // initialized to zeroes
                REQUIRE(ok::range_get_ref(bytes, i) == 0);
                static_assert(
                    std::is_same_v<decltype(ok::range_get_ref(bytes, i)),
                                   uint8_t&>);
                ok::range_get_ref(bytes, i) = i;
                REQUIRE(bytes[i] == i);
            }
        }

        SUBCASE("range_get_ref array")
        {
            std::array<int, 5> arr = {0, 1, 2, 3, 4};
            for (size_t i = 0; i < arr.size(); ++i) {
                REQUIRE(ok::range_get_ref(arr, i) == i);
                static_assert(
                    std::is_same_v<decltype(ok::range_get_ref(arr, i)), int&>);
                ok::range_get_ref(arr, i) = 0;
                REQUIRE(arr.at(i) == 0);
            }
        }

        SUBCASE("range_get_ref c style array")
        {
            int arr[5] = {0, 1, 2, 3, 4};

            const int arr2[5] = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); ++i) {
                REQUIRE(ok::range_get_ref(arr, i) == i);
                static_assert(
                    std::is_same_v<decltype(ok::range_get_ref(arr, i)), int&>);
                ok::range_get_ref(arr, i) = 0;
                REQUIRE(arr[i] == 0);
            }

            for (size_t i = 0; i < sizeof(arr2) / sizeof(arr2[0]); ++i) {
                const int& iter = ok::range_get_ref(arr2, i);
                REQUIRE(ok::range_get_ref(arr2, i) == i);
                REQUIRE(ok::range_get_ref(arr2, i) == iter);
            }
        }

        SUBCASE("range_set vector")
        {
            std::vector<int> ints;
            ints.reserve(50);
            for (size_t i = 0; i < 50; ++i) {
                // TODO: requires default construction here before we can set.
                // how does std::back_inserter work? does it avoid this? maybe
                // have some trait for things with emplace functions?
                ints.emplace_back();
                ok::range_set(ints, i, i);
                REQUIRE(ints.at(i) == i);
            }
            // is iota
            for (size_t i = 0; i < ints.size(); ++i) {
                REQUIRE(ints.at(i) == i);
            }
        }

        SUBCASE("range_set example range")
        {
            example_range_cstyle bytes;
            for (size_t i = 0; i < bytes.size(); ++i) {
                ok::range_set(bytes, i, i);
                REQUIRE(bytes[i] == i);
            }
            // is iota
            for (size_t i = 0; i < bytes.size(); ++i) {
                REQUIRE(bytes[i] == i);
            }
        }

        SUBCASE("range_set array")
        {
            std::array<int, 50> arr = {};
            for (size_t i = 0; i < arr.size(); ++i) {
                ok::range_set(arr, i, i);
                REQUIRE(arr.at(i) == i);
            }
            // is iota
            for (size_t i = 0; i < arr.size(); ++i) {
                REQUIRE(arr.at(i) == i);
            }
        }

        SUBCASE("range_set c-style array")
        {
            int arr[50];
            for (size_t i = 0; i < 50; ++i) {
                ok::range_set(arr, i, i);
                REQUIRE(arr[i] == i);
            }
            // is iota
            for (size_t i = 0; i < 50; ++i) {
                REQUIRE(arr[i] == i);
            }
        }

        SUBCASE("range_get_temporary_ref on vector")
        {
            std::vector<int> ints = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < ints.size(); ++i) {
                const int& tref = ok::range_get(ints, i);
                REQUIRE(tref == i);
                static_assert(
                    range_gets_by_ref_matching_constness_c<std::vector<int>>);
                static_assert(std::is_same_v<decltype(ok::range_get(ints, i)),
                                             const int&>,
                              "std::vector not giving a const int& for "
                              "temporary reference, specialization is broken?");
            }
        }

        SUBCASE("range_get on example range")
        {
            example_range_cstyle bytes;

            for (size_t i = 0; i < bytes.size(); ++i) {
                const int& tref = ok::range_get(bytes, i);
                REQUIRE(tref == 0); // example range inits to 0
                static_assert(std::is_same_v<decltype(ok::range_get(bytes, i)),
                                             const uint8_t&>,
                              "example range not giving a const uint8_t& for "
                              "temporary reference, specialization is broken?");
            }
        }

        SUBCASE("range_get on array")
        {
            std::array<int, 5> ints = {0, 1, 2, 3, 4};

            for (size_t i = 0; i < ints.size(); ++i) {
                const int& tref = ok::range_get(ints, i);
                REQUIRE(tref == i);
                static_assert(std::is_same_v<decltype(ok::range_get(ints, i)),
                                             const int&>,
                              "std::array not giving a const int& for "
                              "temporary reference, specialization is broken?");
            }
        }

        SUBCASE("range_get all of array")
        {
            std::array<int, 100> ints;
            for (int i = 0; i < ints.size(); ++i) {
                ints[i] = i;
            }

            for (size_t i = 0; i < ints.size(); ++i) {
                int copied = ok::range_get(ints, i);
                REQUIRE(copied == ok::range_get_ref(ints, i));
                REQUIRE(copied == i);
            }
        }

        SUBCASE("range_get all of example range")
        {
            example_range_cstyle bytes;
            for (int i = 0; i < bytes.size(); ++i) {
                bytes[i] = i;
            }

            for (size_t i = 0; i < bytes.size(); ++i) {
                int copied = ok::range_get(bytes, i);
                REQUIRE(copied == ok::range_get_ref(bytes, i));
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
            example_range_cstyle_child begin_able;
            size_t begin = ok::begin(begin_able);
        }

        SUBCASE("ok::begin() on example range with free function begin()")
        {
            using cursor_t = size_t;
            using range_t = example_range_cstyle;
            range_t range;
            cursor_t begin = ok::begin(range);
            static_assert(ok::begin(range) == 0);
            REQUIRE(begin == 0);
        }

        SUBCASE("ok::begin() and ok::end() on c style array")
        {
            int myints[500];
            static_assert(ok::is_inbounds(myints, 499));
            static_assert(!ok::is_inbounds(myints, 500));
            static_assert(ok::begin(myints) == 0);

            for (size_t i = ok::begin(myints); ok::is_inbounds(myints, i);
                 ok::increment(myints, i)) {
                REQUIRE((i >= 0 && i < 500));
                myints[i] = i;
            }
        }

        SUBCASE("ok::begin() and ok::end() on simple range")
        {
            example_range_cstyle range;
            REQUIRE(!ok::is_inbounds(range, range.size()));
            REQUIRE(ok::begin(range) == 0);

            for (size_t i = ok::begin(range); ok::is_inbounds(range, i);
                 ok::increment(range, i)) {
                REQUIRE((i >= 0 && i < 100)); // NOTE: 100 is size always
                range[i] = i;
            }
            // sanity check :)
            REQUIRE(range[50] == 50);
        }
    }

    TEST_CASE("foreach loop")
    {
        SUBCASE("foreach loop c array no macro")
        {
            int myints[500];

            for (size_t i = ok::begin(myints); ok::is_inbounds(myints, i);
                 ok::increment(myints, i)) {
                int& iter = myints[i];

                iter = i;
            }

            for (size_t i = 0; i < sizeof(myints) / sizeof(int); ++i) {
                REQUIRE(myints[i] == i);
            }
        }

        SUBCASE("foreach loop c array no macro")
        {
            int myints[500];

            for (size_t i = ok::begin(myints); ok::is_inbounds(myints, i);
                 ok::increment(myints, i)) {
                int& iter = myints[i];

                iter = i;
            }

            for (size_t i = 0; i < sizeof(myints) / sizeof(int); ++i) {
                REQUIRE(myints[i] == i);
            }
        }

        SUBCASE("foreach loop on bidirectional type, no macro")
        {
            example_range_bidirectional bytes;

            for (auto i = ok::begin(bytes); ok::is_inbounds(bytes, i);
                 ok::increment(bytes, i)) {
                uint8_t& iter = bytes.get(i);

                iter = i.inner();
            }

            for (auto i = ok::begin(bytes); ok::is_inbounds(bytes, i);
                 ok::increment(bytes, i)) {
                uint8_t& iter = bytes.get(i);

                REQUIRE(iter == i.inner());
            }
        }

        SUBCASE("foreach loop c array with macro")
        {
            int myints[500];

            const int& ref = ok::range_get_ref(myints, 0UL);

            ok_foreach(int& i, myints) i = 20;

            ok_foreach(const int& i, myints) { REQUIRE(i == 20); }

            auto check_in_lambda = [](const int(&array)[500]) {
                ok_foreach(const int& i, array) { REQUIRE(i == 20); }
            };

            check_in_lambda(myints);
        }

        SUBCASE("foreach loop user defined type with wrapper")
        {
            example_range_cstyle bytes;

            range_def_for<example_range_cstyle> test1;

            for (uint8_t& i : bytes | std_for)
                i = 20;

            for (const uint8_t& i : bytes | std_for) {
                REQUIRE(i == 20);
            }
        }

        SUBCASE("enumerated foreach loop")
        {
            example_range_cstyle bytes;

            for (uint8_t& i : bytes | std_for)
                i = 20;

            for (auto [byte, index] : bytes | enumerate | std_for) {
                REQUIRE(byte == 20);
            }

            // instantiated twice to make sure duplicate labels dont happen from
            // macro
            ok_foreach(ok_pair(byte, index), enumerate(bytes))
            {
                REQUIRE(byte == 20);
            }
            ok_foreach(ok_pair(byte, index), enumerate(bytes))
            {
                REQUIRE(byte == 20);
            }
        }
    }
}
