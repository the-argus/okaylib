#ifndef __OKAYLIB_CONTAINERS_ARCPOOL_H__
#define __OKAYLIB_CONTAINERS_ARCPOOL_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/ok_assert.h"
#include "okay/platform/atomic.h"

namespace ok {
namespace arcpool {
template <typename T> struct with_capacity_t;
}

namespace detail {
struct arcpool_counters_t
{
  private:
    // never touched if strongcount is nonzero
    ok::atomic_t<uint64_t> generation;
    ok::atomic_t<uint64_t> strongcount;

  public:
    void* volatile next;

    [[nodiscard]] constexpr uint64_t load_strongcount() const noexcept
    {
        return strongcount.load();
    }

    [[nodiscard]] constexpr uint64_t load_generation() const noexcept
    {
        return generation.load();
    }

    constexpr uint64_t decrement_strongcount() noexcept
    {
        return strongcount.fetch_sub(1);
    }

    constexpr uint64_t increment_generation() noexcept
    {
        return generation.fetch_add(1);
    }

    // should only be done when cloning a strong arc
    constexpr uint64_t increment_strongcount() noexcept
    {
        return strongcount.fetch_add(1);
    }

    enum class promotion_result : uint8_t
    {
        success,
        needs_destroy,
        failure,
    };

    [[nodiscard]] constexpr promotion_result
    try_promote_weakptr(uint64_t expected_generation) noexcept;
};

template <typename item_t> struct arcpool_freestack_t
{
    ok::atomic_t<uint64_t> top; // pointer, cast to uint
    volatile uint64_t size = 0;

    static_assert(sizeof(item_t*) <= sizeof(uint64_t));

    // items are aligned enough so bottom few bits are unused
    static constexpr uint64_t freestack_locked_bit = 0b1;
    [[nodiscard]] constexpr item_t* lock() noexcept
    {
        uint64_t freestack_int;
        do {
            freestack_int =
                top.fetch_or(freestack_locked_bit, ok::memory_order::acq_rel);
        } while (freestack_int & freestack_locked_bit);
        return freestack_int;
    }

    constexpr void unlock(item_t* new_top)
    {
        top.store(uint64_t(new_top), ok::memory_order::release);
        // developer assert
        __ok_assert((uint64_t(new_top) & freestack_locked_bit) == 0,
                    "Didn't unlock freestack, unaligned ptr?");
    }

    constexpr void push(item_t* item) noexcept
    {
        uint64_t prev_front = lock();
        auto* const freestack_head = static_cast<item_t*>(prev_front);
        item->counters.next = freestack_head;
        ++size;
        unlock(item);
    }

    [[nodiscard]] constexpr item_t* pop() noexcept
    {
        // lock the freestack
        const uint64_t prev_front = lock();

        if (size) {
            // pop an item off
            auto* item = static_cast<item_t*>(prev_front);
            auto* new_front = static_cast<item_t*>(item->counters.next);
            // developer assert
            __ok_assert((uint64_t(new_front) & freestack_locked_bit) == 0,
                        "misaligned addr");
            --size;
            // unlock the freestack, it is now pointing at the next item and
            // we have exclusive access to this one
            unlock(new_front);
            return item;
        } else {
            // return freestack to previous state and leave
            unlock(prev_front);
            return nullptr;
        }
    }
};

} // namespace detail

/// An array_arcpool can be thought of as an allocator for objects of type T,
/// where you can only allocate the objects inside arcs (shared pointers). This
/// enables the pool to reuse the reference counters even when weak pointers to
/// the counters are still alive. Destroying the arcpool does not call any
/// destructors but in debug mode will assert that they are all dead (strong
/// reference count of 0).
/// Small side effect: attempting to promote a weak pointer to a strong pointer
/// may call the destructor of the object on the promoting thread, sort of
/// "stealing" the destructor call from a thread which destroyed a strong
/// reference at the exact same time the weak reference attempted promotion.
/// Also, this requires more compare exchanges and spinning than a typical
/// strongcount/weakcount implementation
template <typename T,
          ::ok::detail::is_derived_from_c<ok::allocator_t> allocator_impl_t>
class arcpool_t
{
    struct item_t;
    friend struct item_t;

    using freestack_t = detail::arcpool_freestack_t<item_t>;

    template <typename U> friend class arcpool::with_capacity_t;

    struct item_buffer_t
    {
        size_t length;
        item_buffer_t* next = nullptr;
        item_t items[];
    };

    struct initial_buffer_t
    {
        freestack_t freestack;
        item_buffer_t item_buffer;
    };

    initial_buffer_t* m_root = nullptr;
    item_buffer_t* m_back = nullptr;
    allocator_impl_t* m_allocator = nullptr;

