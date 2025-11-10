// disable backtraces which change the size of statuses, so then static_assert
// that the statis is the same size as the enum fails
#undef OKAYLIB_TESTING_BACKTRACE
#include "test_header.h"
// test header must be first
#include "okay/anystatus.h"
#include "okay/error.h"

#include <array>

using namespace ok;

enum class GenericError : uint8_t
{
    success,
    no_value,
    evil,
};
enum class OtherError : uint8_t
{
    success,
    no_value,
    oom,
    not_allowed,
};

class ExamplePolymorphicStatus : public ok::abstract_status_t
{
    bool success = false;

  public:
    [[nodiscard]] void* try_cast_to(uint64_t typehash) noexcept override
    {
        return ok::ctti::typehash<ExamplePolymorphicStatus>() == typehash
                   ? this
                   : nullptr;
    }

    [[nodiscard]] bool is_success() const noexcept override { return success; }

    // no need to free it, the tests will just put it into some stack space
    [[nodiscard]] opt<ok::allocator_t&> allocator() noexcept override
    {
        return {};
    }

    ~ExamplePolymorphicStatus() override = default;
};

static_assert(sizeof(status<GenericError>) == 1,
              "status is not a single byte in size");

TEST_SUITE("status")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("construction")
        {
            status<GenericError> stat(GenericError::success);
            status<GenericError> stat2 = GenericError::success;
            status<GenericError> stat3 = GenericError::evil;
            REQUIRE(stat.is_success());
            REQUIRE(stat2.is_success());
            REQUIRE(!stat3.is_success());
            std::array<decltype(stat), 3> statuses = {stat, stat2, stat3};
            for (auto s : statuses) {
                anyerr_t any = s;
                REQUIRE(s.is_success() == any.is_success());
            }
        }
        SUBCASE("copy assignment")
        {
            status<GenericError> stat(GenericError::success);
            auto stat2 = stat;

            static_assert(std::is_copy_assignable_v<decltype(stat)>,
                          "Status is not copy assignable");

            REQUIRE((stat2.is_success() && stat.is_success()));
        }
    }
    TEST_CASE("functionality")
    {
        //  NOTE: this case is really just to make sure it compiles
        SUBCASE("turning different statuses into anystatus")
        {
            auto memalloc = []() -> status<OtherError> {
                void* bytes = malloc(100);
                if (!bytes)
                    return OtherError::oom;
                free(bytes);
                return OtherError::success;
            };

            auto floatmath = []() -> status<GenericError> {
                const long i = 12378389479823989;
                const long j = 85743323894782374;

                const float test_one =
                    static_cast<float>(i) / static_cast<float>(j);
                const double test_two =
                    static_cast<double>(i) / static_cast<double>(j);

                if (std::abs(static_cast<double>(test_one) - test_two) <
                    double(0.1)) {
                    return GenericError::success;
                }
                return GenericError::evil;
            };

            auto dostuff = [memalloc, floatmath]() -> anyerr_t {
                auto stat = memalloc();
                if (!stat.is_success())
                    return stat;
                auto stat2 = floatmath();
                return stat2;
            };

            dostuff();
        }

        SUBCASE("anystatus conversion at runtime")
        {
            auto fakealloc = [](bool should_alloc) -> status<OtherError> {
                if (!should_alloc)
                    return OtherError::not_allowed;
                return OtherError::success;
            };

            auto yesorno = [](bool cond) -> status<GenericError> {
                if (cond)
                    return GenericError::success;
                else
                    return GenericError::evil;
            };

            auto dostuff = [fakealloc, yesorno](bool one,
                                                bool two) -> anyerr_t {
                auto status1 = fakealloc(one);
                if (!status1.is_success())
                    return status1;
                return yesorno(two);
            };

            REQUIRE(dostuff(true, true).is_success());
            REQUIRE(!dostuff(false, true).is_success());
            REQUIRE(!dostuff(true, false).is_success());
            REQUIRE(!dostuff(false, false).is_success());
        }

        SUBCASE("polymorphic anystatus conversions")
        {
            detail::uninitialized_storage_t<ExamplePolymorphicStatus> uninit;

            auto test = [&]() -> res<int, anystatus_t> {
                const float i = rand() / float(INT_MAX);
                if (i > .5f) {
                    return int(i * float(INT_MAX));
                } else {
                    stdc::construct_at(&uninit.value);
                    return anystatus_t(uninit.value);
                }
            }();
        }
    }
}
