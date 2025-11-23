#ifndef __OKAYLIB_ALLOCATORS_ARENA_H__
#define __OKAYLIB_ALLOCATORS_ARENA_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/noexcept.h"
#include "okay/opt.h"
#include "okay/slice.h"
#include "okay/stdmem.h"

namespace ok {

// TODO: use atomics and conditionally allow arena to implement threadsafe
// allocate_interface_t if its allocator_impl_t also does
template <allocator_c allocator_impl_t = ok::allocator_t>
class arena_t : public ok::memory_resource_t,
                public ok::arena_allocator_restore_scope_interface_t
{
  public:
    inline explicit arena_t(bytes_t static_buffer) OKAYLIB_NOEXCEPT;
    // owning constructor (if you initialize the arena this way, then destroy()
    // will free the initial buffer)
    inline explicit arena_t(bytes_t initial_buffer,
                            allocator_impl_t& backing_allocator)
        OKAYLIB_NOEXCEPT;

    inline arena_t(arena_t&& other) OKAYLIB_NOEXCEPT;
    inline arena_t& operator=(arena_t&& other) OKAYLIB_NOEXCEPT;

    arena_t& operator=(const arena_t&) = delete;
    arena_t(const arena_t&) = delete;

    ~arena_t() OKAYLIB_NOEXCEPT_FORCE { destroy(); }

    inline void clear() OKAYLIB_NOEXCEPT;

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] virtual inline void* new_scope() OKAYLIB_NOEXCEPT;
    virtual inline void restore_scope(void* handle) OKAYLIB_NOEXCEPT;

  private:
    inline void destroy() OKAYLIB_NOEXCEPT;

    bytes_t m_memory;
    bytes_t m_available_memory;
    opt<allocator_impl_t&> m_backing;
};

template <typename arena_impl_t>
class arena_compat_wrapper_t : public ok::allocator_t
{
  public:
    arena_compat_wrapper_t() = delete;
    arena_compat_wrapper_t(const arena_compat_wrapper_t&) = delete;
    arena_compat_wrapper_t& operator=(const arena_compat_wrapper_t&) = delete;
    arena_compat_wrapper_t(arena_compat_wrapper_t&&) = delete;
    arena_compat_wrapper_t& operator=(arena_compat_wrapper_t&&) = delete;

    explicit arena_compat_wrapper_t(arena_impl_t& arena)
        : m_arena(ok::addressof(arena))
    {
    }

  protected:
    alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT final
    {
        return m_arena->allocate(request);
    }

    alloc::feature_flags impl_features() const OKAYLIB_NOEXCEPT final
    {
        // doesnt implement any nice features
        return {};
    }

    void impl_deallocate(void* memory) OKAYLIB_NOEXCEPT final
    {
        // deallocating with an arena is a no-op
        return;
    }

