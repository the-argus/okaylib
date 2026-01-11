#ifndef __OKAYLIB_ALLOCATORS_SLAB_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_SLAB_ALLOCATOR_H__

#include "okay/allocators/block_allocator.h"
#include "okay/containers/array.h"

namespace ok {

namespace slab_allocator {

struct blocks_description_t
{
    size_t blocksize{};
    size_t alignment{};
};

template <size_t nblocksizes> struct options_t
{
    inline static constexpr size_t num_blocksizes = nblocksizes;

    ok::array_t<blocks_description_t, num_blocksizes> available_blocksizes;
    size_t num_initial_blocks_per_blocksize;
};

struct with_blocks_t;
} // namespace slab_allocator

template <size_t num_blocksizes> class slab_allocator_t : public ok::allocator_t
{
  private:
    array_t<block_allocator_t, num_blocksizes> m_allocators;

  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_reclaim |
        alloc::feature_flags::can_predictably_realloc_in_place;

    friend ok::slab_allocator::with_blocks_t;

    slab_allocator_t() = delete;

    constexpr slab_allocator_t(slab_allocator_t&& other)
        OKAYLIB_NOEXCEPT = default;
    constexpr slab_allocator_t&
    operator=(slab_allocator_t&& other) OKAYLIB_NOEXCEPT = default;

    constexpr slab_allocator_t& operator=(const slab_allocator_t&) = delete;
    constexpr slab_allocator_t(const slab_allocator_t&) = delete;

    constexpr ~slab_allocator_t() = default;

    constexpr void clear() OKAYLIB_NOEXCEPT;

  protected:
    [[nodiscard]] constexpr alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] constexpr alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT final
    {
        return type_features;
    }

    constexpr void impl_deallocate(void* memory,
                                   size_t size_hint) OKAYLIB_NOEXCEPT final;

    [[nodiscard]] constexpr alloc::result_t<bytes_t>
    impl_reallocate(const alloc::reallocate_request_t&) OKAYLIB_NOEXCEPT final;
};

template <size_t num_blocksizes>
[[nodiscard]] constexpr alloc::result_t<bytes_t>
slab_allocator_t<num_blocksizes>::impl_allocate(const alloc::request_t& request)
    OKAYLIB_NOEXCEPT
{
    for (size_t i = 0; i < num_blocksizes; ++i) {
        auto& allocator = m_allocators[i];
        if (allocator.block_size() >= request.num_bytes &&
            allocator.block_align() >= request.alignment) {
            alloc::result_t<bytes_t> result = allocator.allocate(request);
            if (result.status() == alloc::error::oom) [[unlikely]] {
                // try the next allocator
                continue;
            }
            __ok_internal_assert(!ok::is_success(result) ||
                                 allocator.contains(result.unwrap()));
            return result;
        }
    }
    // no matching sub block allocators
    return alloc::error::oom;
}

template <size_t num_blocksizes>
constexpr void slab_allocator_t<num_blocksizes>::clear() OKAYLIB_NOEXCEPT
{
    for (size_t i = 0; i < num_blocksizes; ++i) {
        m_allocators[i].clear();
    }
}

template <size_t num_blocksizes>
constexpr void slab_allocator_t<num_blocksizes>::impl_deallocate(
    void* memory, size_t /* size_hint */) OKAYLIB_NOEXCEPT
{
    for (size_t i = 0; i < num_blocksizes; ++i) {
        auto& allocator = m_allocators[i];
        if (allocator.contains(memory)) {
            allocator.deallocate(memory);
            return;
        }
    }
    __ok_assert(false,
                "Freeing something with slab allocator which does not appear "
                "to be contained within any of its block allocators.");
}

