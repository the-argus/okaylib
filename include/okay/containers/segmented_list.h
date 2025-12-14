#ifndef __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__
#define __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/defer.h"
#include "okay/error.h"
#include "okay/iterables/iterables.h"
#include "okay/math/math.h"
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
[[nodiscard]] constexpr size_t
num_blocks_needed_for_spots(size_t num_spots) noexcept
{
    return log2_uint_ceil(num_spots + 1);
}

[[nodiscard]] constexpr size_t
get_num_spots_for_blocks(const size_t num_blocks) noexcept
{
    return two_to_the_power_of(num_blocks) - 1;
}

/// Returns a tuple of the index of the block this item belongs to in the
/// blocklist, and the sub-index of that item within the block (its
/// offset within the block)
[[nodiscard]] constexpr ok::tuple<size_t, size_t>
get_block_index_and_offset(const size_t idx) noexcept
{
    const auto blockidx = log2_uint(idx + 1);
    return ok::tuple{blockidx, idx - (two_to_the_power_of(blockidx) - 1)};
}

[[nodiscard]] constexpr size_t size_of_block_at(size_t idx) OKAYLIB_NOEXCEPT
{
    return two_to_the_power_of(idx);
}

static_assert(size_of_block_at(0) == 1);
static_assert(size_of_block_at(1) == 2);
static_assert(size_of_block_at(2) == 4);
static_assert(num_blocks_needed_for_spots(0) == 0);
static_assert(num_blocks_needed_for_spots(1) == 1);
static_assert(num_blocks_needed_for_spots(2) == 2);
static_assert(num_blocks_needed_for_spots(3) == 2);
static_assert(num_blocks_needed_for_spots(4) == 3);
static_assert(num_blocks_needed_for_spots(7) == 3);
static_assert(num_blocks_needed_for_spots(8) == 4);
static_assert(get_num_spots_for_blocks(0) == 0);
static_assert(get_num_spots_for_blocks(1) == 1);
static_assert(get_num_spots_for_blocks(2) == 3);
static_assert(get_num_spots_for_blocks(3) == 7);
static_assert(get_num_spots_for_blocks(4) == 15);
static_assert(get_block_index_and_offset(0) == ok::tuple{0, 0});
static_assert(get_block_index_and_offset(1) == ok::tuple{1, 0});
static_assert(get_block_index_and_offset(2) == ok::tuple{1, 1});
static_assert(get_block_index_and_offset(3) == ok::tuple{2, 0});
static_assert(get_block_index_and_offset(4) == ok::tuple{2, 1});
static_assert(get_block_index_and_offset(5) == ok::tuple{2, 2});
static_assert(get_block_index_and_offset(6) == ok::tuple{2, 3});
static_assert(get_block_index_and_offset(7) == ok::tuple{3, 0});
} // namespace detail
} // namespace segmented_list