    constexpr arcpool_t() = default;

    constexpr arcpool_t(allocator_impl_t& allocator,
                        initial_buffer_t* initial_buffer_owned)
        : m_allocator(ok::addressof(allocator)), m_root(initial_buffer_owned),
          m_back(ok::addressof(initial_buffer_owned->item_buffer))
    {
    }

    struct item_t
    {
        detail::arcpool_counters_t counters;
        ok::detail::uninitialized_storage_t<T> item;

        constexpr void destroy_raw(freestack_t* freestack) OKAYLIB_NOEXCEPT
        {
            item.value.~T();
            counters.increment_generation();
            freestack->push(this);
        }

        constexpr void destroy_with(freestack_t* freestack) OKAYLIB_NOEXCEPT
        {
            if (counters.decrement_strongcount() == 1)
                destroy_raw(freestack);
        }
    };

    static_assert(alignof(item_t) >= 2,
                  "item_t needs at least 2 alignment so the last bit of the "
                  "pointer is always empty");

    constexpr ok::alloc::error
    alloc_more(size_t starting_root_length = 4) OKAYLIB_NOEXCEPT
    {
        if (!m_allocator)
            return ok::alloc::error::usage;

        constexpr auto init_item_buffer = [](item_buffer_t& buffer,
                                             size_t len) {
            buffer = {
                .length = len,
            };

            for (size_t i = 0; i < len; ++i) {
                buffer->items[i].counters.next =
                    (i != len - 1) ? buffer->items + i + 1 : nullptr;
            }
        };

        if (m_root) {
            // we have a root, allocate a new buffer
            const size_t new_buffer_size =
                ok::partial_max(size_t(float(m_back->length) * 1.5f), 1UL)
                    .ref_or_panic();
            res allocation = m_allocator->allocate(ok::alloc::request_t{
                .num_bytes =
                    sizeof(item_buffer_t) + (sizeof(item_t) * new_buffer_size),
                .alignment = alignof(item_buffer_t),
                .leave_nonzeroed = true,
            });

            if (!ok::is_success(allocation)) [[unlikely]]
                return allocation.status();

            bytes_t& bytes = allocation.unwrap();

            auto* new_buffer = reinterpret_cast<item_buffer_t*>(
                bytes.unchecked_address_of_first_item());

            init_item_buffer(new_buffer, new_buffer_size);

            // add this buffer to our list of buffers
            m_back->next = new_buffer;
            m_back = new_buffer;

            // mark all items in this buffer as free
            item_t* oldtop = m_root->freestack.lock();
            new_buffer->items[new_buffer_size - 1] = oldtop;
            m_root->freestack.size += new_buffer_size;
            m_root->freestack.unlock(new_buffer->items);
        } else {
            // no root item, allocate that with four spots
            res allocation = m_allocator->allocate(ok::alloc::request_t{
                .num_bytes = sizeof(initial_buffer_t) +
                             (sizeof(item_t) * starting_root_length),
                .alignment = alignof(initial_buffer_t),
                .leave_nonzeroed = true,
            });

            if (!ok::is_success(allocation)) [[unlikely]]
                return allocation.status();

            bytes_t& bytes = allocation.unwrap();

            auto* initial_buffer = reinterpret_cast<initial_buffer_t*>(
                bytes.unchecked_address_of_first_item());

            init_item_buffer(initial_buffer->item_buffer, starting_root_length);

            m_root = initial_buffer;
            m_back = ok::addressof(m_root->item_buffer);
            // developer assert
            __ok_assert(m_root->freestack.size == 0,
                        "bad initialization of freestack");
            auto&& ignored_garbage = m_root->freestack.lock();
            m_root->freestack.size = starting_root_length;
            m_root->freestack.unlock(m_root->item_buffer.items);
        }
    }

  public:
    class weak_t;
    class strong_t
    {
        item_t* m_ptr;
        freestack_t* m_freestack;

        constexpr strong_t(freestack_t* freestack, item_t* item)
            : m_ptr(item), m_freestack(freestack)
        {
        }

      public:
        friend class arcpool_t;
        friend class arcpool_t::weak_t;

        strong_t() = delete;

        constexpr item_t& operator*() const noexcept { return *m_ptr; }
        constexpr item_t* operator->() const noexcept { return m_ptr; }

        constexpr strong_t(const strong_t& other) OKAYLIB_NOEXCEPT
            : m_ptr(other.m_ptr)
        {
            __ok_assert(other.m_freestack == m_freestack,
                        "constructing strong_t with different source pool");
            m_ptr->counters.increment_strongcount();
        }

