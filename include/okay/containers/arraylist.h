#ifndef __OKAYLIB_CONTAINERS_ARRAY_LIST_H__
#define __OKAYLIB_CONTAINERS_ARRAY_LIST_H__

#include "okay/allocators/allocator.h"
#include "okay/defer.h"
#include "okay/error.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
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

    // for use in factory functions but not public API. this effectively creates
    // an uninitialized array list object (at least, invariants are not upheld)
    arraylist_t() = default;

  public:
    // this constructor should only be called by private implementations-
    // members_t is private
    constexpr arraylist_t(members_t&& members) OKAYLIB_NOEXCEPT
        : m(stdc::forward<members_t>(members))
    {
    }

    friend class arraylist::detail::spots_preallocated_t<T>;
    friend class arraylist::detail::copy_items_from_range_t;
    friend class arraylist::detail::empty_t<T>;
#if defined(OKAYLIB_USE_FMT)
    friend struct fmt::formatter<arraylist_t>;
#endif

    static_assert(!stdc::is_reference_c<T>,
                  "arraylist_t cannot store references.");
    static_assert(!stdc::is_void_v<T>, "cannot create an array list of void.");
    static_assert(
        stdc::is_trivially_copyable_v<T> || stdc::is_move_constructible_v<T> ||
            is_std_constructible_c<T, T&&>,
        "Type given to arraylist_t must be either trivially copyable or move "
        "constructible, otherwise it cannot move the items when reallocating.");
    static_assert(!is_const_c<T>,
                  "Attempt to create an arraylist with const objects, which is "
                  "not possible. Remove the const, and consider passing a "
                  "const reference to the arraylist instead.");

    using value_type = T;

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
        other.m.items = nullptr;
        other.m.size = 0;
        other.m.capacity = 0;
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
            if (!status.is_success()) [[unlikely]] {
                return status;
            }
        } else if (this->capacity() <= this->size()) {
            // 2x growth rate
            auto status =
                this->reallocate(sizeof(T), this->capacity() * sizeof(T));
            if (!status.is_success()) [[unlikely]] {
                return status;
            }
        }
        return alloc::error::success;
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
            if (!status.is_success()) [[unlikely]] {
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
            if constexpr (stdc::is_trivially_copyable_v<T>) {
                ::memmove(m.items + idx + 1, m.items + idx,
                          (this->size() - idx) * sizeof(T));
            } else {
                __ok_internal_assert(this->size() != 0);
                // move last item into uninitialized memory
                ok::stdc::construct_at(m.items + this->size(),
                                       stdc::move(m.items[this->size() - 1]));

                // move the rest of the items on top of each other
                for (size_t i = this->size() - 1; i > idx; --i) {
                    T& target = m.items[i];
                    T& source = m.items[i - 1];
                    target = stdc::move(source);
                }

                // make the item we are inserting into uninitialized
                // since we are inserting a new thing over it anyways,
                // there's not way to benefit from anything like leftover memory
                // after move optimizations
                if constexpr (!stdc::is_trivially_destructible_v<T>) {
                    m.items[idx].~T();
                }
            }
        }

        // populate moved-out item
        auto& uninit = m.items[idx];

        using enum_t = decltype(ok::make_into_uninitialized<T>(
            stdc::declval<T&>(), stdc::forward<args_t>(args)...));
        if constexpr (!stdc::is_void_v<enum_t>) {
            static_assert(
                is_convertible_to_c<alloc::error, enum_t>,
                "In order to use a potentially failing constructor with "
                "arraylist_t::append(), the constructor's enum type must "
                "define a conversion from alloc::error.");
            auto result = ok::make_into_uninitialized<T>(
                uninit, stdc::forward<args_t>(args)...);

            if (result == enum_t::success) [[likely]] {
                ++m.size;
            } else {
                // move all other items BACK to where they were before (this is
                // supposed to be the cold path and it only invokes nonfailing
                // operations so it should be fine to do this)
                if constexpr (stdc::is_trivially_copyable_v<T>) {
                    ::memmove(m.items + idx, m.items + idx + 1,
                              (this->size() - idx) * sizeof(T));
                } else {
                    // this accesses spots[i + 1] but that is initialized
                    // at this point
                    for (size_t i = idx; i < this->size(); ++i) {
                        T& target = m.items[i];
                        T& source = m.items[i + 1];
                        target = stdc::move(source);
                    }
                    // make the stuff off the end uninitialized again
                    if constexpr (!stdc::is_trivially_destructible_v<T>) {
                        m.items[this->size()].~T();
                    }
                }
            }
            return status(enum_t(result));
        } else {
            ok::make_into_uninitialized<T>(uninit,
                                           stdc::forward<args_t>(args)...);
            ++m.size;
            return status(alloc::error::success);
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
        T out(stdc::move(removed));

        defer decrement([this] { --m.size; });

        if (idx == this->size() - 1) {
            // we've left `removed` in a valid state because we thought we were
            // going to shift stuff above it down into it, but there's nothing
            // above so just destroy it.
            if constexpr (!stdc::is_trivially_destructible_v<T>) {
                removed.~T();
            }
            return out;
        }

        if constexpr (stdc::is_trivially_copyable_v<T>) {
            const size_t idxplusone = idx + 1;
            ::memmove(m.items + idx, m.items + idxplusone,
                      (this->size() - idxplusone) * sizeof(T));
        } else {
            for (size_t i = this->size() - 1; i > idx; --i) {
                T& source = m.items[i];
                T& target = m.items[i - 1];
                target = stdc::move(source);
            }

            // only call the destructor of the last-most item
            if constexpr (!stdc::is_trivially_destructible_v<T>) {
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
        T out(stdc::move(target));

        defer decrement([this, target] { --m.size; });

        if (idx == this->size() - 1) {
            if constexpr (!stdc::is_trivially_destructible_v<T>) {
                target.~T();
            }
            return out;
        }

        T& last = m.items[this->size() - 1];

        target = stdc::move(last);

        if constexpr (!stdc::is_trivially_destructible_v<T>) {
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

        if (this->size() == 0) {
            m.backing_allocator->deallocate(m.items);
            m.items = nullptr;
            m.capacity = 0;
            m.size = 0;
            return;
        }

        const bytes_t bytes =
            reinterpret_as_bytes(raw_slice(*m.items, this->capacity()));

        alloc::result_t<bytes_t> reallocated =
            m.backing_allocator->reallocate(alloc::reallocate_request_t{
                .memory = bytes,
                .new_size_bytes = size() * sizeof(T),
                // flags besides in_place_orelse_fail not really necessary
                .flags = alloc::realloc_flags::in_place_orelse_fail |
                         alloc::realloc_flags::leave_nonzeroed,
            });

        if (!reallocated.is_success()) [[unlikely]]
            return;

        bytes_t& new_bytes = reallocated.unwrap();
        __ok_assert((void*)new_bytes.unchecked_address_of_first_item() ==
                        m.items,
                    "Backing allocator for arraylist did not reallocate "
                    "properly, different memory returned but "
                    "in_place_orelse_fail was passed.");
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
    /// If type stored is trivially default constructible, and the default
    /// constructor is selected, then new memory is zeroed.
    /// Args `args` must select a nonfailing constructor.
    /// The constructor may get called multiple times, for each new element.
    template <typename... args_t>
        requires is_infallible_constructible_c<T, args_t...>
    [[nodiscard]] constexpr status<alloc::error>
    resize(size_t new_size, args_t&&... args) OKAYLIB_NOEXCEPT
    {
        if (this->size() == new_size) [[unlikely]] {
            return alloc::error::success;
        }

        if (new_size == 0) [[unlikely]] {
            clear();
            return alloc::error::success;
        }

        if (this->capacity() == 0) {
            auto status = make_first_allocation(new_size * sizeof(T));
            if (!status.is_success()) {
                return status;
            }
            __ok_assert(this->capacity() >= new_size,
                        "Allocator did not return enough memory to arraylist");
            if constexpr (stdc::is_trivially_default_constructible_v<T> &&
                          sizeof...(args_t) == 0) {
                ::memset(m.items, 0, sizeof(T) * new_size);
            } else {
                for (size_t i = 0; i < new_size; ++i) {
                    ok::make_into_uninitialized<T>(
                        m.items[i], stdc::forward<args_t>(args)...);
                }
            }
            m.size = new_size;
            return alloc::error::success;
        }

        const bool shrinking = this->size() > new_size;

        if (shrinking) {
            if constexpr (!stdc::is_trivially_destructible_v<T>) {
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

                if (!status.is_success())
                    return status;
            }

            // reallocation worked, right?
            __ok_internal_assert(this->capacity() >= new_size);

            // initialize new elements
            if constexpr (stdc::is_trivially_default_constructible_v<T> &&
                          sizeof...(args_t) == 0) {
                // manually zero memory
                // TODO: some ifdef here maybe to avoid zeroing based on build
                // option? or a template param?
                ::memset(m.items + this->size(), 0,
                         (new_size - this->size()) * sizeof(T));
            } else {
                for (size_t i = this->size(); i < new_size; ++i) {
                    ok::make_into_uninitialized<T>(
                        m.items[i], stdc::forward<args_t>(args)...);
                }
            }

            m.size = new_size;
        }

        return alloc::error::success;
    }

    /// the old shrink and leak. the shrinky leaky
    [[nodiscard]] constexpr slice<T> shrink_and_leak() OKAYLIB_NOEXCEPT
    {
        shrink_to_reclaim_unused_memory();

        if (this->capacity() == 0) [[unlikely]] {
            __ok_internal_assert(m.size == 0);
            return make_null_slice<T>();
        }

        slice<T> out = raw_slice(*m.items, m.size);

        // we no longer own the memory
        m.items = nullptr;
        m.capacity = 0;
        m.size = 0;

        return out;
    }

    template <typename... args_t>
        requires is_constructible_c<T, args_t...>
    constexpr auto append(args_t&&... args) OKAYLIB_NOEXCEPT
        // append may return the error type from an erroring constructor
        -> decltype(insert_at(this->size(), stdc::forward<args_t>(args)...))
    {
        return insert_at(this->size(), stdc::forward<args_t>(args)...);
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

        __ok_internal_assert(this->capacity() >= this->size());

        if constexpr (detail::range_impls_size_c<other_range_t>) {
            const size_t size = ok::size(range);
            const size_t extra_space = this->capacity() - this->size();
            if (size > extra_space) {
                auto status =
                    this->increase_capacity_by_at_least(size - extra_space);
                if (!status.is_success()) [[unlikely]] {
                    return status;
                }
            }
        }

        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            auto status = this->append(ok::range_get_best(range, cursor));
            if constexpr (!detail::range_impls_size_c<other_range_t>) {
                if (!status.is_success()) [[unlikely]] {
                    return status;
                }
            }
        }
        return alloc::error::success;
    }

    constexpr ~arraylist_t() { destroy(); }

  private:
    [[nodiscard]] constexpr status<alloc::error>
    make_first_allocation(size_t initial_bytes = sizeof(T) * 4)
    {
        alloc::result_t<bytes_t> res =
            m.backing_allocator->allocate(alloc::request_t{
                .num_bytes = initial_bytes,
                .alignment = alignof(T),
                .leave_nonzeroed = true,
            });

        if (!res.is_success()) [[unlikely]] {
            return res.status();
        }

        bytes_t& memory = res.unwrap();
        m.items =
            reinterpret_cast<T*>(memory.unchecked_address_of_first_item());
        m.capacity = memory.size() / sizeof(T);

        return alloc::error::success;
    }

    /// "spots" is the currently allocated spots, including uninitialized
    /// capacity. it is modified by this function
    [[nodiscard]] constexpr status<alloc::error>
    reallocate(size_t required_bytes, size_t preferred_bytes)
    {
        using namespace alloc;
        const auto realloc_flags = realloc_flags::leave_nonzeroed;

        if constexpr (!stdc::is_trivially_copyable_v<T>) {
            // if we're not trivially copyable, dont let the allocator do
            // the memcpying, we will do it ourselves after
            result_t<potentially_in_place_reallocation_t> res =
                reallocate_in_place_orelse_keep_old_nocopy(
                    *m.backing_allocator,
                    reallocate_request_t{
                        .memory = reinterpret_as_bytes(
                            raw_slice(*m.items, this->capacity())),
                        .new_size_bytes =
                            this->items().size_bytes() + required_bytes,
                        .preferred_size_bytes =
                            this->items().size_bytes() + preferred_bytes,
                        .flags =
                            realloc_flags | realloc_flags::in_place_orelse_fail,
                    });

            if (!res.is_success()) [[unlikely]] {
                return res.status();
            }

            auto& reallocation = res.unwrap();
            if (reallocation.was_in_place) {
                // sanity checks
                __ok_assert(
                    m.items == reinterpret_cast<T*>(
                                   reallocation.memory
                                       .unchecked_address_of_first_item()),
                    "Reallocation was supposedly in-place, but returned a "
                    "different pointer.");

                m.capacity = reallocation.memory.size() / sizeof(T);
            } else {
                // perform move
                T* const src = m.items;
                T* const dest = reinterpret_cast<T*>(
                    reallocation.memory.unchecked_address_of_first_item());

                if constexpr (stdc::is_trivially_copyable_v<T>) {
                    ::memcpy((void*)dest, (void*)src, m.size * sizeof(T));
                } else {
                    for (size_t i = 0; i < m.size; ++i) {
                        T& src_item = src[i];
                        ok::stdc::construct_at(dest + i, stdc::move(src_item));
                        if constexpr (!stdc::is_trivially_destructible_v<T>) {
                            src_item.~T();
                        }
                    }
                }

                // free old allocation
                m.backing_allocator->deallocate(m.items);

                m.items = dest;
                m.capacity = reallocation.memory.size() / sizeof(T);
            }
            return alloc::error::success;
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

            if (!res.is_success()) [[unlikely]] {
                return res.status();
            }

            bytes_t& bytes = res.unwrap();

            m.items =
                reinterpret_cast<T*>(bytes.unchecked_address_of_first_item());
            m.capacity = bytes.size() / sizeof(T);
            return alloc::error::success;
        }
    }

    constexpr void call_destructor_on_all_items() OKAYLIB_NOEXCEPT
    {
        __ok_internal_assert(this->capacity() > 0);
        if constexpr (!stdc::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < m.size; ++i) {
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

        m.backing_allocator->deallocate(m.items);
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
            .backing_allocator = ok::addressof(allocator),
        };
    }
};

template <typename T> struct spots_preallocated_t
{
    template <typename backing_allocator_t, typename...>
    using associated_type =
        ok::arraylist_t<T, ok::remove_cvref_t<backing_allocator_t>>;

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               size_t num_spots_preallocated) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, num_spots_preallocated);
    }

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr alloc::error
    make_into_uninit(arraylist_t<T, backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     size_t num_spots_preallocated) const OKAYLIB_NOEXCEPT
    {
        using output_t = arraylist_t<T, backing_allocator_t>;
        auto res = allocator.allocate(alloc::request_t{
            .num_bytes = sizeof(T) * num_spots_preallocated,
            .alignment = alignof(T),
            .leave_nonzeroed = true,
        });

        if (!res.is_success()) [[unlikely]] {
            return res.status();
        }

        bytes_t& bytes = res.unwrap();
        T* start =
            reinterpret_cast<T*>(bytes.unchecked_address_of_first_item());
        const size_t num_bytes_allocated = bytes.size();

        ok::stdc::construct_at(
            ok::addressof(output),
            typename output_t::members_t{
                .items = start,
                .capacity = num_bytes_allocated / sizeof(T),
                .size = 0,
                .backing_allocator = ok::addressof(allocator),
            });
        return alloc::error::success;
    }
};