template <typename T, allocator_c backing_allocator_t = ok::allocator_t>
class segmented_list_t
{
  public:
    static_assert(!stdc::is_reference_v<T>,
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
        : m(stdc::move(other.m))
    {
        other.m.blocklist = nullptr;
    }

    constexpr segmented_list_t&
    operator=(segmented_list_t&& other) OKAYLIB_NOEXCEPT
    {
        __ok_assert(other.m.allocator == m.allocator,
                    "attempt to move assign a container that does not shared "
                    "an allocator with the other container.");
        std::swap(this->m, other.m);
        other.clear();
        return *this;
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

        // TODO: we can predict how many blocks we need here and allocate them
        // in 1-2 allocations (an extra one may be needed to reallocate the
        // blocklist as well)
        while (size + additional_allocated_spots > this->capacity()) {
            if (auto status = this->new_block(); !ok::is_success(status))
                [[unlikely]] {
                return status;
            }
        }

        return alloc::error::success;
    }

    constexpr void clear() OKAYLIB_NOEXCEPT
    {
        if (this->is_empty()) [[unlikely]] {
            return;
        }
        if constexpr (!stdc::is_trivially_destructible_v<T>) {

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

        T& removal_target = this->unchecked_access(idx);
        T out = stdc::move(removal_target);
        defer decr_size([this] { m.size--; });

        // early out if you're popping the last item
        if (idx == this->size() - 1) {
            if constexpr (!stdc::is_trivially_destructible_v<T>) {
                removal_target->~T();
            }
            return out;
        }

        // simple loop through all elements, moving them towards the start of
        // the list, one at a time
        size_t i = idx;
        for (size_t cap = size() - 1; i < cap; ++i) {
            T& moved_out = this->unchecked_access(i);
            T& still_occupied = this->unchecked_access(i + 1);
            moved_out = stdc::move(still_occupied);
        }

        // clear out the last item, it is in a moved-from state
        if constexpr (!stdc::is_trivially_destructible_v<T>) {
            this->unchecked_access(i).~T();
        }

        return out;
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
        T out = stdc::move(*removal_target);
        *removal_target = stdc::move(*last);

        if constexpr (!stdc::is_trivially_destructible_v<T>) {
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
        if (!m.blocklist) [[unlikely]] {
            if (ok::status status = initialize_blocklist();
                !status.is_success()) [[unlikely]] {
                return status;
            }
            if (ok::status status = new_block(); !status.is_success())
                [[unlikely]] {
                return status;
            }
        }
        __ok_internal_assert(m.blocklist);
        __ok_assert(idx <= this->size(),
                    "out of bounds access in segmented_list_t<T>::insert_at");
        if (this->size() == this->capacity()) {
            if (ok::status status =
                    ensure_additional_blocklist_capacity_is_at_least_one();
                !status.is_success()) [[unlikely]] {
                return status;
            }
            __ok_internal_assert(m.blocklist && (m.blocklist->capacity >
                                                 m.blocklist->num_blocks));

            if (auto status = this->new_block(); !status.is_success()) {
                [[unlikely]] return status;
            }
        }
        __ok_internal_assert(this->capacity() > this->size());

        if (idx == this->size()) {
            // append to end
            T& new_item = this->unchecked_access(this->size());

            ok::make_into_uninitialized<T>(new_item,
                                           ok::stdc::forward<args_t>(args)...);
            ++m.size;
            return new_item;
        } else {
            // TODO: use memmove here for trivially copyable types

            T* existing_item = nullptr;
            for (size_t i = this->size(); i > idx; --i) {
                existing_item = ok::addressof(this->unchecked_access(i - 1));
                T& nonexisting_item = this->unchecked_access(i);
                stdc::construct_at(ok::addressof(nonexisting_item),
                                   stdc::move(*existing_item));
            }

            __ok_internal_assert(existing_item);
            __ok_internal_assert(existing_item ==
                                 ok::addressof(this->unchecked_access(idx)));

            // the last "existing" item no longer exists as we moved everything
            // down by one
            ok::make_into_uninitialized<T>(*existing_item,
                                           ok::stdc::forward<args_t>(args)...);
            ++m.size;
            return *existing_item;
        }
    }

    template <typename... args_t>
    [[nodiscard]] constexpr ok::alloc::result_t<T&>
    append(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        return insert_at(this->size(), ok::stdc::forward<args_t>(args)...);
    }

    /// Returns an error only if allocation to expand space for the new items
    /// errored.
    // template <typename other_range_t>
    // [[nodiscard]] constexpr status<alloc::error>
    // append_range(const other_range_t& range) OKAYLIB_NOEXCEPT
    // {
    //     static_assert(
    //         is_infallible_constructible_c<T, value_type_for<other_range_t>>,
    //         "Cannot append the given range: the contents of the arraylist "
    //         "cannot be constructed from the contents of the given range (at "
    //         "least not without a potential error at each construction).");
    //     static_assert(!detail::range_marked_infinite_c<other_range_t>,
    //                   "Cannot append an infinite range.");
    // }

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
            if constexpr (!stdc::is_trivially_destructible_v<T>) {
                auto items =
                    ok::raw_slice(*m.blocklist->blocks[i], size_of_block_at(i));

                for (size_t j = 0; j < items.size() && visited < this->size();
                     ++j) {
                    items[j].~T();
                    ++visited;
                }
                m.allocator->deallocate(
                    items.unchecked_address_of_first_item());
            } else {
                m.allocator->deallocate(m.blocklist->blocks[i]);
            }
        }

        m.allocator->deallocate(m.blocklist);
    }

    [[nodiscard]] constexpr status<alloc::error> new_block() OKAYLIB_NOEXCEPT
    {
        using namespace segmented_list::detail;
        const size_t bytes_needed =
            (m.blocklist ? size_of_block_at(m.blocklist->num_blocks) : 1) *
            sizeof(T);

        auto new_buffer_result = m.allocator->allocate(alloc::request_t{
            .num_bytes = bytes_needed,
            .alignment = alignof(T),
            .leave_nonzeroed = true,
        });

        if (!new_buffer_result.is_success()) [[unlikely]]
            return new_buffer_result.status();

        bytes_t& new_buffer_bytes = new_buffer_result.unwrap();
        defer free_new_buffer = [&] {
            m.allocator->deallocate(
                new_buffer_bytes.unchecked_address_of_first_item());
        };

        ok::status blocklist_status =
            ensure_additional_blocklist_capacity_is_at_least_one();
        if (!ok::is_success(blocklist_status)) [[unlikely]]
            return blocklist_status;

        __ok_internal_assert(m.blocklist &&
                             m.blocklist->capacity > m.blocklist->num_blocks);

        free_new_buffer.cancel();

        m.blocklist->blocks[m.blocklist->num_blocks] = reinterpret_cast<T*>(
            new_buffer_bytes.unchecked_address_of_first_item());
        m.blocklist->num_blocks++;

        return alloc::error::success;
    }

    [[nodiscard]] constexpr status<alloc::error>
    initialize_blocklist() OKAYLIB_NOEXCEPT
    {
        __ok_internal_assert(!m.blocklist);
        auto blocklist_result = m.allocator->allocate(alloc::request_t{
            .num_bytes = sizeof(blocklist_t) + (sizeof(T*) * m.size),
            .alignment = alignof(blocklist_t),
            .leave_nonzeroed = true,
        });

        if (!blocklist_result.is_success()) [[unlikely]]
            return blocklist_result.status();

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
        return alloc::error::success;
    }

    /// Reallocates the blocklist to have space for at least one more block
    [[nodiscard]] constexpr status<alloc::error>
    ensure_additional_blocklist_capacity_is_at_least_one() OKAYLIB_NOEXCEPT
    {
        if (!m.blocklist) {
            if (ok::status status = initialize_blocklist();
                !status.is_success()) [[unlikely]]
                return status;
        } else if (m.blocklist->capacity <= m.blocklist->num_blocks) {
            auto new_blocklist_result =
                m.allocator->reallocate(alloc::reallocate_request_t{
                    .memory = ok::raw_slice<uint8_t>(
                        *reinterpret_cast<uint8_t*>(m.blocklist),
                        (m.blocklist->capacity * sizeof(T*)) +
                            sizeof(blocklist_t)),
                    .new_size_bytes = sizeof(blocklist_t) +
                                      (sizeof(T*) * m.blocklist->capacity + 1),
                    .preferred_size_bytes =
                        sizeof(blocklist_t) +
                        (sizeof(T*) * m.blocklist->capacity * 2),
                });

            if (!new_blocklist_result.is_success()) [[unlikely]]
                return new_blocklist_result.status();

            bytes_t& blocklist_bytes = new_blocklist_result.unwrap();
            m.blocklist = reinterpret_cast<blocklist_t*>(
                blocklist_bytes.unchecked_address_of_first_item());
            m.blocklist->capacity =
                (blocklist_bytes.size() - sizeof(blocklist_t)) / sizeof(T*);
        }
        return alloc::error::success;
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
    // if true, the empty constructor will make all the allocations necessary
    // to hold up to expected_max_capacity elements. If false, it only allocates
    // enough for the blocklist.
    bool should_preallocate = false;
};

namespace detail {
template <typename T> struct empty_t
{

    template <typename backing_allocator_t, typename...>
    using associated_type =
        ok::segmented_list_t<T, ok::remove_cvref_t<backing_allocator_t>>;

    template <allocator_c backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const empty_options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <allocator_c backing_allocator_t>
    [[nodiscard]] constexpr alloc::error
    make_into_uninit(ok::segmented_list_t<T, backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     const empty_options_t& options) const OKAYLIB_NOEXCEPT
    {
        using M =
            typename ok::segmented_list_t<T, backing_allocator_t>::members_t;

        // expected_max_capacity + 1 because 4 expected -> log2_uint_ceil(4) ==
        // 2, but 2 blocks only gives us 3. log2_uint_ceil(16) == 4, but 4
        // blocks only gives us 15 spots.
        const size_t blocks_needed =
            log2_uint_ceil(ok::max(2UL, options.expected_max_capacity + 1));

        output.m = M{
            .blocklist = nullptr,
            // when blocklist is nullptr, "size" actually means size of initial
            // blocklist allocation
            .size = blocks_needed,
            .allocator = ok::addressof(allocator),
        };

        if (options.should_preallocate) {
            for (size_t i = 0; i < blocks_needed; ++i) {
                ok::status status = output.new_block();
                if (!ok::is_success(status)) {
                    // a failed call to new_block leaves the list in a valid
                    // state, so just early return, having not gotten all the
                    // spots. maybe we got some of them!
                    return status.as_enum();
                }
            }
        }

        return alloc::error::success;
    };
};

struct copy_items_from_range_t
{
    template <typename backing_allocator_t, typename input_range_t, typename...>
    using associated_type =
        ok::segmented_list_t<value_type_for<const input_range_t&>,
                             stdc::remove_reference_t<backing_allocator_t>>;

    template <allocator_c backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, range);
    }

    template <allocator_c backing_allocator_t, typename input_iterable_t>
    [[nodiscard]] constexpr alloc::error make_into_uninit(
        ok::segmented_list_t<value_type_for<const input_iterable_t&>,
                             backing_allocator_t>& output,
        backing_allocator_t& allocator,
        const input_iterable_t& iterable) const OKAYLIB_NOEXCEPT
        requires iterable_c<decltype(iterable)> &&
                 is_iterable_sized<decltype(iterable)>
    {
        using T = value_type_for<decltype(iterable)>;

        using M =
            typename ok::segmented_list_t<T, backing_allocator_t>::members_t;

        using blocklist_t =
            typename ok::segmented_list_t<T, backing_allocator_t>::blocklist_t;

        const size_t num_initial_blocks = log2_uint_ceil(ok::size(iterable));
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
        defer free_mainbuf([&] {
            allocator.deallocate(mainbuf.unchecked_address_of_first_item());
        });

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
        __ok_internal_assert(
            ok_memcontains(.outer = mainbuf,
                           .inner = reinterpret_as_bytes(itembuf)));

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

        for (auto item : ok::iter(iterable)) {
            auto stat = output.insert_at(output.size(), stdc::move(item));
            __ok_internal_assert(ok::is_success(stat));
        }

        free_mainbuf.cancel();
        return alloc::error::success;
    };
};
} // namespace detail

template <typename T> inline constexpr segmented_list::detail::empty_t<T> empty;

inline constexpr segmented_list::detail::copy_items_from_range_t
    copy_items_from_range;

} // namespace segmented_list
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
