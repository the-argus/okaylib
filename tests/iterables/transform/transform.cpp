#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/ranges/indices.h"
#include "okay/ranges/views/enumerate.h"
#include "okay/ranges/views/transform.h"
#include "okay/slice.h"
#include "okay/stdmem.h"
#include <array>

using namespace ok;

TEST_SUITE("transform")
{
    TEST_CASE("functionality")
    {
        SUBCASE("identity transform")
        {
            std::array<int, 50> ints = {};
            std::fill(ints.begin(), ints.end(), 0);

            for (auto c = ok::begin(ints); ok::is_inbounds(ints, c);
                 ok::increment(ints, c)) {
                int& item = ok::range_get_ref(ints, c);
                item = c;
            }

            auto identity = ints | transform([](auto i) { return i; });

            for (auto c = ok::begin(identity); ok::is_inbounds(identity, c);
                 ok::increment(identity, c)) {
                int item = ok::range_get(identity, c);
                REQUIRE(item == c);
            }
        }

        SUBCASE("identity transform with macros")
        {
            std::array<int, 50> ints = {};
            memfill(slice(ints), 0);

            size_t c = 0;
            ok_foreach(auto& item, ints)
            {
                item = c;
                ++c;
            }

            auto identity = ints | transform([](auto i) { return i; });
            c = 0;
            ok_foreach(const auto& item, identity)
            {
                REQUIRE(item == c);
                ++c;
            }
        }

        SUBCASE("squared view with std::array")
        {
            auto squared = transform([](auto i) { return i * i; });

            std::array<int, 50> ints;

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            size_t c = 0;
            ok_foreach(const int i, ints | squared)
            {
                REQUIRE(i == c * c);
                ++c;
            }
        }

        SUBCASE("squared view with rvalue std::array")
        {
            auto squared = transform([](auto i) { return i * i; });

            std::array<int, 50> ints;

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            size_t c = 0;
            ok_foreach(const int i, std::move(ints) | squared)
            {
                REQUIRE(i == c * c);
                ++c;
            }
        }

        SUBCASE("squared view with c-style array")
        {
            auto squared = transform([](auto i) { return i * i; });

            int ints[50];

            ok_foreach(ok_pair(item, index), enumerate(ints)) item = index;

            size_t c = 0;
            ok_foreach(const int i, ints | squared)
            {
                REQUIRE(i == c * c);
                ++c;
            }
        }

        SUBCASE("can still get the size of transformed things")
        {
            auto squared = transform([](auto i) { return i * i; });
            std::array<int, 50> stdarray;
            int carray[35];
            std::vector<int> vector;
            vector.resize(25);

            const size_t arraysize = ok::size(stdarray);
            const size_t carraysize = ok::size(carray);
            const size_t vectorsize = ok::size(vector);

            REQUIRE(ok::size(stdarray | squared) == arraysize);
            REQUIRE(ok::size(carray | squared) == carraysize);
            REQUIRE(ok::size(vector | squared) == vectorsize);
        }

        SUBCASE("reference wrapper semantics when holding an lvalue")
        {
            std::array<int, 50> ints{};

            auto identity = [](int& i) -> int& { return i; };

            // const here, but we get a mutable reference later
            const auto tf_view = ints | transform(identity);

            static_assert(
                std::is_same_v<value_type_for<decltype(tf_view)>, int>);
            static_assert(detail::consuming_range_c<decltype(tf_view)>);

            for (size_t i = 0; i < ok::size(tf_view); ++i) {
                ok::range_get(tf_view, i) = i;
            }

            REQUIRE(ranges_equal(ints, ok::indices));
        }

        SUBCASE("transform returning a const ref has the correct value_type")
        {
            std::array<const int, 50> ints{};

            auto identity = [](const int& i) -> const int& { return i; };

            // const here, but we get a mutable reference later
            const auto tf_view = ints | transform(identity);

            static_assert(
                std::is_same_v<value_type_for<decltype(tf_view)>, const int>);
            static_assert(detail::valid_range_value_type_c<
                          value_type_for<decltype(tf_view)>>);
            static_assert(!detail::consuming_range_c<decltype(tf_view)>);
            static_assert(detail::producing_range_c<decltype(tf_view)>);
        }
    }
}
