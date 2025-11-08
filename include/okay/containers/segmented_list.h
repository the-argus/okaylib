#ifndef __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__
#define __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/defer.h"
#include "okay/error.h"
#include "okay/math/math.h"
#include "okay/ranges/ranges.h"
#include "okay/stdmem.h"
#include "okay/tuple.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {

namespace segmented_list {
namespace detail {
template <typename T> struct empty_t;
struct copy_items_from_range_t;
[[nodiscard]] static constexpr size_t
num_blocks_needed_for_spots(size_t num_spots) noexcept
{
    return log2_uint_ceil(num_spots + 1);
}

static_assert(num_blocks_needed_for_spots(1) == 1);
static_assert(num_blocks_needed_for_spots(2) == 2);
static_assert(num_blocks_needed_for_spots(3) == 2);
static_assert(num_blocks_needed_for_spots(4) == 3);
static_assert(num_blocks_needed_for_spots(5) == 3);
static_assert(num_blocks_needed_for_spots(6) == 3);
static_assert(num_blocks_needed_for_spots(7) == 3);
static_assert(num_blocks_needed_for_spots(8) == 4);

[[nodiscard]] static constexpr size_t
get_num_spots_for_blocks(const size_t num_blocks) noexcept
{
    return two_to_the_power_of(num_blocks) - 1;
}

static_assert(get_num_spots_for_blocks(0) == 0);
static_assert(get_num_spots_for_blocks(1) == 1);
static_assert(get_num_spots_for_blocks(2) == 3);
static_assert(get_num_spots_for_blocks(3) == 7);
static_assert(get_num_spots_for_blocks(4) == 15);
static_assert(get_num_spots_for_blocks(5) == 31);

/// Returns a tuple of the index of the block this item belongs to in the
/// blocklist, and the sub-index of that item within the block (ie. its
/// offset within the block)
[[nodiscard]] static constexpr ok::tuple<size_t, size_t>
get_block_index_and_offset(const size_t idx) noexcept
{
    const size_t blocks_needed = num_blocks_needed_for_spots(idx + 1);
    return ok::make_tuple(blocks_needed,
                          get_num_spots_for_blocks(blocks_needed) - idx);
}

[[nodiscard]] constexpr static size_t
size_of_block_at(size_t idx) OKAYLIB_NOEXCEPT
{
    return two_to_the_power_of(idx);
}

} // namespace detail
} // namespace segmented_list