        constexpr strong_t& operator=(const strong_t& other) OKAYLIB_NOEXCEPT
        {
            __ok_assert(other.m_freestack == m_freestack,
                        "assigning strong_t with different source pool");
            if (m_ptr)
                m_ptr->destroy_with(m_freestack);
            m_ptr = other.m_ptr;
            m_ptr->counters.increment_strongcount();
            return *this;
        }

        constexpr strong_t(strong_t&& other) OKAYLIB_NOEXCEPT
            : m_ptr(other.m_ptr)
        {
            __ok_assert(other.m_freestack == m_freestack,
                        "constructing strong_t with different source pool");
            other.m_ptr = nullptr;
        }

        constexpr strong_t& operator=(strong_t&& other) OKAYLIB_NOEXCEPT
        {
            __ok_assert(other.m_freestack == m_freestack,
                        "assigning strong_t with different source pool");
            if (m_ptr)
                m_ptr->destroy_with(m_freestack);
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            return *this;
        }

        constexpr ~strong_t()
        {
            if (m_ptr)
                m_ptr->destroy_with(m_freestack);
        }

        constexpr operator weak_t() const noexcept;
    };

    class weak_t
    {
        freestack_t* m_freestack; // unused except when promoting
        item_t* m_ptr = nullptr;
        uint64_t m_generation;

        constexpr weak_t(freestack_t* freestack, item_t* ptr)
            : m_freestack(freestack), m_ptr(ptr),
              m_generation(ptr->counters.load_generation())
        {
        }

      public:
        friend class arcpool_t;

        constexpr weak_t() = default;
        constexpr weak_t(const weak_t& other) = default;
        constexpr weak_t& operator=(const weak_t& other) = default;
        constexpr weak_t(weak_t&& other) = default;
        constexpr weak_t& operator=(weak_t&& other) = default;

        constexpr ok::opt<strong_t> try_promote()
        {
            if (!m_ptr)
                return {};

            switch (m_ptr->counters.try_promote_weakptr(m_generation)) {
            case detail::arcpool_counters_t::promotion_result::success:
                return strong_t(m_freestack, m_ptr);
            case detail::arcpool_counters_t::promotion_result::failure:
                m_ptr = nullptr;
                return {};
            case detail::arcpool_counters_t::promotion_result::needs_destroy:
                // destroy_raw because we already know the strong count is zero
                m_ptr->destroy_raw(m_freestack);
                m_ptr = nullptr;
                return {};
            }
            // unreachable
        }
    };

    template <typename... constructor_args_t>
    [[nodiscard]] constexpr auto make(constructor_args_t&&... args) noexcept
    {
        using status_type = decltype(ok::make_into_uninitialized<T>(
            ok::stdc::declval<T&>(),
            std::forward<constructor_args_t>(args)...));
        static_assert(
            std::is_void_v<status_type> ||
                ok::is_convertible_to_c<ok::alloc::error, status_type>,
            "Given constructor does not return an error which is "
            "compatible with ok::alloc::error- cannot use it in "
            "function arcpool_t::make() which can also OOM");

        item_t* item = this->m_root->freestack->pop();

        if (!item) {
            // developer assert
            __ok_assert(
                m_root->freestack.size == 0,
                "pop() should only fail when no items are left in freelist");

            if (auto status = this->alloc_more(); !ok::is_success(status)) {
                if constexpr (ok::stdc::is_void_v<status_type>) {
                    return ok::status(status);
                } else {
                    using return_type = ok::res<strong_t, status_type>;
                    return return_type(status_type(status));
                }
            }

            // developer assert
            __ok_assert(m_root->freestack.size,
                        "alloc_more should add more items to freelist");

            item = m_root->freestack.pop();
            // developer assert
            __ok_assert(item, "");
        }

        item->counters.increment_generation(ok::memory_order::release);
        // incrementing strongcount means no more incrementing generation until
        // every strong reference is dead, so we did the generation change
        // before this
        item->counters.increment_strongcount(ok::memory_order::release);

        if constexpr (std::is_void_v<status_type>) {
            ok::make_into_uninitialized<T>(
                item->item.value, std::forward<constructor_args_t>(args)...);
            return ok::status(alloc::error::success);
        } else {
            status_type status = ok::make_into_uninitialized<T>(
                item->item.value, std::forward<constructor_args_t>(args)...);

            if (ok::is_success(status)) [[likely]] {
                return return_type(strong_t(this, item));
            } else {
                return return_type(std::move(status));
            }
        }
    }

    constexpr explicit arcpool_t(allocator_impl_t& allocator)
        : m_allocator(ok::addressof(allocator))
    {
    }

