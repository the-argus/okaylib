#ifndef __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"
#include "okay/math/rounding.h"

namespace ok {

namespace block_allocator {
struct fixed_buffer_options_t
{
    bytes_t fixed_buffer;
    size_t num_bytes_per_block;
    size_t minimum_alignment;
};
struct alloc_initial_buf_options_t
{
    size_t num_initial_spots;
    size_t num_bytes_per_block;
    size_t minimum_alignment;
};
namespace detail {
struct alloc_initial_buf_t;

template <typename free_block_t>
constexpr free_block_t*
free_everything_in_block_allocator_buffer(bytes_t memory, size_t blocksize,
                                          free_block_t* initial_iter = nullptr)
{
    free_block_t* free_list_iter = initial_iter;
    for (int64_t i = (memory.size() / blocksize) - 1; i >= 0; --i) {
        auto* ptr =
            reinterpret_cast<free_block_t*>(memory.data() + (i * blocksize));
        __ok_internal_assert(uintptr_t(ptr) % alignof(free_block_t) == 0);
        *ptr = free_block_t{.prev = std::exchange(free_list_iter, ptr)};
    }
    return free_list_iter;
}
} // namespace detail
} // namespace block_allocator

template <typename allocator_impl_t>
class block_allocator_t : public ok::allocator_t
{
  private:
    struct free_block_t
    {
        free_block_t* prev;
    };

    struct members_t
    {
        bytes_t memory;
        size_t blocksize;
        size_t minimum_alignment;
        free_block_t* free_head;
        allocator_impl_t* backing;
    } m;

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (!m.backing)
            return;

        m.backing->deallocate(m.memory);
    }

    constexpr block_allocator_t(members_t&& m) OKAYLIB_NOEXCEPT
        : m(std::forward<members_t>(m))
    {
    }

    constexpr void grow() OKAYLIB_NOEXCEPT;

  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_clear;

    friend class block_allocator::detail::alloc_initial_buf_t;

    block_allocator_t() = delete;
    constexpr block_allocator_t(
        const block_allocator::fixed_buffer_options_t& options) OKAYLIB_NOEXCEPT
    {
        const size_t actual_minimum_alignment =
            ok::max(options.minimum_alignment, alignof(free_block_t));
        const size_t actual_blocksize = runtime_round_up_to_multiple_of(
            actual_minimum_alignment,
            ok::max(options.num_bytes_per_block, sizeof(free_block_t)));
        const size_t num_blocks =
            options.fixed_buffer.size() / actual_blocksize;
        __ok_usage_error(num_blocks > 0,
                         "Fixed buffer given to block allocator not large "
                         "enough to fit any blocks, it will OOM immediately.");

        m = {
            .memory = options.fixed_buffer,
            .blocksize = actual_blocksize,
            .minimum_alignment = actual_minimum_alignment,
            .free_head = block_allocator::detail::
                free_everything_in_block_allocator_buffer<free_block_t>(
                    options.fixed_buffer, actual_blocksize),
            .backing = nullptr,
        };
    }

    constexpr block_allocator_t(block_allocator_t&& other) OKAYLIB_NOEXCEPT
        : m(other.m)
    {
        other.m.free_head = nullptr; // not really necessary
        other.m.backing = nullptr;
    }

    constexpr block_allocator_t&
    operator=(block_allocator_t&& other) OKAYLIB_NOEXCEPT
    {
        destroy();
        m = other.m;
        other.m.free_head = nullptr; // not really necessary
        other.m.backing = nullptr;
        return *this;
    }

    block_allocator_t& operator=(const block_allocator_t&) = delete;
    block_allocator_t(const block_allocator_t&) = delete;

    inline ~block_allocator_t() OKAYLIB_NOEXCEPT_FORCE { destroy(); }

    constexpr size_t block_size() const noexcept { return m.blocksize; }
    constexpr size_t block_align() const noexcept
    {
        return m.minimum_alignment;
    }

    constexpr bool contains(const bytes_t& bytes) const noexcept
    {
        return ok_memcontains(.inner = bytes, .outer = m.memory);
    }

  protected:
    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    inline void impl_clear() OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    inline void impl_deallocate(bytes_t) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<bytes_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] inline alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT final
    {
        return alloc::error::unsupported;
    }
};

block_allocator_t(const block_allocator::fixed_buffer_options_t& static_buffer)
    -> block_allocator_t<ok::allocator_t>;

template <typename allocator_impl_t>
constexpr void block_allocator_t<allocator_impl_t>::grow() OKAYLIB_NOEXCEPT
{
    __ok_internal_assert(!m.free_head);
    if (!m.backing)
        return;

    alloc::result_t<bytes_t> reallocation =
        m.backing->reallocate(alloc::reallocate_request_t{
            .memory = m.memory,
            .new_size_bytes = m.memory.size() + m.blocksize,
            .preferred_size_bytes = m.memory.size() * 2,
            .flags = alloc::flags::in_place_orelse_fail &
                     alloc::flags::leave_nonzeroed,
        });

    if (!reallocation.okay()) [[unlikely]] {
        return;
    }

    bytes_t& newmem = reallocation.release_ref();
    // there may be extra space at the end
    const size_t padding = m.memory.size() % m.blocksize;
    uint8_t* const first_new_byte = newmem.data() + m.memory.size() - padding;
    const size_t additional_size = newmem.size() - m.memory.size() + padding;

    m.free_head =
        block_allocator::detail::free_everything_in_block_allocator_buffer(
            ok::raw_slice(*first_new_byte, additional_size), m.blocksize,
            m.free_head);

    __ok_internal_assert(m.free_head);
}

