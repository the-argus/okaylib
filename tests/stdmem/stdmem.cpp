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

            slice_t<u8> a(bytes, 0, 100);
            slice_t<u8> b(bytes, 20, 110);

            REQUIRE(!memcopy(a, b));
            REQUIRE(!memcopy_lenient(b, a));
            // normally okay to copy into the bigger buffer, but these overlap
            REQUIRE(!memcopy_lenient(a, b));

            slice_t<u8> c(bytes, 200, 250);
            REQUIRE(memcopy_lenient(b, c));
            REQUIRE(memcopy_lenient(a, c));
            // c is smallest so you cant copy stuff into it
            REQUIRE(!memcopy_lenient(c, b));
            REQUIRE(!memcopy_lenient(c, a));

            REQUIRE(!memcompare(a, b));
            REQUIRE(!memcompare(a, c));
            REQUIRE(!memcompare(b, c));
            REQUIRE(memcompare(c, c));
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

            slice_t<u8> a(bytes, 0, 100);
            slice_t<u8> b(bytes, 20, 110);
            slice_t<u8> c(bytes, 100, 200);
            REQUIRE(memoverlaps(a, b));
            REQUIRE(!memoverlaps(a, c));
            REQUIRE(memoverlaps(c, b));
        }

        SUBCASE("memfill")
        {
            std::array<u8, 512> bytes;
            memfill(slice_t<u8>(bytes), u8(0));
            for (u8 byte : bytes) {
                REQUIRE(byte == 0);
            }

            memfill(slice_t<u8>(bytes, 0, 100), u8(1));
            for (size_t i = 0; i < bytes.size(); ++i) {
                REQUIRE(bytes[i] == (i < 100 ? 1 : 0));
            }
        }

        SUBCASE("memcontains")
        {
            std::array<u8, 512> bytes;
            slice_t<u8> a(bytes, 0, 512);
            slice_t<u8> b(bytes, 256, 512);
            slice_t<u8> c(bytes, 255, 511);
            REQUIRE(memcontains(a, b));
            REQUIRE(memcontains(a, c));
            // nothing can contain A!
            REQUIRE(!memcontains(b, a));
            REQUIRE(!memcontains(c, a));

            // no way for b or c to contain the other, they are the same size
            // just offset
            REQUIRE(!memcontains(b, c));
            REQUIRE(!memcontains(c, b));
        }

        SUBCASE("memcontains_one")
        {
            struct Test
            {
                int i;
                float j;
            };
            std::array<Test, 200> tests;

            REQUIRE(memcontains_one<Test>((tests), tests.data() + 100));
            REQUIRE(!memcontains_one<Test>((tests), tests.data() + 200));
            REQUIRE(!memcontains_one<Test>((tests), tests.data() + 201));
            REQUIRE(memcontains_one<Test>((tests), tests.data() + 199));
            ok::slice_t<Test> tmem = tests;
            REQUIRE(memcontains_one(tmem, tests.data() + 100));
            REQUIRE(!memcontains_one(tmem, tests.data() + 200));
            REQUIRE(!memcontains_one(tmem, tests.data() + 201));
            REQUIRE(memcontains_one(tmem, tests.data() + 199));
            ok::slice_t<uint8_t> tmem_bytes =
                raw_slice(*reinterpret_cast<uint8_t*>(tmem.data()),
                          sizeof(Test) * tmem.size());
            REQUIRE(memcontains_one(tmem_bytes, tests.data() + 100));
            REQUIRE(!memcontains_one(tmem_bytes, tests.data() + 200));
            REQUIRE(!memcontains_one(tmem_bytes, tests.data() + 201));
            REQUIRE(memcontains_one(tmem_bytes, tests.data() + 199));
        }
    }
}
