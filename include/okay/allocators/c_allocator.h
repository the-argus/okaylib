#ifndef __OKAYLIB_C_ALLOCATOR_H__
#define __OKAYLIB_C_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/stdmem.h"
#include <cstring>

namespace ok {

class c_allocator_t : public ok::allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_reclaim;

    c_allocator_t() = default;

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final;

    inline void impl_deallocate(void* memory,
                                size_t size_hint) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final;

  private:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    realloc_inner(bytes_t memory, size_t new_size,
                  bool zeroed) OKAYLIB_NOEXCEPT;
};

// definitions -----------------------------------------------------------------

[[nodiscard]] inline alloc::result_t<bytes_t>
c_allocator_t::impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT
{
    // NOTE: alignment over 16 is not possible on most platforms, I don't think?
    // TODO: figure out what platforms this is a problem for, maybe alloc more
    // and then align when alignment is big
    assert(request.alignment <= 16);
    if (request.alignment > 16) [[unlikely]]
        return alloc::error::unsupported;
    const auto nbytes =
#ifndef NDEBUG
        // add one just in case you are mistakenly relying on the returned bytes
        // being the same as requested
        request.num_bytes + 1;
#else
        request.num_bytes;
#endif

    uint8_t* const mem = static_cast<uint8_t*>(::malloc(nbytes));

    if (!mem) [[unlikely]]
        return alloc::error::oom;

    assert(!mem || ((uintptr_t)mem % request.alignment) == 0);

    auto out = ok::raw_slice(*mem, nbytes);

    // TODO: disable this if platform guarantees memory is zeroed?
    if (!(request.leave_nonzeroed)) [[unlikely]] {
        ok::memfill(out, 0);
    }

    return out;
}

inline void c_allocator_t::impl_deallocate(void* memory, size_t /* size_hint */)
    OKAYLIB_NOEXCEPT
{
    ::free(memory);
}

[[nodiscard]] inline alloc::result_t<bytes_t> c_allocator_t::impl_reallocate(
    const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT
{
    using namespace alloc;

    const bool zeroed = !(options.flags & realloc_flags::leave_nonzeroed);

    auto res = realloc_inner(options.memory, options.calculate_preferred_size(),
                             zeroed);
    if (!res.is_success()) [[unlikely]]
        return res.status();

    return res.unwrap();
}

[[nodiscard]] inline alloc::result_t<bytes_t>
c_allocator_t::realloc_inner(bytes_t memory, size_t new_size,
                             bool zeroed) OKAYLIB_NOEXCEPT
{
    if (memory.size() == 0) [[unlikely]] {
        __ok_assert(false, "Attempt to realloc a slice of zero bytes.");
        return alloc::error::unsupported;
    }
    using namespace alloc;
    void* mem = ::realloc(memory.unchecked_address_of_first_item(), new_size);

    if (!mem) [[unlikely]]
        return error::oom;

    uint8_t* start_ptr = static_cast<uint8_t*>(mem);
    uint8_t& start = *start_ptr;

    const auto out = raw_slice(start, new_size);

    const bool expanding = new_size > memory.size();

    // usually realloc is expanding, optimize for that (likely marker)
    if (expanding && zeroed) [[likely]] {
        // TODO: have way to turn this off based on platform guarantees
        // for zeroed memory?
        ::memset(start_ptr + memory.size(), 0, new_size - memory.size());
    }

    return out;
}

[[nodiscard]] alloc::feature_flags
c_allocator_t::impl_features() const OKAYLIB_NOEXCEPT
{
    return type_features;
}
} // namespace ok

#endif
