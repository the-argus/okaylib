#include "test_header.h"
// test header must be first
#include "okay/containers/array.h"
#include "okay/detail/traits/is_std_container.h"
#include "okay/macros/foreach.h"
#include "okay/ranges/algorithm.h"
#include "okay/ranges/views/drop.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/slice.h"
#include "okay/stdmem.h"
#include <array>
#include <vector>

static_assert(
    ok::detail::std_arraylike_container<std::array<int, 1>>,
    "is_std_container not properly detecting size() and data() functions");

static_assert(
    ok::detail::std_arraylike_container<std::vector<int>>,
    "is_std_container not properly detecting size() and data() functions");

static_assert(!ok::detail::std_arraylike_container<int>,
              "int registered as container?");

using namespace ok;

static_assert(std::is_same_v<slice<const uint8_t>::viewed_type, const uint8_t>,
              "slice::value_type doesnt work as expected");

// undefined memory should not be iterable
static_assert(!ok::range_c<ok::undefined_memory_t<int>>);

TEST_SUITE("slice")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("construction")
        {
            std::array<uint8_t, 512> mem;

            // unfortunately the template argument is not deducable
            slice<uint8_t> sl(mem);
            REQUIRE(sl.size() == mem.size());
            REQUIRE(sl.unchecked_address_of_first_item() == mem.data());

            auto subslice_a = subslice(mem, {.start = 10, .length = 110});
            static_assert(std::is_same_v<decltype(subslice_a), slice<uint8_t>>);

            REQUIREABORTS(auto&& _ =
                              subslice(mem, {.start = 10, .length = 600}));
            REQUIREABORTS(auto&& _ =
                              subslice(mem, {.start = 0, .length = 513}));
            {
                auto subslice_b = subslice(mem, {.start = 0, .length = 512});
                slice<uint8_t> subslice_c(mem);
                REQUIRE(subslice_b.is_alias_for(subslice_c));
            }
            REQUIRE(subslice_a.size() == 110);
            REQUIRE(subslice_a.unchecked_address_of_first_item() == &mem[10]);

            static_assert(!std::is_default_constructible_v<slice<uint8_t>>,
                          "Slice should not be default constructible because "
                          "it is non-nullable");
        }

        SUBCASE("construct from array with const qualified value type")
        {
            std::array<const int, 5> carray = {12, 34, 43, 98, 28};

            slice<const int> cslice = carray;
            REQUIRE(carray.size() == cslice.size());
            REQUIRE(carray.data() == cslice.unchecked_address_of_first_item());

            for (size_t i = 0; i < carray.size(); ++i) {
                REQUIRE(carray[i] == cslice[i]);
            }
        }

        SUBCASE("construct from c-style array with const qualified value type")
        {
            const int carray[5] = {12, 34, 43, 98, 28};

            slice<const int> cslice = carray;
            REQUIRE(5 == cslice.size());
            REQUIRE(carray == cslice.unchecked_address_of_first_item());

            for (size_t i = 0; i < 5; ++i) {
                REQUIRE(carray[i] == cslice[i]);
            }
        }

        SUBCASE("construct from c-style array with non-const value type")
        {
            int carray[5] = {12, 34, 43, 98, 28};

            slice<const int> cslice = carray;
            REQUIRE(5 == cslice.size());
            REQUIRE(carray == cslice.unchecked_address_of_first_item());

            for (size_t i = 0; i < 5; ++i) {
                REQUIRE(carray[i] == cslice[i]);
            }
        }

        SUBCASE(
            "construct from const qualified array with non-const value type")
        {
            const std::array<int, 5> carray = {12, 34, 43, 98, 28};
            std::array<const int, 5> carray_2 = {12, 34, 43, 98, 28};

            static_assert(
                detail::std_arraylike_container_of<decltype(carray), int>);
            static_assert(!detail::std_arraylike_container_of<decltype(carray),
                                                              const int>);
            static_assert(
                !detail::std_arraylike_container_of<decltype(carray_2), int>);
            static_assert(detail::std_arraylike_container_of<decltype(carray_2),
                                                             const int>);

            static_assert(std::is_same_v<decltype(carray.data()), const int*>);

            static_assert(
                !is_std_constructible_c<slice<int>, decltype(carray)&>);
            static_assert(!std::is_constructible_v<slice<int>,
                                                   std::array<const int, 5>&>);
            // cannot be constructed by a value ofc
            static_assert(
                !std::is_constructible_v<slice<int>, std::array<int, 5>>);

            // can never be constructed from rvalues to things
            static_assert(
                !is_std_constructible_c<slice<const int>, decltype(carray)&&>);
            static_assert(!is_std_constructible_c<slice<const int>,
                                                  decltype(carray_2)&&>);
            static_assert(
                !is_std_constructible_c<slice<int>, decltype(carray)&&>);
            static_assert(
                !is_std_constructible_c<slice<int>, decltype(carray_2)&&>);

            slice<const int> cslice = carray;
            slice<const int> cslice_2 = carray_2;
        }

        SUBCASE("convert nonconst slice to const slice")
        {
            std::array<int, 100> arr;

            slice<int> ints = arr;

            slice<const int> cints = ints;

            REQUIRE(ints.is_alias_for(cints));
        }

        SUBCASE("construct from single item")
        {
            int oneint[1] = {0};

            slice<int> ints = slice_from_one((int&)oneint[0]);
            REQUIRE(ints.size() == 1);
            ok_foreach(int i, ints) REQUIRE(i == oneint[0]);

            slice<const int> ints_const = slice_from_one(oneint[0]);
            REQUIRE(ints.size() == 1);
            const int oneint_const[1] = {0};
            ints_const = slice_from_one(oneint_const[0]);
            REQUIRE(ints.size() == 1);
        }

        SUBCASE("const correctness")
        {
            int oneint[1] = {0};
            slice<int> ints = slice_from_one(oneint[0]);
            static_assert(
                std::is_same_v<
                    int*, decltype(ints.unchecked_address_of_first_item())>);

            slice<const int> ints_const = slice_from_one((int&)oneint[0]);
            static_assert(
                std::is_same_v<
                    const int*,
                    decltype(ints_const.unchecked_address_of_first_item())>);

            auto get_nonconst_by_const_ref = [](const ok::slice<int>& guy) {
                static_assert(
                    std::is_same_v<
                        decltype(guy.unchecked_address_of_first_item()), int*>);
                return guy;
            };

            auto copy = get_nonconst_by_const_ref(ints);
            static_assert(
                std::is_same_v<decltype(copy.unchecked_address_of_first_item()),
                               int*>);

            ok::slice<const int> cint_1 = slice_from_one<const int>(oneint[0]);
            cint_1 = slice_from_one(oneint[0]);
            ok::slice<const int> cint_2(cint_1);
        }

        SUBCASE("empty subslice")
        {
            std::array<uint8_t, 512> mem;

            slice<uint8_t> slice = subslice(mem, {0, 0});
            REQUIRE(slice.size() == 0);

            size_t index = 0;
            ok_foreach(auto byte, slice) { ++index; }
            REQUIRE(index == 0);
        }

        SUBCASE("iteration")
        {
            std::array<uint8_t, 128> mem;
            slice slice(mem);

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
            const slice<uint8_t> slice(mem);

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
            slice<uint8_t> sl(mem);
            slice<uint8_t> subslice =
                ok::subslice(sl, {.start = 10, .length = 118});

            REQUIRE(subslice.size() < sl.size());
        }

        SUBCASE("foreach loop w/ enumerate over slice uses references")
        {
            uint8_t mem[128];
            auto mslice = raw_slice(mem[0], sizeof(mem));
            REQUIRE(mslice.is_alias_for(bytes_t(mem)));
            memfill<uint8_t>(mem, 0);

            ok_foreach(ok_pair(byte, index), enumerate(mslice))
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

    TEST_CASE("bit_slice")
    {
        static_assert(!std::is_default_constructible_v<bit_slice_t>);
        static_assert(!std::is_default_constructible_v<const_bit_slice_t>);
        static_assert(is_convertible_to_c<bit_slice_t, const_bit_slice_t>);
        static_assert(is_convertible_to_c<bit_slice_t&, const_bit_slice_t&>);

        SUBCASE("size() of bit slice is correct")
        {
            uint8_t bytes[500];
            memfill(slice(bytes), 0);

            bit_slice_t bs = raw_bit_slice(slice(bytes), 500 * 8, 0);

            REQUIRE(bs.size() == 500 * 8);
            REQUIRE(sizeof(bytes) * 8 == bs.size());

            // require all bits are off
            const bool eql = ok::all_of(bs, [](ok::bit b) { return !b; });
            REQUIRE(eql);
            REQUIRE(!bs.is_empty());
        }

        SUBCASE("size() of const bit slice is correct")
        {
            const uint8_t bytes[] = {0, 0, 0, 0};

            const_bit_slice_t bs =
                raw_bit_slice(slice(bytes), sizeof(bytes) * 8, 0);

            REQUIRE(bs.size() == 4 * 8);
            REQUIRE(sizeof(bytes) * 8 == bs.size());

            // require all bits are off
            const bool eql = ok::all_of(bs, [](ok::bit b) { return !b; });
            REQUIRE(eql);
            REQUIRE(!bs.is_empty());
        }

        SUBCASE("subslice() with no offset")
        {
            uint8_t bytes[] = {0, 0, 0, 0, 0, 0, 0, 0};
            REQUIREABORTS(bit_slice_t all_bits = raw_bit_slice(
                              slice(bytes), sizeof(bytes) * 8, 1));
            REQUIREABORTS(bit_slice_t all_bits =
                              raw_bit_slice(slice(bytes).subslice({
                                                .start = 1,
                                                .length = sizeof(bytes) - 1,
                                            }),
                                            sizeof(bytes) * 8, 0));
            const bit_slice_t all_bits =
                raw_bit_slice(slice(bytes), sizeof(bytes) * 8, 0);

            bit_slice_t first_half =
                all_bits.subslice({.length = all_bits.size() / 2});

            // set all the bits to on in the first half
            for (auto c = ok::begin(first_half); ok::is_inbounds(first_half, c);
                 ok::increment(first_half, c)) {
                ok::range_set(first_half, c, bit::on());
            }

            constexpr uint8_t all_ones = ~uint8_t(0);

            maybe_undefined_array_t<uint8_t, sizeof(bytes)> expected = {
                all_ones, all_ones, all_ones, all_ones, 0, 0, 0, 0};

            REQUIRE(ranges_equal(bytes, expected));

            const_bit_slice_t all_expected_bits =
                raw_bit_slice(slice(expected), expected.size() * 8, 0);
            const_bit_slice_t first_half_expected =
                all_bits.subslice({.length = all_expected_bits.size() / 2});

            REQUIRE(ranges_equal(first_half_expected, first_half));
            REQUIRE(ranges_equal(all_expected_bits, all_bits));
        }

        SUBCASE("subslice() with some offset")
        {
            zeroed_array_t<uint8_t, 100> a{};

            const bit_slice_t offsetted =
                raw_bit_slice(slice(a), a.items().size_bits() - 5, 5);

            for (size_t i = 0; i < offsetted.size(); ++i) {
                offsetted.set_bit(i, bit::on());
            }

            // all bits have been set to 1
            REQUIRE(a.items().last() == 255);

            bool all_set =
                all_of(a | drop(1), [](uint8_t byte) { return byte == 255; });
            REQUIRE(all_set);

            // first five least significant bits are skipped
            REQUIRE(a[0] == uint8_t(0b11100000));
        }

        SUBCASE("raw_bit_slice with zero size")
        {
            zeroed_array_t<uint8_t, 10> bytes{};

            REQUIREABORTS(auto bs = raw_bit_slice(slice(bytes), 0, 8));
            REQUIREABORTS(auto bs = raw_bit_slice(slice(bytes), 0, 20));
            REQUIREABORTS(auto bs =
                              raw_bit_slice(slice<const uint8_t>(bytes), 0, 8));
            REQUIREABORTS(
                auto bs = raw_bit_slice(slice<const uint8_t>(bytes), 0, 20));

            auto bs = raw_bit_slice(slice(bytes), 0, 5);

            REQUIRE(bs.is_empty());
            REQUIRE(
                raw_bit_slice(slice<const uint8_t>(bytes), 0, 5).is_empty());
        }

        SUBCASE("bit_slice_t::subslice with zero size")
        {
            zeroed_array_t<uint8_t, 10> bytes{};
            auto bs = raw_bit_slice(slice(bytes), bytes.items().size_bits(), 0);

            REQUIRE(bs.subslice({.start = 40, .length = 0}).is_empty());
            REQUIRE(bs.subslice({.start = 0, .length = 0}).is_empty());
            REQUIRE(bs.subslice({.start = 21, .length = 0}).is_empty());
        }

        SUBCASE("is_byte_aligned()")
        {
            zeroed_array_t<uint8_t, 10> bytes;
            auto bs = raw_bit_slice(slice(bytes), bytes.items().size_bits(), 0);

            REQUIRE(bs.subslice({.start = 32, .length = 0}).is_byte_aligned());
            REQUIRE(bs.subslice({.start = 0, .length = 0}).is_byte_aligned());
            REQUIRE(!bs.subslice({.start = 21, .length = 0}).is_byte_aligned());

            // try with addition of multiple offsets
            bs = raw_bit_slice(slice(bytes), bytes.items().size_bits() - 3, 3);
            REQUIRE(bs.subslice({.start = 5, .length = 0}).is_byte_aligned());
            REQUIRE(!bs.is_byte_aligned());
            REQUIRE(!bs.subslice({.start = 0, .length = 0}).is_byte_aligned());
        }

        SUBCASE("toggle_bit()")
        {
            zeroed_array_t<uint8_t, 10> bytes;
            auto bs = raw_bit_slice(slice(bytes), bytes.items().size_bits(), 0);

            bs.toggle_bit(0);
            REQUIRE(bytes[0] == 1);
            bs.toggle_bit(1);
            REQUIRE(bytes[0] == 3);
            bs.toggle_bit(8);
            REQUIRE(bytes[1] == 1);
            bs.toggle_bit(8);
            REQUIRE(bytes[1] == 0);
        }
    }

#if defined(OKAYLIB_USE_FMT)
    TEST_CASE("formattable")
    {
        SUBCASE("basic slice is formattable")
        {
            std::array myints = {0, 1, 2};
            slice<int> intslice = myints;
            slice<const int> intslice_c = myints;
            const slice<const int> double_intslice_c = myints;
            fmt::println("int slice: {}", intslice);
            fmt::println("int slice: {}", intslice_c);
            fmt::println("int slice: {}", double_intslice_c);
        }

        SUBCASE("slice with formattable contents is formattable")
        {
            std::array bits = {bit::on(), bit::off()};
            slice<ok::bit> my_bit_slice = bits;
            slice<const ok::bit> my_bit_slice_c = bits;

            fmt::println("bit slice: {}", my_bit_slice);
            fmt::println("const bit slice: {}", my_bit_slice_c);
        }

        SUBCASE("slice of const char can be formatted as a string")
        {
            const char mystr[] = "Hello, World";
            slice<const char> chars = mystr;
            fmt::println("whole string: {}, subslice normally: {}, subslice as "
                         "string: {:s}",
                         mystr, chars, chars);
        }
    }
#endif
}
