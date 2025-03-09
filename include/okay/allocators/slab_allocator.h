#ifndef __OKAYLIB_ALLOCATORS_SLAB_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_SLAB_ALLOCATOR_H__

#include "okay/allocators/block_allocator.h"
#include "okay/containers/array.h"

namespace ok {

namespace slab_allocator {

struct blocks_description_t
{
    size_t blocksize;
    size_t alignment;
};

template <size_t nblocksizes> struct options_t
{
    inline static constexpr size_t num_blocksizes = nblocksizes;

    ok::array_t<blocks_description_t, num_blocksizes> available_blocksizes;
    size_t num_initial_blocks_per_blocksize;
};

struct with_blocks_t;
} // namespace slab_allocator

template <typename allocator_impl_t, size_t num_blocksizes>
class slab_allocator_t : public ok::allocator_t
{
  private:
    struct members_t
    {
        array_t<block_allocator_t<allocator_impl_t>, num_blocksizes> allocators;
    } m;

  public:
    static constexpr alloc::feature_flags type_features =
        alloc::feature_flags::can_expand_back |
        alloc::feature_flags::can_predictably_realloc_in_place |
        alloc::feature_flags::can_clear;

    friend ok::slab_allocator::with_blocks_t;

    slab_allocator_t() = delete;

    constexpr slab_allocator_t(slab_allocator_t&& other)
        OKAYLIB_NOEXCEPT = default;
    constexpr slab_allocator_t&
    operator=(slab_allocator_t&& other) OKAYLIB_NOEXCEPT = default;

    slab_allocator_t& operator=(const slab_allocator_t&) = delete;
    slab_allocator_t(const slab_allocator_t&) = delete;

    inline ~slab_allocator_t() = default;

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

template <typename allocator_impl_t, size_t num_blocksizes>
[[nodiscard]] inline alloc::result_t<bytes_t>
slab_allocator_t<allocator_impl_t, num_blocksizes>::impl_allocate(
    const alloc::request_t& request) OKAYLIB_NOEXCEPT
{
    for (size_t i = 0; i < num_blocksizes; ++i) {
        auto& allocator = m.allocators[i];
        if (allocator.block_size() >= request.num_bytes &&
            allocator.block_align() >= request.alignment) {
            auto result = allocator.allocate(request);
            if (!result.okay()) [[unlikely]] {
                // try the next allocator
                continue;
            }
            return result;
        }
    }
    // no matching sub block allocators
    return alloc::error::oom;
}

template <typename allocator_impl_t, size_t num_blocksizes>
inline void slab_allocator_t<allocator_impl_t, num_blocksizes>::impl_clear()
    OKAYLIB_NOEXCEPT
{
    for (size_t i = 0; i < num_blocksizes; ++i) {
        m.allocators[i].clear();
    }
}

template <typename allocator_impl_t, size_t num_blocksizes>
inline void slab_allocator_t<allocator_impl_t, num_blocksizes>::impl_deallocate(
    bytes_t bytes) OKAYLIB_NOEXCEPT
{
    for (size_t i = 0; i < num_blocksizes; ++i) {
        auto& allocator = m.allocators[i];
        if (allocator.contains(bytes)) {
            allocator.deallocate(bytes);
            return;
        }
    }
    __ok_assert(false,
                "Freeing something with slab allocator which does not appear "
                "to be contained within any of its block allocators.");
}

template <typename allocator_impl_t, size_t num_blocksizes>
[[nodiscard]] inline alloc::result_t<bytes_t>
slab_allocator_t<allocator_impl_t, num_blocksizes>::impl_reallocate(
    const alloc::reallocate_request_t& request) OKAYLIB_NOEXCEPT
{
    if (request.flags & alloc::flags::in_place_orelse_fail) {
        for (size_t i = 0; i < num_blocksizes; ++i) {
            auto& allocator = m.allocators[i];
            if (allocator.contains(request.memory)) {
                return allocator.reallocate(request);
            }
        }
    } else {
        // first pass: find allocator which contains original memory and returns
        // OOM when we try to realloc in place
        block_allocator_t<allocator_impl_t>* oomed_allocator = nullptr;
        for (size_t i = 0; i < num_blocksizes; ++i) {
            block_allocator_t<allocator_impl_t>* allocator =
                ok::addressof(m.allocators[i]);

            if (allocator->contains(request.memory)) {
                alloc::result_t<bytes_t> result =
                    allocator->reallocate(request);
                if (result.okay()) {
                    return result;
                } else if (result.err() != alloc::error::oom) {
                    // found
                    return result.err();
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
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!result.okay()) [[unlikely]] {
            return result;
        }

        bytes_t& newbytes = result.release_ref();

        auto&& _ = ok_memcopy(.to = newbytes, .from = request.memory);

        if (!(request.flags & alloc::flags::leave_nonzeroed)) {
            std::memset(newbytes.data() + request.memory.size(), 0,
                        newbytes.size() - request.memory.size());
        }

        deallocate(request.memory);
        return newbytes;
    }

    __ok_assert(false,
                "Reallocating something with slab allocator which does not "
                "appear to be contained within any of its block allocators.");
}

namespace slab_allocator {
struct with_blocks_t
{
    template <typename allocator_impl_t_c_ref, typename options_ref,
              typename...>
    using associated_type =
        slab_allocator_t<detail::remove_cvref_t<allocator_impl_t_c_ref>,
                         detail::remove_cvref_t<options_ref>::num_blocksizes>;

    template <typename allocator_impl_t, size_t num_blocksizes>
    [[nodiscard]] constexpr auto
    make(allocator_impl_t& allocator,
         const options_t<num_blocksizes>& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <typename allocator_impl_t, size_t num_blocksizes>
    [[nodiscard]] constexpr status<alloc::error>
    operator()(associated_type<allocator_impl_t,
                               const options_t<num_blocksizes>&>& uninit,
               allocator_impl_t& allocator,
               const options_t<num_blocksizes>& options) const OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < options.available_blocksizes.size(); ++i) {
            const blocks_description_t& desc = options.available_blocksizes[i];

            status<alloc::error> status = block_allocator::alloc_initial_buf(
                uninit.m.allocators[i], allocator,
                block_allocator::alloc_initial_buf_options_t{
                    .num_initial_spots =
                        options.num_initial_blocks_per_blocksize,
                    .num_bytes_per_block = desc.blocksize,
                    .minimum_alignment = desc.alignment,
                });

            // since a simple status is returned and we dont construct any
            // owning references of any kind, we have to manually call
            // destruction on partially initialized blocks
            if (!status.okay()) [[unlikely]] {
                for (int64_t j = i - 1; j >= 0; --j) {
                    uninit.m.allocators[j]
                        .~block_allocator_t<allocator_impl_t>();
                }
                return status.err();
            }
        }

        return alloc::error::okay;
    }
};

inline constexpr with_blocks_t with_blocks;

} // namespace slab_allocator

} // namespace ok

#endif
