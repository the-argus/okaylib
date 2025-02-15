#ifndef __OKAYLIB_CONTAINERS_ARRAY_LIST_H__
#define __OKAYLIB_CONTAINERS_ARRAY_LIST_H__

#include "okay/allocators/allocator.h"

namespace ok {
template <typename T, typename backing_allocator_t = ok::allocator_t>
class array_list_t
{
  private:
    // arraylist is 32 bytes on the stack:
    // - 8 bytes: number spots allocated / capacity
    // - 8 bytes: pointer to start of allocation
    // - 8 bytes: number of spots used / length
    // - 8 bytes: backing_allocator pointer
    struct M
    {
        opt_t<slice_t<T>> allocated_spots;
        size_t spots_occupied;
        backing_allocator_t& backing_allocator;
    };

    constexpr array_list_t(M members) OKAYLIB_NOEXCEPT : m(members) {}

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (m.allocated_spots) {
            m.backing_allocator.deallocate(
                reinterpret_as_bytes(m.allocated_spots.value()));
        }
    }

  public:
    static_assert(!std::is_reference_v<T>,
                  "array_list_t cannot store references.");
    static_assert(!std::is_void_v<T>, "cannot create an array list of void.");
    static_assert(
        std::is_trivially_copyable_v<T> || std::is_move_constructible_v<T> ||
            std::is_constructible_v<T, T&&>,
        "Type given to array_list_t must be either trivially copyable or move "
        "constructible, otherwise it cannot move the items when reallocating.");

    // no default constructor or copy, but can move
    array_list_t() = delete;
    array_list_t(const array_list_t&) = delete;
    array_list_t& operator=(const array_list_t&) = delete;

    constexpr array_list_t(array_list_t&& other) : m(other.m)
    {
        other.m.allocated_spots = nullopt;
    }

    constexpr array_list_t& operator=(array_list_t&& other)
    {
        this->destroy();
        this->m = other.m;
        other.m.allocated_spots = nullopt;
    }

    template <typename input_range_t>
    [[nodiscard]] static constexpr alloc::result_t<array_list_t>
    try_make_by_copying(backing_allocator_t& allocator,
                        const input_range_t& range) OKAYLIB_NOEXCEPT
    {
        static_assert(
            ok::is_range_v<input_range_t>,
            "Second argument given to try_make_from_range is not a range.");
        static_assert(
            ok::detail::range_definition_has_size_v<input_range_t>,
            "Size of range unknown, not valid for try_make_by_copying.");
        static_assert(
            std::is_constructible_v<T, const typename ok::range_def_for<
                                           input_range_t>::value_type&>,
            "Attempt to use try_make_by_copying but the type held by the "
            "array_list_t is not constructible from the type outputted by the "
            "range.");

        // TODO: make this a warning
        __ok_assert(
            allocator.features() & alloc::feature_flags::can_expand_back,
            "Allocator given to array_list_t cannot expand_back, which will "
            "cause an error after appending some number of elements.");

        const size_t num_items = ok::size(range);

        alloc::result_t<maybe_defined_memory_t> res =
            allocator.allocate(alloc::request_t{
                .num_bytes = num_items * sizeof(T),
                .alignment = alignof(T),
                .flags = alloc::flags::leave_nonzeroed,
            });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        auto& maybe_undefined = res.release_ref();
        uint8_t* const memory = maybe_undefined.data_maybe_defined();
        const size_t bytes_allocated = maybe_undefined.size();

        // TODO: if contiguous range and trivially copyable, do memcpy
        size_t i = 0;
        for (auto cursor = ok::begin(range);
             ok::is_inbounds(range, cursor, ok::prefer_after_bounds_check);
             ok::increment(range, cursor)) {
            // perform a copy either through
            new (memory + i) T(ok::iter_get_temporary_ref(range, cursor));
            ++i;
        }

        return array_list_t(M{
            .allocated_spots = raw_slice(*reinterpret_cast<T*>(memory),
                                         bytes_allocated / sizeof(T)),
            .spots_occupied = num_items,
            .backing_allocator = allocator,
        });
    }

    /// Append a new item and construct it with the given args. Return the
    /// status of the allocation.
    template <typename... args_t>
    [[nodiscard]] constexpr status_t<alloc::error>
    append(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_constructible_v<T, args_t...>,
                      "Constructor invoked by array_list_t::append may throw.");

