#ifndef __OKAYLIB_ALLOCATORS_RESERVING_reserving_page_allocator_H__
#define __OKAYLIB_ALLOCATORS_RESERVING_reserving_page_allocator_H__

#include "okay/allocators/allocator.h"
#include "okay/math/rounding.h"
#include "okay/platform/memory_map.h"
#include <cstring>

namespace ok {

/// Similar to page_allocator_t, except it reserves a (configurable, but
/// constant) number of pages for each allocation. Reallocating will almost
/// always be able to be done in-place. This allocator behaves as though you
/// always passed the in_place_orelse_fail flag.
/// Freeing a subslice of the original allocation with this allocator is
/// defined behavior, unlike page_allocator_t.
class reserving_page_allocator_t : public nonthreadsafe_allocator_t
{
  public:
    // NOTE: page allocator not threadsafe, uses errno and GetLastError on win
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        // NOTE: although this says it can reclaim, the current implementation
        // doesn't actually return pages back to the OS because it doesn't do
        // bookkeeping so it can't know how many pages are currently allocated
        alloc::feature_flags::can_reclaim |
        alloc::feature_flags::can_predictably_realloc_in_place;

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
        if (request.alignment > page_size) [[unlikely]] {
            return alloc::error::unsupported;
        }

        const size_t total_bytes =
            runtime_round_up_to_multiple_of(page_size, request.num_bytes);

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
            std::memset(reservation_result.data, 0, total_bytes);
        }

        return ok::raw_slice(*static_cast<uint8_t*>(reservation_result.data),
                             total_bytes);
    }

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(void* memory) OKAYLIB_NOEXCEPT final
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

        if (request.flags & alloc::flags::shrink_back) [[unlikely]] {
            // TODO: probably try to reclaim memory here, however it requires
            // inserting bookkeeping data that says how many pages were
            // initially allocated
            return request.memory.subslice(
                {.start = 0, .length = request.new_size_bytes});
        }

        const size_t actual_size_bytes =
            ok::max(request.preferred_size_bytes, request.new_size_bytes);

        const size_t num_bytes =
            runtime_round_up_to_multiple_of(page_size, actual_size_bytes);
        const size_t num_pages = num_bytes / page_size;

        int64_t code = mmap::commit_pages(
            request.memory.unchecked_address_of_first_item(), num_pages);
        if (code != 0) [[unlikely]] {
            return alloc::error::oom;
        }

        return raw_slice(*request.memory.unchecked_address_of_first_item(),
                         num_bytes);
    }

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final
    {
        return alloc::error::unsupported;
    }

  private:
    size_t m_pages_reserved;
};
} // namespace ok

#endif
