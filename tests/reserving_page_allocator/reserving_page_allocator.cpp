#include "okay/defer.h"
#include "test_header.h"
// test header must be first
#include "okay/allocators/reserving_page_allocator.h"

using namespace ok;

const auto free_res = [](auto& ally, const alloc::result_t<bytes_t>& res) {
    ally.deallocate(
        res.transform_value(&bytes_t::unchecked_address_of_first_item)
            .copy_or(nullptr));
};

TEST_SUITE("reserving page allocator")
{
    TEST_CASE("special member functions")
    {
        SUBCASE("construction / destruction")
        {
            reserving_page_allocator_t ally({.pages_reserved = 1});
        }

        SUBCASE("move construction")
        {
            reserving_page_allocator_t ally({.pages_reserved = 1});
            reserving_page_allocator_t ally2 = std::move(ally);
        }
    }

    TEST_CASE("impl_allocate")
    {
        SUBCASE("bad alignment is not supported")
        {
            reserving_page_allocator_t ally({.pages_reserved = 1});

            auto res = ally.allocate(alloc::request_t{
                .num_bytes = 1,
                .alignment = mmap::get_page_size() * 2,
            });

            REQUIRE(!res.is_success());
        }

        SUBCASE("can allocate different sizes not rounded to pagesize, above "
                "or below the reserved size")
        {
            reserving_page_allocator_t ally({.pages_reserved = 2});

            const size_t page_size = mmap::get_page_size();

            const auto res = ally.allocate(alloc::request_t{
                .num_bytes = page_size,
            });
            const defer f1 = [&] { free_res(ally, res); };

            const auto res2 = ally.allocate(alloc::request_t{
                .num_bytes = page_size - 1,
            });
            const defer f2 = [&] { free_res(ally, res2); };

            const auto res3 = ally.allocate(alloc::request_t{
                .num_bytes = page_size + 1,
            });
            const defer f3 = [&] { free_res(ally, res3); };

            // cannot allocate over reserved page numbers
            const auto res4 = ally.allocate(alloc::request_t{
                .num_bytes = page_size * 2,
            });
            const defer f4 = [&] { free_res(ally, res4); };
            const auto res5 = ally.allocate(alloc::request_t{
                .num_bytes = (page_size * 2) + 1,
            });
            const defer f5 = [&] { free_res(ally, res5); };

            REQUIRE(res.is_success());
            REQUIRE(res2.is_success());
            REQUIRE(res3.is_success());
            REQUIRE(res4.is_success());
            REQUIRE(res5.is_success());
        }

        SUBCASE("cannot reallocate beyond reserved size")
        {
            reserving_page_allocator_t ally({.pages_reserved = 2});

            const size_t page_size = mmap::get_page_size();

            const auto smallmem_res = ally.allocate(alloc::request_t{
                .num_bytes = page_size * 1,
            });
            bytes_t smallmem = OKAYLIB_REQUIRE_RES_WITH_BACKTRACE(smallmem_res);
            uint8_t* begin = smallmem.unchecked_address_of_first_item();
            defer f1 = [&] { free_res(ally, smallmem); };

            const auto smallmem_reallocate_res = ally.reallocate({
                .memory = smallmem,
                .new_size_bytes = 2 * page_size,
                .flags = alloc::realloc_flags::in_place_orelse_fail,
            });
            smallmem =
                OKAYLIB_REQUIRE_RES_WITH_BACKTRACE(smallmem_reallocate_res);

            const auto bigmem_reallocate_res = ally.reallocate({
                .memory = smallmem,
                .new_size_bytes = 3 * page_size,
                .flags = alloc::realloc_flags::in_place_orelse_fail,
            });

            REQUIRE(!bigmem_reallocate_res.is_success());
        }
    }
}
