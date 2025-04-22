#ifndef __OKAYLIB_CONTAINERS_ARRAY_LIST_H__
#define __OKAYLIB_CONTAINERS_ARRAY_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/defer.h"
#include "okay/ranges/ranges.h"
#include "okay/status.h"

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

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
        T* items;
        size_t capacity;
        size_t size;
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
#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<arraylist_t>;
#endif

    static_assert(!std::is_reference_v<T>,
                  "arraylist_t cannot store references.");
    static_assert(!std::is_void_v<T>, "cannot create an array list of void.");
    static_assert(
        std::is_trivially_copyable_v<T> || std::is_move_constructible_v<T> ||
            is_std_constructible_v<T, T&&>,
        "Type given to arraylist_t must be either trivially copyable or move "
        "constructible, otherwise it cannot move the items when reallocating.");
    static_assert(!std::is_const_v<T>,
                  "Attempt to create an arraylist with const objects, which is "
                  "not possible. Remove the const, and consider passing a "
                  "const reference to the arraylist instead.");

    using viewed_type = T;

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return m.size;
    }

    [[nodiscard]] constexpr slice<T> items() & OKAYLIB_NOEXCEPT
    {
        if (this->size() == 0) {
            return make_null_slice<T>();
        }
        return raw_slice(*m.items, m.size);
    }

    [[nodiscard]] constexpr slice<const T> items() const& OKAYLIB_NOEXCEPT
    {
        return const_cast<arraylist_t*>(this)->items(); // call nonconst impl
    }

    [[nodiscard]] constexpr const T&
    operator[](size_t index) const& OKAYLIB_NOEXCEPT
    {
        if (index >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::arraylist_t");
        }
        return m.items[index];
    }
    [[nodiscard]] constexpr T& operator[](size_t index) & OKAYLIB_NOEXCEPT
    {
        if (index >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to ok::arraylist_t");
        }
        return m.items[index];
    }

    // no default constructor or copy, but can move
    arraylist_t(const arraylist_t&) = delete;
    arraylist_t& operator=(const arraylist_t&) = delete;

    constexpr arraylist_t(arraylist_t&& other) : m(other.m) OKAYLIB_NOEXCEPT
    {
        // prevent other from destroying our buffer when it goes, but keep it in
        // a valid state
        other.items = nullptr;
        other.size = 0;
        other.capacity = 0;
    }

    constexpr arraylist_t& operator=(arraylist_t&& other) OKAYLIB_NOEXCEPT
    {
        this->clear();
        std::swap(other.m, this->m);
        __ok_internal_assert(other.size() == 0);
        return *this;
    }

    /// If there is not space for another item, reallocate.
    [[nodiscard]] constexpr status<alloc::error>
    ensure_additional_capacity() OKAYLIB_NOEXCEPT
    {
        if (this->capacity() == 0) {
            auto status = this->make_first_allocation();
            if (!status.okay()) [[unlikely]] {
                return status;
            }
        } else if (this->capacity() <= this->size()) {
            // 2x growth rate
            auto status =
                this->reallocate(raw_slice(*m.items, this->capacity()),
                                 sizeof(T), this->capacity() * sizeof(T));
            if (!status.okay()) [[unlikely]] {
                return status;
            }
        }
        return alloc::error::okay;
    }

    /// Returns error describing any potential failure due to allocation, but
    /// just aborts if called with an out of bound index.
    template <typename... args_t>
    [[nodiscard]] constexpr auto insert_at(const size_t idx,
                                           args_t&&... args) OKAYLIB_NOEXCEPT
    {
        // bounds check out of the gate
        if (idx > this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to arraylist in insert_at.");
        }

        // if else handles initial allocation and then future reallocation
        {
            auto status = this->ensure_additional_capacity();
            if (!status.okay()) [[unlikely]] {
                return status;
            }
        }

        // make sure we have one free spot available for the thing being
        // inserted
        __ok_assert(this->capacity() > this->size(),
                    "Backing allocator for arraylist did not give back "
                    "expected amount of memory");

        if (idx < this->size()) {
            // move all other items towards the back of the arraylist
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(m.items + idx + 1, m.items + idx,
                             (this->size() - idx) * sizeof(T));
            } else {
                __ok_internal_assert(this->size() != 0);
                // move last item into uninitialized memory
                new (m.items + this->size())
                    T(std::move(m.items[this->size() - 1]));

                // move the rest of the items on top of each other
                for (size_t i = this->size() - 1; i > idx; --i) {
                    T& target = m.items[i];
                    T& source = m.items[i - 1];
                    target = std::move(source);
                }

                // make the item we are inserting into uninitialized
                // since we are inserting a new thing over it anyways,
                // there's not way to benefit from anything like leftover memory
                // after move optimizations
                if constexpr (!std::is_trivially_destructible_v<T>) {
                    m.items[idx].~T();
                }
            }
        }

        // populate moved-out item
        auto& uninit = m.items[idx];

        using make_result_type = decltype(ok::make_into_uninitialized<T>(
            std::declval<T&>(), std::forward<args_t>(args)...));
        if constexpr (!std::is_void_v<make_result_type>) {
            using enum_t = typename make_result_type::enum_type;
            static_assert(
                std::is_convertible_v<alloc::error, enum_t>,
                "In order to use a potentially failing constructor with "
                "arraylist_t::append(), the constructor's enum type must "
                "define a conversion from alloc::error.");
            auto result = ok::make_into_uninitialized<T>(
                uninit, std::forward<args_t>(args)...);

            // only add to occupied spots if the construction was successful
            if (result.okay()) [[likely]] {
                ++m.spots_occupied;
            } else {
                // move all other items BACK to where they were before (this is
                // supposed to be the cold path and it only invokes nonfailing
                // operations so it should be fine to do this)
                if constexpr (std::is_trivially_copyable_v<T>) {
                    std::memmove(m.items + idx, m.items + idx + 1,
                                 (this->size() - idx) * sizeof(T));
                } else {
                    // this accesses spots[i + 1] but that is initialized
                    // at this point
                    for (size_t i = idx; i < this->size(); ++i) {
                        T& target = m.items[i];
                        T& source = m.items[i + 1];
                        target = std::move(source);
                    }
                    // make the stuff off the end uninitialized again
                    if constexpr (!std::is_trivially_destructible_v<T>) {
                        m.items[this->size()].~T();
                    }
                }
            }
            return status(enum_t(result.err()));
        } else {
            ok::make_into_uninitialized<T>(uninit,
                                           std::forward<args_t>(args)...);
            ++m.spots_occupied;
            return status(alloc::error::okay);
        }
    }

    [[nodiscard]] constexpr status<alloc::error>
    increase_capacity_by_at_least(size_t new_spots) OKAYLIB_NOEXCEPT
    {
        if (new_spots == 0) [[unlikely]] {
            // TODO: can we just guarantee that all allocators do this?
            __ok_assert(false, "Attempt to increase capacity by 0.");
            return alloc::error::unsupported;
        }
        if (this->capacity() == 0) {
            return make_first_allocation(new_spots * sizeof(T));
        } else {
            return reallocate(new_spots * sizeof(T), 0);
        }
    }

    constexpr T remove(size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access in arraylist_t::remove()");
        }
        T& removed = m.items[idx];
        // moved out at index
        T out(std::move(removed));

        // we would have crashed at this point if out of bounds, so assume idx
        // is in bounds

        defer decrement([this] { --m.spots_occupied; });

        if (idx == this->size() - 1) {
            // we've left `removed` in a valid state because we thought we were
            // going to shift stuff above it down into it, but there's nothing
            // above so just destroy it.
            if constexpr (!std::is_trivially_destructible_v<T>) {
                removed.~T();
            }
            return out;
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            const size_t idxplusone = idx + 1;
            std::memmove(m.items + idx, m.items + idxplusone,
                         (this->size() - idxplusone) * sizeof(T));
        } else {
            for (size_t i = this->size() - 1; i > idx; --i) {
                T& source = m.items[i];
                T& target = m.items[i - 1];
                target = std::move(source);
            }

            // only call the destructor of the last-most item
            if constexpr (!std::is_trivially_destructible_v<T>) {
                m.items[this->size() - 1].~T();
            }
        }
        return out;
    }

    constexpr T remove_and_swap_last(size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort(
                "Out of bounds access in arraylist_t::remove_and_swap_last()");
        }
        T& target = m.items[idx];
        // moved out at index
        T out(std::move(target));

        defer decrement([this, target] { --m.spots_occupied; });

        if (idx == this->size() - 1) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                target.~T();
            }
            return out;
        }

        T& last = m.items[this->size() - 1];

        target = std::move(last);

        if constexpr (!std::is_trivially_destructible_v<T>) {
            last.~T();
        }

        return out;
    }

    constexpr void shrink_to_reclaim_unused_memory() OKAYLIB_NOEXCEPT
    {
        // allocator cant reclaim shrunk memory anyways
        if (!(m.backing_allocator->features() &
              alloc::feature_flags::can_reclaim)) {
            return;
        }

        bytes_t bytes =
            reinterpret_as_bytes(raw_slice(*m.items, this->capacity()));

        if (this->size() == 0) {
            m.backing_allocator->deallocate(bytes);
            m.items = nullptr;
            m.capacity = 0;
            m.size = 0;
            return;
        }

        alloc::result_t<bytes_t> reallocated =
            m.backing_allocator->reallocate(alloc::reallocate_request_t{
                .memory = bytes,
                .new_size_bytes = size() * sizeof(T),
                // flags besides in_place_orelse_fail not really necessary
                .flags = alloc::flags::shrink_back |
                         alloc::flags::in_place_orelse_fail |
                         alloc::flags::leave_nonzeroed,
            });

        if (!reallocated.okay()) [[unlikely]]
            return;

        bytes_t& new_bytes = reallocated.release_ref();
        __ok_assert((void*)new_bytes.unchecked_address_of_first_item() ==
                        m.items,
                    "Backing allocator for arraylist did not reallocate "
                    "properly, different memory returned but shrink_back and "
                    "in_place_orelse_fail were passed.");
        __ok_assert(
            new_bytes.size() == size() * sizeof(T),
            "Shrinking / rellocating did not return expected size exactly, "
            "which it is supposed to when shrinking in place.");

        // this should never happen considering the above asserts...
        __ok_assert(uintptr_t(new_bytes.unchecked_address_of_first_item()) %
                            alignof(T) ==
                        0,
                    "Misaligned bytes?");

        m.items =
            reinterpret_cast<T*>(new_bytes.unchecked_address_of_first_item());
        m.capacity = new_bytes.size() / sizeof(T);
    }

    [[nodiscard]] constexpr size_t capacity() const OKAYLIB_NOEXCEPT
    {
        return m.capacity;
    }

    [[nodiscard]] constexpr bool is_empty() const OKAYLIB_NOEXCEPT
    {
        return this->size() == 0;
    }

    constexpr opt<T> pop_last() OKAYLIB_NOEXCEPT
    {
        if (is_empty())
            return {};
        return remove(this->size() - 1);
    }

    [[nodiscard]] constexpr backing_allocator_t&
    allocator() const OKAYLIB_NOEXCEPT
    {
        return *m.backing_allocator;
    }

    constexpr void clear() OKAYLIB_NOEXCEPT
    {
        if (this->capacity() == 0) [[unlikely]]
            return;

        this->call_destructor_on_all_items();

        m.size = 0;
    }

    /// Does not reclaim unused memory when shrinking
    /// If type stored is trivially constructible, then new memory is zeroed
    /// Only possible if T is default constructible.
    /// Args `args` must select nonfailing constructor.
    template <typename... args_t>
    [[nodiscard]] constexpr auto resize(size_t new_size,
                                        args_t&&... args) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<is_infallible_constructible_v<T, args_t...>,
                            status<alloc::error>>
    {
        if (this->size() == new_size) [[unlikely]] {
            return alloc::error::okay;
        }

        if (new_size == 0) [[unlikely]] {
            clear();
            return alloc::error::okay;
        }

        if (this->capacity() == 0) {
            auto status = make_first_allocation(new_size * sizeof(T));
            if (!status.okay()) {
                return status;
            }
            __ok_assert(this->capacity() >= new_size,
                        "Allocator did not return enough memory to arraylist");
            if constexpr (std::is_trivially_default_constructible_v<T> &&
                          sizeof...(args_t) == 0) {
                std::memset(m.items, 0, sizeof(T) * new_size);
            } else {
                for (size_t i = 0; i < new_size; ++i) {
                    ok::make_into_uninitialized<T>(
                        m.items[i], std::forward<args_t>(args)...);
                }
            }
            m.size = new_size;
            return alloc::error::okay;
        }

        const bool shrinking = this->size() > new_size;

        if (shrinking) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = new_size; i < this->size(); ++i) {
                    m.items[i].~T();
                }
            }
            m.size = new_size;
        } else {
            // growing amount
            if (capacity() < new_size) {
                status<alloc::error> status =
                    reallocate((new_size - capacity()) * sizeof(T), 0);

                if (!status.okay())
                    return status;
            }

            // reallocation worked, right?
            __ok_internal_assert(this->size() >= new_size);

            // initialize new elements
            if constexpr (std::is_trivially_default_constructible_v<T> &&
                          sizeof...(args_t) == 0) {
                // manually zero memory
                // TODO: some ifdef here maybe to avoid zeroing based on build
                // option? or a template param?
                std::memset(m.items + this->size(), 0,
                            (new_size - this->size()) * sizeof(T));
            } else {
                for (size_t i = this->size(); i < new_size; ++i) {
                    ok::make_into_uninitialized<T>(
                        m.items[i], std::forward<args_t>(args)...);
                }
            }

            m.size = new_size;
        }

        return alloc::error::okay;
    }

    /// the old shrink and leak. the shrinky leaky
    [[nodiscard]] constexpr opt<slice<T>> shrink_and_leak() OKAYLIB_NOEXCEPT
    {
        shrink_to_reclaim_unused_memory();

        if (this->capacity() == 0) [[unlikely]] {
            __ok_internal_assert(m.size == 0);
            return nullopt;
        }

        opt out = raw_slice(*m.items, m.size);

        // we no longer own the memory
        m.items = nullptr;
        m.capacity = 0;
        m.size = 0;

        return out;
    }

    template <typename... args_t>
    constexpr auto append(args_t&&... args) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<
            is_constructible_v<T, args_t...>,
            // append may return the error type from an erroring constructor
            decltype(insert_at(this->size(), std::forward<args_t>(args)...))>
    {
        return insert_at(this->size(), std::forward<args_t>(args)...);
    }

    /// Returns an error only if allocation to expand space for the new items
    /// errored.
    template <typename other_range_t>
    [[nodiscard]] constexpr status<alloc::error>
    append_range(const other_range_t& range) OKAYLIB_NOEXCEPT
    {
        static_assert(
            is_infallible_constructible_v<T, value_type_for<other_range_t>>,
            "Cannot append the given range: the contents of the arraylist "
            "cannot be constructed from the contents of the given range (at "
            "least not without a potential error at each construction).");
        static_assert(!detail::range_marked_infinite_v<other_range_t>,
                      "Cannot append an infinite range.");

        __ok_internal_assert(this->capacity() >= this->size());

        if constexpr (detail::range_impls_size_v<other_range_t>) {
            const size_t size = ok::size(range);
            const size_t extra_space = this->capacity() - this->size();
            if (size > extra_space) {
                auto status = this->increase_capacity_by(size - extra_space);
                if (!status.okay()) [[unlikely]] {
                    return status;
                }
            }
        }

        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            auto status =
                this->append(ok::iter_get_temporary_ref(range, cursor));
            if constexpr (!detail::range_impls_size_v<other_range_t>) {
                if (!status.okay()) [[unlikely]] {
                    return status;
                }
            }
        }
        return alloc::error::okay;
    }

    inline ~arraylist_t() { destroy(); }

  private:
    [[nodiscard]] constexpr status<alloc::error>
    make_first_allocation(size_t initial_bytes = sizeof(T) * 4)
    {
        alloc::result_t<bytes_t> res =
            m.backing_allocator->allocate(alloc::request_t{
                .num_bytes = initial_bytes,
                .alignment = alignof(T),
                .flags = alloc::flags::leave_nonzeroed,
            });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        bytes_t& memory = res.release_ref();
        m.items =
            reinterpret_cast<T*>(memory.unchecked_address_of_first_item());
        m.capacity = memory.size() / sizeof(T);

        return alloc::error::okay;
    }

    /// "spots" is the currently allocated spots, including uninitialized
    /// capacity. it is modified by this function
    [[nodiscard]] constexpr status<alloc::error>
    reallocate(size_t required_bytes, size_t preferred_bytes)
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
                        .memory = reinterpret_as_bytes(
                            raw_slice(*m.items, this->capacity())),
                        .required_bytes_back = required_bytes,
                        .preferred_bytes_back = preferred_bytes,
                        .flags = realloc_flags | flags::in_place_orelse_fail,
                    });

            if (!res.okay()) [[unlikely]] {
                return res.err();
            }

            auto& reallocation = res.release_ref();
            if (reallocation.was_in_place) {
                // sanity checks
                __ok_assert(
                    m.items == reinterpret_cast<T*>(
                                   reallocation.memory
                                       .unchecked_address_of_first_item()),
                    "Reallocation was supposedly in-place, but returned a "
                    "different pointer.");
                __ok_assert(reallocation.bytes_offset_front == 0,
                            "No front reallocation was done in arraylist_t but "
                            "some byte offset was given by allocator.");

                m.capacity = reallocation.memory.size() / sizeof(T);
            } else {
                // perform move
                T* const src = m.items;
                T* const dest = reinterpret_cast<T*>(
                    reallocation.memory.unchecked_address_of_first_item());

                if constexpr (std::is_trivially_copyable_v<T>) {
                    std::memcpy((void*)dest, (void*)src,
                                m.spots_occupied * sizeof(T));
                } else {
                    for (size_t i = 0; i < m.spots_occupied; ++i) {
                        T& src_item = src[i];
                        new (dest + i) T(std::move(src_item));
                        if constexpr (!std::is_trivially_destructible_v<T>) {
                            src_item.~T();
                        }
                    }
                }

                // free old allocation
                m.backing_allocator->deallocate(
                    reinterpret_as_bytes(raw_slice(*m.items, m.capacity)));

                m.items = dest;
                m.capacity = reallocation.memory.size() / sizeof(T);
            }
            return alloc::error::okay;
        } else {
            const size_t capacity_bytes = this->capacity() * sizeof(T);
            result_t<bytes_t> res =
                m.backing_allocator->reallocate(reallocate_request_t{
                    .memory = reinterpret_as_bytes(
                        raw_slice(*m.items, this->capacity())),
                    .new_size_bytes = capacity_bytes + required_bytes,
                    .preferred_size_bytes =
                        preferred_bytes == 0 ? 0
                                             : capacity_bytes + preferred_bytes,
                    .flags = realloc_flags,
                });

            if (!res.okay()) [[unlikely]] {
                return res.err();
            }

            bytes_t& bytes = res.release_ref();

            m.items =
                reinterpret_cast<T*>(bytes.unchecked_address_of_first_item());
            m.capacity = bytes.size() / sizeof(T);
            return alloc::error::okay;
        }
    }

    constexpr void call_destructor_on_all_items() OKAYLIB_NOEXCEPT
    {
        __ok_internal_assert(this->capacity() > 0);
        if (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < m.spots_occupied; ++i) {
                m.items[i].~T();
            }
        }
    }

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (this->capacity() == 0) {
            return;
        }

        this->call_destructor_on_all_items();

        m.backing_allocator->deallocate(
            reinterpret_as_bytes(raw_slice(*m.items, m.capacity)));
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
    template <typename backing_allocator_t>
    [[nodiscard]] constexpr arraylist_t<T, backing_allocator_t>
    operator()(backing_allocator_t& allocator) const noexcept
    {
        return typename arraylist_t<T, backing_allocator_t>::members_t{
            .items = nullptr,
            .capacity = 0,
            .size = 0,
            .backing_allocator = 0,
        };
    }
};

