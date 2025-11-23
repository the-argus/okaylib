#include "test_header.h"
// test header must be first
#include "okay/reflection/nameof.h"
#include "okay/reflection/typehash.h"

#include <cstdio>
#include <string>

using namespace ok;

struct MyPodStruct
{
    int a;
    float b;
};

enum UnscopedEnumTest
{
    UnscopedEnumVariant
};

enum class ScopedEnumTest
{
    ScopedEnumVariant
};

static_assert(ok::nameof<MyPodStruct>() == "MyPodStruct");
static_assert(ok::nameof<int>() == "int");
// static_assert(ok::nameof<&MyPodStruct::a>() == "MyPodStruct::a");

TEST_SUITE("reflection")
{
    TEST_CASE("typenames are null terminated")
    {
        SUBCASE("POD structs")
        {
            std::string name = ok::nameof<MyPodStruct>().data();
            REQUIRE(name == "MyPodStruct");
            printf("name of MyPodStruct: %s\n",
                   ok::nameof<MyPodStruct>().data());
        }

        SUBCASE("std:: classes")
        {
            std::string name = ok::nameof<std::string>().data();
            REQUIRE(name == "std::string");
            printf("name of std::string: %s\n",
                   ok::nameof<std::string>().data());
        }

        SUBCASE("primitive type")
        {
            std::string name = ok::nameof<int>().data();
            REQUIRE(name == "int");
            printf("name of int: %s\n", ok::nameof<int>().data());
        }

        SUBCASE("scoped enum type")
        {
            std::string name = ok::nameof<ScopedEnumTest>().data();
            REQUIRE(name == "ScopedEnumTest");
            printf("name of ScopedEnumTest: %s\n",
                   ok::nameof<ScopedEnumTest>().data());
        }
        SUBCASE("UNscoped enum type")
        {
            std::string name = ok::nameof<UnscopedEnumTest>().data();
            REQUIRE(name == "UnscopedEnumTest");
            printf("name of UnscopedEnumTest: %s\n",
                   ok::nameof<UnscopedEnumTest>().data());
        }

        SUBCASE("member variable pointer")
        {
            std::string name_a = ok::nameof<decltype(&MyPodStruct::a)>().data();
            REQUIRE(name_a == "int MyPodStruct::*");
            printf("name of &MyPodStruct::a: %s\n",
                   ok::nameof<decltype(&MyPodStruct::a)>().data());
            std::string name_b = ok::nameof<decltype(&MyPodStruct::b)>().data();
            REQUIRE(name_b == "float MyPodStruct::*");
            printf("name of &MyPodStruct::b: %s\n",
                   ok::nameof<decltype(&MyPodStruct::b)>().data());
        }
    }

    TEST_CASE("type hashes appear unique")
    {
        static_assert(ok::typehash<int>() == ok::typehash<int32_t>());
        constexpr detail::constexpr_array_t hashes{
            ok::typehash<MyPodStruct>(),
            ok::typehash<int>(),
            ok::typehash<float>(),
            ok::typehash<decltype(&MyPodStruct::a)>(),
            ok::typehash<UnscopedEnumTest>(),
            ok::typehash<ScopedEnumTest>(),
        };

        for (size_t i = 0; i < hashes.size(); ++i) {
            for (size_t j = 0; j < hashes.size(); ++j) {
                if (i == j)
                    continue;
                REQUIRE(hashes[i] != hashes[j]);
            }
        }
    }
}
