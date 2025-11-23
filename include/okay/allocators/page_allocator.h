#ifndef __OKAYLIB_ALLOCATORS_PAGE_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_PAGE_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/math/rounding.h"
#include "okay/platform/memory_map.h"
#include <cstring>

namespace ok {

/// The page allocator can only allocate and deallocate. Unlike other
/// allocators, it does not keep any bookeeping data to track the actual size of
/// allocations, meaning that freeing a small subslice of the original
/// allocation may cause a memory leak on some platforms.
/// Usually, this is a backing allocator for other allocators.
class page_allocator_t : public allocator_t
{
  public:
    // NOTE: page allocator not threadsafe, uses errno and GetLastError on win
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_reclaim;

    page_allocator_t() = default;

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

        mmap::map_result_t result =
            mmap::alloc_pages(nullptr, total_bytes / page_size);

        if (result.code != 0) [[unlikely]] {
            return alloc::error::oom;
        }

        __ok_internal_assert(result.bytes >= total_bytes);

        if (!(request.leave_nonzeroed)) {
            ::memset(result.data, 0, result.bytes);
        }

        return ok::raw_slice(*static_cast<uint8_t*>(result.data), result.bytes);
    }

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(void* memory) OKAYLIB_NOEXCEPT final
    {
        const size_t page_size = mmap::get_page_size();
        if (page_size == 0) [[unlikely]] {
            __ok_assert(false, "unable to get page size on this platform");
            return;
        }
        // NOTE: just passing in page_size always- I think the kernel internally
        // should keep track of contiguously allocated pages, on
        // windows/linux/mac? maybe not, in that case we need to insert size
        // markers into the page allocations
        const auto code = mmap::memory_unmap(memory, page_size);
        __ok_internal_assert(code == 0);
    }

    // you cannot realloc pages
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final
    {
        return alloc::error::unsupported;
    }

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final
    {
        return alloc::error::unsupported;
    }
};
} // namespace ok

#endif
