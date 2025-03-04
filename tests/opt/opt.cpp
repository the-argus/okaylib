#include "okay/ranges/views/enumerate.h"
#include "test_header.h"
// test header must be first
#include "okay/macros/foreach.h"
#include "okay/opt.h"
#include "okay/slice.h"
#include "okay/stdmem.h"
#include "testing_types.h"

#include <array>
#include <optional>

using namespace ok;
struct big
{
    int i[100];
};
static_assert(!std::is_convertible_v<std::optional<big>, const int&>);
static_assert(!std::is_convertible_v<opt<big>, const int&>);
static_assert(!std::is_convertible_v<int*, const int&>);
static_assert(!std::is_convertible_v<std::optional<int*>, const int&>);
static_assert(!std::is_convertible_v<opt<int&>, const int&>);

static_assert(sizeof(opt<slice_t<int>>) == sizeof(slice_t<int>),
              "Optional slice types are a different size than slices");

static_assert(sizeof(opt<int&>) == sizeof(int*),
              "Optional reference types are a different size than pointers");

TEST_SUITE("opt")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            opt<int> def;
            REQUIRE((!def.has_value()));
            REQUIRE((def != 0));
            REQUIRE((0 != def));
            def = 1;
            REQUIRE(def);
            def = nullopt;
            REQUIRE(!def);
        }

        SUBCASE("Construction with value")
        {
            opt<int> has = 10;
            REQUIRE(has.has_value());
            REQUIRE(has == 10);
            REQUIRE(has.value() == 10);
            has = nullopt;
            REQUIRE(has != 10);
        }

        SUBCASE("comparison")
        {
            opt<int> one = 100;
            opt<int> two;
            REQUIRE(one != two);
            REQUIRE(two != one);
            two = 200;
            {
                bool both_has_value = one.has_value() && two.has_value();
                REQUIRE(both_has_value);
            }
            REQUIRE(one != two);
            REQUIRE(two != one);
            one.reset();
            two.reset();
            {
                bool both_dont_has_value = !one.has_value() && !two.has_value();
                REQUIRE(both_dont_has_value);
            }
            REQUIRE(one == two);
            one = 1;
            two = 1;
            REQUIRE(one == two);
        }

        SUBCASE("convertible to bool")
        {
            opt<int> nothing;
            REQUIRE(!nothing);
            REQUIRE(!nothing.has_value());
            opt<int> something = 1;
            REQUIRE(something);
            REQUIRE(something.has_value());

            auto bool_to_optional = [](bool input) -> opt<int> {
                return input ? opt<int>(3478) : opt<int>{};
            };

            if (auto result = bool_to_optional(true)) {
                REQUIRE(result);
                REQUIRE(result.value() == 3478);
                REQUIRE(result == 3478);

                REQUIRE(result != opt<int>(3477));
                REQUIRE(result != 3477);
                REQUIRE(result != opt<int>{});
            }

            if (auto result = bool_to_optional(false)) {
                REQUIRE(false); // should never happen
            }
        }

        SUBCASE("non owning slice")
        {
            std::vector<uint8_t> bytes = {20, 32, 124, 99, 1};
            opt<slice_t<uint8_t>> maybe_array;
            REQUIRE(!maybe_array.has_value());

            // opt<std::vector<uint8_t>> optional_vector_copy(bytes);
            // maybe_array = optional_vector_copy;

            // TODO: make sure slice and opt<slice_t> cannot assume ownership
            // of objects that are not trivially moveable
        }

