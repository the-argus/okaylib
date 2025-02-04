#include "test_header.h"
// test header must be first
#include "okay/macros/try.h"
#include "okay/res.h"
#include "testing_types.h"

#include <array>
#include <optional>
#include <vector>

using namespace ok;

static_assert(sizeof(slice_t<uint8_t>) ==
              sizeof(res_t<slice_t<uint8_t>, StatusCodeA>));

TEST_SUITE("res")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Return status codes and success from functions")
        {
            auto getresiftrue = [](bool cond) -> res_t<trivial_t, StatusCodeA> {
                if (cond) {
                    return trivial_t{
                        .whatever = 10,
                        .nothing = nullptr,
                    };
                } else {
                    return StatusCodeA::bad_access;
                }
            };

            REQUIRE((getresiftrue(true).okay()));
            REQUIRE((getresiftrue(true).release().whatever == 10));
            REQUIRE((!getresiftrue(false).okay()));
            REQUIRE(getresiftrue(false).err() == StatusCodeA::bad_access);
        }

        SUBCASE("construct type directly into result")
        {
            struct constructed_type
            {
                const char* string = nullptr;
                inline constructed_type(const std::string& instr) noexcept
                {
                    string = instr.c_str();
                }
            };
            using result = res_t<constructed_type, StatusCodeA>;
            // random thought: i assume this is undefined behavior (dangling
            // pointer to string contents) but is it actually? maybe the
            // standard specifies a const char* optimization
            auto constructed_result = [](bool cond) -> result {
                return cond ? result(std::string("hello"))
                            : result(StatusCodeA::oom);
            };

            REQUIRE(constructed_result(true).okay());
            REQUIRE(constructed_result(true).release().string != nullptr);
            REQUIRE(!constructed_result(false).okay());
            REQUIRE(constructed_result(false).err() == StatusCodeA::oom);
        }
    }

    TEST_CASE("Functionality")
    {
#ifdef OKAYLIB_USE_FMT
        SUBCASE("formattable")
        {
            using result_t = res_t<int, StatusCodeB>;
            using refresult_t = res_t<int&, StatusCodeB>;
            result_t result(10);
            int target = 10;
            refresult_t refresult(target);
            fmt::println("Result int: {}", result);
            fmt::println("Reference result int: {}", refresult);
            auto unused = result.release();
            auto unusedref = refresult.release();
            fmt::println("Released result: {}", result);
            fmt::println("Reference result int: {}", refresult);
        }
#endif

        SUBCASE("aborts on bad access")
        {
            using res = res_t<int, StatusCodeB>;
            res result(StatusCodeB::nothing);
            REQUIREABORTS({ auto nothing = result.release(); });
        }

        SUBCASE("Result released code after release is called")
        {
            using res = res_t<trivial_t, StatusCodeB>;
            res result(trivial_t{
                .whatever = 19,
                .nothing = nullptr,
            });
            REQUIRE(result.okay());
            REQUIRE(result.release().whatever == 19);
            REQUIRE(!result.okay());
            REQUIRE(result.err() == StatusCodeB::result_released);
        }

        enum class VectorCreationStatusCode : uint8_t
        {
            okay,
            result_released,
            oom,
        };

        SUBCASE("std::vector result")
        {
            using res = res_t<std::vector<size_t>, VectorCreationStatusCode>;

            res vec_result((std::vector<size_t>()));
            REQUIRE(vec_result.okay());
            auto vec = vec_result.release();
            REQUIRE(!vec_result.okay());
            vec.push_back(42);
            // pass a copy of the vector into the result
            res vec_result_modified(std::move(vec));

            std::vector<size_t> vec_modified = vec_result_modified.release();
            REQUIRE(vec_modified.size() == 1);
            REQUIRE(vec_modified[0] == 42);
        }

        SUBCASE("res inplace construction")
        {
            static size_t copies = 0;
            struct Test
            {
                std::array<int, 300> contents;
                Test() noexcept = default;
                Test(const Test& other) noexcept : contents(other.contents)
                {
                    ++copies;
                }
            };

            enum TestCode : uint8_t
            {
                okay,
                result_released,
                error
            };

            auto getRes = []() -> res_t<Test, TestCode> {
                return res_t<Test, TestCode>{std::in_place};
            };

            auto myres = getRes();
            auto& testref = myres.release_ref();

            REQUIRE(copies == 0);
        }

        SUBCASE("reference result")
        {
            enum class ReferenceCreationStatusCode : uint8_t
            {
                okay,
                result_released,
                null_reference,
            };

            using res = res_t<std::vector<int>&, ReferenceCreationStatusCode>;

            auto makeveciftrue = [](bool cond) -> res {
                if (cond) {
                    auto* vec = new std::vector<int>;
                    vec->push_back(5);
                    return *vec;
                } else {
                    return ReferenceCreationStatusCode::null_reference;
                }
            };

            REQUIRE(!makeveciftrue(false).okay());
            REQUIREABORTS({ auto nothing = makeveciftrue(false).release(); });

            res result = makeveciftrue(true);
            REQUIRE(result.okay());
            auto& vec = result.release();
            REQUIRE(vec[0] == 5);
            vec.push_back(10);
            REQUIRE(!result.okay());
            delete &vec;
        }

        SUBCASE("res::to_opt() for reference result")
        {
            using res = res_t<int&, StatusCodeA>;

            int i = 9;
            res test = i;
            REQUIRE(test.okay() == test.to_opt().has_value());
            REQUIRE(test.to_opt().is_alias_for(i));
            res test2 = StatusCodeA::oom;
            REQUIRE(test2.okay() == test2.to_opt().has_value());
        }

        SUBCASE("res::to_opt() for value result")
        {
            using res = res_t<int&, StatusCodeA>;

            int i = 9;
            res test = i;
            REQUIRE(test.okay() == test.to_opt().has_value());
            res test2 = StatusCodeA::oom;
            REQUIRE(test2.okay() == test2.to_opt().has_value());
        }

        SUBCASE("const reference result")
        {
            enum class ReferenceCreationStatusCode : uint8_t
            {
                okay,
                result_released,
                null_reference,
            };

            using res =
                res_t<const std::vector<int>&, ReferenceCreationStatusCode>;

            auto makeveciftrue = [](bool cond) -> res {
                if (cond) {
                    auto* vec = new std::vector<int>;
                    return *vec;
                } else {
                    return ReferenceCreationStatusCode::null_reference;
                }
            };

            REQUIRE(!makeveciftrue(false).okay());
            REQUIREABORTS({ auto nothing = makeveciftrue(false).release(); });

            res result = makeveciftrue(true);
            REQUIRE(result.okay());
            // this doesnt work
            // std::vector<int>& vec = result.release();
            const auto& vec = result.release();
            // this doesnt work
            // vec.push_back(10);
            REQUIRE(!result.okay());
            delete &vec;
        }

        SUBCASE("how much result copies its contents")
        {
            static int copies = 0;
            static int moves = 0;
            struct increment_on_copy_or_move
            {
                int one = 1;
                float two = 2;
                // NOLINTNEXTLINE
                constexpr increment_on_copy_or_move(int _one,
                                                    float _two) noexcept
                    : one(_one), two(_two)
                {
                }
                inline increment_on_copy_or_move&
                operator=(const increment_on_copy_or_move& other)
                {
                    if (&other == this)
                        return *this;
                    ++copies;
                    one = other.one;
                    two = other.two;
                    return *this;
                }
                inline increment_on_copy_or_move&
                operator=(increment_on_copy_or_move&& other) noexcept
                {
                    if (&other == this)
                        return *this;
                    ++moves;
                    one = other.one;
                    two = other.two;
                    other.one = 0;
                    other.two = 0;
                    return *this;
                }
                inline increment_on_copy_or_move(
                    const increment_on_copy_or_move& other)
                {
                    *this = other;
                }
                inline increment_on_copy_or_move(
                    increment_on_copy_or_move&& other) noexcept
                {
                    *this = std::move(other);
                }
            };

            enum class dummy_status_code_e : uint8_t
            {
                okay,
                result_released,
                dummy_error,
            };

            using res = res_t<increment_on_copy_or_move, dummy_status_code_e>;

            // no copy, only move
            res res_1 = increment_on_copy_or_move(1, 2);
            REQUIRE(res_1.okay());
            REQUIRE(copies == 0);
            REQUIRE(moves == 1);
            auto& resref_1 = res_1.release_ref();
            REQUIRE(copies == 0);
            REQUIRE(moves == 1);
            res res_2 = increment_on_copy_or_move(1, 2);
            // moves increment to move the item into the result
            REQUIRE(moves == 2);
            REQUIRE(copies == 0);
            REQUIRE(res_2.okay());
            REQUIREABORTS({ auto nothing = res_1.release(); });
            increment_on_copy_or_move dummy_2 = res_2.release();
            // moves incremented because release() moves the item out of the
            // result
            REQUIRE(copies == 0);
            REQUIRE(moves == 3);
            increment_on_copy_or_move dummy_3 = dummy_2; // NOLINT
            REQUIRE(copies == 1);
            REQUIRE(moves == 3);
        }
    }

    TEST_CASE("slice result")
    {
        static std::array<int, 8> mem{};
        using slice_int_result = res_t<slice_t<int>, StatusCodeA>;
        auto get_slice = []() -> slice_int_result { return mem; };

        SUBCASE("slice release_ref and conversion")
        {
            auto slice_res = get_slice();
            REQUIRE(slice_res.okay());

            static_assert(
                std::is_same_v<decltype(slice_res.release()), slice_t<int>>);
            static_assert(std::is_same_v<decltype(slice_res.release_ref()),
                                         slice_t<int>&>);

            auto& slice = slice_res.release_ref();

            for (size_t i = 0; i < slice.size(); ++i) {
                REQUIRE(slice[i] == mem.at(i));
            }
        }

        SUBCASE("slice release copy")
        {
            auto slice_res = get_slice();
            REQUIRE(slice_res.okay());
            auto slice = slice_res.release();
            for (size_t i = 0; i < slice.size(); ++i) {
                REQUIRE(slice[i] == mem.at(i));
            }
        }

        SUBCASE("slice not always okay")
        {
            slice_int_result res = StatusCodeA::oom;
            REQUIRE(!res.okay());
            REQUIRE(res.err() == StatusCodeA::oom);
        }

        SUBCASE("cannot assign okay to res")
        {
            try {
                slice_int_result res = StatusCodeA::okay;
                REQUIRE(false); // above should throw
            } catch (...) {
            }
        }

        // this would mean slice is default constructible
        static_assert(
            !std::is_constructible_v<slice_int_result, std::in_place_t>);

        SUBCASE("slice res to opt")
        {
            std::array<int, 100> myints;
            slice_int_result myslice = myints;
            REQUIRE(myslice.okay() == myslice.to_opt().has_value());
            slice_int_result myslicetwo = StatusCodeA::oom;
            REQUIRE(myslicetwo.okay() == myslicetwo.to_opt().has_value());
        }
    }

    TEST_CASE("try macro")
    {
        enum class ExampleError : uint8_t
        {
            okay,
            result_released,
            error,
        };
        SUBCASE("try with matching return type")
        {
            std::array<uint8_t, 512> memory;
            std::fill(memory.begin(), memory.end(), 1);

            auto fake_alloc =
                [&memory](bool should_succeed,
                          size_t bytes) -> res_t<uint8_t*, ExampleError> {
                if (should_succeed) {
                    return memory.data();
                } else {
                    return ExampleError::error;
                }
            };

            auto make_zeroed_buffer =
                [fake_alloc](bool should_succeed,
                             size_t bytes) -> res_t<uint8_t*, ExampleError> {
                TRY_BLOCK(yielded_memory, fake_alloc(should_succeed, bytes), {
                    static_assert(
                        std::is_same_v<decltype(yielded_memory), uint8_t*>);
                    for (size_t i = 0; i < bytes; ++i) {
                        yielded_memory[i] = 0;
                    }
                    return yielded_memory;
                });
            };

            auto failed_result = make_zeroed_buffer(false, 100);
            for (size_t i = 0; i < 100; ++i) {
                REQUIRE(memory[i] == 1);
            }
            auto succeeded_result = make_zeroed_buffer(true, 100);
            REQUIRE(!failed_result.okay());
            REQUIRE(succeeded_result.okay());
            for (size_t i = 0; i < 100; ++i) {
                REQUIRE(memory[i] == 0);
            }
        }

        SUBCASE("try_ref macro")
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
                [](bool should_succeed) -> res_t<BigThing, ExampleError> {
                if (should_succeed)
                    return res_t<BigThing, ExampleError>{std::in_place};
                else
                    return ExampleError::error;
            };

            static_assert(
                !decltype(detail::is_lvalue(try_make_big_thing(true)))::value,
                "return value from function not recognized as rvalue");
            {
                int test = 0;
                static_assert(decltype(detail::is_lvalue(test))::value,
                              "local integer not recognized as lvalue");
            }

            // makes no copies: uses try_ref
            auto attempt =
                [try_make_big_thing](bool should_succeed) -> ExampleError {
                TRY_REF_BLOCK(big_thing, try_make_big_thing(should_succeed), {
                    for (int& number : big_thing.numbers) {
                        number = 0;
                    }
                    return ExampleError::okay;
                });
            };

            auto attempt_copy =
                [try_make_big_thing](bool should_succeed) -> ExampleError {
                TRY_BLOCK(big_thing, try_make_big_thing(should_succeed), {
                    for (int& number : big_thing.numbers) {
                        number = 0;
                    }
                    return ExampleError::okay;
                });
            };

            auto try_make_big_thing_optional =
                [](bool should_succeed) -> std::optional<BigThing> {
                if (should_succeed)
                    return std::optional<BigThing>{std::in_place};
                else
                    return {};
            };

            auto optional_attempt = [try_make_big_thing_optional]() -> bool {
                auto attempt = try_make_big_thing_optional(true);
                if (!attempt.has_value())
                    return false;
                // avoiding copy here with reference
                auto& big_thing = attempt.value();

                {
                    for (int& number : big_thing.numbers) {
                        number = 0;
                    }
                    return true;
                }
            };

            REQUIRE(attempt(false) == ExampleError::error);
            REQUIRE(copy_count == 0);
            attempt(true);
            REQUIRE(copy_count == 0);
            attempt_copy(true);
            REQUIRE(copy_count == 1);
            optional_attempt();
            // optional causes no copies- stored as reference
            REQUIRE(copy_count == 1);
        }
    }
}
