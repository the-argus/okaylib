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
    constexpr explicit arena_t(bytes_t static_buffer) OKAYLIB_NOEXCEPT;
    constexpr explicit arena_t(allocator_t& backing_allocator) OKAYLIB_NOEXCEPT;

    constexpr arena_t(arena_t&& other) OKAYLIB_NOEXCEPT;
    constexpr arena_t& operator=(arena_t&& other) OKAYLIB_NOEXCEPT;

    arena_t& operator=(const arena_t&) = delete;
    arena_t(const arena_t&) = delete;

    constexpr ~arena_t() OKAYLIB_NOEXCEPT_FORCE { destroy(); }

    constexpr void clear() OKAYLIB_NOEXCEPT;

  protected:
    [[nodiscard]] constexpr alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] constexpr void*
    impl_arena_new_scope() OKAYLIB_NOEXCEPT override;
    constexpr void
    impl_arena_restore_scope(void* handle) OKAYLIB_NOEXCEPT override;

    constexpr ok::status<alloc::error> impl_arena_push_destructor(
        destructor_t destructor) OKAYLIB_NOEXCEPT override;

    constexpr void impl_deallocate(void* memory,
                                   size_t size_hint) OKAYLIB_NOEXCEPT final
    {
        // deallocating with an arena is a no-op
        return;
    }

    constexpr alloc::result_t<bytes_t> impl_reallocate(
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
        // NOTE: if destructor_and_object is null, that indicates that this node
        // just marks a change in allocator scope
        destructor_t destructor_and_object;
        opt<destructor_list_node_t&> prev = nullptr;
    };

    constexpr alloc::feature_flags impl_features() const OKAYLIB_NOEXCEPT final
    {
        return alloc::feature_flags::can_restore_scope |
               alloc::feature_flags::keeps_destructor_list;
    }

    enum class destructor_list_clear_mode
    {
        clear_all,
        stop_after_current_scope,
    };

    constexpr void destroy() OKAYLIB_NOEXCEPT;
    constexpr void
    call_all_destructors(destructor_list_clear_mode mode) OKAYLIB_NOEXCEPT;

    bytes_t m_memory;
    size_t m_first_available_byte_index;
    opt<allocator_t&> m_backing;
    opt<destructor_list_node_t&> m_last_pushed_destructor;
};

constexpr arena_t::arena_t(bytes_t static_buffer) OKAYLIB_NOEXCEPT
    : m_memory(static_buffer),
      m_first_available_byte_index(0)
{
}

constexpr arena_t& arena_t::operator=(arena_t&& other) OKAYLIB_NOEXCEPT
{
    if (&other == this) [[unlikely]]
        return *this;
    destroy();
    m_memory = other.m_memory;
    m_first_available_byte_index = other.m_first_available_byte_index;
    m_backing = stdc::exchange(other.m_backing, nullopt);
    return *this;
}

constexpr arena_t::arena_t(arena_t&& other) OKAYLIB_NOEXCEPT
    : m_memory(other.m_memory),
      m_first_available_byte_index(other.m_first_available_byte_index),
      m_backing(stdc::exchange(other.m_backing, nullopt))
{
}

constexpr arena_t::arena_t(allocator_t& backing_allocator) OKAYLIB_NOEXCEPT
    : m_memory(ok::make_null_slice<uint8_t>()),
      m_first_available_byte_index(0),
      m_backing(backing_allocator)
{
}

constexpr void arena_t::destroy() OKAYLIB_NOEXCEPT
{
    call_all_destructors(destructor_list_clear_mode::clear_all);
    if (m_backing && !m_memory.is_empty()) {
        m_backing.ref_unchecked().deallocate(
            m_memory.unchecked_address_of_first_item());
    }
}

constexpr void
arena_t::call_all_destructors(destructor_list_clear_mode mode) OKAYLIB_NOEXCEPT
{
    opt<destructor_list_node_t&> node = m_last_pushed_destructor;
    while (node) {
        destructor_list_node_t& noderef = node.ref_or_panic();

        if (mode == destructor_list_clear_mode::stop_after_current_scope &&
            noderef.destructor_and_object.destructor == nullptr) [[unlikely]] {
            m_last_pushed_destructor = noderef.prev;
            return;
        }

        noderef.destructor_and_object.destructor(
            noderef.destructor_and_object.object);
        node = noderef.prev;
    }
    m_last_pushed_destructor.reset();
}

