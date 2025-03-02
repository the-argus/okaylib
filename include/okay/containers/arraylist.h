#ifndef __OKAYLIB_CONTAINERS_ARRAY_LIST_H__
#define __OKAYLIB_CONTAINERS_ARRAY_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/status.h"

namespace ok {
namespace arraylist {
template <typename T> struct empty;

struct copy_items_from_range;
} // namespace arraylist

template <typename T, typename backing_allocator_t = ok::allocator_t>
class arraylist_t
{
    // arraylist is 32 bytes on the stack:
    // - 8 bytes: number spots allocated / capacity
    // - 8 bytes: pointer to start of allocation
    // - 8 bytes: number of spots used / length
    // - 8 bytes: backing_allocator pointer
    struct members_t
    {
        opt_t<slice_t<T>> allocated_spots;
        size_t spots_occupied;
        backing_allocator_t* backing_allocator;
    };
    members_t m;

    constexpr arraylist_t(members_t&& members) OKAYLIB_NOEXCEPT
        : m(std::forward<members_t>(members))
    {
    }

    // for use in factory functions but not public API. this effectively creates
    // an uninitialized array list object (at least, invariants are not upheld)
    arraylist_t() = default;

  public:
    friend class arraylist::copy_items_from_range;
    friend class arraylist::empty<T>;
    static_assert(!std::is_reference_v<T>,
                  "arraylist_t cannot store references.");
    static_assert(!std::is_void_v<T>, "cannot create an array list of void.");
    static_assert(
        std::is_trivially_copyable_v<T> || std::is_move_constructible_v<T> ||
            is_std_constructible_v<T, T&&>,
        "Type given to arraylist_t must be either trivially copyable or move "
        "constructible, otherwise it cannot move the items when reallocating.");

    using value_type = T;

    const T* data() const& OKAYLIB_NOEXCEPT
    {
        return m.allocated_spots ? m.allocated_spots.value().data() : nullptr;
    }

    T* data() & OKAYLIB_NOEXCEPT
    {
        return m.allocated_spots ? m.allocated_spots.value().data() : nullptr;
    }

    size_t size() const OKAYLIB_NOEXCEPT { return m.spots_occupied; }

