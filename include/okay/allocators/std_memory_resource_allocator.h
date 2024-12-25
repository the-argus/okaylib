#ifndef __OKAYLIB_STD_MEMORY_RESOURCE_H__
#define __OKAYLIB_STD_MEMORY_RESOURCE_H__

#include "okay/allocators/allocator_interface.h"

#include <memory_resource>

namespace ok {

class std_memory_resource_allocator_t : public allocator_interface_t
{
  public:
    static constexpr feature_flags type_features =
        feature_flags::free_and_realloc | feature_flags::nocopy |
        feature_flags::keep_old | feature_flags::nothrow |
        feature_flags::shrinking;

    std_memory_resource_allocator_t() = delete;

    inline constexpr std_memory_resource_allocator_t(
        std::pmr::memory_resource* resource) noexcept;

    [[nodiscard]] inline allocation_result
    allocate_bytes(size_t nbytes, size_t alignment = default_align) final;

    [[nodiscard]] inline reallocation_result
    reallocate_bytes(const allocator_interface_t::reallocate_options&) final;

    inline void clear() noexcept final;

    [[nodiscard]] inline bool
    register_destruction_callback(void*, destruction_callback) noexcept final;

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
    size_t nbytes, size_t alignment) -> allocation_result
{
    assert(alignment > 0);
    assert(nbytes > 0);
    void* const mem = m_resource->allocate(nbytes, alignment);
    if (!mem)
        return {};
    return {mem, nbytes};
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
    const allocator_interface_t::reallocate_options& options)
    -> reallocation_result
{
    assert(((uintptr_t)options.data % options.alignment) == 0);

    const bool shrinking_back = options.flags & flags::shrink_back;
    const bool shrinking_front = options.flags & flags::shrink_front;
    const bool expanding_front = options.flags & flags::expand_front;
    const bool expanding_back = options.flags & flags::expand_back;

    if ((shrinking_front && expanding_front) ||
        (shrinking_back && expanding_back)) {
        assert(false);
        return {};
    }

    size_t newsize = options.size;
    if (shrinking_back) [[unlikely]] {
        if (options.required_bytes_back >= options.size) [[unlikely]]
            return {};

        newsize -= options.required_bytes_back;
        assert(options.preferred_bytes_back == 0);
    } else if (expanding_back) {
        if (options.preferred_bytes_back < options.required_bytes_back)
            [[unlikely]]
            return {};

        newsize += options.required_bytes_back;
    }

    if (shrinking_front) [[unlikely]] {
        if (shrinking_back &&
            options.required_bytes_front + options.required_bytes_back >=
                options.size) [[unlikely]]
            return {};

        newsize -= options.required_bytes_front;
        assert(options.preferred_bytes_front == 0);
    } else if (expanding_front) {
        if (options.preferred_bytes_front < options.required_bytes_front)
            [[unlikely]]
            return {};

        newsize += options.required_bytes_front;
    }

    assert(newsize != 0);

    void* mem = m_resource->allocate(newsize);
    if (!mem) [[unlikely]]
        return {};

    const bool copying = !(options.flags & flags::keep_old_nocopy);

    // kept = true if not copying
    auto out = reallocation_result(mem, newsize, nullptr, !copying);

    if (shrinking_front && copying) {
        std::memcpy(
            out.data(), (uint8_t*)options.data + options.required_bytes_front,
            expanding_back ? newsize - options.required_bytes_back : newsize);
        // original location is lost here, its not copied
    } else if (expanding_front) {
        set_reallocation_result_data_original_offset(
            out, (uint8_t*)out.data() + options.required_bytes_front);
        if (copying) {
            std::memcpy(out.data_original_offset(), options.data,
                        expanding_back
                            ? options.size
                            : newsize - options.required_bytes_front);
        }
    } else [[likely]] {
        set_reallocation_result_data_original_offset(out, out.data());
        if (copying) {
            std::memcpy(out.data_original_offset(), options.data,
                        std::min(options.size, newsize));
        }
    }

    if (!out.kept())
        m_resource->deallocate(options.data, options.size);

    return out;
}

inline bool std_memory_resource_allocator_t::register_destruction_callback(
    void*, destruction_callback) noexcept
{
    assert(false);
    return false;
}
} // namespace ok

#endif