    alloc::result_t<bytes_t> impl_reallocate(
        const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT final
    {
        // just allocate a new block with the new size and do a copy
        res allocation = m_arena->allocate(alloc::request_t{
            .num_bytes = options.preferred_size_bytes,
            .alignment = options.min_alignment(),
            .leave_nonzeroed = true,
        });

        if (!allocation.is_success()) [[unlikely]]
            return allocation;

        bytes_t newmem = allocation.unwrap();

        // ignore result of memcopy
        auto&& _ = ok_memcopy(.to = newmem, .from = options.memory);

        // freeing the old allocation is not possible with arena
        return newmem;
    }

    alloc::result_t<alloc::reallocation_extended_t> impl_reallocate_extended(
        const alloc::reallocate_extended_request_t& options)
        OKAYLIB_NOEXCEPT final
    {
        // arena cannot do a reallocate extended request
        // TODO: could sort of implement a compat for this
        return alloc::error::unsupported;
    }

  private:
    arena_impl_t* m_arena;
};

template <allocator_c allocator_impl_t>
inline arena_t<allocator_impl_t>::arena_t(bytes_t static_buffer)
    OKAYLIB_NOEXCEPT : m_memory(static_buffer),
                       m_available_memory(static_buffer)
{
}

template <allocator_c allocator_impl_t>
inline arena_t<allocator_impl_t>&
arena_t<allocator_impl_t>::operator=(arena_t&& other) OKAYLIB_NOEXCEPT
{
    if (&other == this) [[unlikely]]
        return *this;
    destroy();
    m_memory = other.m_memory;
    m_available_memory = other.m_available_memory;
    m_backing = stdc::exchange(other.m_backing, nullopt);
    return *this;
}

template <allocator_c allocator_impl_t>
inline arena_t<allocator_impl_t>::arena_t(arena_t&& other) OKAYLIB_NOEXCEPT
    : m_memory(other.m_memory),
      m_available_memory(other.m_available_memory),
      m_backing(stdc::exchange(other.m_backing, nullopt))
{
}

template <allocator_c allocator_impl_t>
inline arena_t<allocator_impl_t>::arena_t(bytes_t initial_buffer,
                                          allocator_impl_t& backing_allocator)
    OKAYLIB_NOEXCEPT : m_memory(initial_buffer),
                       m_available_memory(initial_buffer),
                       m_backing(backing_allocator)
{
}

template <allocator_c allocator_impl_t>
inline void arena_t<allocator_impl_t>::destroy() OKAYLIB_NOEXCEPT
{
    if (m_backing)
        m_backing.ref_unchecked().deallocate(
            m_memory.unchecked_address_of_first_item());
}

template <allocator_c allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
arena_t<allocator_impl_t>::impl_allocate(const alloc::request_t& request)
    OKAYLIB_NOEXCEPT
{
    using namespace alloc;
    void* aligned_start_voidptr =
        m_available_memory.unchecked_address_of_first_item();
    size_t space_remaining_after_alignment = m_available_memory.size();
    if (!std::align(request.alignment, request.num_bytes, aligned_start_voidptr,
                    space_remaining_after_alignment)) {
        return error::oom;
    }
    uint8_t* const aligned_start = static_cast<uint8_t*>(aligned_start_voidptr);

    const size_t amount_moved_to_be_aligned =
        aligned_start - m_available_memory.unchecked_address_of_first_item();
    __ok_internal_assert(amount_moved_to_be_aligned < request.alignment);
    __ok_internal_assert(request.num_bytes <= space_remaining_after_alignment);

    const size_t start_index =
        (aligned_start + request.num_bytes - 1) -
        m_available_memory.unchecked_address_of_first_item();

    m_available_memory = m_available_memory.subslice({
        .start = start_index,
        .length = m_available_memory.size() - start_index,
    });

    if (!request.leave_nonzeroed) {
        ::memset(aligned_start, 0, request.num_bytes);
    }
    return raw_slice(*aligned_start, request.num_bytes);
}

template <allocator_c allocator_impl_t>
[[nodiscard]] inline void*
arena_t<allocator_impl_t>::new_scope() OKAYLIB_NOEXCEPT
{
    ok::res result = this->make_non_owning<bytes_t>(m_available_memory);
    if (result.is_success()) [[likely]]
        return ok::addressof(result.unwrap_unchecked());
    else
        return nullptr;
}

template <allocator_c allocator_impl_t>
inline void
arena_t<allocator_impl_t>::restore_scope(void* handle) OKAYLIB_NOEXCEPT
{
    if (!handle) [[unlikely]]
        return;

    m_available_memory = *static_cast<bytes_t*>(handle);
}

template <allocator_c allocator_impl_t>
void arena_t<allocator_impl_t>::clear() OKAYLIB_NOEXCEPT
{
#ifndef NDEBUG
    ok::memfill(m_memory, 0);
#endif
    m_available_memory = m_memory;
}
} // namespace ok

#endif