    constexpr arcpool_t(arcpool_t&& other) OKAYLIB_NOEXCEPT
        : m_root(other.m_root),
          m_back(other.m_back),
          m_allocator(other.m_allocator)
    {
        other.m_root = nullptr;
        other.m_allocator = nullptr;
        other.m_back = 0;
    }

    constexpr arcpool_t& operator=(arcpool_t&& other) OKAYLIB_NOEXCEPT
    {
        m_root = other.m_root;
        m_allocator = other.m_allocator;
        m_back = other.m_back;
        other.m_root = nullptr;
        other.m_allocator = nullptr;
        other.m_back = 0;
    }

    constexpr arcpool_t(const arcpool_t& other) OKAYLIB_NOEXCEPT = delete;
    constexpr arcpool_t&
    operator=(const arcpool_t& other) OKAYLIB_NOEXCEPT = delete;

    constexpr ~arcpool_t()
    {
        if (!m_allocator)
            return;

        item_buffer_t* iter = m_root->item_buffer.next;

        constexpr auto dbg = [](item_t* start, size_t len) {
#ifndef NDEBUG
            for (size_t i = 0; i < len; ++i) {
                __ok_assert(start[i]->counters.load_strongcount() == 0,
                            "Attempt to destroy an arcpool while some of the "
                            "pointers are still live");
            }
#endif
        };

        while (iter) {
            auto* temp = iter->next;
            dbg(iter->items, iter->length);
            m_allocator->deallocate(iter);
            iter = temp;
        }
        dbg(m_root->items, m_root->length);
        m_allocator->deallocate(m_root);
    }
};

template <typename T,
          ::ok::detail::is_derived_from_c<ok::allocator_t> allocator_impl_t>
[[nodiscard]] constexpr arcpool_t<T, allocator_impl_t>::strong_t::
operator arcpool_t<T, allocator_impl_t>::weak_t() const noexcept
{
    return {this->m_freestack, this->m_ptr};
}

[[nodiscard]] constexpr detail::arcpool_counters_t::promotion_result
detail::arcpool_counters_t::try_promote_weakptr(
    uint64_t expected_generation) noexcept
{
    uint64_t expected_strongcount;
    uint64_t current_generation;
    do {
        expected_strongcount = strongcount.load();
        current_generation = generation.load();

        // the object is not alive, we cannot increment strong count
        // NOTE: guarantees that we never increment strongcount from
        // zero as a weakptr. only the arcpool itself does that when
        // constructing new items (and in that case it can guarantee
        // exclusive access)
        if (expected_strongcount == 0)
            return promotion_result::failure;

        if (current_generation != expected_generation)
            return promotion_result::failure;

        // NOTE: it is possible that, right here, another thread
        // decrements expected_strongcount to 0, increments generation,
        // then somebody else creates a new object in the same spot,
        // incrementing generation and strongcount back up to 1. that
        // means that we see the strongcount as unchanged by the
        // generation did change. This is the A-B-A problem

    } while (!strongcount.compare_exchange_weak(expected_strongcount,
                                                expected_strongcount + 1));

    // check for A-B-A after the fact
    if (generation.load() != expected_generation) [[unlikely]] {
        // in this case, we just incremented the strong counter on a
        // thing we don't own. now we need to decrement it to repair
        // things
        const auto old_strongcount = strongcount.fetch_sub(1);

        // we may have accidentally acquired exclusive access to this
        // object, once again by a very unlikely series of events, A-B-A
        // problem followed by the last strong arc being destroyed
        // before we can decrement.
        // NOTE: access is exclusive in the needs_destroy case because
        // weak pointers never increment from zero- if we decremented to
        // zero, there's no way anybody else can increment it, except
        // the arcpool itself, but this element is not in the linked
        // list for recreation, so nobody will touch it.
        if (old_strongcount == 1) [[unlikely]]
            return promotion_result::needs_destroy;
        else
            return promotion_result::failure;
    }

    // generation matched what we expected and we incremented the
    // strongcount on a live object
    return promotion_result::success;
}

namespace arcpool {
template <typename T> struct with_capacity_t
{
    template <typename backing_allocator_t, typename...>
    using associated_type =
        ok::arcpool_t<T, ok::detail::remove_cvref_t<backing_allocator_t>>;

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               size_t capacity) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, capacity);
    }

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr alloc::error
    make_into_uninit(arcpool_t<T, backing_allocator_t>& output,
                     backing_allocator_t& allocator,
                     size_t capacity) const OKAYLIB_NOEXCEPT
    {
        using output_t = arcpool_t<T, backing_allocator_t>;
        // arcpool can be default constructed with nothing in it
        ok::stdc::construct_at(output);
        return output.alloc_more(capacity);
    }
};
} // namespace arcpool

} // namespace ok

#endif