#ifndef OKAYLIB_NO_CHECKED_MOVES
#ifndef OKAYLIB_DISALLOW_EXCEPTIONS
        SUBCASE("moved type is marked as nullopt")
        {
            std::vector<int> nums = {1203, 12390, 12930, 430};

            auto consume = [](opt<std::vector<int>>&& maybe_moved) {
                if (!maybe_moved)
                    return;

                REQUIRE(!maybe_moved.value().empty());
                opt<std::vector<int>> our_nums = std::move(maybe_moved);
                REQUIRE(!maybe_moved.has_value());
                REQUIRE(!our_nums.value().empty());
                our_nums.value().resize(0);
            };

            opt<std::vector<int>> maybe_copy;
            REQUIRE(!maybe_copy);
            consume(std::move(maybe_copy));
            REQUIRE(!maybe_copy); // this is defined behavior with checked moves

            opt<std::vector<int>> maybe_moved = std::move(nums);
            REQUIRE(maybe_moved);
            REQUIRE(!maybe_moved.value().empty());
            consume(std::move(maybe_moved));
            REQUIRE(!maybe_moved);
        }
#endif
#endif
    }

    TEST_CASE("Functionality")
    {
        SUBCASE("Resetting")
        {
#ifndef OKAYLIB_DISALLOW_EXCEPTIONS
            opt<std::vector<int>> vec;
            // null by default
            REQUIRE(!vec.has_value());
            vec.emplace();
            REQUIRE(vec.has_value());
            vec.value().push_back(42);
            REQUIRE(vec.value()[0] == 42);
            vec.reset();
            REQUIRE(!vec.has_value());
#endif
        }

        SUBCASE("Aborts on null")
        {
            opt<int> nope;
            REQUIREABORTS(++nope.value());
        }

        SUBCASE("moving non-trivially-copyable type")
        {
            moveable_t moveguy;
            int bytes = std::snprintf(moveguy.nothing, 50, "nope");

            opt<moveable_t> maybe_moveguy = std::move(moveguy);
            REQUIRE(maybe_moveguy.has_value());
            // and this shouldnt work
            // opt<moveable_t> maybe_moveguy = moveguy;

            REQUIRE(strcmp(maybe_moveguy.value().nothing, "nope") == 0);
        }

        SUBCASE("null optional references are not aliases for each other")
        {
            opt<int&> a;
            opt<int&> b;
            REQUIRE(!a.is_alias_for(b));
        }

        SUBCASE("pointer convertible to optional reference")
        {
            opt<int&> iref = nullptr;
            REQUIRE(!iref);
            iref = nullptr;
            REQUIRE(!iref);
            int i = 0;
            iref = i;
            REQUIRE(iref);
            iref = &i;
            REQUIRE(iref);
        }

        SUBCASE("constness of optional reference follows const correctness")
        {
            // cannot go from const to nonconst
            static_assert(
                !std::is_convertible_v<opt<const int&>, opt<int&>>);
            // can go from nonconst to const
            static_assert(
                std::is_convertible_v<opt<int&>, opt<const int&>>);
            int i = 10;
            opt<int&> mut_iref = i;
            opt<const int&> iref = i;

            opt<const int&> iref_2 = mut_iref;
        }

        SUBCASE("optional of reference to derived can be converted to optional "
                "of reference to base")
        {
            struct test_base
            {
                int i;
            };

            struct test_derived : public test_base
            {
                bool b;
            };

            test_base t{0};
            test_derived td{0, true};

            opt<test_base&> tref = t;
            REQUIRE(tref.is_alias_for(t));
            tref = td;
            REQUIRE(tref.is_alias_for(td));

            // cannot go from base to derived
            static_assert(!std::is_convertible_v<opt<test_base&>,
                                                 opt<test_derived&>>);
        }

        SUBCASE("optional reference types")
        {
            int test = 10;
            opt<const int&> testref;
            opt<int&> testref2;
            REQUIRE(!testref.has_value());
            REQUIRE(!testref2.has_value());
            testref = test;
            REQUIRE(testref.value() == test);
            REQUIRE(testref.is_alias_for(test));
            static_assert(
                std::is_same_v<opt<const int&>::pointer_t, const int>);
            static_assert(!std::is_convertible_v<opt<int&>, const int>);
            REQUIRE(!testref.is_alias_for(testref2));

            int test2 = 10;
            testref2 = test2;
            REQUIRE(testref2.value() == test2);
            REQUIRE(testref2.is_alias_for(test2));
            REQUIRE(!testref.is_alias_for(test2));
            REQUIRE(testref.value() == test2);
            REQUIRE(!testref.is_alias_for(testref2));

            testref = testref2;
        }

        SUBCASE("inplace return")
        {
            static size_t copy_count = 0;
            struct BigThing
            {
                std::array<int, 300> numbers;
                inline constexpr BigThing() noexcept : numbers({}) {}
                inline constexpr BigThing(const BigThing& other) noexcept
                    : numbers(other.numbers)
                {
                    ++copy_count;
                }
                BigThing& operator=(const BigThing& other) = delete;
            };

            auto try_make_big_thing =
                [](bool should_succeed) -> opt<BigThing> {
                if (should_succeed)
                    return opt<BigThing>{std::in_place};
                else
                    return {};
            };

            opt<BigThing> maybe_thing = try_make_big_thing(true);
            opt<BigThing> maybe_not_thing = try_make_big_thing(true);
            REQUIRE(copy_count == 0);
            // one copy required to get it out of the optional
            BigThing thing = try_make_big_thing(true).value();
            REQUIRE(copy_count == 1);
        }

        SUBCASE("emplace")
        {
#ifndef OKAYLIB_DISALLOW_EXCEPTIONS
            opt<std::vector<int>> mvec;
            REQUIRE(!mvec.has_value());
            mvec.emplace();
            REQUIRE(mvec.has_value());
#endif
        }

        SUBCASE("safely return copies from value optionals")
        {
            const auto get_maybe_int = []() -> opt<int> { return 1; };

            static_assert(
                std::is_same_v<decltype(get_maybe_int().value()), int&&>);

            int my_int = get_maybe_int().value();
            my_int++;
            if (my_int == 2)
                printf("no asan!\n");
        }

        SUBCASE("safely return copies from slice optionals")
        {
            std::array<uint8_t, 512> mem;
            const auto get_maybe_slice = [&mem]() -> opt<slice_t<uint8_t>> {
                return slice_t<uint8_t>(mem);
            };

            static_assert(std::is_same_v<decltype(get_maybe_slice().value()),
                                         slice_t<uint8_t>&>);

            slice_t<uint8_t> my_slice = get_maybe_slice().value();
            ok::memfill(my_slice, 0);
            for (auto byte : mem) {
                REQUIRE(byte == 0);
            }
        }

        // SUBCASE("emplace slice types")
        // {
        //     std::array<uint8_t, 128> bytes{0};
        //     for (auto [byte, index] : enumerate_mut(bytes)) {
        //         byte = index;
        //     }

        //     opt<slice_t<uint8_t>> maybe_bytes;
        //     maybe_bytes.emplace(bytes);

        //     for (auto [byte, index] : enumerate(maybe_bytes.value())) {
        //         REQUIRE(byte == bytes[index]);
        //     }
        // }

        SUBCASE("moving or copying trivially copyable type")
        {
            struct thing
            {
                int yeah = 10234;
                bool no = false;
            };

            thing copyguy;
            opt<thing> maybe_copyguy = copyguy;
            // identical to:
            opt<thing> maybe_copyguy_moved = std::move(copyguy); // NOLINT

            REQUIRE(maybe_copyguy.has_value());
            REQUIRE(maybe_copyguy_moved.has_value());
        }

        SUBCASE("copying slice")
        {
            std::array<uint8_t, 128> bytes;
            opt<slice_t<uint8_t>> maybe_bytes(bytes);

            opt<slice_t<uint8_t>> other_maybe_bytes(maybe_bytes);
            REQUIRE(
                other_maybe_bytes.value().is_alias_for(maybe_bytes.value()));

            slice_t<uint8_t> bytes_slice = other_maybe_bytes.value();
        }

#ifdef OKAYLIB_USE_FMT
        SUBCASE("formattable")
        {
            opt<std::string_view> str;
            str = "yello";
            fmt::println("optional string BEFORE: {}", str);
            str.reset();
            fmt::println("optional string AFTER: {}", str);

            std::string_view target = "reference yello";
            opt<std::string_view&> refstr(target);
            fmt::println("optional reference string BEFORE: {}", refstr);
            refstr.reset();
            fmt::println("optional reference string AFTER: {}", refstr);

            std::array<uint8_t, 128> bytes;
            opt<slice_t<uint8_t>> maybe_bytes;
            maybe_bytes.emplace(bytes);
            fmt::println("optional slice: {}", maybe_bytes);
        }
#endif
    }

    TEST_CASE("opt is range")
    {
        SUBCASE("can get size of opt")
        {
            opt<char> optchar;
            REQUIRE(ok::size(optchar) == 0);
            optchar = 'c';
            REQUIRE(ok::size(optchar) == 1);
        }

        SUBCASE("can ok_foreach over opt")
        {
            opt<char> maybechar;

            ok_foreach(auto c, maybechar)
            {
                static_assert(std::is_same_v<decltype(c), char>);
                // unreachable here
                REQUIRE(false);
            }

            ok_foreach(auto& c, maybechar)
            {
                static_assert(std::is_same_v<decltype(c), char&>);
                REQUIRE(false);
            }

            ok_foreach(const auto& c, maybechar)
            {
                static_assert(std::is_same_v<decltype(c), const char&>);
                REQUIRE(false);
            }

            maybechar = 'c';

            ok_foreach(const auto& c, maybechar) { REQUIRE(c == 'c'); }
        }

        SUBCASE("ok_foreach over optional reference has the same semantics as "
                "optional value")
        {
            opt<char&> maybechar_ref;

            ok_foreach(auto c, maybechar_ref)
            {
                static_assert(std::is_same_v<decltype(c), char>);
                // unreachable here
                REQUIRE(false);
            }

            ok_foreach(auto& c, maybechar_ref)
            {
                static_assert(std::is_same_v<decltype(c), char&>);
                REQUIRE(false);
            }

            ok_foreach(const auto& c, maybechar_ref)
            {
                static_assert(std::is_same_v<decltype(c), const char&>);
                REQUIRE(false);
            }

            static char char_c = 'c';
            maybechar_ref = char_c;

            ok_foreach(const auto& c, maybechar_ref) { REQUIRE(c == 'c'); }
        }

        SUBCASE("ok_foreach over const ref or value")
        {
            const opt<char> maybechar;
            opt<const char&> maybechar_ref;

            ok_foreach(auto c, maybechar_ref)
            {
                static_assert(std::is_same_v<decltype(c), char>);
                REQUIRE(false);
            }
            ok_foreach(auto c, maybechar)
            {
                static_assert(std::is_same_v<decltype(c), char>);
                REQUIRE(false);
            }

            ok_foreach(auto& c, maybechar_ref)
            {
                static_assert(std::is_same_v<decltype(c), const char&>);
                REQUIRE(false);
            }
            ok_foreach(auto& c, maybechar)
            {
                static_assert(std::is_same_v<decltype(c), const char&>);
                REQUIRE(false);
            }

            ok_foreach(const auto& c, maybechar_ref)
            {
                static_assert(std::is_same_v<decltype(c), const char&>);
                REQUIRE(false);
            }
            ok_foreach(const auto& c, maybechar)
            {
                static_assert(std::is_same_v<decltype(c), const char&>);
                REQUIRE(false);
            }

            static const char char_c = 'c';
            maybechar_ref = char_c;

            ok_foreach(const auto& c, maybechar_ref) { REQUIRE(c == 'c'); }
        }

        SUBCASE("nested ok_foreach for optional slice")
        {
            std::array<uint8_t, 12> bytes;
            opt<slice_t<uint8_t>> opt_bytes = bytes;
            // fill with indices
            ok_foreach(auto& slice, opt_bytes)
            {
                ok_foreach(ok_pair(byteref, index), slice | enumerate)
                {
                    byteref = index;
                }
            }

            for (size_t i = 0; i < bytes.size(); ++i) {
                REQUIRE(bytes[i] == i);
            }
        }
    }
}