template <typename T, typename backing_allocator_t> class segmented_list_t
{
  public:
    static_assert(!std::is_reference_v<T>,
                  "Cannot create a segmented list of references.");
    static_assert(
        !is_const_c<T>,
        "Attempt to create a segmented list with const objects, "
        "which is not possible. Remove the const, and pass a const reference "
        "to the segmented list of mutable objects instead.");

    template <typename U> friend struct ok::segmented_list::detail::empty_t;
    friend struct ok::segmented_list::detail::copy_items_from_range_t;

    [[nodiscard]] constexpr size_t capacity() const noexcept
    {
        using namespace segmented_list::detail;
        if (!m.blocklist) {
            return 0;
        }

        return get_num_spots_for_blocks(m.blocklist->num_blocks);
    }

    [[nodiscard]] constexpr size_t size() const noexcept
    {
        // if no blocklist, size is zero, otherwise it is m.size
        // (enables encoding other info in size when blocklist is null)
        return bool(m.blocklist) * m.size;
    }

    [[nodiscard]] constexpr bool is_empty() const OKAYLIB_NOEXCEPT
    {
        return this->size() == 0;
    }

    constexpr opt<T> pop_last() OKAYLIB_NOEXCEPT
    {
        if (is_empty())
            return {};
        return this->remove(this->size() - 1);
    }

    [[nodiscard]] constexpr backing_allocator_t&
    allocator() const OKAYLIB_NOEXCEPT
    {
        return *m.backing_allocator;
    }

    template <typename incoming_allocator_t = ok::nonthreadsafe_allocator_t>
    [[nodiscard]] constexpr alloc::result_t<slice<slice<const T>>>
    get_blocks(incoming_allocator_t& allocator) const& OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        static_assert(
            detail::is_derived_from_c<incoming_allocator_t,
                                      ok::nonthreadsafe_allocator_t>,
            "First argument to segmented_list_t::get_blocks is not an "
            "allocator.");

        if (this->is_empty()) {
            return make_null_slice<slice<const T>>();
        }

        const size_t num_blocks_in_use =
            num_blocks_needed_for_spots(this->size());

        const size_t bytes_needed = sizeof(slice<const T>) * num_blocks_in_use;
        alloc::result_t<bytes_t> alloc_result =
            allocator.allocate(alloc::request_t{
                .num_bytes = bytes_needed,
                .alignment = alignof(slice<const T>),
                .leave_nonzeroed = true,
            });

        if (!alloc_result.is_success()) [[unlikely]] {
            return alloc_result.status();
        }

        slice<slice<const T>> blocks = reinterpret_bytes_as<slice<const T>>(
            alloc_result.unwrap().subslice({.length = bytes_needed}));

        __ok_internal_assert(get_num_spots_for_blocks(num_blocks_in_use) >=
                             this->size());
        const size_t extra_spots =
            get_num_spots_for_blocks(num_blocks_in_use) - this->size();

        // initialize the slices in `blocks` to point to all the blocks
        {
            size_t block_size = two_to_the_power_of(m.initial_size_exponent);

            for (size_t i = 0; i < blocks.size(); ++i) {
                slice<const T>& uninit = blocks.unchecked_access(i);
                new (&uninit)
                    slice<const T>(raw_slice(*m.get_block(i), block_size));
                block_size *= 2;
            }
        }

        // chop off the last `extra_spots` of the last block
        blocks.last() = blocks.last().subslice(
            {.length = blocks.last().size() - extra_spots});

        return blocks;
    }

    template <typename incoming_allocator_t = ok::allocator_t>
        [[nodiscard]] constexpr alloc::result_t<slice<slice<T>>>
        get_blocks(incoming_allocator_t& allocator) & OKAYLIB_NOEXCEPT
    {
        //  HACK: this is to reduce code duplication for now but it might be
        //  causing unecessary copies in and out of results
        if (this->is_empty()) {
            return make_null_slice<slice<T>>();
        }

        auto result =
            static_cast<const segmented_list_t*>(this)->get_blocks(allocator);
        if (!result.is_success()) [[unlikely]] {
            return result.status();
        }

        slice<slice<const T>>& blocks = result.unwrap();

        return raw_slice(*reinterpret_cast<slice<T>*>(
                             blocks.unchecked_address_of_first_item()),
                         blocks.size());
    }

    [[nodiscard]] constexpr const T&
    operator[](size_t index) const& OKAYLIB_NOEXCEPT
    {
        return (*const_cast<segmented_list_t*>(this))[index];
    }

    [[nodiscard]] constexpr T& operator[](size_t index) & OKAYLIB_NOEXCEPT
    {
        if (index >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::segmented_list_t");
        }

        return unchecked_access(index);
    }

    constexpr segmented_list_t(segmented_list_t&& other) noexcept
        : m(std::move(other.m))
    {
        other.m.blocklist = nullptr;
    }

    constexpr segmented_list_t& operator=(segmented_list_t&& other) noexcept
    {
        this->destroy();
        this->m = std::move(other.m);
        other.m.blocklist = nullptr;
    }

    [[nodiscard]] constexpr status<alloc::error>
    ensure_total_capacity_is_at_least(size_t total_allocated_spots)
        OKAYLIB_NOEXCEPT
    {
        const auto sz = size();

        if (total_allocated_spots <= sz)
            return alloc::error::success;

        return ensure_additional_capacity_is_at_least(total_allocated_spots -
                                                      sz);
    }

    [[nodiscard]] constexpr status<alloc::error>
    ensure_additional_capacity_is_at_least(size_t additional_allocated_spots)
        OKAYLIB_NOEXCEPT
    {
        const size_t size = this->size();
        const size_t capacity = this->capacity();
        __ok_internal_assert(size <= capacity);

        if (size != capacity) {
            return alloc::error::success;
        }

        const size_t bytes_needed =
            (m.blocklist ? size_of_block_at(m.blocklist->num_blocks)
                         : two_to_the_power_of(m.initial_size_exponent)) *
            sizeof(T);

        auto new_buffer_result = m.allocator->allocate(alloc::request_t{
            .num_bytes = bytes_needed,
            .alignment = alignof(T),
            .leave_nonzeroed = true,
        });

        if (!new_buffer_result.is_success()) [[unlikely]] {
            return new_buffer_result.status();
        }

        bytes_t& new_buffer_bytes = new_buffer_result.unwrap();
        defer free_new_buffer = [&] {
            m.allocator->deallocate(new_buffer_bytes);
        };

        if (!m.blocklist) {
            auto blocklist_result = m.allocator->allocate(alloc::request_t{
                .num_bytes = sizeof(blocklist_t) + (sizeof(T*) * m.size),
                .alignment = alignof(blocklist_t),
                .flags = alloc::flags::leave_nonzeroed,
            });

            if (!blocklist_result.is_success()) [[unlikely]] {
                return blocklist_result.status();
            }

            bytes_t& blocklist_bytes = blocklist_result.unwrap();
            m.blocklist = reinterpret_cast<blocklist_t*>(
                blocklist_bytes.unchecked_address_of_first_item());

            m.blocklist->num_blocks = 0;
            m.blocklist->capacity =
                (blocklist_bytes.size() - sizeof(blocklist_t)) / sizeof(T*);
            // m.size used to encode initial number of block pointers to
            // allocate, now it actually stores number of items in this
            // segmented list
            m.size = 0;
        } else if (m.blocklist->capacity <= m.blocklist->num_blocks) {
            auto new_blocklist_result =
                m.allocator->reallocate(alloc::reallocate_request_t{
                    .preferred_size_bytes =
                        sizeof(blocklist_t) +
                        (sizeof(T*) * m.blocklist->capacity * 2),
                    .new_size_bytes = sizeof(blocklist_t) +
                                      (sizeof(T*) * m.blocklist->capacity + 1),
                    .flags = alloc::flags::expand_back,
                });

            if (!new_blocklist_result.is_success()) [[unlikely]] {
                return new_blocklist_result.status();
            }

            bytes_t& blocklist_bytes = new_blocklist_result.unwrap();
            m.blocklist = reinterpret_cast<blocklist_t*>(
                blocklist_bytes.unchecked_address_of_first_item());
            m.blocklist->capacity =
                (blocklist_bytes.size() - sizeof(blocklist_t)) / sizeof(T*);
        }

        __ok_internal_assert(m.blocklist->capacity > m.blocklist->num_blocks);

        free_new_buffer.cancel();

        m.blocklist->blocks[m.blocklist->num_blocks] = reinterpret_cast<T*>(
            new_buffer_bytes.unchecked_address_of_first_item());
        m.blocklist->num_blocks++;

        return alloc::error::success;
    }

    constexpr void clear() OKAYLIB_NOEXCEPT
    {
        if (this->is_empty()) [[unlikely]] {
            return;
        }
        if constexpr (!std::is_trivially_destructible_v<T>) {

            size_t block_size = 1; // first block always 1 big
            size_t visited = 0;
            for (size_t block_idx = 0; block_idx < m.blocklist->num_blocks;
                 ++block_idx) {
                for (size_t i = 0; i < block_size && visited < this->size();
                     ++i) {
                    m.blocklist->blocks[block_idx][i].~T();
                    ++visited;
                }
                block_size *= 2;
            }
        }

        // if no blocklist, then m.size is encoding the number of blocklist
        // pointers to allocate and needs to be preserved between calls to
        // clear()
        __ok_internal_assert(m.blocklist);
        m.size = 0;
    }

    constexpr T remove(size_t idx) OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to segmented_list_t in "
                       "remove()");
        }

        T* removal_target = ok::addressof(this->unchecked_access(idx));
        T out = std::move(*removal_target);
        defer decr_size([this] { m.size--; });

        // early out if youre popping the last item
        if (idx == this->size() - 1) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                removal_target->~T();
            }
            return out;
        }

        auto [block_idx, item_offset_in_block] =
            get_block_index_and_offset(idx);

        auto [_, end_idx_in_block] =
            get_block_index_and_offset(this->size() - 1);

        const size_t blocks_in_use = num_blocks_needed_for_spots(this->size());
        // use memmove
        slice<T> block = make_null_slice<T>();
        for (size_t i = block_idx; i < blocks_in_use; ++i) {
            {
                slice<T> newblock =
                    this->get_block_slice(i).drop(item_offset_in_block);

                // if the previous block points to something, we need to
                // copy our first item back into its last item
                if (!block.is_empty()) {
                    if constexpr (std::is_trivially_copyable_v<T>) {
                        std::memcpy(block.unchecked_address_of_first_item(),
                                    newblock.unchecked_address_of_first_item(),
                                    sizeof(T));
                    } else {
                        // call move assignment, it was moved out of but is
                        // still in a valid state
                        block.unchecked_access(block.size() - 1) =
                            std::move(newblock.first());
                    }
                }

                block = newblock;
            }
            // if we are on the last block, do not perform a memmove of
            // any of the uninitialized memory at the end of the block.
            if (i == blocks_in_use - 1) {
                block = block.subslice({.length = end_idx_in_block + 1});
            }
            // move everything above the item by 1 down into the item.
            if constexpr (std::is_trivially_copyable_v<T>) {
                ok_memmove(.to = block, .from = block.drop(1));
            } else {
                for (size_t i = 0; i < block.size() - 1; ++i) {
                    block.unchecked_access(i) =
                        std::move(block.unchecked_access(i) + 1);
                }
            }
            // there should only be item offset first time around the loop
            item_offset_in_block = 0;
        }

        if (!std::is_trivially_destructible_v<T>) {
            // take what was previously the last item (now has been moved out
            // of) and destroy it
            block.unchecked_access(end_idx_in_block).~T();
        }
    }

    constexpr T remove_and_swap_last(size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to segmented_list_t in "
                       "remove_and_swap_last()");
        }

        T* removal_target = ok::addressof(this->unchecked_access(idx));
        __ok_internal_assert(this->size() != 0);
        T* last = ok::addressof(this->unchecked_access(this->size() - 1));
        T out = std::move(*removal_target);
        *removal_target = std::move(*last);

        if constexpr (!std::is_trivially_destructible_v<T>) {
            last->~T();
        }

        --m.size;

        return out;
    }

    constexpr T& last() & OKAYLIB_NOEXCEPT
    {
        if (this->is_empty()) [[unlikely]] {
            __ok_abort(
                "Attempt to get last() item from empty segmented_list_t.");
        }
        return this->unchecked_access(this->size() - 1);
    }

    constexpr const T& last() const& OKAYLIB_NOEXCEPT
    {
        return const_cast<segmented_list_t*>(this)->last();
    }

    constexpr T& first() & OKAYLIB_NOEXCEPT
    {
        if (this->is_empty()) [[unlikely]] {
            __ok_abort(
                "Attempt to get first() item from empty segmented_list_t.");
        }
        __ok_internal_assert(m.blocklist);
        return m.blocklist->blocks[0][0];
    }

    constexpr const T& first() const& OKAYLIB_NOEXCEPT
    {
        return const_cast<segmented_list_t*>(this)->first();
    }

    template <typename... args_t>
    [[nodiscard]] constexpr ok::alloc::result_t<T&>
    insert_at(const size_t idx, args_t&&... args) OKAYLIB_NOEXCEPT
    {
        __ok_assert(idx < this->size() || idx == 0,
                    "out of bounds access in segmented_list_t<T>::insert_at");
        if (this->size() == this->capacity()) {
            // realloc
        }
        // okaylib developer assert
        __ok_assert(this->capacity() > this->size(), "bad realloc?");

        T& new_item = this->unchecked_access(this->size());

        ok::make_into_uninitialized<T>(new_item,
                                       ok::stdc::forward<args_t>(args)...);

        ++m.size;

        return alloc::error::success;
    }

    template <typename... args_t>
    [[nodiscard]] constexpr ok::alloc::result_t<T&>
    append(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        return insert_at(0, ok::stdc::forward<args_t>(args)...);
    }

    /// Returns an error only if allocation to expand space for the new items
    /// errored.
    template <typename other_range_t>
    [[nodiscard]] constexpr status<alloc::error>
    append_range(const other_range_t& range) OKAYLIB_NOEXCEPT
    {
        static_assert(
            is_infallible_constructible_c<T, value_type_for<other_range_t>>,
            "Cannot append the given range: the contents of the arraylist "
            "cannot be constructed from the contents of the given range (at "
            "least not without a potential error at each construction).");
        static_assert(!detail::range_marked_infinite_c<other_range_t>,
                      "Cannot append an infinite range.");
    }

  private:
    struct blocklist_t
    {
        size_t num_blocks;
        size_t capacity;
        T* blocks[];

        constexpr T* get_block(size_t idx) OKAYLIB_NOEXCEPT
        {
            __ok_internal_assert(idx < num_blocks);
            return this->blocks[idx];
        }

        constexpr const T* get_block(size_t idx) const OKAYLIB_NOEXCEPT
        {
            __ok_internal_assert(idx < num_blocks);
            return this->blocks[idx];
        }
    };

    constexpr void destroy()
    {
        using namespace segmented_list::detail;
        if (!m.blocklist)
            return;

        size_t visited = 0;
        for (size_t i = 0; i < m.blocklist->num_blocks; ++i) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                auto items =
                    ok::raw_slice(*m.blocklist->blocks[i], size_of_block_at(i));

                for (size_t j = 0; j < items.size() && visited < this->size();
                     ++j) {
                    items[j].~T();
                    ++visited;
                }
                m.allocator->deallocate(reinterpret_as_bytes(items));
            } else {
                auto bytes = ok::raw_slice(
                    *reinterpret_cast<uint8_t*>(m.blocklist->blocks[i]),
                    size_of_block_at(i) * sizeof(T));
                m.allocator->deallocate(bytes);
            }
        }

        auto bytes = ok::raw_slice(m.blocklist,
                                   sizeof(blocklist_t) +
                                       (m.blocklist->capacity * sizeof(T*)));
        m.allocator->deallocate(bytes);
    }

    [[nodiscard]] constexpr T& unchecked_access(size_t index) & OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        auto [block, sub_index] = get_block_index_and_offset(index);
        return m.blocklist->blocks[block][sub_index];
    }

    [[nodiscard]] constexpr const T&
    unchecked_access(size_t index) const& OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        auto [block, sub_index] = get_block_index_and_offset(index);
        return m.blocklist->blocks[block][sub_index];
    }

    [[nodiscard]] constexpr slice<T>
        get_block_slice(size_t idx) & OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        __ok_internal_assert(m.blocklist);
        const size_t block_idx = num_blocks_needed_for_spots(idx + 1);
        return raw_slice(*m.blocklist->get_block(idx),
                         size_of_block_at(block_idx));
    }

    [[nodiscard]] constexpr slice<const T>
    get_block_slice(size_t idx) const& OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        __ok_internal_assert(m.blocklist);
        const size_t block_idx = num_blocks_needed_for_spots(idx + 1);
        return raw_slice(*m.blocklist->get_block(idx),
                         size_of_block_at(block_idx));
    }

    struct members_t
    {
        blocklist_t* blocklist;
        size_t size;
        backing_allocator_t* allocator;
    } m;
};

