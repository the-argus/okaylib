#ifndef __OKAYLIB_C_ALLOCATOR_H__
#define __OKAYLIB_C_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/stdmem.h"

namespace ok {

class c_allocator_t : public allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::expand_back | alloc::feature_flags::can_reclaim |
        alloc::feature_flags::threadsafe;

    c_allocator_t() = default;

    [[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final;

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_t>
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

[[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
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
    assert(!mem || ((uintptr_t)mem % request.alignment) == 0);

    auto out = ok::raw_slice(*mem, nbytes);

    if (!(request.flags & alloc::flags::leave_nonzeroed)) [[unlikely]] {
        memfill(out, 0);
    }

    return out;
}

inline void c_allocator_t::impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT
{
    ::free(bytes.data());
}

[[nodiscard]] inline alloc::result_t<alloc::reallocation_t>
c_allocator_t::impl_reallocate(const alloc::reallocate_request_t& options)
    OKAYLIB_NOEXCEPT
{
    using namespace alloc;
    if (!options.is_valid()) [[unlikely]] {
        __ok_assert(false,
                    "invalid reallocate_request_t passed to c allocator");
        return error::usage;
    }

    if ((options.flags & flags::keep_old_nocopy) ||
        (options.flags & flags::inplace_or_fail)) [[unlikely]] {
        __ok_assert(false, "unsupported flag passed to c allocator");
        return error::unsupported;
    }

    const bool zeroed = !(options.flags & flags::leave_nonzeroed);

    auto res = realloc_inner(options.memory, options.new_size_bytes, zeroed);
    if (!res.okay()) [[unlikely]]
        return res.err();

    return reallocation_t{.memory = res.release_ref()};
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
    if (!options.is_valid()) [[unlikely]] {
        __ok_assert(
            false,
            "invalid reallocate_extended_request_t passed to c allocator");
        return error::usage;
    }

    if ((options.flags & flags::keep_old_nocopy) ||
        (options.flags & flags::inplace_or_fail)) [[unlikely]] {
        __ok_assert(false, "unsupported flag passed to c allocator");
        return error::unsupported;
    }

    const bool shrinking_back = options.flags & flags::shrink_back;
    const bool shrinking_front = options.flags & flags::shrink_front;
    const bool expanding_front = options.flags & flags::expand_front;
    const bool expanding_back = options.flags & flags::expand_back;
    const bool zeroed = !(options.flags & flags::leave_nonzeroed);

    const size_t amount_changed_back =
        ok::max(options.required_bytes_back, options.preferred_bytes_back);
    const size_t amount_changed_front =
        ok::max(options.required_bytes_front, options.preferred_bytes_front);

    // calculate the new size
    size_t new_size;
    {
        new_size = options.memory.size();
        if (expanding_back)
            new_size += amount_changed_back;
        else if (shrinking_back)
            new_size -= amount_changed_back;

        if (expanding_front)
            new_size += amount_changed_front;
        else if (shrinking_front)
            new_size -= amount_changed_front;
    }

    // early out if this looks like a regular realloc
    if (amount_changed_front == 0) [[likely]] {
        auto res = this->realloc_inner(options.memory, new_size, zeroed);
        if (!res.okay()) [[unlikely]]
            return res.err();

        return reallocation_extended_t{.memory = res.release_ref()};
    }

    // if a change in the front is requested, we can do malloc/free to emulate
    void* newmem = ::malloc(new_size);

    if (!newmem) [[unlikely]]
        return error::oom;

    uint8_t* copy_dest = static_cast<uint8_t*>(newmem);
    uint8_t* copy_src = options.memory.data();
    size_t size = options.memory.size();

    if (shrinking_front) {
        copy_src += amount_changed_front;
        size -= amount_changed_front;
    } else {
        __ok_internal_assert(expanding_front);
        copy_dest += amount_changed_front;
    }

    if (shrinking_back)
        size -= amount_changed_back;

    std::memcpy(copy_dest, copy_src, size);

    ::free(options.memory.data());

    return reallocation_extended_t{
        .memory = raw_slice(*static_cast<uint8_t*>(newmem), new_size)};
}

[[nodiscard]] alloc::feature_flags
c_allocator_t::impl_features() const OKAYLIB_NOEXCEPT
{
    return type_features;
}

inline void c_allocator_t::impl_clear() OKAYLIB_NOEXCEPT
{
    __ok_assert(false,
                "Potential leak: trying to clear allocator but it does not "
                "support clearing. Check features() before calling clear?");
}

} // namespace ok

#endif
