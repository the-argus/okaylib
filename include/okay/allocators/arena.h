#ifndef __OKAYLIB_ALLOCATORS_ARENA_H__
#define __OKAYLIB_ALLOCATORS_ARENA_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/noexcept.h"
#include "okay/opt.h"
#include "okay/slice.h"
#include "okay/stdmem.h"

namespace ok {

template <allocator_c allocator_impl_t = ok::allocator_t>
class arena_t : public ok::allocator_t
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

    [[nodiscard]] inline void* impl_arena_new_scope() OKAYLIB_NOEXCEPT override;
    inline void
    impl_arena_restore_scope(void* handle) OKAYLIB_NOEXCEPT override;

    ok::status<alloc::error> impl_arena_push_destructor(destructor_t destructor)
        OKAYLIB_NOEXCEPT override;

    void impl_deallocate(void* memory, size_t size_hint) OKAYLIB_NOEXCEPT final
    {
        // deallocating with an arena is a no-op
        return;
    }

    alloc::result_t<bytes_t> impl_reallocate(
        const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT final
    {
        // just allocate a new block with the new size and do a copy
        res allocation = this->allocate(alloc::request_t{
            .num_bytes = options.preferred_size_bytes,
            .alignment = options.alignment,
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

  private:
    struct destructor_list_node_t
    {
        destructor_t destructor_and_object;
        opt<destructor_list_node_t&> prev = nullptr;
    };

    alloc::feature_flags impl_features() const OKAYLIB_NOEXCEPT final
    {
        return alloc::feature_flags::can_restore_scope |
               alloc::feature_flags::keeps_destructor_list;
    }

    inline void destroy() OKAYLIB_NOEXCEPT;
    inline void call_all_destructors() OKAYLIB_NOEXCEPT;

    bytes_t m_memory;
    bytes_t m_available_memory;
    opt<allocator_impl_t&> m_backing;
    opt<destructor_list_node_t&> m_last_pushed_destructor;
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
    call_all_destructors();
    if (m_backing)
        m_backing.ref_unchecked().deallocate(
            m_memory.unchecked_address_of_first_item());
}

template <allocator_c allocator_impl_t>
inline void arena_t<allocator_impl_t>::call_all_destructors() OKAYLIB_NOEXCEPT
{
    opt<destructor_list_node_t&> node = m_last_pushed_destructor;
    while (node) {
        destructor_list_node_t& noderef = node.ref_or_panic();
        noderef.destructor_and_object.destructor(
            noderef.destructor_and_object.object);
        node = noderef.prev;
    }
    m_last_pushed_destructor.reset();
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
arena_t<allocator_impl_t>::impl_arena_new_scope() OKAYLIB_NOEXCEPT
{
    ok::res result = this->make_non_owning<bytes_t>(m_available_memory);
    if (result.is_success()) [[likely]]
        return ok::addressof(result.unwrap_unchecked());
    else
        return nullptr;
}

template <allocator_c allocator_impl_t>
inline void arena_t<allocator_impl_t>::impl_arena_restore_scope(void* handle)
    OKAYLIB_NOEXCEPT
{
    if (!handle) [[unlikely]]
        return;

    m_available_memory = *static_cast<bytes_t*>(handle);
}

template <allocator_c allocator_impl_t>
ok::status<alloc::error> arena_t<allocator_impl_t>::impl_arena_push_destructor(
    destructor_t destructor) OKAYLIB_NOEXCEPT
{
    res noderes = this->make_non_owning<destructor_list_node_t>(destructor);
    if (!ok::is_success(noderes)) [[unlikely]]
        return noderes.status();

    destructor_list_node_t& node = noderes.unwrap();

    node.prev = m_last_pushed_destructor;
    m_last_pushed_destructor = node;
    return alloc::error::success;
}

template <allocator_c allocator_impl_t>
void arena_t<allocator_impl_t>::clear() OKAYLIB_NOEXCEPT
{
    call_all_destructors();
#ifndef NDEBUG
    ok::memfill(m_memory, 0);
#endif
    m_available_memory = m_memory;
}
} // namespace ok

#endif