namespace segmented_list {

struct empty_options_t
{
    size_t expected_max_capacity = 0;
};

namespace detail {
template <typename T> struct empty_t
{

    template <typename backing_allocator_t, typename...>
    using associated_type =
        ok::segmented_list_t<T,
                             ok::detail::remove_cvref_t<backing_allocator_t>>;

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const empty_options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr alloc::error
    make_into_uninit(ok::segmented_list_t<T, backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     const empty_options_t& options) const OKAYLIB_NOEXCEPT
    {
        using M =
            typename ok::segmented_list_t<T, backing_allocator_t>::members_t;

        output.m = M{
            .blocklist = nullptr,
            // when blocklist is nullptr, "size" actually means size of initial
            // blocklist allocation
            .size = log2_uint_ceil(ok::min(1UL, options.expected_max_capacity)),
            .allocator = ok::addressof(allocator),
        };

        return alloc::error::success;
    };
};

struct copy_items_from_range_t
{
    template <typename backing_allocator_t, typename input_range_t, typename...>
    using associated_type =
        ok::segmented_list_t<value_type_for<const input_range_t&>,
                             std::remove_reference_t<backing_allocator_t>>;

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, range);
    }

    template <typename backing_allocator_t,
              ok::detail::producing_range_c input_range_t>
    [[nodiscard]] constexpr alloc::error
    make_into_uninit(ok::segmented_list_t<value_type_for<const input_range_t&>,
                                          backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     const input_range_t& range) const OKAYLIB_NOEXCEPT
        requires ok::detail::range_marked_sized_c<input_range_t>
    {
        using T = value_type_for<const input_range_t&>;

        using M =
            typename ok::segmented_list_t<T, backing_allocator_t>::members_t;

        using blocklist_t =
            typename ok::segmented_list_t<T, backing_allocator_t>::blocklist_t;

        const size_t num_initial_blocks = log2_uint_ceil(ok::size(range));
        const size_t num_initial_spots =
            get_num_spots_for_blocks(num_initial_blocks);

        const size_t blocklist_size_bytes =
            sizeof(blocklist_t) + (sizeof(T*) * num_initial_blocks);

        const size_t buffer_size_bytes = sizeof(T) * num_initial_spots;

        alloc::result_t<bytes_t> mainbuf_result =
            allocator.allocate(alloc::request_t{
                .num_bytes =
                    blocklist_size_bytes + alignof(T) + buffer_size_bytes,
                .alignment = alignof(T),
                .leave_nonzeroed = true,
            });

        if (!mainbuf_result.is_success()) [[unlikely]]
            return mainbuf_result.status();

        bytes_t& mainbuf = mainbuf_result.unwrap();
        defer free_mainbuf([&] { allocator.deallocate(mainbuf); });

        auto* blocklist = reinterpret_cast<blocklist_t*>(
            mainbuf.unchecked_address_of_first_item());

        *blocklist = blocklist_t{
            .num_blocks = 0,
            .capacity = num_initial_blocks,
        };

        void* blocklist_end =
            mainbuf.unchecked_address_of_first_item() + blocklist_size_bytes;

        size_t remaining_space = mainbuf.size() - blocklist_size_bytes;

        const bool aligned = std::align(alignof(T), buffer_size_bytes,
                                        blocklist_end, remaining_space);
        __ok_assert(
            aligned,
            "Allocator did not return enough space for a segmented list");

        auto itembuf =
            ok::raw_slice(*static_cast<T*>(blocklist_end), num_initial_spots);
        // okaylib developer assert
        __ok_assert(ok_memcontains(.inner = reinterpret_as_bytes(itembuf),
                                   .outer = mainbuf),
                    "Bad implementation of segmented list");

        size_t iter = 0;
        for (size_t i = 0; i < num_initial_blocks; ++i) {
            blocklist->blocks[i] = ok::addressof(itembuf[iter]);
            iter += segmented_list::detail::size_of_block_at(i);
        }

        output.m = M{
            .blocklist = blocklist,
            .size = 0,
            .allocator = ok::addressof(allocator),
        };

        free_mainbuf.cancel();
        return alloc::error::success;
    };
};
} // namespace detail