template <size_t num_blocksizes>
[[nodiscard]] constexpr alloc::result_t<bytes_t>
slab_allocator_t<num_blocksizes>::impl_reallocate(
    const alloc::reallocate_request_t& request) OKAYLIB_NOEXCEPT
{
    if (request.flags & alloc::realloc_flags::in_place_orelse_fail) {
        for (size_t i = 0; i < num_blocksizes; ++i) {
            auto& allocator = m_allocators[i];
            if (allocator.contains(request.memory)) {
                return allocator.reallocate(request);
            }
        }
    } else {
        // first pass: find allocator which contains original memory and returns
        // OOM when we try to realloc in place
        block_allocator_t* oomed_allocator = nullptr;
        for (size_t i = 0; i < num_blocksizes; ++i) {
            block_allocator_t* allocator = ok::addressof(m_allocators[i]);

            if (allocator->contains(request.memory)) {
                alloc::result_t<bytes_t> result =
                    allocator->reallocate(request);
                if (result.is_success()) {
                    return result;
                } else if (result.status() != alloc::error::oom) {
                    // found
                    return result.status();
                } else {
                    oomed_allocator = allocator;
                    break;
                }
            }
        }

        // if none of the block allocators contained the memory, oomed_allocator
        // will not have been set/found
        __ok_assert(
            oomed_allocator,
            "Reallocating something with slab allocator which does not "
            "appear to be contained within any of its block allocators.");

        alloc::result_t<bytes_t> result = allocate(alloc::request_t{
            .num_bytes =
                ok::max(request.new_size_bytes, request.preferred_size_bytes),
            .alignment = oomed_allocator->block_align(),
            .leave_nonzeroed = true,
        });

        if (!result.is_success()) [[unlikely]] {
            return result;
        }

        bytes_t& newbytes = result.unwrap();

        auto&& _ = ok_memcopy(.to = newbytes, .from = request.memory);

        if (!(request.flags & alloc::realloc_flags::leave_nonzeroed)) {
            ::memset(newbytes.unchecked_address_of_first_item() +
                         request.memory.size(),
                     0, newbytes.size() - request.memory.size());
        }

        deallocate(request.memory.unchecked_address_of_first_item());
        return newbytes;
    }

    __ok_assert(false,
                "Reallocating something with slab allocator which does not "
                "appear to be contained within any of its block allocators.");
}

namespace slab_allocator {
struct with_blocks_t
{
    static constexpr auto implemented_make_function =
        ok::implemented_make_function::make_into_uninit;

    template <typename allocator_impl_fully_qualified_t,
              typename options_fully_qualified_t, typename...>
    using associated_type = slab_allocator_t<
        remove_cvref_t<options_fully_qualified_t>::num_blocksizes>;

    template <size_t num_blocksizes>
    [[nodiscard]] constexpr auto
    operator()(allocator_t& allocator,
               const options_t<num_blocksizes>& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <size_t num_blocksizes>
    [[nodiscard]] constexpr alloc::error make_into_uninit(
        slab_allocator_t<num_blocksizes>& uninit, allocator_t& allocator,
        const options_t<num_blocksizes>& options) const OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < options.available_blocksizes.size(); ++i) {
            const blocks_description_t& desc = options.available_blocksizes[i];

            alloc::error status =
                block_allocator::alloc_initial_buf.make_into_uninit(
                    uninit.m_allocators[i], allocator,
                    block_allocator::alloc_initial_buf_options_t{
                        .num_initial_spots =
                            options.num_initial_blocks_per_blocksize,
                        .num_bytes_per_block = desc.blocksize,
                        .minimum_alignment = desc.alignment,
                    });

            // since a simple status is returned and we dont construct any
            // owning references of any kind, we have to manually call
            // destruction on partially initialized blocks
            if (!ok::is_success(status)) [[unlikely]] {
                for (int64_t j = i - 1; j >= 0; --j) {
                    uninit.m_allocators[j].~block_allocator_t();
                }
                return status;
            }
        }

        return alloc::error::success;
    }
};

inline constexpr with_blocks_t with_blocks;

} // namespace slab_allocator

} // namespace ok

#endif
