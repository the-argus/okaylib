#include "test_header.h"
// test header must be first
#include "okay/error.h"
#include "okay/macros/try.h"
#include "okay/slice.h"
#include "testing_types.h"

#include <array>
#include <optional>
#include <vector>

using namespace ok;

static_assert(status_enum<StatusCodeA>);
static_assert(status_enum<StatusCodeB>);
static_assert(status_object<status<StatusCodeA>>);
static_assert(status_object<status<StatusCodeB>>);
// can't nest a res as a status object
static_assert(!status_object<res<int, status<StatusCodeB>>>);
// if status and payload are both trivially copyable, then so is the res
static_assert(std::is_trivially_copyable_v<res<int, status<StatusCodeB>>>);
static_assert(std::is_trivially_copyable_v<res<int&, status<StatusCodeB>>>);

struct destroyed
{
    static int destructions;
    int me = 0;
    ~destroyed() { destructions++; }
};
int destroyed::destructions = 0;

TEST_SUITE("res")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Return status codes and success from functions")
        {
            auto getresiftrue = [](bool cond) -> res<trivial_t, StatusCodeA> {
                if (cond) {
                    return trivial_t{
                        .whatever = 10,
                        .nothing = nullptr,
                    };
                } else {
                    return StatusCodeA::bad_access;
                }
            };

            REQUIRE((getresiftrue(true).is_success()));
            REQUIRE((getresiftrue(true).unwrap().whatever == 10));
            REQUIRE((!getresiftrue(false).is_success()));
            REQUIRE(getresiftrue(false).status() == StatusCodeA::bad_access);
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
            using result = res<constructed_type, StatusCodeA>;
            // random thought: i assume this is undefined behavior (dangling
            // pointer to string contents) but is it actually? maybe the
            // standard specifies a const char* optimization
            auto constructed_result = [](bool cond) -> result {
                return cond ? result(std::string("hello"))
                            : result(StatusCodeA::oom);
            };

            REQUIRE(constructed_result(true).is_success());
            REQUIRE(constructed_result(true).unwrap().string != nullptr);
            REQUIRE(!constructed_result(false).is_success());
            REQUIRE(constructed_result(false).status() == StatusCodeA::oom);
        }
    }

    TEST_CASE("Functionality")
    {
#ifdef OKAYLIB_USE_FMT
        SUBCASE("formattable")
        {
            using result_t = res<int, StatusCodeB>;
            using refresult_t = res<int&, StatusCodeB>;
            result_t result(10);
            int target = 10;
            refresult_t refresult(target);
            fmt::println("Result int: {}", result);
            fmt::println("Reference result int: {}", refresult);
            auto unused = result.unwrap();
            auto unusedref = refresult.unwrap();
            fmt::println("Released result: {}", result);
            fmt::println("Reference result int: {}", refresult);
        }
#endif

        SUBCASE("aborts on bad access")
        {
            using res = res<int, StatusCodeB>;
            res result(StatusCodeB::nothing);
            REQUIREABORTS({ auto nothing = result.unwrap(); });
        }

        SUBCASE("Result released code after release is called")
        {
            using res = res<trivial_t, StatusCodeB>;
            res result(trivial_t{
                .whatever = 19,
                .nothing = nullptr,
            });
            REQUIRE(result.is_success());
            REQUIRE(result.unwrap().whatever == 19);
            REQUIRE(result.is_success());
            REQUIRE(result.status() == StatusCodeB::success);
        }

        enum class VectorCreationStatusCode : uint8_t
        {
            success,
            oom,
        };

        SUBCASE("std::vector result")
        {
            using res = res<std::vector<size_t>, VectorCreationStatusCode>;

            res vec_result = std::vector<size_t>();
            REQUIRE(vec_result.is_success());
            auto vec = vec_result.unwrap();
            vec.push_back(42);
            // pass a copy of the vector into the result
            res vec_result_modified(std::move(vec));

            std::vector<size_t> vec_modified = vec_result_modified.unwrap();
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
                success,
                error
            };

            auto getRes = []() -> res<Test, TestCode> {
                return res<Test, TestCode>(ok::in_place);
            };

            copies = 0;

            auto myres = getRes();
            Test& testref = myres.unwrap();

            REQUIRE(copies == 0);
        }

        SUBCASE("creating a successful res only calls move if possible")
        {
            static size_t copies = 0;
            static size_t moves = 0;
            static size_t destructions = 0;
            struct Test
            {
                std::vector<int> contents;

                Test() = delete;
                Test(int i) { contents.resize(i); }
                Test(const Test& other) =
                    delete; //: contents(other.contents) { ++copies; }
                Test& operator=(const Test& other) = delete;
                // {
                //     ++copies;
                //     contents = other.contents;
                //     return *this;
                // }
                Test(Test&& other) : contents(std::move(other.contents))
                {
                    ++moves;
                }
                Test& operator=(Test&& other)
                {
                    ++moves;
                    contents = std::move(other.contents);
                    return *this;
                }
                ~Test() { ++destructions; }
            };

            auto maketest = [] { return res<Test, StatusCodeA>(Test(1)); };

            copies = 0;
            moves = 0;
            destructions = 0;

            auto res = maketest();

            REQUIRE(copies == 0);
            REQUIRE(moves == 1);
            REQUIRE(destructions == 1);

            res = maketest();

            REQUIRE(copies == 0);
            // move when constructing res in the function, then assignment
            REQUIRE(moves == 3);
            // each move has the previous shell get destroyed
            REQUIRE(destructions == 3);
        }

        SUBCASE("reference result")
        {
            enum class ReferenceCreationStatusCode : uint8_t
            {
                success,
                null_reference,
            };

            using res = res<std::vector<int>&, ReferenceCreationStatusCode>;

            auto makeveciftrue = [](bool cond) -> res {
                if (cond) {
                    auto* vec = new std::vector<int>;
                    vec->push_back(5);
                    return *vec;
                } else {
                    return ReferenceCreationStatusCode::null_reference;
                }
            };

            REQUIRE(!makeveciftrue(false).is_success());
            REQUIREABORTS({ auto nothing = makeveciftrue(false).unwrap(); });

            res result = makeveciftrue(true);
            REQUIRE(result.is_success());
            auto& vec = result.unwrap();
            REQUIRE(vec[0] == 5);
            vec.push_back(10);
            delete &vec;
        }

        SUBCASE("res::to_opt() for reference result")
        {
            using res = res<int&, StatusCodeA>;

            int i = 9;
            res test = i;
            REQUIRE(test.is_success() == test.to_opt().has_value());
            REQUIRE(test.to_opt().is_alias_for(i));
            res test2 = StatusCodeA::oom;
            REQUIRE(test2.is_success() == test2.to_opt().has_value());
        }

        SUBCASE("res::to_opt() for value result")
        {
            using res = res<int&, StatusCodeA>;

            int i = 9;
            res test = i;
            REQUIRE(test.is_success() == test.to_opt().has_value());
            res test2 = StatusCodeA::oom;
            REQUIRE(test2.is_success() == test2.to_opt().has_value());
        }

        SUBCASE("const reference result")
        {
            enum class ReferenceCreationStatusCode : uint8_t
            {
                success,
                null_reference,
            };

            using res =
                res<const std::vector<int>&, ReferenceCreationStatusCode>;

            auto makeveciftrue = [](bool cond) -> res {
                if (cond) {
                    auto* vec = new std::vector<int>;
                    return *vec;
                } else {
                    return ReferenceCreationStatusCode::null_reference;
                }
            };

            REQUIRE(!makeveciftrue(false).is_success());
            REQUIREABORTS({ auto nothing = makeveciftrue(false).unwrap(); });

            res result = makeveciftrue(true);
            REQUIRE(result.is_success());
            // this doesnt work
            // std::vector<int>& vec = result.unwrap();
            const auto& vec = result.unwrap();
            // this doesnt work
            // vec.push_back(10);
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
                success,
                dummy_error,
            };

            using res = res<increment_on_copy_or_move, dummy_status_code_e>;

            // no copy, only move
            res res_1 = increment_on_copy_or_move(1, 2);
            REQUIRE(res_1.is_success());
            REQUIRE(copies == 0);
            REQUIRE(moves == 1);
            auto& resref_1 = res_1.unwrap();
            REQUIRE(copies == 0);
            REQUIRE(moves == 1);
            res res_2 = increment_on_copy_or_move(1, 2);
            // moves increment to move the item into the result
            REQUIRE(moves == 2);
            REQUIRE(copies == 0);
            REQUIRE(res_2.is_success());
            increment_on_copy_or_move dummy_2 = std::move(res_2).unwrap();
            REQUIRE(copies == 0);
            REQUIRE(moves == 3);
            increment_on_copy_or_move dummy_3 = dummy_2; // NOLINT
            REQUIRE(copies == 1);
            REQUIRE(moves == 3);
        }

        SUBCASE("res only destroys its contents if its not an error")
        {
            REQUIRE(destroyed::destructions == 0);
            {
                destroyed test;
            }
            REQUIRE(destroyed::destructions == 1);
            destroyed::destructions = 0;
            {
                res<destroyed, StatusCodeA> r1 = StatusCodeA::oom;
                res<destroyed, StatusCodeA> r2 = StatusCodeA::oom;
                res<destroyed, StatusCodeA> r3 = StatusCodeA::oom;
                res<destroyed, StatusCodeA> r4 = StatusCodeA::oom;
            }
            REQUIRE(destroyed::destructions == 0);
            {
                res<destroyed, StatusCodeA> r1 = ok::in_place;
                res<destroyed, StatusCodeA> r2 = ok::in_place;
                res<destroyed, StatusCodeA> r3 = ok::in_place;
                res<destroyed, StatusCodeA> r4 = ok::in_place;
            }
            REQUIRE(destroyed::destructions == 4);
        }
    }

    TEST_CASE("slice result")
    {
        static std::array<int, 8> mem{};
        using slice_int_result = res<slice<int>, StatusCodeA>;
        static_assert(std::is_convertible_v<decltype(mem)&, slice<int>>);
        auto get_slice = []() -> slice_int_result { return mem; };

        SUBCASE("slice unwrap and conversion")
        {
            auto slice_res = get_slice();
            REQUIRE(slice_res.is_success());

            static_assert(
                std::is_same_v<decltype(slice_res.unwrap()), slice<int>&>);

            auto& slice = slice_res.unwrap();

            for (size_t i = 0; i < slice.size(); ++i) {
                REQUIRE(slice[i] == mem.at(i));
            }
        }

        SUBCASE("slice release copy")
        {
            auto slice_res = get_slice();
            REQUIRE(slice_res.is_success());
            auto slice = slice_res.unwrap();
            for (size_t i = 0; i < slice.size(); ++i) {
                REQUIRE(slice[i] == mem.at(i));
            }
        }

        SUBCASE("slice not always success")
        {
            slice_int_result res = StatusCodeA::oom;
            REQUIRE(!res.is_success());
            REQUIRE(res.status() == StatusCodeA::oom);
        }

        SUBCASE("cannot assign success to res")
        {
            try {
                slice_int_result res = StatusCodeA::success;
                REQUIRE(false); // above should throw
            } catch (reserve::_abort_exception& e) {
                // minor detail to decrease spamming of logs here
                e.cancel_stack_trace_print();
            }
        }

        // this would mean slice is default constructible
        static_assert(
            !is_std_constructible_v<slice_int_result, ok::in_place_t>);

        SUBCASE("slice res to opt")
        {
            std::array<int, 100> myints;
            slice_int_result myslice = myints;
            REQUIRE(myslice.is_success() == myslice.to_opt().has_value());
            slice_int_result myslicetwo = StatusCodeA::oom;
            REQUIRE(myslicetwo.is_success() == myslicetwo.to_opt().has_value());
        }
    }

    TEST_CASE("try macro")
    {
        enum class ExampleError : uint8_t
        {
            success,
            error,
        };
        SUBCASE("try with matching return type")
        {
            std::array<uint8_t, 512> memory;
            std::fill(memory.begin(), memory.end(), 1);

            auto fake_alloc =
                [&memory](bool should_succeed,
                          size_t bytes) -> res<uint8_t*, ExampleError> {
                if (should_succeed) {
                    return memory.data();
                } else {
                    return ExampleError::error;
                }
            };

            auto make_zeroed_buffer =
                [fake_alloc](bool should_succeed,
                             size_t bytes) -> res<uint8_t*, ExampleError> {
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
            REQUIRE(!failed_result.is_success());
            REQUIRE(succeeded_result.is_success());
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
                [](bool should_succeed) -> res<BigThing, ExampleError> {
                if (should_succeed)
                    return res<BigThing, ExampleError>{ok::in_place};
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
                    return ExampleError::success;
                });
            };

            auto attempt_copy =
                [try_make_big_thing](bool should_succeed) -> ExampleError {
                TRY_BLOCK(big_thing, try_make_big_thing(should_succeed), {
                    for (int& number : big_thing.numbers) {
                        number = 0;
                    }
                    return ExampleError::success;
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
