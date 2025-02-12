#include "test_header.h"
// test header must be first
#include "okay/short_arithmetic_types.h"
#include "okay/stdmem.h"
#include <array>

using namespace ok;

TEST_SUITE("stdmem")
{
    TEST_CASE("functions")
    {
        SUBCASE("invalid arguments")
        {
            std::array<u8, 512> bytes;

            bytes_t a = subslice(bytes, {.start = 0, .length = 40});
            bytes_t b = subslice(bytes, {.start = 20, .length = 90});

            // memcopying these always aborts, because they are overlapping
            REQUIREABORTS(auto&& _ = ok_memcopy(.to = b, .from = a););
            REQUIREABORTS(auto&& _ = ok_memcopy(.to = a, .from = b););

            bytes_t c = subslice(bytes, {.start = 200, .length = 50});
            auto ignored = ok_memcopy(.to = b, .from = c);
            // c is smaller than a, should abort
            REQUIREABORTS(auto&& _ = ok_memcopy(.to = a, .from = c););

            REQUIRE(!memcompare(a, b));
            REQUIRE(!memcompare(a, c));
            REQUIRE(!memcompare(b, c));
            REQUIRE(memcompare(c, c));
            // bytes to bytes identity transform
            REQUIRE(memcompare(reinterpret_as_bytes(c), c));
            REQUIRE(memcompare(reinterpret_bytes_as<uint8_t>(c), c));
        }

        SUBCASE("memcompare for string")
        {
            std::array<char, 512> chars;
            const char* string = "testing string!";
            size_t length = strlen(string);
            REQUIRE(std::snprintf(chars.data(), chars.size(), "%s", string) ==
                    length);

            ok::slice_t<const char> strslice = raw_slice(*string, length);
            ok::slice_t<const char> array_strslice =
                raw_slice<const char>(*chars.data(), length);
            REQUIRE(memcompare(strslice, array_strslice));
        }

        SUBCASE("memoverlaps")
        {
            std::array<u8, 512> bytes;

            bytes_t a = subslice(bytes, {.start = 0, .length = 100});
            bytes_t b = subslice(bytes, {.start = 20, .length = 90});
            bytes_t c = subslice(bytes, {.start = 100, .length = 100});
            REQUIRE(memoverlaps(a, b));
            REQUIRE(!memoverlaps(a, c));
            REQUIRE(memoverlaps(c, b));
        }

        SUBCASE("memfill")
        {
            std::array<u8, 512> bytes;
            for (auto& b : bytes)
                b = 1;

            memfill(slice_t<u8>(bytes), u8(0));
            for (u8 byte : bytes) {
                REQUIRE(byte == 0);
            }

            memfill(subslice(bytes, {.start = 0, .length = 100}), u8(1));
            for (size_t i = 0; i < bytes.size(); ++i) {
                REQUIRE(bytes[i] == (i < 100 ? 1 : 0));
            }
        }

        SUBCASE("memcontains")
        {
            std::array<u8, 512> bytes;
            bytes_t a = subslice(bytes, {.start = 0, .length = 512});
            bytes_t b = subslice(bytes, {.start = 256, .length = 256});
            bytes_t c = subslice(bytes, {.start = 255, .length = 256});
            REQUIRE(ok_memcontains(.outer = a, .inner = b));
            REQUIRE(ok_memcontains(a, c));
            // nothing can contain A!
            REQUIRE(!ok_memcontains(b, a));
            REQUIRE(!ok_memcontains(.outer = c, .inner = a));

            // no way for b or c to contain the other, they are the same size
            // just offset
            REQUIRE(!ok_memcontains(b, c));
            REQUIRE(!ok_memcontains(.outer = c, .inner = b));
        }

        SUBCASE("memcontains")
        {
            struct Test
            {
                int i;
                float j;
            };
            std::array<Test, 200> tests_arr;
            slice_t tests = tests_arr;

            bool c = ok_memcontains(.outer = tests,
                                    .inner = slice_from_one(tests[100]));
            // manually do slice_from_one
            REQUIRE(ok_memcontains(
                tests, subslice(tests, {.start = 100, .length = 1})));
            REQUIRE(ok_memcontains(
                tests, subslice(tests, {.start = 199, .length = 1})));
            REQUIRE(ok_memcontains(tests, slice_from_one(tests[199])));
            ok::slice_t<Test> tmem = tests;
            REQUIRE(ok_memcontains(tmem, slice_from_one(tests[100])));
            REQUIRE(ok_memcontains(tmem, slice_from_one(tests[199])));

            auto first_half = tests.subslice({.start = 0, .length = 100});
            auto second_half = tests.subslice({.start = 99, .length = 100});
            REQUIRE(!ok_memcontains(first_half, second_half));

            // can go to and from bytes, get the same thing back
            auto restored =
                reinterpret_bytes_as<Test>(reinterpret_as_bytes(tests));
            REQUIRE(restored.is_alias_for(tests));
            REQUIRE(memcompare(tests, restored));
        }
    }
}
