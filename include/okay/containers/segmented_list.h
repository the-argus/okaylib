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
    static_assert(!std::is_const_v<T>,
                  "Attempt to create a segmented list with const objects, "
                  "which is not possible. Remove the const, and consider "
                  "passing a const reference to the segmented list instead.");

    template <typename U> friend struct ok::segmented_list::detail::empty_t;
    friend struct ok::segmented_list::detail::copy_items_from_range_t;

    [[nodiscard]] constexpr size_t capacity() const noexcept
    {
        if (!m.blocklist) {
            return 0;
        }

        size_t total = 0;
        size_t blocksize = 2 << m.initial_size_exponent;
        for (size_t i = 0; i < m.blocklist->num_blocks; ++i) {
            total += blocksize;
            blocksize *= 2;
        }

        return total;
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

    [[nodiscard]] static constexpr size_t
    num_blocks_needed_for_spots(size_t initial_size_exponent,
                                size_t num_spots) noexcept
    {
        if (num_spots == 0) [[unlikely]] {
            return 0;
        }
        // if initial_size_exponent was zero, meaning our first block has one
        // item, the equation is:
        // num_blocks = log2_uint(num_spots) + 1
        //
        // accounting for initial_size_exponent by offsetting box and shelf
        // index:
        return (ok::log2_uint(
                    size_t(num_spots + (2UL << initial_size_exponent))) -
                initial_size_exponent) +
               1;
    }

    static_assert(num_blocks_needed_for_spots(1, 1) == 1);
    static_assert(num_blocks_needed_for_spots(2, 4) == 1);
    static_assert(num_blocks_needed_for_spots(2, 8) == 2);
    static_assert(num_blocks_needed_for_spots(2, 12) == 2);
    static_assert(num_blocks_needed_for_spots(2, 13) == 3);

    [[nodiscard]] constexpr size_t
    size_of_block_at(size_t idx) const OKAYLIB_NOEXCEPT
    {
        return m.initial_size << (idx + 1);
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
    size_t num_initial_contiguous_spots;
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
                2 << ok::log2_uint_ceil(options.num_block_pointers),
            .size = 0,
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
            ok::detail::range_definition_has_size_v<input_range_t>,
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
                .num_bytes = sizeof(T) * (2 << initial_size_exponent),
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
            size_t block_size = 2 << segmented_list.m.initial_size_exponent;

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
