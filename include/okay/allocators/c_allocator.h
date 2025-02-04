#ifndef __OKAYLIB_C_ALLOCATOR_H__
#define __OKAYLIB_C_ALLOCATOR_H__

#include "okay/allocators/allocator_interface.h"

namespace ok {

class c_allocator_t : public allocator_interface_t
{
  public:
    static constexpr feature_flags type_features =
        feature_flags::free_and_realloc | feature_flags::threadsafe;

    c_allocator_t() = default;

    [[nodiscard]] inline result_t<bytes_t>
    allocate_bytes(size_t nbytes,
                   size_t alignment = default_align) noexcept final;

    [[nodiscard]] inline reallocation_result_t reallocate_bytes(
        const allocator_interface_t::reallocate_options_t&) noexcept final;

    [[nodiscard]] inline feature_flags features() const noexcept final;

    [[nodiscard]] inline status_t<error>
    register_destruction_callback(void*, destruction_callback_t) noexcept final;

    inline void clear() noexcept final;

    inline void
    deallocate_bytes(void* p, size_t nbytes,
                     size_t alignment = default_align) noexcept final;
};

// definitions -----------------------------------------------------------------

inline auto
c_allocator_t::allocate_bytes(size_t nbytes,
                              size_t alignment) noexcept -> result_t<bytes_t>
{
    // NOTE: alignment over 16 is not possible on most platforms, I don't think?
    // TODO: figure out what platforms this is a problem for, maybe alloc more
    // and then align when alignment is big
    assert(alignment <= 16);
    if (alignment > 16) [[unlikely]]
        return error::unsupported;
    uint8_t* const mem = static_cast<uint8_t*>(::malloc(nbytes));
    assert(!mem || ((uintptr_t)mem % alignment) == 0);
    return ok::raw_slice(*mem, nbytes);
}

inline void c_allocator_t::deallocate_bytes(void* p, size_t nbytes,
                                            size_t alignment) noexcept
{
    assert((uintptr_t)p % alignment == 0);
    ::free(p);
}

inline auto c_allocator_t::reallocate_bytes(
    const allocator_interface_t::reallocate_options_t& options) noexcept
    -> reallocation_result_t
{
    assert(((uintptr_t)options.data % options.alignment) == 0);

    if (options.flags & flags::keep_old_nocopy) [[unlikely]] {
        assert(false);
        return error::unsupported;
    }

    const bool shrinking_back = options.flags & flags::shrink_back;
    const bool shrinking_front = options.flags & flags::shrink_front;
    const bool expanding_front = options.flags & flags::expand_front;
    const bool expanding_back = options.flags & flags::expand_back;

    if ((shrinking_front && expanding_front) ||
        (shrinking_back && expanding_back)) {
        assert(false);
        return error::usage;
    }

    // validate expanding_back first- it can happen in early return
    if (expanding_back &&
        options.preferred_bytes_back < options.required_bytes_back) [[unlikely]]
        return error::usage;

    // if we arent doing any weird business, we can use run of the mill realloc
    if (!shrinking_back && !shrinking_front && !expanding_front) [[likely]] {
        void* mem =
            ::realloc(options.data, options.size + options.required_bytes_back);

        if (!mem) [[unlikely]]
            return error::oom;

        return reallocation_result_t{
            ok::raw_slice(*static_cast<uint8_t*>(mem),
                          options.size + options.required_bytes_back),
            mem,
            false,
        };
    }

    // validate inputs- you cant shrink by more than the size of the original
    // allocation
    if (shrinking_front) [[unlikely]] {
        if (options.required_bytes_front >= options.size) [[unlikely]]
            return error::usage;
        if (shrinking_back &&
            options.required_bytes_front + options.required_bytes_back >=
                options.size) [[unlikely]]
            return error::usage;
        // preferred bytes doesnt make sense when shrinking, setting this is
        // a mistake
        assert(options.preferred_bytes_front == 0);
    }

    if (shrinking_back) [[unlikely]] {
        if (options.required_bytes_back >= options.size) [[unlikely]]
            return error::usage;
        // preferred bytes doesnt make sense when shrinking, setting this is
        // a mistake
        assert(options.preferred_bytes_back == 0);
    }

    if (expanding_front && options.preferred_bytes_front <
                               options.required_bytes_front) [[unlikely]]
        return error::usage;

    size_t newsize = options.size;
    if (expanding_back)
        newsize += options.required_bytes_back;
    else if (shrinking_back)
        newsize -= options.required_bytes_back;

    if (expanding_front)
        newsize += options.required_bytes_front;
    else if (shrinking_front)
        newsize -= options.required_bytes_front;

    // assert for implementor. this should never fire if implementation is good
    assert(newsize != 0);

    void* const mem = ::malloc(newsize);
    if (!mem) [[unlikely]]
        return error::oom;

    void* original = nullptr;
    if (shrinking_front) {
        std::memcpy(mem, (uint8_t*)options.data + options.required_bytes_front,
                    expanding_back ? newsize - options.required_bytes_back
                                   : newsize);
        // original location is lost here, its not copied
    } else if (expanding_front) {
        original = (uint8_t*)mem + options.required_bytes_front;
        std::memcpy(original, options.data,
                    expanding_back ? options.size
                                   : newsize - options.required_bytes_front);
    } else [[likely]] {
        original = mem;
        std::memcpy(original, options.data, std::min(options.size, newsize));
    }

    ::free(options.data);

    return reallocation_result_t{
        ok::raw_slice(*static_cast<uint8_t*>(mem), newsize),
        original,
        false,
    };
}

auto c_allocator_t::features() const noexcept -> feature_flags
{
    // TODO: is there a way to check if malloc is guaranteed threadsafe on this
    // platform? is that standardized?
    return type_features;
}

inline void c_allocator_t::clear() noexcept
{
    // c allocator cannot clear all allocations
    assert(false);
}

inline auto c_allocator_t::register_destruction_callback(
    void*, destruction_callback_t) noexcept -> status_t<error>
{
    // basic c allocator does not provide destruction callback registration so
    // it can be zero-sized when used without vtable
    // TODO: check codegen for this, maybe destruction callback list pointer is
    // fine
    assert(false);
    return error::unsupported;
}
} // namespace ok

#endif