struct copy_items_from_range_t
{
    template <typename backing_allocator_t, typename input_range_t>
    using associated_type =
        ok::arraylist_t<value_type_for<input_range_t>,
                        ok::remove_cvref_t<backing_allocator_t>>;

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, range);
    }

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr alloc::error
    make_into_uninit(ok::arraylist_t<value_type_for<const input_range_t&>,
                                     backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(ok::detail::range_can_size_c<input_range_t>,
                      "Size of range unknown, refusing to copy out its items "
                      "using arraylist::copy_items_from_range constructor.");
        using T = value_type_for<const input_range_t&>;
        using output_t = arraylist_t<T, backing_allocator_t>;

        const size_t num_items = ok::size(range);

        alloc::result_t<bytes_t> res = allocator.allocate(alloc::request_t{
            .num_bytes = num_items * sizeof(T),
            .alignment = alignof(T),
            .leave_nonzeroed = true,
        });

        if (!res.is_success()) [[unlikely]] {
            return res.status();
        }

        auto& bytes = res.unwrap();
        T* const memory =
            reinterpret_cast<T*>(bytes.unchecked_address_of_first_item());
        const size_t bytes_allocated = bytes.size();

        // TODO: if contiguous range and trivially copyable, do memcpy
        size_t i = 0;
        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            ok::stdc::construct_at(memory + i,
                                   ok::range_get_best(range, cursor));
            ++i;
        }

        ok::stdc::construct_at(
            ok::addressof(output),
            typename output_t::members_t{
                .items = reinterpret_cast<T*>(memory),
                .capacity = bytes_allocated / sizeof(T),
                .size = num_items,
                .backing_allocator = ok::addressof(allocator),
            });

        return alloc::error::success;
    };
};

} // namespace detail

template <typename T> inline constexpr detail::empty_t<T> empty;

template <typename T>
inline constexpr detail::spots_preallocated_t<T> spots_preallocated;

inline constexpr detail::copy_items_from_range_t copy_items_from_range;
}; // namespace arraylist

template <typename T, typename backing_allocator_t>
struct range_definition<ok::arraylist_t<T, backing_allocator_t>>
{
    using range_t = ok::arraylist_t<T, backing_allocator_t>;

    static constexpr auto flags =
        ok::range_flags::arraylike | ok::range_flags::sized |
        ok::range_flags::consuming | ok::range_flags::producing;

    using value_type = T;

    static constexpr value_type& get(range_t& r, size_t c) OKAYLIB_NOEXCEPT
    {
        return r[c];
    }

    static constexpr const value_type& get(const range_t& r,
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
