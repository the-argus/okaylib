#include "okay/stdmem.h"
#include "test_header.h"
// test header must be first
#include "okay/detail/traits/is_container.h"
#include "okay/iterable/enumerate.h"
#include "okay/macros/foreach.h"
#include "okay/slice.h"
#include <array>
#include <vector>

static_assert(
    ok::detail::is_container_v<std::array<int, 1>>,
    "is_container not properly detecting size() and data() functions");

static_assert(
    ok::detail::is_container_v<std::vector<int>>,
    "is_container not properly detecting size() and data() functions");

static_assert(!ok::detail::is_container_v<int>, "int registered as container?");

static_assert(ok::detail::is_container_v<ok::slice_t<int>>,
              "slice is not container");

static_assert(ok::detail::is_container_v<ok::slice_t<const int>>,
              "const slice is not container");

using namespace ok;

static_assert(std::is_same_v<slice_t<const uint8_t>::value_type, const uint8_t>,
              "slice::value_type doesnt work as expected");

TEST_SUITE("slice")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("construction")
        {
            std::array<uint8_t, 512> mem;

            // unfortunately the template argument is not deducable
            slice_t<uint8_t> sl(mem);
            REQUIRE(sl.size() == mem.size());
            REQUIRE(sl.data() == mem.data());

            auto subslice_a = make_subslice(mem, 10, 120);
            static_assert(
                std::is_same_v<decltype(subslice_a), slice_t<uint8_t>>);
            REQUIREABORTS(make_subslice(mem, 10, 600));
            REQUIREABORTS(make_subslice(mem, 0, 513));
            REQUIREABORTS(make_subslice(mem, 100, 50));
            {
                auto subslice_b = make_subslice(mem, 0, 512);
                slice_t<uint8_t> subslice_c(mem);
                REQUIRE(subslice_b == subslice_c);
            }
            REQUIRE(subslice_a.size() == 110);
            REQUIRE(subslice_a.data() == &mem[10]);

            static_assert(!std::is_default_constructible_v<slice_t<uint8_t>>,
                          "Slice should not be default constructible because "
                          "it is non-nullable");
        }

        SUBCASE("construct from array with const qualified value type")
        {
            std::array<const int, 5> carray = {12, 34, 43, 98, 28};

            slice_t<const int> cslice = carray;
            REQUIRE(carray.size() == cslice.size());
            REQUIRE(carray.data() == cslice.data());

            for (size_t i = 0; i < carray.size(); ++i) {
                REQUIRE(carray[i] == cslice[i]);
            }
        }

        SUBCASE("construct from c-style array with const qualified value type")
        {
            const int carray[5] = {12, 34, 43, 98, 28};

            slice_t<const int> cslice = carray;
            REQUIRE(5 == cslice.size());
            REQUIRE(carray == cslice.data());

            for (size_t i = 0; i < 5; ++i) {
                REQUIRE(carray[i] == cslice[i]);
            }
        }

        SUBCASE("construct from c-style array with non-const value type")
        {
            int carray[5] = {12, 34, 43, 98, 28};

            slice_t<const int> cslice = carray;
            REQUIRE(5 == cslice.size());
            REQUIRE(carray == cslice.data());

            for (size_t i = 0; i < 5; ++i) {
                REQUIRE(carray[i] == cslice[i]);
            }
        }

        SUBCASE(
            "construct from const qualified array with non-const value type")
        {
            const std::array<int, 5> carray = {12, 34, 43, 98, 28};

            static_assert(std::is_same_v<decltype(carray.data()), const int*>);

            static_assert(
                !std::is_constructible_v<slice_t<int>, decltype(carray)>);
            static_assert(
                std::is_constructible_v<slice_t<const int>, decltype(carray)>);

            slice_t<const int> cslice = carray;
        }

        SUBCASE("construct from single item")
        {
            int oneint[1] = {0};

            slice_t<int> ints = make_slice_of_one((int&)oneint[0]);
            REQUIRE(ints.size() == 1);
            ok_foreach(int i, ints) REQUIRE(i == oneint[0]);

            slice_t<const int> ints_const = make_slice_of_one(oneint[0]);
            REQUIRE(ints.size() == 1);
            const int oneint_const[1] = {0};
            ints_const = make_slice_of_one(oneint_const[0]);
            REQUIRE(ints.size() == 1);
        }

        SUBCASE("const correctness")
        {
            int oneint[1] = {0};
            slice_t<int> ints = make_slice_of_one(oneint[0]);
            static_assert(std::is_same_v<int*, decltype(ints.data())>);

            slice_t<const int> ints_const = make_slice_of_one((int&)oneint[0]);
            static_assert(
                std::is_same_v<const int*, decltype(ints_const.data())>);

            auto get_nonconst_by_const_ref = [](const ok::slice_t<int>& guy) {
                static_assert(std::is_same_v<decltype(guy.data()), int*>);
                return guy;
            };

            auto copy = get_nonconst_by_const_ref(ints);
            static_assert(std::is_same_v<decltype(copy.data()), int*>);

            ok::slice_t<const int> cint_1 =
                make_slice_of_one<const int>(oneint[0]);
            cint_1 = make_slice_of_one(oneint[0]);
            ok::slice_t<const int> cint_2(cint_1);
        }

        SUBCASE("empty subslice")
        {
            std::array<uint8_t, 512> mem;

            slice_t<uint8_t> slice = make_subslice(mem, 0, 0);
            REQUIRE(slice.size() == 0);

            size_t index = 0;
            ok_foreach(auto byte, slice) { ++index; }
            REQUIRE(index == 0);
        }

        SUBCASE("iteration")
        {
            std::array<uint8_t, 128> mem;
            slice_t slice(mem);

            memfill(slice, 0);
            uint8_t index = 0;
            ok_foreach(auto& byte, slice)
            {
                REQUIRE(byte == 0);
                byte = index;
                ++index;
            }

            // make sure that also changed mem
            index = 0;
            for (auto byte : mem) {
                REQUIRE(byte == index);
                ++index;
            }
        }

        SUBCASE("const iteration")
        {
            std::array<uint8_t, 128> mem;
            std::fill(mem.begin(), mem.end(), 0);
            const slice_t<uint8_t> slice(mem);

            uint8_t index = 0;
            ok_foreach(const auto& byte, slice)
            {
                REQUIRE(byte == 0);
                mem[index] = index;
                ++index;
            }

            // make sure that also changed slice
            index = 0;
            ok_foreach(const auto& byte, slice)
            {
                REQUIRE(byte == index);
                ++index;
            }
        }

        SUBCASE("subslice construction")
        {
            std::array<uint8_t, 128> mem;
            slice_t<uint8_t> sl(mem);
            slice_t<uint8_t> subslice = make_subslice(sl, 10, 127);

            REQUIRE(subslice.size() < sl.size());
        }

        SUBCASE("foreach loop w/ enumerate over slice uses references")
        {
            uint8_t mem[128];
            auto slice = raw_slice(mem[0], sizeof(mem));
            REQUIRE(slice == slice_t<uint8_t>(mem));
            memfill(slice_t(mem), 0);

            ok_foreach(ok_pair(byte, index), enumerate(slice))
            {
                static_assert(std::is_same_v<decltype(byte), uint8_t&>);
                static_assert(std::is_same_v<decltype(index), const size_t>);
                byte = index;
            }

            for (size_t i = 0; i < sizeof(mem); ++i) {
                REQUIRE(mem[i] == i);
            }
        }
    }
}
