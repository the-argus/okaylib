#ifndef __OKAYLIB_STD_MEMORY_RESOURCE_H__
#define __OKAYLIB_STD_MEMORY_RESOURCE_H__

#include "okay/allocators/allocator.h"

#include <cstring>
#include <memory_resource>

namespace ok {

class std_memory_resource_allocator_t : public allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::clearing | alloc::feature_flags::expand_back |
        alloc::feature_flags::expand_front;

    std_memory_resource_allocator_t() = delete;

    inline constexpr std_memory_resource_allocator_t(
        std::pmr::memory_resource* resource) noexcept;

    [[nodiscard]] inline result_t<bytes_t>
    allocate_bytes(size_t nbytes, size_t alignment = default_align) final;

    [[nodiscard]] inline reallocation_result_t
    reallocate_bytes(const allocator_t::reallocate_options_t&) final;

    inline void clear() noexcept final;

    [[nodiscard]] inline status<error>
    register_destruction_callback(void*, destruction_callback_t) noexcept final;

    [[nodiscard]] inline feature_flags features() const noexcept final;

    inline void deallocate_bytes(void* p, size_t nbytes,
                                 size_t alignment = default_align) final;

  private:
    std::pmr::memory_resource* m_resource;
};

inline constexpr std_memory_resource_allocator_t::
    std_memory_resource_allocator_t(
        std::pmr::memory_resource* resource) noexcept
    : m_resource(resource)
{
    assert(resource);
}

inline auto std_memory_resource_allocator_t::allocate_bytes(
    size_t nbytes, size_t alignment) -> result_t<bytes_t>
{
    assert(alignment > 0);
    assert(nbytes > 0);
    void* const mem = m_resource->allocate(nbytes, alignment);
    if (!mem)
        return error::oom;
    return ok::raw_slice(*static_cast<uint8_t*>(mem), nbytes);
}

inline void std_memory_resource_allocator_t::deallocate_bytes(void* p,
                                                              size_t nbytes,
                                                              size_t alignment)
{
    assert((uintptr_t)p % alignment == 0);
    assert(alignment > 0);
    assert(nbytes > 0);
    assert(p);
    return m_resource->deallocate(p, nbytes, alignment);
}

inline void std_memory_resource_allocator_t::clear() noexcept
{
    // cant guarantee that we can clear, this will just leak if new_delete
    // resource is used for example
    assert(false);
}

inline auto
std_memory_resource_allocator_t::features() const noexcept -> feature_flags
{
    return type_features;
}

inline auto std_memory_resource_allocator_t::reallocate_bytes(
    const allocator_t::reallocate_options_t& options) -> reallocation_result_t
{
    assert(((uintptr_t)options.data % options.alignment) == 0);

    const bool shrinking_back = options.flags & flags::shrink_back;
    const bool shrinking_front = options.flags & flags::shrink_front;
    const bool expanding_front = options.flags & flags::expand_front;
    const bool expanding_back = options.flags & flags::expand_back;

    if ((shrinking_front && expanding_front) ||
        (shrinking_back && expanding_back)) {
        assert(false);
        return error::usage;
    }

    size_t newsize = options.size;
    if (shrinking_back) [[unlikely]] {
        if (options.required_bytes_back >= options.size) [[unlikely]]
            return error::usage;

        newsize -= options.required_bytes_back;
        assert(options.preferred_bytes_back == 0);
    } else if (expanding_back) {
        if (options.preferred_bytes_back < options.required_bytes_back)
            [[unlikely]]
            return error::usage;

        newsize += options.required_bytes_back;
    }

    if (shrinking_front) [[unlikely]] {
        if (shrinking_back &&
            options.required_bytes_front + options.required_bytes_back >=
                options.size) [[unlikely]]
            return error::usage;

        newsize -= options.required_bytes_front;
        assert(options.preferred_bytes_front == 0);
    } else if (expanding_front) {
        if (options.preferred_bytes_front < options.required_bytes_front)
            [[unlikely]]
            return error::usage;

        newsize += options.required_bytes_front;
    }

    assert(newsize != 0);

    void* mem = m_resource->allocate(newsize);
    if (!mem) [[unlikely]]
        return error::oom;

    const bool copying = !(options.flags & flags::keep_old_nocopy);

    // kept = true if not copying
    auto out = reallocation_t{
        ok::raw_slice(*static_cast<uint8_t*>(mem), newsize),
        nullptr,
        !copying,
    };

    if (shrinking_front && copying) {
        std::memcpy(out.memory.data(),
                    (uint8_t*)options.data + options.required_bytes_front,
                    expanding_back ? newsize - options.required_bytes_back
                                   : newsize);
        // original location is lost here, its not copied
    } else if (expanding_front) {
        out.data_original_offset =
            (uint8_t*)out.memory.data() + options.required_bytes_front;
        if (copying) {
            std::memcpy(out.data_original_offset, options.data,
                        expanding_back
                            ? options.size
                            : newsize - options.required_bytes_front);
        }
    } else [[likely]] {
        out.data_original_offset = out.memory.data();
        if (copying) {
            std::memcpy(out.data_original_offset, options.data,
                        std::min(options.size, newsize));
        }
    }

    if (!out.kept)
        m_resource->deallocate(options.data, options.size);

    return out;
}

inline auto std_memory_resource_allocator_t::register_destruction_callback(
    void*, destruction_callback_t) noexcept -> status<error>
{
    assert(false);
    return error::unsupported;
}
} // namespace ok

#endif
