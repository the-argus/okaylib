#ifndef __OKAYLIB_CONTAINERS_ARRAY_LIST_H__
#define __OKAYLIB_CONTAINERS_ARRAY_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/status.h"

namespace ok {

namespace arraylist {
namespace detail {
template <typename T> struct empty_t;
struct copy_items_from_range_t;
template <typename T> struct spots_preallocated_t;
} // namespace detail
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
        opt<slice<T>> allocated_spots;
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
    friend class arraylist::detail::spots_preallocated_t<T>;
    friend class arraylist::detail::copy_items_from_range_t;
    friend class arraylist::detail::empty_t<T>;
    static_assert(!std::is_reference_v<T>,
                  "arraylist_t cannot store references.");
    static_assert(!std::is_void_v<T>, "cannot create an array list of void.");
    static_assert(
        std::is_trivially_copyable_v<T> || std::is_move_constructible_v<T> ||
            is_std_constructible_v<T, T&&>,
        "Type given to arraylist_t must be either trivially copyable or move "
        "constructible, otherwise it cannot move the items when reallocating.");

    using value_type = T;

    // data and size are here so this can be iterated on directly without
    // calling .items()

    /// Panics if the arraylist has not allocated anything yet
    const T* data() const& OKAYLIB_NOEXCEPT
    {
        return m.allocated_spots.ref_or_panic().data();
    }

    /// Panics if the arraylist has not allocated anything yet
    T* data() & OKAYLIB_NOEXCEPT
    {
        return m.allocated_spots.ref_or_panic().data();
    }

    /// Always valid to call .size() on any arraylist
    size_t size() const OKAYLIB_NOEXCEPT { return m.spots_occupied; }

    slice<T> items() & OKAYLIB_NOEXCEPT
    {
        return m.allocated_spots.ref_or_panic().subslice(
            {.length = m.spots_occupied});
    }

    slice<const T> items() const& OKAYLIB_NOEXCEPT
    {
        return const_cast<arraylist_t*>(this)->items(); // call other impl
    }

    const T& operator[](size_t index) const& OKAYLIB_NOEXCEPT
    {
        if (index >= m.spots_occupied) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::arraylist_t");
        }
        return m.allocated_spots.ref_or_panic().data()[index];
    }
    T& operator[](size_t index) & OKAYLIB_NOEXCEPT
    {
        if (index >= m.spots_occupied) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::arraylist_t");
        }
        return m.allocated_spots.ref_or_panic().data()[index];
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

    /// Append a new item and return either a status representing whether the
    /// allocation succeeded or T's error type. The new item is constructed
    /// using the constructor selected by ok::make() with the given args.
    template <typename... args_t>
    constexpr auto append(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        // if else handles initial allocation and then future reallocation
        if (!m.allocated_spots) {
            first_allocation();
        } else if (slice<T>& spots = m.allocated_spots.ref_or_panic();
                   spots.size() <= m.spots_occupied) {
            reallocate(spots);
        }

        auto& spots = m.allocated_spots.ref_or_panic();
        using make_result_type = decltype(ok::make_into_uninitialized<T>(
            std::declval<T&>(), std::forward<args_t>(args)...));
        constexpr bool returns_result = !std::is_void_v<make_result_type>;

        // this fires if the allocator given to this array_list is misbehaving
        // or OOMed
        if (spots.size() <= m.spots_occupied) [[unlikely]] {
            __ok_assert(
                false, "Allocator did not give back expected amount of memory");
            if constexpr (returns_result) {
                static_assert(detail::is_instance_v<make_result_type, status>);
                using enum_t = typename make_result_type::enum_type;
                static_assert(
                    std::is_convertible_v<alloc::error, enum_t>,
                    "In order to use a potentially failing constructor with "
                    "arraylist_t::append(), the constructor's enum type must "
                    "define a conversion from alloc::error.");
                return status(enum_t(alloc::error::oom));
            } else {
                return status(alloc::error::oom);
            }
        }

        // populate item

        auto& uninit = spots[m.spots_occupied];

        if constexpr (returns_result) {
            auto result = ok::make_into_uninitialized<T>(
                uninit, std::forward<args_t>(args)...);

            // only add to occupied spots if the construction was successful
            if (result.okay()) [[likely]] {
                ++m.spots_occupied;
            }
            using enum_t = typename make_result_type::enum_type;
            return status(enum_t(result.err()));
        } else {
            ok::make_into_uninitialized<T>(uninit,
                                           std::forward<args_t>(args)...);
            ++m.spots_occupied;
            return status(alloc::error::okay);
        }
    }

    inline ~arraylist_t() { destroy(); }

  private:
    constexpr status<alloc::error> first_allocation()
    {
        alloc::result_t<bytes_t> res =
            m.backing_allocator->allocate(alloc::request_t{
                .num_bytes = sizeof(T) * 4,
                .alignment = alignof(T),
                .flags = alloc::flags::leave_nonzeroed,
            });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        auto& maybe_defined = res.release_ref();
        uint8_t* memory = maybe_defined.data();
        const size_t bytes_allocated = maybe_defined.size();

        const size_t spots_allocated = bytes_allocated / sizeof(T);

        m.allocated_spots =
            raw_slice(*reinterpret_cast<T*>(memory), spots_allocated);
        return alloc::error::okay;
    }

    constexpr status<alloc::error> reallocate(slice<T>& spots)
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

