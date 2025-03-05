#ifndef __OKAYLIB_ALLOCATORS_ARENA_H__
#define __OKAYLIB_ALLOCATORS_ARENA_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/noexcept.h"
#include "okay/opt.h"
#include "okay/slice.h"
#include "okay/stdmem.h"

namespace ok {

class arena_t : public allocator_t
{
  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_only_alloc | alloc::feature_flags::can_clear;

    constexpr explicit arena_t(bytes_t static_buffer) OKAYLIB_NOEXCEPT;
    // owning constructor (if you initialize the arena this way, then it will
    // try to realloc if overfull and destroy() will free the initial buffer
    constexpr explicit arena_t(bytes_t&& initial_buffer,
                               allocator_t& backing_allocator) OKAYLIB_NOEXCEPT;

    constexpr arena_t(arena_t&& other) OKAYLIB_NOEXCEPT
        : m_memory(other.m_memory),
          m_available_memory(other.m_available_memory),
          m_backing(std::exchange(other.m_backing, nullopt))
    {
#ifndef NDEBUG
        other.is_destroyed = true;
#endif
    }

    constexpr arena_t& operator=(arena_t&& other) OKAYLIB_NOEXCEPT
    {
        destroy();
        m_memory = other.m_memory;
        m_available_memory = other.m_available_memory;
        m_backing = std::exchange(other.m_backing, nullopt);
#ifndef NDEBUG
        is_destroyed = false;
        other.is_destroyed = true;
#endif
        return *this;
    }

    arena_t& operator=(const arena_t&) = delete;
    arena_t(const arena_t&) = delete;

    constexpr void destroy() OKAYLIB_NOEXCEPT;

#ifndef NDEBUG
    inline ~arena_t() OKAYLIB_NOEXCEPT_FORCE
    {
        __ok_assert(is_destroyed,
                    "arena allowed to go out of scope without being destroyed");
    }
#endif

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final {}

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

  private:
#ifndef NDEBUG
    bool is_destroyed = false;
#endif
    bytes_t m_memory;
    bytes_t m_available_memory;
    opt<allocator_t&> m_backing;
};

constexpr arena_t::arena_t(bytes_t static_buffer) OKAYLIB_NOEXCEPT
    : m_memory(static_buffer),
      m_available_memory(static_buffer)
{
}

constexpr arena_t::arena_t(bytes_t&& initial_buffer,
                           allocator_t& backing_allocator) OKAYLIB_NOEXCEPT
    : m_memory(initial_buffer),
      m_available_memory(initial_buffer),
      m_backing(backing_allocator)
{
}

constexpr void arena_t::destroy() OKAYLIB_NOEXCEPT
{
#ifndef NDEBUG
    is_destroyed = true;
#endif
    if (m_backing) {
        auto& backing = m_backing.value();
        backing.deallocate(m_memory);
    }
}

[[nodiscard]] inline alloc::result_t<bytes_t>
arena_t::impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT
{
    if (request.num_bytes == 0) [[unlikely]] {
        __ok_usage_error(false, "Attempt to allocate 0 bytes from arena.");
        return alloc::error::unsupported;
    }
    using namespace alloc;
    const bool should_zero = !(request.flags & flags::leave_nonzeroed);
    void* aligned_start_voidptr = m_available_memory.data();
    size_t space_remaining_after_alignment = m_available_memory.size();
    if (!std::align(request.alignment, request.num_bytes, aligned_start_voidptr,
                    space_remaining_after_alignment)) {
        return error::oom;
    }
    uint8_t* const aligned_start = static_cast<uint8_t*>(aligned_start_voidptr);
    const size_t allocated_size = request.num_bytes;

    const size_t amount_moved_to_be_aligned =
        aligned_start - m_available_memory.data();
    __ok_internal_assert(amount_moved_to_be_aligned < request.alignment);
    __ok_internal_assert(request.num_bytes <= space_remaining_after_alignment);

    const size_t start_index =
        (aligned_start + allocated_size) - m_available_memory.data();

    m_available_memory = m_available_memory.subslice({
        .start = start_index,
        .length = m_available_memory.size() - start_index,
    });

    if (should_zero) {
        std::memset(aligned_start, 0, allocated_size);
    }
    return raw_slice(*aligned_start, allocated_size);
}

inline void arena_t::impl_clear() OKAYLIB_NOEXCEPT
{
#ifndef NDEBUG
    ok::memfill(m_memory, 0);
#endif
    m_available_memory = m_memory;
}
} // namespace ok

#endif
