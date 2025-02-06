#ifndef __OKAYLIB_C_ALLOCATOR_H__
#define __OKAYLIB_C_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/stdmem.h"

namespace ok {

class c_allocator_t : public allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::free_and_realloc |
        alloc::feature_flags::threadsafe;

    c_allocator_t() = default;

    [[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
    allocate_bytes(const alloc::request_t&) noexcept final;

    inline void clear() noexcept final;

    [[nodiscard]] inline alloc::feature_flags features() const noexcept final;

    inline void deallocate_bytes(bytes_t) noexcept final;

    [[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
    reallocate_bytes(const allocator_t::reallocate_options_t&) noexcept final;

    [[nodiscard]] virtual alloc::result_t<reallocation_extended_t>
    reallocate_bytes_extended(
        const reallocation_options_extended_t& options) noexcept final;
};

// definitions -----------------------------------------------------------------

[[nodiscard]] inline alloc::result_t<maybe_defined_memory_t>
c_allocator_t::allocate_bytes(const alloc::request_t& request) noexcept
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

inline void c_allocator_t::deallocate_bytes(bytes_t bytes) noexcept
{
    ::free(bytes.data());
}

inline auto c_allocator_t::reallocate_bytes_extended(
    const allocator_t::reallocation_options_extended_t& options) noexcept
    -> alloc::result_t<allocator_t::reallocation_extended_t>
{
    using namespace alloc;
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

    const size_t new_size = options.memory.size() + options.required_bytes_back;

    // if we arent doing any weird business, we can use run of the mill realloc
    if (!shrinking_back && !shrinking_front && !expanding_front) [[likely]] {
        void* mem = ::realloc(options.memory.data(), new_size);

        if (!mem) [[unlikely]]
            return error::oom;

        uint8_t& start = *static_cast<uint8_t*>(mem);

        return reallocation_extended_t{
            // .expanded_memory = ok::raw_slice(start, new_size),
            // .original_subslice = ok::raw_slice(start, options.memory.size()),
        };
    }

    // validate inputs- you cant shrink by more than the size of the original
    // allocation
    if (shrinking_front) [[unlikely]] {
        if (options.required_bytes_front >= options.memory.size()) [[unlikely]]
            return error::usage;
        if (shrinking_back &&
            options.required_bytes_front + options.required_bytes_back >=
                options.memory.size()) [[unlikely]]
            return error::usage;
        // preferred bytes doesnt make sense when shrinking, setting this is
        // a mistake
        assert(options.preferred_bytes_front == 0);
    }

    if (shrinking_back) [[unlikely]] {
        if (options.required_bytes_back >= options.memory.size()) [[unlikely]]
            return error::usage;
        // preferred bytes doesnt make sense when shrinking, setting this is
        // a mistake
        assert(options.preferred_bytes_back == 0);
    }

    if (expanding_front && options.preferred_bytes_front <
                               options.required_bytes_front) [[unlikely]]
        return error::usage;

    size_t newsize = options.memory.size();
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
        std::memcpy(
            mem, (uint8_t*)options.memory.data() + options.required_bytes_front,
            expanding_back ? newsize - options.required_bytes_back : newsize);
        // original location is lost here, its not copied
    } else if (expanding_front) {
        original = (uint8_t*)mem + options.required_bytes_front;
        std::memcpy(original, options.memory.data(),
                    expanding_back ? options.memory.size()
                                   : newsize - options.required_bytes_front);
    } else [[likely]] {
        original = mem;
        std::memcpy(original, options.memory.data(),
                    std::min(options.memory.size(), newsize));
    }

    ::free(options.memory.data());

    return reallocation_result_t{
        ok::raw_slice(*static_cast<uint8_t*>(mem), newsize),
        original,
        false,
    };
}

alloc::feature_flags c_allocator_t::features() const noexcept
{
    return type_features;
}

inline void c_allocator_t::clear() noexcept
{
    // c allocator cannot clear all allocations
    assert(false);
}

} // namespace ok

#endif