        result_t<bytes_t> res =
            m.backing_allocator->reallocate(reallocate_request_t{
                .memory = reinterpret_as_bytes(spots),
                .new_size_bytes = (spots.size() + 1) * sizeof(T),
                .preferred_size_bytes = (spots.size() * 2) * sizeof(T),
                .flags = realloc_flags,
            });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        auto& bytes = res.release_ref();
        T* mem = reinterpret_cast<T*>(bytes.data());
        const size_t bytes_allocated = bytes.size();

        spots = raw_slice(*mem, bytes_allocated / sizeof(T));
        return alloc::error::okay;
    }

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (m.allocated_spots) {
            if (!std::is_trivially_destructible_v<T>) {
                T* mem = m.allocated_spots.ref_or_panic().data();
                for (size_t i = 0; i < m.spots_occupied; ++i) {
                    mem[i].~T();
                }
            }

            m.backing_allocator->deallocate(
                reinterpret_as_bytes(m.allocated_spots.ref_or_panic()));
        }
    }
};

// nested templates here to defer instantiation of uninitialized_storage_t until
// arraylist is done being defined
// template <typename T, typename backing_allocator_t>
// template <typename U>

namespace arraylist {

namespace detail {
template <typename T> struct empty_t
{
    template <typename allocator_impl_t>
    [[nodiscard]] constexpr arraylist_t<T, allocator_impl_t>
    operator()(allocator_impl_t& allocator) const noexcept
    {
        return typename arraylist_t<T, allocator_impl_t>::members_t{
            .allocated_spots = ok::nullopt,
            .spots_occupied = 0,
            .backing_allocator = ok::addressof(allocator),
        };
    }
};

template <typename T> struct spots_preallocated_t
{
    template <typename allocator_impl_t>
    static ok::arraylist_t<T, allocator_impl_t>
    associated_type_deducer(allocator_impl_t& allocator, size_t);

    // provide type deduction if you want to use ok::make
    template <typename... args_t>
    using associated_type =
        decltype(associated_type_deducer(std::declval<args_t>()...));

    template <typename allocator_impl_t>
    [[nodiscard]] constexpr auto
    make(allocator_impl_t& allocator,
         size_t num_spots_preallocated) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, num_spots_preallocated);
    }

    template <typename allocator_impl_t>
    [[nodiscard]] constexpr status<alloc::error>
    operator()(arraylist_t<T, allocator_impl_t>& output,
               allocator_impl_t& allocator,
               size_t num_spots_preallocated) const OKAYLIB_NOEXCEPT
    {
        using output_t = arraylist_t<T, allocator_impl_t>;
        auto res = allocator.allocate(alloc::request_t{
            .num_bytes = sizeof(T) * num_spots_preallocated,
            .alignment = alignof(T),
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        bytes_t& bytes = res.release_ref();
        auto& start = *reinterpret_cast<T*>(bytes.data());
        const size_t num_bytes_allocated = bytes.size();

        new (ok::addressof(output)) output_t(typename output_t::members_t{
            .allocated_spots =
                ok::raw_slice(start, num_bytes_allocated / sizeof(T)),
            .spots_occupied = 0,
            .backing_allocator = ok::addressof(allocator),
        });
        return alloc::error::okay;
    }
};

struct copy_items_from_range_t
{
    // has to be public apparently, since its instantiated from another class
    template <typename allocator_impl_t, typename input_range_t>
    static ok::arraylist_t<value_type_for<const input_range_t&>,
                           allocator_impl_t>
    associated_type_deducer(allocator_impl_t& allocator,
                            const input_range_t& range);

    // provide type deduction if you want to use ok::make
    template <typename... args_t>
    using associated_type =
        decltype(associated_type_deducer(std::declval<args_t>()...));

    template <typename allocator_impl_t, typename input_range_t>
    [[nodiscard]] constexpr auto
    make(allocator_impl_t& allocator,
         const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, range);
    }

    template <typename allocator_impl_t, typename input_range_t>
    [[nodiscard]] constexpr status<alloc::error>
    operator()(ok::arraylist_t<value_type_for<const input_range_t&>,
                               allocator_impl_t>& output,
               allocator_impl_t& allocator,
               const input_range_t& range) const OKAYLIB_NOEXCEPT
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

        alloc::result_t<bytes_t> res = allocator.allocate(alloc::request_t{
            .num_bytes = num_items * sizeof(T),
            .alignment = alignof(T),
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        auto& bytes = res.release_ref();
        T* const memory = reinterpret_cast<T*>(bytes.data());
        const size_t bytes_allocated = bytes.size();

        // TODO: if contiguous range and trivially copyable, do memcpy
        size_t i = 0;
        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            // perform a copy either through
            const auto& item = ok::iter_get_temporary_ref(range, cursor);
            new (memory + i) T(item);
            ++i;
        }

        new (ok::addressof(output)) output_t(typename output_t::members_t{
            .allocated_spots = raw_slice(*reinterpret_cast<T*>(memory),
                                         bytes_allocated / sizeof(T)),
            .spots_occupied = num_items,
            .backing_allocator = ok::addressof(allocator),
        });

        return alloc::error::okay;
    };
};

} // namespace detail

template <typename T> inline constexpr detail::empty_t<T> empty;

template <typename T>
inline constexpr detail::spots_preallocated_t<T> spots_preallocated;

inline constexpr detail::copy_items_from_range_t copy_items_from_range;

}; // namespace arraylist

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

} // namespace ok

#endif