template <typename allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
block_allocator_t<allocator_impl_t>::impl_allocate(
    const alloc::request_t& request) OKAYLIB_NOEXCEPT
{
    if (!m.free_head) [[unlikely]] {
        grow();
        if (!m.free_head) [[unlikely]] {
            return alloc::error::oom;
        }
    }

    if (request.num_bytes > m.blocksize ||
        request.alignment > m.minimum_alignment) [[unlikely]] {
        return alloc::error::unsupported;
    }

    bytes_t output_memory = raw_slice(reinterpret_cast<uint8_t*>(std::exchange(
                                          m.free_head, m.free_head->prev)),
                                      m.blocksize);

    if (!(request.flags & alloc::flags::leave_nonzeroed)) {
        ok::memfill(output_memory, 0);
    }

    return output_memory;
}

template <typename allocator_impl_t>
inline void block_allocator_t<allocator_impl_t>::impl_clear() OKAYLIB_NOEXCEPT
{
    block_allocator::detail::free_everything_in_block_allocator_buffer<
        free_block_t>(m.memory, m.blocksize);
}

template <typename allocator_impl_t>
inline void block_allocator_t<allocator_impl_t>::impl_deallocate(bytes_t bytes)
    OKAYLIB_NOEXCEPT
{
    __ok_assert(ok_memcontains(.inner = bytes, .outer = m.memory),
                "Attempt to free bytes from block allocator which do not all "
                "belong to that allocator");
    __ok_assert((bytes.data() - m.memory.data()) % m.blocksize == 0,
                "Attempt to free something from block allocator that is not "
                "aligned to the start of a block.");

    auto* const free_block = reinterpret_cast<free_block_t*>(bytes.data());
    free_block->prev = std::exchange(m.free_head, free_block);
}

template <typename allocator_impl_t>
[[nodiscard]] inline alloc::result_t<bytes_t>
block_allocator_t<allocator_impl_t>::impl_reallocate(
    const alloc::reallocate_request_t& request) OKAYLIB_NOEXCEPT
{
    __ok_assert(ok_memcontains(.inner = request.memory, .outer = m.memory),
                "Attempt to realloc bytes from block allocator which do not "
                "all belong to that allocator");
    __ok_assert((request.memory.data() - m.memory.data()) % m.blocksize == 0,
                "Attempt to realloc something from block allocator that is not "
                "aligned to the start of a block.");
    if (request.new_size_bytes > m.blocksize) [[unlikely]] {
        return alloc::error::oom;
    }

    const size_t newsize =
        request.preferred_size_bytes == 0
            ? request.new_size_bytes
            : ok::min(request.preferred_size_bytes, m.blocksize);

    if (!(request.flags & alloc::flags::leave_nonzeroed)) {
        std::memset(request.memory.data() + request.memory.size(), 0,
                    newsize - request.memory.size());
    }

    return ok::raw_slice(*request.memory.data(), newsize);
}

namespace block_allocator {
namespace detail {
struct alloc_initial_buf_t
{
    template <typename size_type, typename allocator_impl_t>
    using associated_type = block_allocator_t<allocator_impl_t>;

    template <typename allocator_impl_t>
    [[nodiscard]] constexpr auto
    make(allocator_impl_t& allocator,
         const alloc_initial_buf_options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <typename allocator_impl_t>
    [[nodiscard]] constexpr status<alloc::error> operator()(
        ok::block_allocator_t<allocator_impl_t>& uninit,
        allocator_impl_t& allocator,
        const alloc_initial_buf_options_t& options) const OKAYLIB_NOEXCEPT
    {
        using block_allocator_t = ok::block_allocator_t<allocator_impl_t>;
        using free_block_t = typename block_allocator_t::free_block_t;
        const size_t actual_minimum_alignment =
            ok::max(options.minimum_alignment, alignof(free_block_t));
        const size_t actual_blocksize = runtime_round_up_to_multiple_of(
            actual_minimum_alignment,
            ok::max(options.num_bytes_per_block, sizeof(free_block_t)));

        alloc::result_t<bytes_t> result = allocator->allocate(alloc::request_t{
            .num_bytes = actual_blocksize * options.num_initial_spots,
            .alignment = actual_minimum_alignment,
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!result.okay()) [[unlikely]] {
            return result.err();
        }

        bytes_t& allocation = result.release_ref();

        new (ok::addressof(uninit))
            block_allocator_t(typename block_allocator_t::members_t{
                .memory = allocation,
                .blocksize = actual_blocksize,
                .minimum_alignment = actual_minimum_alignment,
                .free_head =
                    free_everything_in_block_allocator_buffer<free_block_t>(
                        allocation, actual_blocksize),
                .backing = ok::addressof(allocator),
            });

        __ok_usage_error(uninit.m.free_head,
                         "Created block allocator without enough memory, it "
                         "will immediately OOM.");

        return alloc::error::okay;
    }
};
} // namespace detail

inline constexpr detail::alloc_initial_buf_t alloc_initial_buf;

} // namespace block_allocator
} // namespace ok

#endif
