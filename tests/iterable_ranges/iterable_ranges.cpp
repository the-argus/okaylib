#include "test_header.h"
// test header must be first
#include "okay/iterable/iterable.h"
#include "okay/iterable/std_for.h"
#include "okay/macros/foreach.h"
#include "testing_types.h"

TEST_SUITE("iterable ranges")
{
    TEST_CASE("begin and end")
    {
        SUBCASE("ok::begin() on array")
        {
            int cstyle_array[500];
            // array's cursor type is size_t
            static_assert(
                std::is_same_v<
                    ok::cursor_type_for<decltype(cstyle_array)>,
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
            static_assert(ok::end(myints) == 500);
            static_assert(ok::begin(myints) == 0);

            for (size_t i = ok::begin(myints); i != ok::end(myints); ++i) {
                REQUIRE((i >= 0 && i < 500));
                myints[i] = i;
            }
        }

        SUBCASE("ok::begin() and ok::end() on simple iterable")
        {
            example_iterable_cstyle iterable;
            REQUIRE(iterable.size() == ok::end(iterable));
            REQUIRE(ok::begin(iterable) == 0);

            for (size_t i = ok::begin(iterable); i != ok::end(iterable); ++i) {
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

            for (size_t i = ok::begin(myints); i != ok::end(myints); ++i) {
                int& iter = myints[i];

                iter = i;
            }

            for (size_t i = 0; i < sizeof(myints) / sizeof(int); ++i) {
                REQUIRE(myints[i] == i);
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
    }
}