template <typename T> struct spots_preallocated_t
{
    template <typename backing_allocator_t, typename...>
    using associated_type =
        ok::arraylist_t<T, ok::detail::remove_cvref_t<backing_allocator_t>>;

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               size_t num_spots_preallocated) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, num_spots_preallocated);
    }

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(arraylist_t<T, backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     size_t num_spots_preallocated) const OKAYLIB_NOEXCEPT
    {
        using output_t = arraylist_t<T, backing_allocator_t>;
        auto res = allocator.allocate(alloc::request_t{
            .num_bytes = sizeof(T) * num_spots_preallocated,
            .alignment = alignof(T),
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        bytes_t& bytes = res.release_ref();
        T* start =
            reinterpret_cast<T*>(bytes.unchecked_address_of_first_item());
        const size_t num_bytes_allocated = bytes.size();

        new (ok::addressof(output)) output_t(typename output_t::members_t{
            .items = start,
            .capacity = num_bytes_allocated / sizeof(T),
            .size = 0,
            .backing_allocator = ok::addressof(allocator),
        });
        return alloc::error::okay;
    }
};

struct copy_items_from_range_t
{
    template <typename backing_allocator_t, typename input_range_t>
    using associated_type =
        ok::arraylist_t<value_type_for<input_range_t>,
                        ok::detail::remove_cvref_t<backing_allocator_t>>;

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, range);
    }

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(ok::arraylist_t<value_type_for<const input_range_t&>,
                                     backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(ok::detail::range_can_size_v<input_range_t>,
                      "Size of range unknown, refusing to copy out its items "
                      "using arraylist::copy_items_from_range constructor.");
        using T = value_type_for<const input_range_t&>;
        using output_t = arraylist_t<T, backing_allocator_t>;

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
        T* const memory =
            reinterpret_cast<T*>(bytes.unchecked_address_of_first_item());
        const size_t bytes_allocated = bytes.size();

        // TODO: if contiguous range and trivially copyable, do memcpy
        size_t i = 0;
        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            new (memory + i) T(ok::iter_get_temporary_ref(range, cursor));
            ++i;
        }

        new (ok::addressof(output)) output_t(typename output_t::members_t{
            .items = reinterpret_cast<T*>(memory),
            .capacity = bytes_allocated / sizeof(T),
            .size = num_items,
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
} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename T, typename backing_allocator_t>
struct fmt::formatter<ok::arraylist_t<T, backing_allocator_t>>
{
    using formatted_type_t = ok::arraylist_t<T, backing_allocator_t>;
    static_assert(
        fmt::is_formattable<T>::value,
        "Attempt to format an arraylist whose items are not formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& arraylist,
                                    format_context& ctx) const
    {
        // TODO: use CTTI to include nice type names in print here
        fmt::format_to(ctx.out(), "[ ");

        for (size_t i = 0; i < arraylist.size(); ++i) {
            fmt::format_to(ctx.out(), "{} ", arraylist[i]);
        }

        return fmt::format_to(ctx.out(), "]");
    }
};
#endif

#endif