template <typename T> inline constexpr segmented_list::detail::empty_t<T> empty;

inline constexpr segmented_list::detail::copy_items_from_range_t
    copy_items_from_range;

} // namespace segmented_list

template <typename T, typename backing_allocator_t>
struct range_definition<ok::segmented_list_t<T, backing_allocator_t>>
{
    static inline constexpr bool is_arraylike = true;

    using range_t = ok::segmented_list_t<T, backing_allocator_t>;
    using value_type = T;

    static constexpr value_type& get_ref(range_t& r, size_t c) OKAYLIB_NOEXCEPT
    {
        return r[c];
    }

    static constexpr const value_type& get_ref(const range_t& r,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        return r[c];
    }

    static constexpr size_t size(const range_t& r) OKAYLIB_NOEXCEPT
    {
        return r.size();
    }
};

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename T, typename backing_allocator_t>
struct fmt::formatter<ok::segmented_list_t<T, backing_allocator_t>>
{
    using formatted_type_t = ok::segmented_list_t<T, backing_allocator_t>;
    static_assert(
        fmt::is_formattable<T>::value,
        "Attempt to format a segmented list whose items are not formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& segmented_list,
                                    format_context& ctx) const
    {
        // TODO: use CTTI to include nice type names in print here
        fmt::format_to(ctx.out(), "segmented_list_t: [ ");

        if (segmented_list.m.blocklist) {
            size_t block_size =
                ok::two_to_the_power_of(segmented_list.m.initial_size_exponent);

            for (size_t b = 0; b < segmented_list.m.blocklist->num_blocks;
                 ++b) {
                T* block = segmented_list.m.blocklist->blocks[b];
                for (size_t i = 0; i < block_size; ++i) {
                    fmt::format_to(ctx.out(), "{} ", block[i]);
                }
                block_size *= 2;
            }
        }

        return fmt::format_to(ctx.out(), "]");
    }
};
#endif
#endif