        // if else handles initial allocation and then future reallocation
        if (!m.allocated_spots) {
            alloc::result_t<maybe_defined_memory_t> res =
                m.backing_allocator.allocate(alloc::request_t{
                    .num_bytes = sizeof(T) * 4,
                    .alignment = alignof(T),
                    .flags = alloc::flags::leave_nonzeroed,
                });

            if (!res.okay()) [[unlikely]] {
                return res.err();
            }

            auto& maybe_defined = res.release_ref();
            uint8_t* memory = maybe_defined.data_maybe_defined();
            const size_t bytes_allocated = maybe_defined.size();

            const size_t spots_allocated = bytes_allocated / sizeof(T);

            m.allocated_spots = raw_slice(*memory, spots_allocated);
        } else if (slice_t<T>& spots = m.allocated_spots.value();
                   spots.size() <= m.spots_occupied) {
            // realloc case
            using namespace alloc;
            const auto realloc_flags =
                flags::leave_nonzeroed | flags::expand_back;

            if constexpr (!std::is_trivially_copyable_v<T>) {
                // if we're not trivially copyable, dont let the allocator do
                // the memcpying, we will do it ourselves after

                result_t<potentially_in_place_reallocation_t> res =
                    reallocate_in_place_orelse_keep_old_nocopy(
                        m.backing_allocator,
                        reallocate_extended_request_t{
                            .required_bytes_back = sizeof(T),
                            // 2x grow rate
                            .preferred_bytes_back = sizeof(T) * spots.size(),
                            .memory = reinterpret_as_bytes(spots),
                            .flags =
                                realloc_flags | flags::in_place_orelse_fail,
                        });

                if (!res.okay()) [[unlikely]] {
                    return res.err();
                }

                auto& reallocation = res.release_ref();
                if (reallocation.was_in_place) {
                    // sanity checks
                    __ok_assert(
                        spots.data() ==
                            reinterpret_cast<T*>(reallocation.memory.data()),
                        "Reallocation was supposedly in-place, but returned a "
                        "different pointer.");
                    __ok_assert(
                        reallocation.bytes_offset_front == 0,
                        "No front reallocation was done in array_list_t but "
                        "some byte offset was given by allocator.");

                    spots = raw_slice(
                        *reinterpret_cast<T*>(reallocation.memory.data()),
                        reallocation.memory.size() / sizeof(T));
                } else {
                    // perform copy
                    const T* const src = spots.data();
                    T* const dest =
                        reinterpret_cast<T*>(reallocation.memory.data());

                    for (size_t i = 0; i < m.spots_occupied; ++i) {
                        new (dest + i) T(std::move(src[i]));
                    }

                    // free old allocation
                    m.backing_allocator.deallocate(reinterpret_as_bytes(spots));

                    spots = raw_slice(*dest,
                                      reallocation.memory.size() / sizeof(T));
                }
            } else {
                result_t<maybe_defined_memory_t> res =
                    m.backing_allocator.reallocate(reallocate_request_t{
                        .new_size_bytes = (spots.size() + 1) * sizeof(T),
                        .preferred_size_bytes = (spots.size() * 2) * sizeof(T),
                        .flags = realloc_flags,
                    });

                if (!res.okay()) [[unlikely]] {
                    return res.err();
                }

                auto& maybe_defined = res.release_ref();
                T* mem =
                    reinterpret_cast<T*>(maybe_defined.data_maybe_defined());
                const size_t bytes_allocated = maybe_defined.size();

                m.allocated_spots =
                    raw_slice(*mem, bytes_allocated / sizeof(T));
            }
        }

        auto& spots = m.allocated_spots.value();

        // this fires if the allocator given to this array_list is misbehaving
        if (!spots.size() > m.spots_occupied) [[unlikely]] {
            __ok_assert(
                false, "Allocator did not give back expected amount of memory");
            return alloc::error::oom;
        }

        // populate item
        new (spots.data() + m.spots_occupied) T(std::forward<args_t>(args)...);
        ++m.spots_occupied;
        return alloc::error::okay;
    }

    inline ~array_list_t() { destroy(); }

  private:
    M m;
};
} // namespace ok

#endif
