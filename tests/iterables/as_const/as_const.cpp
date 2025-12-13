#include "test_header.h"
// test header must be first
#include "okay/iterables/indices.h"
#include "okay/iterables/iterables.h"
#include "testing_types.h"
#include <vector>

using namespace ok;

TEST_SUITE("as_const")
{
    TEST_CASE("iterate lvalue forward iterable as const")
    {
        forward_iterable_reftype_test_t forward;

        static_assert(stdc::is_same_v<value_type_for<decltype(forward)>, int&>);
        static_assert(
            stdc::is_same_v<value_type_for<decltype(forward.iter().as_const())>,
                            const int&>);

        for (const int& i : forward.iter().as_const()) {
        }

        REQUIRE(iterators_equal(forward.iter().as_const(),
                                forward_iterable_reftype_test_t::expected));
    }

    TEST_CASE("iterate rvalue forward iterable as const")
    {
        static_assert(
            stdc::is_same_v<
                value_type_for<decltype(forward_iterable_reftype_test_t{})>,
                int&>);
        static_assert(stdc::is_same_v<
                      value_type_for<decltype(forward_iterable_reftype_test_t{}
                                                  .iter()
                                                  .as_const())>,
                      const int&>);

        for (const int& i :
             forward_iterable_reftype_test_t{}.iter().as_const()) {
        }

        REQUIRE(
            iterators_equal(forward_iterable_reftype_test_t{}.iter().as_const(),
                            forward_iterable_reftype_test_t::expected));
    }

    TEST_CASE("iterate lvalue arraylike as const")
    {
        std::vector<int> ints = {0, 2, 3, 4, 5};

        static_assert(stdc::is_same_v<value_type_for<decltype(ints)>, int&>);
        static_assert(
            stdc::is_same_v<value_type_for<decltype(ok::iter(ints).as_const())>,
                            const int&>);

        for (const int& i : iter(ints).as_const()) {
        }
    }

    TEST_CASE("iterate rvalue arraylike as const")
    {
        auto vec_ret = [] {
            std::vector<int> out;

            out.push_back(0);
            out.push_back(2);
            out.push_back(3);
            out.push_back(4);
            out.push_back(5);

            return out;
        };

        static_assert(stdc::is_same_v<
                      value_type_for<decltype(iter(vec_ret()).as_const())>,
                      const int&>);

        for (const int& i : iter(vec_ret()).as_const()) {
        }
    }
}
