#ifndef __OKAYLIB_C_ALLOCATOR_H__
#define __OKAYLIB_C_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/stdmem.h"
#include <cstring>

namespace ok {

class c_allocator_t : public allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        alloc::feature_flags::can_reclaim | alloc::feature_flags::is_threadsafe;

    c_allocator_t() = default;

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final;

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final;

  private:
    [[nodiscard]] constexpr alloc::result_t<bytes_t>
    realloc_inner(bytes_t memory, size_t new_size,
                  bool zeroed) OKAYLIB_NOEXCEPT;
};

// definitions -----------------------------------------------------------------

[[nodiscard]] inline alloc::result_t<bytes_t>
c_allocator_t::impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT
{
    if (request.num_bytes == 0) [[unlikely]] {
        __ok_usage_error(false,
                         "Attempt to allocate 0 bytes from c_allocator.");
        return alloc::error::unsupported;
    }
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
    assert(!mem || ((uintptr_t)mem % request.alignment) == 0);

    auto out = ok::raw_slice(*mem, nbytes);

    // TODO: disable this if platform guarantees memory is zeroed?
    if (!(request.flags & alloc::flags::leave_nonzeroed)) [[unlikely]] {
        memfill(out, 0);
    }

    return out;
}

inline void c_allocator_t::impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT
{
    ::free(bytes.data());
}

[[nodiscard]] inline alloc::result_t<bytes_t> c_allocator_t::impl_reallocate(
    const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT
{
    using namespace alloc;
    __ok_usage_error(
        options.is_valid(),
        "invalid reallocate request. validation check bypassed. did "
        "you call impl_reallocate() directly?");
    if (options.flags & flags::in_place_orelse_fail) [[unlikely]] {
        __ok_usage_error(
            false,
            "unsupported flag in_place_orelse_fail passed to c allocator");
        return error::unsupported;
    }

    const bool zeroed = !(options.flags & flags::leave_nonzeroed);

    auto res = realloc_inner(options.memory, options.new_size_bytes, zeroed);
    if (!res.okay()) [[unlikely]]
        return res.err();

    return res.release_ref();
}

[[nodiscard]] constexpr alloc::result_t<bytes_t>
c_allocator_t::realloc_inner(bytes_t memory, size_t new_size,
                             bool zeroed) OKAYLIB_NOEXCEPT
{
    using namespace alloc;
    void* mem = ::realloc(memory.data(), new_size);

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
        std::memset(start_ptr + memory.size(), 0, new_size - memory.size());
    }

    return out;
}

[[nodiscard]] inline auto c_allocator_t::impl_reallocate_extended(
    const alloc::reallocate_extended_request_t& options)
    OKAYLIB_NOEXCEPT -> alloc::result_t<alloc::reallocation_extended_t>
{
    using namespace alloc;
    __ok_assert(options.is_valid(),
                "invalid reallocate extended request. validation check "
                "bypassed. did you call impl_reallocate_extended() directly?");
    constexpr auto unsupported =
        flags::in_place_orelse_fail | flags::expand_front;
    if (options.flags & unsupported) [[unlikely]] {
        __ok_assert(false, "unsupported flag passed to c allocator");
        return error::unsupported;
    }

    auto [bytes_offset_back, bytes_offset_front, new_size] =
        options.calculate_new_preferred_size();

    // early out if this looks like a regular realloc
    if (bytes_offset_front == 0) [[likely]] {
        auto res =
            this->realloc_inner(options.memory, new_size,
                                !(options.flags & flags::leave_nonzeroed));
        if (!res.okay()) [[unlikely]]
            return res.err();

        return reallocation_extended_t{.memory = res.release_ref()};
    }

    // shrink_front may be requested. make a new allocation so we dont have to
    // worry about tagging the allocation's start or anything
    uint8_t* newmem = static_cast<uint8_t*>(::malloc(new_size));

    if (!newmem) [[unlikely]]
        return error::oom;

    // memcpy from the old memory and then free the old memory. any shrunk-out
    // memory is lost.
    {
        size_t size = options.memory.size() - bytes_offset_front;
        if (options.flags & flags::shrink_back) {
            size -= bytes_offset_back;
        }

        std::memcpy(newmem, options.memory.data() + bytes_offset_front, size);
    }

    ::free(options.memory.data());

    return reallocation_extended_t{
        .memory = raw_slice(*static_cast<uint8_t*>(newmem), new_size),
        .bytes_offset_front = bytes_offset_front,
    };
}

[[nodiscard]] alloc::feature_flags
c_allocator_t::impl_features() const OKAYLIB_NOEXCEPT
{
    return type_features;
}

inline void c_allocator_t::impl_clear() OKAYLIB_NOEXCEPT
{
    // TODO: make this a warning message
    __ok_assert(false,
                "Potential leak: trying to clear allocator but it does not "
                "support clearing. Check features() before calling clear?");
}

} // namespace ok

#endif