[[nodiscard]] constexpr alloc::result_t<bytes_t>
arena_t::impl_allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT
{
    using namespace alloc;
    constexpr auto extra_bookkeeping_bytes = 100;
    // handle first-time allocation case
    if (m_memory.is_empty()) [[unlikely]] {
        if (!m_backing) [[unlikely]]
            return error::oom;

        __ok_internal_assert(m_first_available_byte_index == 0);

        auto& backing = m_backing.ref_unchecked();
        const alloc::request_t backing_request{
            .num_bytes = request.num_bytes + extra_bookkeeping_bytes,
            .alignment = request.alignment,
            .leave_nonzeroed = true,
        };

        result_t<bytes_t> result = backing.allocate(backing_request);
        if (ok::is_success(result)) [[likely]] {
            m_memory = result.unwrap();
            m_first_available_byte_index = 0;
        } else [[unlikely]] {
            return result;
        }
    }

    // handle needs reallocation case
    const auto align_or_realloc_inplace = [&]() -> result_t<uint8_t*> {
        const auto get_aligned_start = [this] {
            // NOTE: this might return a pointer off the end of the memory,
            // but if so then get_space_remaining() should return 0
            return m_memory.unchecked_address_of_first_item() +
                   m_first_available_byte_index;
        };
        const auto get_space_remaining = [this] {
            __ok_internal_assert(m_first_available_byte_index <=
                                 m_memory.size());
            return m_memory.size() - m_first_available_byte_index;
        };

        void* aligned_start_voidptr = get_aligned_start();
        size_t space_remaining_after_alignment = get_space_remaining();

        if (!std::align(request.alignment, request.num_bytes,
                        aligned_start_voidptr,
                        space_remaining_after_alignment)) {

            if (!m_backing) [[unlikely]]
                return error::oom;

            auto& backing = m_backing.ref_unchecked();

            constexpr auto growth_factor = 2;

            auto maybe_new_memory = backing.reallocate(reallocate_request_t{
                .memory = m_memory,
                .new_size_bytes = ok::max(m_memory.size() * growth_factor,
                                          m_memory.size() + request.num_bytes +
                                              extra_bookkeeping_bytes),
                .alignment = ok::alloc::default_align,
                .flags = realloc_flags::leave_nonzeroed |
                         realloc_flags::in_place_orelse_fail,
            });

            if (!ok::is_success(maybe_new_memory)) [[unlikely]]
                return maybe_new_memory.status();

            __ok_internal_assert(maybe_new_memory.unwrap().address_of_first() ==
                                 m_memory.address_of_first());
            __ok_internal_assert(aligned_start_voidptr == get_aligned_start());
            __ok_internal_assert(maybe_new_memory.unwrap().size() >
                                 m_memory.size());
            __ok_internal_assert(m_first_available_byte_index <=
                                 m_memory.size());

            m_memory = maybe_new_memory.unwrap();
            space_remaining_after_alignment = get_space_remaining();

            __ok_internal_assert(m_first_available_byte_index <
                                 m_memory.size());

            // re-align
#ifndef NDEBUG
            const bool align_succeeded =
#endif
                std::align(request.alignment, request.num_bytes,
                           aligned_start_voidptr,
                           space_remaining_after_alignment);
            __ok_internal_assert(align_succeeded);
        }

        uint8_t* const aligned_start =
            static_cast<uint8_t*>(aligned_start_voidptr);

        return aligned_start;
    };

    const auto maybe_aligned_start = align_or_realloc_inplace();

    if (!ok::is_success(maybe_aligned_start)) [[unlikely]]
        return maybe_aligned_start.status();

    uint8_t* const aligned_start = maybe_aligned_start.unwrap();

    uint8_t* const new_available_start = aligned_start + request.num_bytes;

    __ok_internal_assert(m_first_available_byte_index < m_memory.size());

    m_first_available_byte_index =
        new_available_start - m_memory.unchecked_address_of_first_item();

    // its okay for m_first_available_byte_index to be *equal* to memory.size()
    // here, that means things are full
    __ok_internal_assert(m_first_available_byte_index <= m_memory.size());

    if (!request.leave_nonzeroed) {
        ::memset(aligned_start, 0, request.num_bytes);
    }
    return raw_slice(*aligned_start, request.num_bytes);
}

[[nodiscard]] constexpr void* arena_t::impl_arena_new_scope() OKAYLIB_NOEXCEPT
{
    // a null destructor indicates a change in scope
    impl_arena_push_destructor(destructor_t{});
    return stdc::bit_cast<void*>(m_first_available_byte_index);
}

constexpr void arena_t::impl_arena_restore_scope(void* handle) OKAYLIB_NOEXCEPT
{
    call_all_destructors(destructor_list_clear_mode::stop_after_current_scope);
    __ok_internal_assert(m_first_available_byte_index < m_memory.size());
    m_first_available_byte_index = stdc::bit_cast<size_t>(handle);
    __ok_internal_assert(m_first_available_byte_index < m_memory.size());
}

constexpr ok::status<alloc::error>
arena_t::impl_arena_push_destructor(destructor_t destructor) OKAYLIB_NOEXCEPT
{
    res noderes = this->make_non_owning<destructor_list_node_t>(destructor);
    if (!ok::is_success(noderes)) [[unlikely]]
        return noderes.status();

    destructor_list_node_t& node = noderes.unwrap();

    node.prev = m_last_pushed_destructor;
    m_last_pushed_destructor = node;
    return alloc::error::success;
}

constexpr void arena_t::clear() OKAYLIB_NOEXCEPT
{
    call_all_destructors(destructor_list_clear_mode::clear_all);
    mark_bytes_freed_if_debugging(m_memory);
    m_first_available_byte_index = 0;
}
} // namespace ok

#endif