    const T& operator[](size_t index) const& OKAYLIB_NOEXCEPT
    {
        if (index >= m.spots_occupied) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::arraylist_t");
        }
        return m.allocated_spots.value().data()[index];
    }
    T& operator[](size_t index) & OKAYLIB_NOEXCEPT
    {
        if (index >= m.spots_occupied) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::arraylist_t");
        }
        return m.allocated_spots.value().data()[index];
    }

    // no default constructor or copy, but can move
    arraylist_t(const arraylist_t&) = delete;
    arraylist_t& operator=(const arraylist_t&) = delete;

    constexpr arraylist_t(arraylist_t&& other) : m(std::move(other.m))
    {
        other.m.allocated_spots = nullopt;
    }

    constexpr arraylist_t& operator=(arraylist_t&& other)
    {
        this->destroy();
        this->m = std::move(other.m);
        other.m.allocated_spots = nullopt;
        return *this;
    }

    /// Append a new item and return either a status_t representing whether the
    /// allocation succeeded or T's error type.
    template <typename constructor_t, typename... args_t>
    constexpr auto append(const constructor_t& constructor,
                          args_t&&... args) OKAYLIB_NOEXCEPT
    {
        // if else handles initial allocation and then future reallocation
        if (!m.allocated_spots) {
            first_allocation();
        } else if (slice_t<T>& spots = m.allocated_spots.value();
                   spots.size() <= m.spots_occupied) {
            reallocate(spots);
        }

        auto& spots = m.allocated_spots.value();
        using make_result_type = decltype(ok::make_into_uninitialized<T>(
            std::declval<T&>(), std::forward<args_t>(args)...));
        constexpr bool returns_result = !std::is_void_v<make_result_type>;

        // this fires if the allocator given to this array_list is misbehaving
        // or OOMed
        if (spots.size() <= m.spots_occupied) [[unlikely]] {
            __ok_assert(
                false, "Allocator did not give back expected amount of memory");
            if constexpr (returns_result) {
                return status_t(make_result_type(alloc::error::oom));
            } else {
                return status_t(alloc::error::oom);
            }
        }

        // populate item

        auto& uninit = spots[m.spots_occupied];

        if constexpr (std::is_invocable_v<constructor_t,
                                          detail::uninitialized_storage_t<T>&,
                                          args_t...>) {
            using return_type = decltype(constructor(
                std::declval<detail::uninitialized_storage_t<T>&>(),
                std::forward<args_t>(args)...));
            if constexpr (!std::is_void_v<return_type>) {
                static_assert(detail::is_instance_v<return_type, res_t>);
                using value_type = typename return_type::type;
                static_assert(!std::is_reference_v<value_type>);
            }
        } else if constexpr (std::is_invocable_v<constructor_t, args_t...>) {
            using return_type =
                decltype(constructor(std::forward<args_t>(args)...));
            static_assert(!std::is_reference_v<return_type>);
            static_assert(!std::is_void_v<return_type>);
        } else {
            static_assert(
                false,
                "No matching call found to constructor given to append().");
        }

        if constexpr (returns_result) {
            auto result = ok::make_into_uninitialized<T>(
                uninit, std::forward<args_t>(args)...);

            // only add to occupied spots if the construction was successful
            if (uint8_t(result) == 0) [[likely]] {
                ++m.spots_occupied;
            }
            return status_t(result);
        } else {
            ok::make_into_uninitialized<T>(uninit,
                                           std::forward<args_t>(args)...);
            ++m.spots_occupied;
            return status_t(alloc::error::okay);
        }
    }

    inline ~arraylist_t() { destroy(); }

  private:
    constexpr status_t<alloc::error> first_allocation()
    {
        alloc::result_t<maybe_defined_memory_t> res =
            m.backing_allocator->allocate(alloc::request_t{
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

        m.allocated_spots =
            raw_slice(*reinterpret_cast<T*>(memory), spots_allocated);
        return alloc::error::okay;
    }

    constexpr status_t<alloc::error> reallocate(slice_t<T>& spots)
    {
        using namespace alloc;
        const auto realloc_flags = flags::leave_nonzeroed | flags::expand_back;

        if constexpr (!std::is_trivially_copyable_v<T>) {
            // if we're not trivially copyable, dont let the allocator do
            // the memcpying, we will do it ourselves after

            result_t<potentially_in_place_reallocation_t> res =
                reallocate_in_place_orelse_keep_old_nocopy(
                    *m.backing_allocator,
                    reallocate_extended_request_t{
                        .required_bytes_back = sizeof(T),
                        // 2x grow rate
                        .preferred_bytes_back = sizeof(T) * spots.size(),
                        .memory = reinterpret_as_bytes(spots),
                        .flags = realloc_flags | flags::in_place_orelse_fail,
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
                __ok_assert(reallocation.bytes_offset_front == 0,
                            "No front reallocation was done in arraylist_t but "
                            "some byte offset was given by allocator.");

                spots =
                    raw_slice(*reinterpret_cast<T*>(reallocation.memory.data()),
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
                m.backing_allocator->deallocate(reinterpret_as_bytes(spots));

                spots =
                    raw_slice(*dest, reallocation.memory.size() / sizeof(T));
            }
        }

        result_t<maybe_defined_memory_t> res =
            m.backing_allocator->reallocate(reallocate_request_t{
                .memory = reinterpret_as_bytes(spots),
                .new_size_bytes = (spots.size() + 1) * sizeof(T),
                .preferred_size_bytes = (spots.size() * 2) * sizeof(T),
                .flags = realloc_flags,
            });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        auto& maybe_defined = res.release_ref();
        T* mem = reinterpret_cast<T*>(maybe_defined.data_maybe_defined());
        const size_t bytes_allocated = maybe_defined.size();

        spots = raw_slice(*mem, bytes_allocated / sizeof(T));
        return alloc::error::okay;
    }

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (m.allocated_spots) {
            if (!std::is_trivially_destructible_v<T>) {
                T* mem = m.allocated_spots.value().data();
                for (size_t i = 0; i < m.spots_occupied; ++i) {
                    mem[i].~T();
                }
            }

            m.backing_allocator->deallocate(
                reinterpret_as_bytes(m.allocated_spots.value()));
        }
    }
};

// nested templates here to defer instantiation of uninitialized_storage_t until
// arraylist is done being defined
// template <typename T, typename backing_allocator_t>
// template <typename U>
// constexpr auto arraylist_t<T, backing_allocator_t>::construct(
//     detail::uninitialized_storage_t<U>& output,
//     const spots_preallocated& options)
//     OKAYLIB_NOEXCEPT -> std::enable_if_t<std::is_same_v<arraylist_t, U>,
//                                          alloc::result_t<owning_ref<U>>>
// {
//     auto res = options.allocator.allocate(alloc::request_t{
//         .num_bytes = sizeof(T) * options.num_spots_preallocated,
//         .alignment = alignof(T),
//         .flags = alloc::flags::leave_nonzeroed,
//     });

//     if (!res.okay()) [[unlikely]] {
//         return res.err();
//     }

//     maybe_defined_memory_t& maybe_defined = res.release_ref();
//     auto& start = *reinterpret_cast<T*>(maybe_defined.data_maybe_defined());
//     const size_t num_bytes_allocated = maybe_defined.size();

//     new (ok::addressof(output.value)) arraylist_t(M{
//         .allocated_spots =
//             ok::raw_slice(start, num_bytes_allocated / sizeof(T)),
//         .spots_occupied = 0,
//         .backing_allocator = ok::addressof(options.allocator),
//     });
//     return output.value;
// }

namespace arraylist {

template <typename T> struct empty
{
    template <typename allocator_impl_t>
    constexpr arraylist_t<T, allocator_impl_t>
    operator()(allocator_impl_t& allocator) const noexcept
    {
        return typename arraylist_t<T, allocator_impl_t>::members_t{
            .allocated_spots = ok::nullopt,
            .spots_occupied = 0,
            .backing_allocator = ok::addressof(allocator),
        };
    }
};

struct copy_items_from_range
{
    template <typename allocator_impl_t, typename input_range_t,
              typename std::enable_if_t<
                  detail::is_output_range_v<input_range_t>, bool> = false>
    constexpr status_t<alloc::error> operator()(
        ok::arraylist_t<value_type_for<const input_range_t&>, allocator_impl_t>&
            output,
        allocator_impl_t& allocator, const input_range_t& range) const noexcept
    {
        static_assert(ok::detail::range_definition_has_size_v<input_range_t>,
                      "Size of range unknown, refusing to copy out its items "
                      "using arraylist::copy_items_from_range constructor.");
        using T = value_type_for<const input_range_t&>;
        using output_t = arraylist_t<T, allocator_impl_t>;

        // TODO: make this a warning
        __ok_assert(allocator.features() &
                        alloc::feature_flags::can_expand_back,
                    "Allocator given to arraylist_t cannot expand_back, which"
                    "will cause an error after appending some number of"
                    "elements.");

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
        T* const memory =
            reinterpret_cast<T*>(maybe_undefined.data_maybe_defined());
        const size_t bytes_allocated = maybe_undefined.size();

        // TODO: if contiguous range and trivially copyable, do memcpy
        size_t i = 0;
        for (auto cursor = ok::begin(range);
             ok::is_inbounds(range, cursor, ok::prefer_after_bounds_check);
             ok::increment(range, cursor)) {
            // perform a copy either through
            const auto& item = ok::iter_get_temporary_ref(range, cursor);
            new (memory + i) T(item);
            ++i;
        }

        new (ok::addressof(output)) arraylist_t(typename output_t::members_t{
            .allocated_spots = raw_slice(*reinterpret_cast<T*>(memory),
                                         bytes_allocated / sizeof(T)),
            .spots_occupied = num_items,
            .backing_allocator = ok::addressof(allocator),
        });

        return alloc::error::okay;
    };
};
};

// template <typename T, typename backing_allocator_t> struct empty
// {
//     static_assert(
//         detail::is_derived_from_v<backing_allocator_t, ok::allocator_t>);
//     using associated_type = arraylist_t<T, allocator_t>;
//     backing_allocator_t& allocator;

//     constexpr empty(backing_allocator_t& _allocator) noexcept
//         : allocator(_allocator)
//     {
//     }

//     constexpr associated_type operator()() noexcept { return {}; }
// };

// struct spots_preallocated
// {
//     using associated_type = arraylist_t;

//     backing_allocator_t& allocator;
//     size_t num_spots_preallocated;
// };

} // namespace arraylist

} // namespace ok

#endif
