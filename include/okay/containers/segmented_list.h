#ifndef __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__
#define __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/defer.h"
#include "okay/math/math.h"
#include "okay/ranges/ranges.h"
#include "okay/status.h"

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {

namespace segmented_list {
namespace detail {
template <typename T> struct empty_t;
struct copy_items_from_range_t;
} // namespace detail
} // namespace segmented_list

template <typename T, typename backing_allocator_t> class segmented_list_t
{
  public:
    static_assert(!std::is_reference_v<T>,
                  "Cannot create a segmented list of references.");
    static_assert(
        !std::is_const_v<T>,
        "Attempt to create a segmented list with const objects, "
        "which is not possible. Remove the const, and pass a const reference "
        "to the segmented list of mutable objects instead.");

    template <typename U> friend struct ok::segmented_list::detail::empty_t;
    friend struct ok::segmented_list::detail::copy_items_from_range_t;

    [[nodiscard]] constexpr size_t capacity() const noexcept
    {
        if (!m.blocklist) {
            return 0;
        }

        size_t total = 0;
        size_t blocksize = two_to_the_power_of(m.initial_size_exponent);
        for (size_t i = 0; i < m.blocklist->num_blocks; ++i) {
            total += blocksize;
            blocksize *= 2;
        }

        return total;
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
    ensure_additional_capacity() OKAYLIB_NOEXCEPT
    {
        const size_t size = this->size();
        const size_t capacity = this->capacity();
        __ok_internal_assert(size <= capacity);

        if (size != capacity) {
            return alloc::error::okay;
        }

        const size_t bytes_needed =
            (m.blocklist ? size_of_block_at(m.blocklist->num_blocks)
                         : two_to_the_power_of(m.initial_size_exponent)) *
            sizeof(T);

        auto new_buffer_result = m.allocator->allocate(alloc::request_t{
            .num_bytes = bytes_needed,
            .alignment = alignof(T),
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!new_buffer_result.okay()) [[unlikely]] {
            return new_buffer_result.err();
        }

        bytes_t& new_buffer_bytes = new_buffer_result.release_ref();
        maydefer free_new_buffer = [&] {
            m.allocator->deallocate(new_buffer_bytes);
        };

        if (!m.blocklist) {
            auto blocklist_result = m.allocator->allocate(alloc::request_t{
                .num_bytes = sizeof(blocklist_t) + (sizeof(T*) * m.size),
                .alignment = alignof(blocklist_t),
                .flags = alloc::flags::leave_nonzeroed,
            });

            if (!blocklist_result.okay()) [[unlikely]] {
                return blocklist_result.err();
            }

            bytes_t& blocklist_bytes = blocklist_result.release_ref();
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

            if (!new_blocklist_result.okay()) [[unlikely]] {
                return new_blocklist_result.err();
            }

            bytes_t& blocklist_bytes = new_blocklist_result.release_ref();
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

        return alloc::error::okay;
    }

    constexpr void clear() OKAYLIB_NOEXCEPT
    {
        if (this->is_empty()) [[unlikely]] {
            return;
        }
        if constexpr (!std::is_trivially_destructible_v<T>) {

            size_t block_size = two_to_the_power_of(m.initial_size_exponent);
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

    constexpr T remove(size_t idx) OKAYLIB_NOEXCEPT;

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
        const size_t last_block_idx = block_index(this->size());
        const size_t sub_item_idx = item_index(this->size(), last_block_idx);
        return m.blocklist->blocks[last_block_idx][sub_item_idx];
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

    constexpr void shrink_to_reclaim_unused_memory() OKAYLIB_NOEXCEPT;

    [[nodiscard]] constexpr status<alloc::error>
    increase_capacity_by_at_least(size_t new_spots) OKAYLIB_NOEXCEPT;

    template <typename... args_t>
    [[nodiscard]] constexpr auto insert_at(const size_t idx,
                                           args_t&&... args) OKAYLIB_NOEXCEPT;

    template <typename... args_t>
    [[nodiscard]] constexpr auto append(args_t&&... args) OKAYLIB_NOEXCEPT;

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
        const size_t block = block_index(index);
        const size_t sub_index = item_index(index, block);
        return m.blocklist->blocks[block][sub_index];
    }

    [[nodiscard]] constexpr const T&
    unchecked_access(size_t index) const& OKAYLIB_NOEXCEPT
    {
        return const_cast<segmented_list_t*>(this)->unchecked_access(index);
    }

    [[nodiscard]] constexpr size_t
    block_index(size_t requested_item_index) const noexcept
    {
        const size_t initial_size =
            two_to_the_power_of(m.initial_size_exponent);
        return log2_uint(requested_item_index + initial_size) -
               m.initial_size_exponent;
    }

    [[nodiscard]] constexpr size_t item_index(size_t requested_item_index,
                                              size_t block_index) const noexcept
    {
        const size_t initial_size =
            two_to_the_power_of(m.initial_size_exponent);
        return requested_item_index + initial_size -
               two_to_the_power_of(m.initial_size_exponent + block_index);
    }

    [[nodiscard]] static constexpr size_t
    num_blocks_needed_for_spots_impl(size_t initial_size_exponent,
                                     size_t num_spots) noexcept
    {
        const size_t initial_size = two_to_the_power_of(initial_size_exponent);
        return log2_uint_ceil(num_spots + initial_size) - initial_size_exponent;
    }

    [[nodiscard]] constexpr size_t
    num_blocks_needed_for_spots(size_t num_spots) const noexcept
    {
        return num_blocks_needed_for_spots_impl(m.initial_size_exponent,
                                                num_spots);
    }

    static_assert(num_blocks_needed_for_spots_impl(0, 1) == 1);
    static_assert(num_blocks_needed_for_spots_impl(0, 2) == 2);
    static_assert(num_blocks_needed_for_spots_impl(0, 3) == 2);
    static_assert(num_blocks_needed_for_spots_impl(0, 4) == 3);
    static_assert(num_blocks_needed_for_spots_impl(0, 5) == 3);
    static_assert(num_blocks_needed_for_spots_impl(0, 6) == 3);
    static_assert(num_blocks_needed_for_spots_impl(0, 7) == 3);
    static_assert(num_blocks_needed_for_spots_impl(0, 8) == 4);

    static_assert(num_blocks_needed_for_spots_impl(1, 1) == 1);
    static_assert(num_blocks_needed_for_spots_impl(2, 4) == 1);
    static_assert(num_blocks_needed_for_spots_impl(2, 8) == 2);
    static_assert(num_blocks_needed_for_spots_impl(2, 12) == 2);
    static_assert(num_blocks_needed_for_spots_impl(2, 13) == 3);

    [[nodiscard]] constexpr size_t
    size_of_block_at(size_t idx) const OKAYLIB_NOEXCEPT
    {
        return two_to_the_power_of(m.initial_size_exponent + idx);
    }

    struct members_t
    {
        blocklist_t* blocklist;
        size_t initial_size_exponent; // 2 ^ this number == size of first block
        size_t size;
        backing_allocator_t* allocator;
    } m;
};

namespace segmented_list {

struct empty_options_t
{
    // NOTE: the amount that will be alloced on first allocation is this number
    // rounded up to the next power of two.
    size_t num_initial_contiguous_spots = 4;
    /// The size of the pointers-to-blocks arraylist. This is small and
    /// reallocation of it is the only part of segmented list that causes
    /// reallocs (and so potential memory fragmentation or loss) so it's a good
    /// idea to set it to a largeish number in most cases.
    size_t num_block_pointers = 4;
};

struct copy_items_from_range_options_t
{
    size_t num_block_pointers = 4;
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
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(ok::segmented_list_t<T, backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     const empty_options_t& options) const OKAYLIB_NOEXCEPT
    {
        using M =
            typename ok::segmented_list_t<T, backing_allocator_t>::members_t;

        output.m = M{
            .blocklist = nullptr,
            .initial_size_exponent =
                options.num_initial_contiguous_spots == 0
                    ? 0
                    : ok::log2_uint_ceil(options.num_initial_contiguous_spots),
            .size = options.num_block_pointers,
            .allocator = ok::addressof(allocator),
        };

        return alloc::error::okay;
    };
};

struct copy_items_from_range_t
{
    template <typename backing_allocator_t, typename input_range_t, typename...>
    using associated_type =
        ok::segmented_list_t<value_type_for<const input_range_t&>,
                             std::remove_reference_t<backing_allocator_t>>;

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr auto operator()(
        backing_allocator_t& allocator, const input_range_t& range,
        const copy_items_from_range_options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, range);
    }

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr status<alloc::error> make_into_uninit(
        ok::segmented_list_t<value_type_for<const input_range_t&>,
                             backing_allocator_t>& output,
        backing_allocator_t& allocator, const input_range_t& range,
        const copy_items_from_range_options_t& options) const OKAYLIB_NOEXCEPT
    {
        static_assert(
            ok::detail::range_impls_size_v<input_range_t>,
            "Size of range unknown, refusing to copy out its items "
            "using segmented_list::copy_items_from_range constructor.");

        const size_t initial_size_exponent =
            ok::log2_uint_ceil(ok::size(range));

        using T = value_type_for<const input_range_t&>;

        using M =
            typename ok::segmented_list_t<T, backing_allocator_t>::members_t;

        using blocklist_t =
            typename ok::segmented_list_t<T, backing_allocator_t>::blocklist_t;

        alloc::result_t<bytes_t> mainbuf_result =
            allocator.allocate(alloc::request_t{
                .num_bytes =
                    sizeof(T) * (two_to_the_power_of(initial_size_exponent)),
                .alignment = alignof(T),
                .flags = alloc::flags::leave_nonzeroed,
            });

        if (!mainbuf_result.okay()) [[unlikely]] {
            return mainbuf_result.err();
        }

        bytes_t& mainbuf = mainbuf_result.release_ref();
        maydefer free_mainbuf = [&] { allocator.deallocate(mainbuf); };

        alloc::result_t<bytes_t> blocklist_result =
            allocator.allocate(alloc::request_t{
                .num_bytes = sizeof(blocklist_t) +
                             (options.num_block_pointers * sizeof(T*)),
                .alignment = alignof(blocklist_t),
                .flags = alloc::flags::leave_nonzeroed,
            });

        if (!blocklist_result.okay()) [[unlikely]] {
            return blocklist_result.err();
        }

        bytes_t& blocklist_bytes = blocklist_result.release_ref();

        blocklist_t* blocklist = &blocklist_bytes[0];

        *blocklist = blocklist_t{
            .num_blocks = 0,
            .capacity = blocklist_bytes.size() / sizeof(T*),
        };

        // give blocklist pointer to our first buffer
        blocklist->blocks[0] = &mainbuf[0];

        output.m = M{
            .blocklist = blocklist,
            .initial_size_exponent = initial_size_exponent,
            .size = 0,
            .allocator = ok::addressof(allocator),
        };

        free_mainbuf.cancel();
        return alloc::error::okay;
    };
};
} // namespace detail

template <typename T> inline constexpr segmented_list::detail::empty_t<T> empty;

inline constexpr segmented_list::detail::copy_items_from_range_t
    copy_items_from_range;

} // namespace segmented_list

} // namespace ok

#ifdef OKAYLIB_USE_FMT
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
