#include "test_header.h"
// test header must be first
#include "okay/iterable/ranges.h"
#include "testing_types.h"

constexpr size_t begin(const example_iterable_cstyle&) { return 0; }

constexpr size_t initial_iterator_value = 2;

constexpr example_multiple_cursor_iterable::iterator
begin(const example_multiple_cursor_iterable& i)
{
    return {.actual = initial_iterator_value};
}

TEST_SUITE("iterable ranges")
{
    TEST_CASE("begin and end")
    {
        SUBCASE("ok::begin() on array")
        {
            int cstyle_array[500];
            // array's cursor type is size_t
            static_assert(std::is_same_v<ok::detail::default_cursor_type_meta_t<
                                             decltype(cstyle_array)>::type,
                                         size_t>);

            // begin for arrays always returns 0 for the index of first elem
            static_assert(ok::begin(cstyle_array) == 0);
            static_assert(ok::begin_for_cursor<size_t>(cstyle_array) == 0);
            size_t begin = ok::begin(cstyle_array);
            REQUIRE(begin == 0);
        }

        SUBCASE("ok::begin() on user defined type with member .begin()")
        {
            example_iterable_with_begin begin_able;
            size_t begin = ok::begin(begin_able);

            // different value from this class as opposed to its parent which
            // has the free function at the top of this file, which returns 0
            // for the begin
            static_assert(example_iterable_with_begin::begin_value != 0);

            static_assert(ok::begin(begin_able) ==
                          example_iterable_with_begin::begin_value);
            static_assert(ok::begin(begin_able) ==
                          ok::begin_for_cursor<size_t>(begin_able));
            REQUIRE(ok::begin(begin_able) ==
                    example_iterable_with_begin::begin_value);
        }

        SUBCASE("ok::begin() on example iterable with free function begin()")
        {
            using cursor_t = size_t;
            using iterable_t = example_iterable_cstyle;
            iterable_t iterable;
            cursor_t begin = ok::begin(iterable);
            static_assert(ok::begin_for_cursor<size_t>(iterable) ==
                              ok::begin(iterable),
                          "begin_for_cursor() with default type as argument "
                          "isnt the same as ok::begin()");
            static_assert(ok::begin(iterable) == 0);
            REQUIRE(begin == 0);
        }

        SUBCASE("ok::begin() and ok::begin_for_cursor() on custom type")
        {
            using iterable_t = example_multiple_cursor_iterable;
            using cursor_t = iterable_t::iterator;
            iterable_t iterable;
            static_assert(ok::begin_for_cursor<size_t>(iterable) ==
                          iterable_t::initial_size_t_cursor_value);
            // ok::begin() uses default cursor type, and for this type that is
            // handled by a free function. assert that the free function's magic
            // number is what we find, and not whatever
            // .begin<default_cursor_t>() would have gotten us, nor 0
            static_assert(ok::begin(iterable).actual == initial_iterator_value);
        }
    }
}
