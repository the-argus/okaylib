#ifndef __OKAYLIB_ALLOCATORS_RESERVING_reserving_page_allocator_H__
#define __OKAYLIB_ALLOCATORS_RESERVING_reserving_page_allocator_H__

#include "okay/allocators/allocator.h"
#include "okay/math/math.h"
#include "okay/math/rounding.h"
#include "okay/platform/memory_map.h"
#include <cstring>

namespace ok {

/// Similar to page_allocator_t, except it reserves a (configurable, but
/// constant per allocator) number of pages for each allocation. Reallocating
/// will almost always be able to be done in-place. This allocator behaves as
/// though you always passed the in_place_orelse_fail flag.
///
/// It is undefined behavior to free or reallocate memory that is not a pointer
/// to the start of an allocation made with a reserving_page_allocator_t.
///
/// If you try to shrink the allocation without passing the flag
/// realloc_flags::shrink_back, you may get a failure when in fact the
/// allocation has remained valid. Generally just avoid shrinking, this
/// allocator can't support it anyways.
class reserving_page_allocator_t : public allocator_t
{
  public:
    // NOTE: page allocator not threadsafe, uses errno and GetLastError on win
    static constexpr alloc::feature_flags type_features =
        // NOTE: current implementation does not un-commit pages when shrinking
        // on reallocation, although it does free things when deallocating
        alloc::feature_flags::can_reclaim |
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::needs_accurate_sizehint;

    struct options_t
    {
        // four gigabytes on systems with 4K page size
        size_t pages_reserved = 1000000UL;
    };

    reserving_page_allocator_t() = delete;
    explicit reserving_page_allocator_t(const options_t& options) noexcept
        : m_pages_reserved(options.pages_reserved)
    {
    }

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT final
    {
        const auto page_size = mmap::get_page_size();
        if (page_size == 0) [[unlikely]] {
            __ok_assert(false, "unable to get page size on this platform");
            return alloc::error::platform_failure;
        }

        // reservation allocator only supports alignments that are powers of two
        if (two_to_the_power_of(log2_uint(request.alignment)) !=
            request.alignment) [[unlikely]] {
            return alloc::error::unsupported;
        }

        // part of only supporting alignments that are powers of two is the
        // assumption that page size is also a large power of two
        if (page_size % request.alignment != 0) [[unlikely]] {
            return alloc::error::oom;
        }

        const size_t total_bytes =
            runtime_round_up_to_multiple_of(page_size, request.num_bytes);

        const size_t total_pages = total_bytes / page_size;

        // if you try to allocate more than the number of reserved pages, this
        // basically behaves as a page allocator and reallocation will do a
        // syscall and probably fail
        if (total_pages >= m_pages_reserved) {
            mmap::map_result_t result = mmap::alloc_pages(nullptr, total_pages);
            if (result.code != 0) [[unlikely]]
                return alloc::error::oom;
            return ok::raw_slice(*static_cast<uint8_t*>(result.data),
                                 result.bytes);
        }

        mmap::map_result_t reservation_result =
            mmap::reserve_pages(nullptr, m_pages_reserved);

        if (reservation_result.code != 0) [[unlikely]] {
            return alloc::error::oom;
        }

        int64_t code = mmap::commit_pages(reservation_result.data,
                                          total_bytes / page_size);

        if (code != 0) [[unlikely]] {
            mmap::memory_unmap(reservation_result.data,
                               reservation_result.bytes);
            return alloc::error::oom;
        }

        __ok_internal_assert(reservation_result.bytes >= total_bytes);

        if (!(request.leave_nonzeroed)) {
            ::memset(reservation_result.data, 0, total_bytes);
        }

        return ok::raw_slice(*static_cast<uint8_t*>(reservation_result.data),
                             total_bytes);
    }

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(void* memory,
                                size_t size_hint) OKAYLIB_NOEXCEPT final
    {
        size_t page_size = mmap::get_page_size();
        if (page_size == 0) [[unlikely]] {
            __ok_assert(false, "unable to get page size on this platform?");

            // last ditch effort, hopefully we can still free
            page_size = 4096;
        }
        const auto code =
            mmap::memory_unmap(memory, page_size * m_pages_reserved);
        __ok_internal_assert(code == 0);
    }

    [[nodiscard]] inline alloc::result_t<bytes_t> impl_reallocate(
        const alloc::reallocate_request_t& request) OKAYLIB_NOEXCEPT final
    {
        __ok_assert(uintptr_t(&request.memory[0]) % mmap::get_page_size() == 0,
                    "misaligned memory requested for reallocation");

        __ok_usage_error(request.flags & alloc::flags::leave_nonzeroed,
                         "Always pass leave_nonzeroed to reservation allocator "
                         "because it does not zero memory when reallocating.");

        const size_t page_size = mmap::get_page_size();

        if (page_size == 0) [[unlikely]] {
            __ok_assert(false, "unable to get page size on this platform?");
            return alloc::error::platform_failure;
        }

        // if we are shrinking the allocation just give a subslice
        if (request.preferred_size_bytes == 0 &&
            request.new_size_bytes < request.memory.size_bytes())
            return request.memory.subslice(
                {.start = 0, .length = request.new_size_bytes});

        const size_t actual_size_bytes = request.calculate_preferred_size();

        const size_t num_bytes =
            runtime_round_up_to_multiple_of(page_size, actual_size_bytes);
        const size_t num_pages = num_bytes / page_size;

        // it is undefined behavior to try to commit memory which was not
        // reserved. So if someone tries to commit memory bigger than what
        // we can guarantee was reserved, we just fail.
        if (num_pages > m_pages_reserved) [[unlikely]]
            return alloc::error::oom;

        int64_t code = mmap::commit_pages(
            request.memory.unchecked_address_of_first_item(), num_pages);
        if (code != 0) [[unlikely]] {
            return alloc::error::oom;
        }

        return raw_slice(*request.memory.unchecked_address_of_first_item(),
                         num_bytes);
    }

  private:
    size_t m_pages_reserved;
};
} // namespace ok

#endif
