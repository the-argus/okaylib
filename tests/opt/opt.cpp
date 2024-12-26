#include "test_header.h"
// test header must be first
#include "okay/opt.h"
#include "okay/slice.h"
#include "testing_types.h"

#include <array>

using namespace ok;

static_assert(sizeof(opt_t<slice_t<int>>) == sizeof(slice_t<int>),
              "Optional slice types are a different size than slices");

static_assert(sizeof(opt_t<int&>) == sizeof(int*),
              "Optional reference types are a different size than pointers");

TEST_SUITE("opt")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            opt_t<int> def;
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
            opt_t<int> has = 10;
            REQUIRE(has.has_value());
            REQUIRE(has == 10);
            REQUIRE(has.value() == 10);
        }

        SUBCASE("comparison")
        {
            opt_t<int> one = 100;
            opt_t<int> two;
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
            opt_t<int> nothing;
            REQUIRE(!nothing);
            REQUIRE(!nothing.has_value());
            opt_t<int> something = 1;
            REQUIRE(something);
            REQUIRE(something.has_value());

            auto bool_to_optional = [](bool input) -> opt_t<int> {
                return input ? opt_t<int>(3478) : opt_t<int>{};
            };

            if (auto result = bool_to_optional(true)) {
                REQUIRE(result);
                REQUIRE(result.value() == 3478);
                REQUIRE(result == 3478);

                REQUIRE(result != opt_t<int>(3477));
                REQUIRE(result != 3477);
                REQUIRE(result != opt_t<int>{});
            }

            if (auto result = bool_to_optional(false)) {
                REQUIRE(false); // should never happen
            }
        }

        SUBCASE("non owning slice")
        {
            std::vector<uint8_t> bytes = {20, 32, 124, 99, 1};
            opt_t<slice_t<uint8_t>> maybe_array;
            REQUIRE(!maybe_array.has_value());

            // opt_t<std::vector<uint8_t>> optional_vector_copy(bytes);
            // maybe_array = optional_vector_copy;

            // TODO: make sure slice and opt_t<slice_t> cannot assume ownership
            // of objects that are not trivially moveable
        }

#ifndef OKAYLIB_NO_CHECKED_MOVES
        SUBCASE("moved type is marked as nullopt")
        {
            std::vector<int> nums = {1203, 12390, 12930, 430};

            auto consume = [](opt_t<std::vector<int>>&& maybe_moved) {
                if (!maybe_moved)
                    return;

                REQUIRE(!maybe_moved.value().empty());
                std::vector<int> our_nums = std::move(maybe_moved.value());
                REQUIRE(!maybe_moved.has_value());
                REQUIRE(!our_nums.empty());
                our_nums.resize(0);
            };

            opt_t<std::vector<int>> maybe_copy;
            REQUIRE(!maybe_copy);
            consume(std::move(maybe_copy));
            REQUIRE(!maybe_copy); // this is defined behavior with checked moves

            opt_t<std::vector<int>> maybe_moved = std::move(nums);
            REQUIRE(maybe_moved);
            REQUIRE(!maybe_moved.value().empty());
            consume(std::move(maybe_moved));
            REQUIRE(!maybe_moved);
        }
#endif
    }

    TEST_CASE("Functionality")
    {
        SUBCASE("Resetting")
        {
            opt_t<std::vector<int>> vec;
            // null by default
            REQUIRE(!vec.has_value());
            vec.emplace();
            REQUIRE(vec.has_value());
            vec.value().push_back(42);
            REQUIRE(vec.value()[0] == 42);
            vec.reset();
            REQUIRE(!vec.has_value());
        }

        SUBCASE("Aborts on null")
        {
            opt_t<int> nope;
            REQUIREABORTS(++nope.value());
        }

        SUBCASE("moving non-trivially-copyable type")
        {
            moveable_t moveguy;
            int bytes = std::snprintf(moveguy.nothing, 50, "nope");

            opt_t<moveable_t> maybe_moveguy = std::move(moveguy);
            REQUIRE(maybe_moveguy.has_value());
            // and this shouldnt work
            // opt<moveable_t> maybe_moveguy = moveguy;

            REQUIRE(strcmp(maybe_moveguy.value().nothing, "nope") == 0);
        }

        SUBCASE("optional reference types")
        {
            int test = 10;
            opt_t<const int&> testref;
            opt_t<int&> testref2;
            REQUIRE(!testref.has_value());
            REQUIRE(!testref2.has_value());
            testref = test;
            REQUIRE(testref.value() == test);
            REQUIRE(!testref.is_alias(testref2));

            int test2 = 10;
            testref2 = test2;
            REQUIRE(testref2.value() == test2);
            REQUIRE(testref2.is_alias(test2));
            REQUIRE(!testref.is_alias(test2));
            REQUIRE(testref.value() == test2);
            REQUIRE(!testref.is_alias(testref2));

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
                [](bool should_succeed) -> opt_t<BigThing> {
                if (should_succeed)
                    return opt_t<BigThing>{std::in_place};
                else
                    return {};
            };

            opt_t<BigThing> maybe_thing = try_make_big_thing(true);
            opt_t<BigThing> maybe_not_thing = try_make_big_thing(true);
            REQUIRE(copy_count == 0);
            // one copy required to get it out of the optional
            BigThing thing = try_make_big_thing(true).value();
            REQUIRE(copy_count == 1);
        }

        SUBCASE("emplace")
        {
            opt_t<std::vector<int>> mvec;
            REQUIRE(!mvec.has_value());
            mvec.emplace();
            REQUIRE(mvec.has_value());
        }

        SUBCASE("safely return copies from value optionals")
        {
            const auto get_maybe_int = []() -> opt_t<int> { return 1; };

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
            const auto get_maybe_slice = [&mem]() -> opt_t<slice_t<uint8_t>> {
                return slice_t<uint8_t>(mem);
            };

            static_assert(std::is_same_v<decltype(get_maybe_slice().value()),
                                         slice_t<uint8_t>&>);

            slice_t<uint8_t> my_slice = get_maybe_slice().value();
            std::fill(my_slice.begin(), my_slice.end(), 0);
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

        //     opt_t<slice_t<uint8_t>> maybe_bytes;
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
            opt_t<thing> maybe_copyguy = copyguy;
            // identical to:
            opt_t<thing> maybe_copyguy_moved = std::move(copyguy); // NOLINT

            REQUIRE(maybe_copyguy.has_value());
            REQUIRE(maybe_copyguy_moved.has_value());
        }

        SUBCASE("copying slice")
        {
            std::array<uint8_t, 128> bytes;
            opt_t<slice_t<uint8_t>> maybe_bytes(bytes);

            opt_t<slice_t<uint8_t>> other_maybe_bytes(maybe_bytes);
            REQUIRE(other_maybe_bytes == maybe_bytes);

            slice_t<uint8_t> bytes_slice = other_maybe_bytes.value();
        }

#ifdef OKAYLIB_USE_FMT
        SUBCASE("formattable")
        {
            opt_t<std::string_view> str;
            str = "yello";
            fmt::println("optional string BEFORE: {}", str);
            str.reset();
            fmt::println("optional string AFTER: {}", str);

            std::string_view target = "reference yello";
            opt_t<std::string_view&> refstr(target);
            fmt::println("optional reference string BEFORE: {}", refstr);
            refstr.reset();
            fmt::println("optional reference string AFTER: {}", refstr);

            std::array<uint8_t, 128> bytes;
            opt_t<slice_t<uint8_t>> maybe_bytes;
            maybe_bytes.emplace(bytes);
            fmt::println("optional slice: {}", maybe_bytes);
        }
#endif
    }
}
