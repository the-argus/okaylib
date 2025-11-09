#include "test_header.h"
// test header must be first
#include "okay/allocators/reserving_page_allocator.h"

using namespace ok;

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

        SUBCASE("can allocate different sizes around a page")
        {
            reserving_page_allocator_t ally({.pages_reserved = 2});

            size_t page_size = mmap::get_page_size();

            auto res = ally.allocate(alloc::request_t{
                .num_bytes = page_size,
            });

            auto res2 = ally.allocate(alloc::request_t{
                .num_bytes = page_size - 1,
            });

            auto res3 = ally.allocate(alloc::request_t{
                .num_bytes = page_size + 1,
            });

            // cannot allocate over reserved page numbers
            auto res4 = ally.allocate(alloc::request_t{
                .num_bytes = page_size * 2,
            });
            auto res5 = ally.allocate(alloc::request_t{
                .num_bytes = (page_size * 2) + 1,
            });

            REQUIRE(res.is_success());
            REQUIRE(res2.is_success());
            REQUIRE(res3.is_success());
            REQUIRE(res4.is_success());
            REQUIRE(!res5.is_success());
        }
    }
}
