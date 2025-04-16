#ifndef __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__
#define __OKAYLIB_CONTAINERS_SEGMENTED_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/math/math.h"
#include "okay/ranges/ranges.h"
#include "okay/status.h"

namespace ok {

namespace segmented_list {
namespace detail {
struct empty_t;
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

    friend struct ok::segmented_list::detail::empty_t;
    friend struct ok::segmented_list::detail::copy_items_from_range_t;

    [[nodiscard]] constexpr size_t capacity() const noexcept {}

  private:
    struct blocklist_t
    {
        size_t num_blocks;
        T* blocks[];

        constexpr T* get_block(size_t idx) OKAYLIB_NOEXCEPT
        {
            __ok_internal_assert(idx < num_blocks);
            return this->blocks + idx;
        }
        constexpr const T* get_block(size_t idx) const OKAYLIB_NOEXCEPT
        {
            __ok_internal_assert(idx < num_blocks);
            return this->blocks + idx;
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
                    size_t(num_spots + std::pow(initial_size_exponent, 2UL))) -
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
        return std::pow(m.initial_size, idx + 1);
    }

    struct members_t
    {
        blocklist_t* blocklist;
        size_t initial_size_exponent; // 2 ^ this number == size of first block
        size_t size;
        backing_allocator_t* allocator;
    } m;
};
} // namespace ok

#endif
