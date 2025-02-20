#ifndef __OKAYLIB_SMART_POINTERS_ARC_H___
#define __OKAYLIB_SMART_POINTERS_ARC_H___

#include "okay/allocators/allocator.h"
#include "okay/defer.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/ok_unreachable.h"
#include "okay/opt.h"
#include <atomic>

namespace ok {
namespace detail {
// NOTE: refcounts and payload always allocated together, so undropped weak
// references will prevent the deallocation of memory for the object.
template <typename T, typename allocator_impl_t> struct arc_payload_t
{
    std::atomic<uint64_t> strong_refcount;
    std::atomic<uint64_t> weak_refcount;
    allocator_impl_t* allocator;
    T object;
};
static_assert(std::atomic<uint64_t>::is_always_lock_free);
inline constexpr uint64_t lock_bit = 1UL << 63;
} // namespace detail

template <typename T, typename allocator_impl_t> struct variant_arc_t;
template <typename T, typename allocator_impl_t> struct ro_arc_t;
template <typename T, typename allocator_impl_t> struct weak_arc_t;

/// Atomically borrow counted *mutable* reference
template <typename T, typename allocator_impl_t = ok::allocator_t>
struct unique_rw_arc_t
{
    static_assert(detail::is_derived_from_v<allocator_impl_t, allocator_t>,
                  "Allocator type parameter to unique_rw_arc_t must be derived "
                  "from ok::allocator_t");

    friend ro_arc_t<T, allocator_impl_t>;
    friend variant_arc_t<T, allocator_impl_t>;
    struct make;
    friend struct make;

    unique_rw_arc_t(const unique_rw_arc_t&) = delete;
    unique_rw_arc_t& operator=(const unique_rw_arc_t&) = delete;
    constexpr unique_rw_arc_t(unique_rw_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(std::exchange(other.m_payload, nullptr))
    {
        __ok_assert(m_payload,
                    "Use-after-move (or to_readonly) of unique_rw_arc_t");
    }

    constexpr unique_rw_arc_t&
    operator=(unique_rw_arc_t&& other) OKAYLIB_NOEXCEPT
    {
        __ok_assert(other.m_payload,
                    "Use-after-move (or to_readonly) of unique_rw_arc_t");
        __ok_assert(other.m_payload != m_payload,
                    "Two unique_rw_arc_t to the same object found.");
        destroy();
        m_payload = std::exchange(other.m_payload, nullptr);
        return *this;
    }

    [[nodiscard]] constexpr T& deref() OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload,
                    "Use-after-move (or to_readonly) of unique_rw_arc_t");
        return m_payload->object;
    }

    [[nodiscard]] constexpr const T& deref() const OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload,
                    "Use-after-move (or to_readonly) of unique_rw_arc_t");
        return m_payload->object;
    }

    [[nodiscard]] constexpr ro_arc_t<T, allocator_impl_t>
    demote_to_readonly() OKAYLIB_NOEXCEPT;

    [[nodiscard]] constexpr weak_arc_t<T, allocator_impl_t>
    spawn_weak_arc() OKAYLIB_NOEXCEPT;

    ~unique_rw_arc_t() noexcept { destroy(); }

  private:
    unique_rw_arc_t() = default;

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (!m_payload) {
            return;
        }

        const auto before_unlock =
            m_payload->strong_refcount.fetch_and(~detail::lock_bit);
        // when ownership is held by unique arc, only the lock bit should be set
        __ok_internal_assert(before_unlock == detail::lock_bit);

        // value is now zero, meaning no promotion of weak pointers will work,
        // we have exclusive access and can perform destruction
        if constexpr (!std::is_trivially_destructible_v<T>) {
            m_payload->object.~T();
        }

        // try also to deallocate if there are no weak references (theres no
        // case where a weak reference promotes itself when thie strong refcount
        // is zero, so we can rule out the possibility of any weak refs
        // existing)
        if (m_payload->weak_refcount.load() == 1) {
            m_payload->allocator->deallocate(
                reinterpret_as_bytes(slice_from_one(*m_payload)));
        }
    }

    constexpr unique_rw_arc_t(
        detail::arc_payload_t<T, allocator_impl_t>* payload) OKAYLIB_NOEXCEPT
        : m_payload(payload)
    {
    }

    detail::arc_payload_t<T, allocator_impl_t>* m_payload;
};

/// Read-only atomically refcounted pointer
template <typename T, typename allocator_impl_t = ok::allocator_t>
struct ro_arc_t
{
    static_assert(detail::is_derived_from_v<allocator_impl_t, allocator_t>,
                  "Allocator type parameter to ro_arc_t must be derived from "
                  "ok::allocator_t");

    using out_error_type = status_t<alloc::error>;

    struct make;
    friend struct make;

    friend unique_rw_arc_t<T, allocator_impl_t>;
    friend variant_arc_t<T, allocator_impl_t>;
    friend weak_arc_t<T, allocator_impl_t>;

    [[nodiscard]] constexpr ro_arc_t duplicate() const OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload,
                    "Use-after-move (or promotion/demotion) of ro_arc_t");

        // spin until we hold lock bit
        while (m_payload->strong_refcount.fetch_or(detail::lock_bit) &
               detail::lock_bit)
            ;
        const auto old = m_payload->strong_refcount.fetch_add(1);
        __ok_internal_assert(old > 1); // 1 is reserved for unique mutable arc
        // unlock
        m_payload->strong_refcount.fetch_and(~detail::lock_bit);
        return *this;
    }

    constexpr ro_arc_t(ro_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(std::exchange(other.m_payload, nullptr))
    {
        __ok_assert(m_payload,
                    "Use-after-move (or promotion/demotion) of ro_arc_t");
    }

    constexpr ro_arc_t& operator=(ro_arc_t&& other) OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload,
                    "Use-after-move (or promotion/demotion) of ro_arc_t");
        // avoid destroy / atomic decrement if the payloads are the same
        // (refcount is not changing)
        if (other.m_payload == m_payload) {
            other.m_payload = nullptr;
            return *this;
        }
        // destroy ourselves and then overwrite with the context of the other
        destroy();
        m_payload = std::exchange(other.m_payload, nullptr);
        return *this;
    }

    [[nodiscard]] constexpr const T& deref() const OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload,
                    "Use-after-move (or promotion/demotion) of ro_arc_t");
        return m_payload->object;
    }

    /// If this function succeeds, it is no longer valid to access the original
    /// ro_arc_t object. If it fails (returns null) the original object is
    /// still valid
    [[nodiscard]] constexpr ok::opt_t<unique_rw_arc_t<T, allocator_impl_t>>
    try_promote_and_consume_into_unique() OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload,
                    "Use-after-move (or promotion/demotion) of ro_arc_t");
        // spin until we hold lock bit
        uint64_t old = 0;
        while ((old = m_payload->strong_refcount.fetch_or(detail::lock_bit)) &
               detail::lock_bit)
            ;
        // should be at least one reference, being equal to exactly lock_bit
        // means the unique ref is active
        __ok_internal_assert(old != detail::lock_bit);

        if (old == detail::lock_bit + 1) {
            // looks like we were the only reference
            ok::opt_t<unique_rw_arc_t<T, allocator_impl_t>> out =
                unique_rw_arc_t<T, allocator_impl_t>(m_payload);
            // when a unique rw arc is active, lock bit is the only thing set
            m_payload->strong_refcount.store(detail::lock_bit);
            m_payload = nullptr;
            return out;
        } else {
            // other references existed, remove lock bit and fail
            m_payload->strong_refcount.fetch_and(~detail::lock_bit);
            return nullopt;
        }
    }

    [[nodiscard]] constexpr weak_arc_t<T, allocator_impl_t>
    demote_to_weak() OKAYLIB_NOEXCEPT;

    [[nodiscard]] constexpr weak_arc_t<T, allocator_impl_t>
    spawn_weak_arc() OKAYLIB_NOEXCEPT;

    ~ro_arc_t() noexcept { destroy(); }

  private:
    ro_arc_t() = default;
    ro_arc_t(const ro_arc_t&) = default;
    ro_arc_t& operator=(const ro_arc_t&) = delete;

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (!m_payload)
            return;

        uint64_t old = 0;
        // spin until we hold lock bit
        while ((old = m_payload->strong_refcount.fetch_or(detail::lock_bit)) &
               detail::lock_bit)
            ;
        __ok_internal_assert(old != 0);

        // early out: other references exist so just decrement refcount and
        // unlock
        if (old != 1) {
            m_payload->strong_refcount.store(old - 1);
            return;
        }

        // only one strong const ref exists. destroy the object
        if constexpr (!std::is_trivially_destructible_v<T>) {
            m_payload->object.~T();
        }

        // additionally, if weak ref is one, then no weak refs exist (nor
        // are any spinning + waiting to promote) so we can deallocate
        // everything
        if (m_payload->weak_refcount.load() == 1) {
            m_payload->allocator->deallocate(
                ok::reinterpret_as_bytes(ok::slice_from_one(*m_payload)));
        } else {
            // weak refs exist, mark as destroyed and leave allocated
            m_payload->strong_refcount.store(0);
        }
    }

    constexpr ro_arc_t(detail::arc_payload_t<T, allocator_impl_t>* payload)
        OKAYLIB_NOEXCEPT : m_payload(payload)
    {
    }

    detail::arc_payload_t<T, allocator_impl_t>* m_payload;
};

template <typename T, typename allocator_impl_t = ok::allocator_t>
struct weak_arc_t
{
    friend unique_rw_arc_t<T, allocator_impl_t>;
    friend ro_arc_t<T, allocator_impl_t>;
    friend variant_arc_t<T, allocator_impl_t>;

    [[nodiscard]] constexpr weak_arc_t duplicate() const OKAYLIB_NOEXCEPT
    {
        if (m_payload) {
            // okay to add to weak refcount because there is already at least
            // one weak refcount. in unique rw arc, we check if weak ref is zero
            // and if so assume we can deallocate
            m_payload->weak_refcount.fetch_add(1);
        }
        return *this;
    }

    constexpr weak_arc_t(weak_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(std::exchange(other.m_payload, nullptr))
    {
    }

    constexpr weak_arc_t& operator=(weak_arc_t&& other) OKAYLIB_NOEXCEPT
    {
        // decrement our refcount, maybe deallocate if the other is being
        // use-after-moved
        // NOTE: if we treat use-after-move as undefined behavior, we have a
        // guarantee here that we will never perform deallocation and can elide
        // the check in destroy() that compares weak count to zero.
        destroy();
        m_payload = std::exchange(other.m_payload, nullptr);
        return *this;
    }

    /// Checks if there is some nonzero number of const references to the main
    /// pointer payload, and if so this weak reference becomes another const.
    /// If this function returns success, it is not valid to access this
    /// weak_arc_t afterwards.
    [[nodiscard]] constexpr ok::opt_t<ro_arc_t<T, allocator_impl_t>>
    try_spawn_readonly() OKAYLIB_NOEXCEPT
    {
        if (!m_payload) {
            return nullopt;
        }

        uint64_t old = 0;
        // spin until we hold lock bit
        while ((old = m_payload->strong_refcount.fetch_or(detail::lock_bit)) &
               detail::lock_bit) {
            // check if mutable reference is the one holding the lock, in which
            // case fail
            if (old == detail::lock_bit) {
                return nullopt;
            }
        }

        // no refcounts, the object has been destroyed, so release lock and fail
        if (old == 0) {
            m_payload->strong_refcount.store(old);
            m_payload = nullptr;
            return nullopt;
        }

        // increment refcount and convert ourselves to a const pointer,
        // releasing the lock at the same time
        m_payload->strong_refcount.store(old + 1);
        using ro_arc_t = ro_arc_t<T, allocator_impl_t>;
        return ok::opt_t<ro_arc_t>(ro_arc_t(std::exchange(m_payload, nullptr)));
    }

    ~weak_arc_t() OKAYLIB_NOEXCEPT { destroy(); }

  private:
    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (!m_payload) {
            return;
        }
        if (m_payload->weak_refcount.fetch_sub(1) == 1) {
            __ok_internal_assert(m_payload->strong_refcount.load() == 0);
            // if weak just hit zero, then there also must be no strong refcount
            m_payload->allocator->deallocate(
                reinterpret_as_bytes(slice_from_one(*m_payload)));
        }
    }

    weak_arc_t() = default;
    weak_arc_t(const weak_arc_t&) = default;
    weak_arc_t& operator=(const weak_arc_t&) = default;
    constexpr weak_arc_t(detail::arc_payload_t<T, allocator_impl_t>* payload)
        OKAYLIB_NOEXCEPT : m_payload(payload)
    {
    }

    detail::arc_payload_t<T, allocator_impl_t>* m_payload;
};

template <typename T, typename allocator_impl_t>
[[nodiscard]] constexpr auto ro_arc_t<T, allocator_impl_t>::demote_to_weak()
    OKAYLIB_NOEXCEPT -> weak_arc_t<T, allocator_impl_t>
{
    // okay to add to weak because we have some in strong
    m_payload->weak_refcount.fetch_add(1);
    weak_arc_t<T, allocator_impl_t> out(m_payload);
    destroy(); // decrement strong refcount
    m_payload = nullptr;
    return out;
}

template <typename T, typename allocator_impl_t>
[[nodiscard]] constexpr ro_arc_t<T, allocator_impl_t>
unique_rw_arc_t<T, allocator_impl_t>::demote_to_readonly() OKAYLIB_NOEXCEPT
{
    __ok_assert(m_payload,
                "Use-after-move (or to_readonly) of unique_rw_arc_t");
    __ok_internal_assert(m_payload->strong_refcount.load() == detail::lock_bit);
    ro_arc_t<T, allocator_impl_t> out(m_payload);
    m_payload = nullptr;
    return out;
}

template <typename T, typename allocator_impl_t>
[[nodiscard]] constexpr weak_arc_t<T, allocator_impl_t>
unique_rw_arc_t<T, allocator_impl_t>::spawn_weak_arc() OKAYLIB_NOEXCEPT
{
    __ok_assert(m_payload,
                "Use-after-move (or to_readonly) of unique_rw_arc_t");
    m_payload->weak_refcount.fetch_add(1);
    return weak_arc_t(m_payload);
}

template <typename T, typename allocator_impl_t>
[[nodiscard]] constexpr weak_arc_t<T, allocator_impl_t>
ro_arc_t<T, allocator_impl_t>::spawn_weak_arc() OKAYLIB_NOEXCEPT
{
    __ok_assert(m_payload,
                "Use-after-move (or promotion/demotion) of ro_arc_t");
    // safe to do this without lock because weak refcount wont get read unless
    // the last const ref gets destroyed, but this is performed by a valid
    // unique_rw_arc
    m_payload->weak_refcount.fetch_add(1);
    return weak_arc_t(m_payload);
}

enum class arc_ownership : uint8_t
{
    // unique ownership, read/write access
    unique_rw,
    // shared ownership but can only read from ref
    shared_ro,
    // owns the allocation but not the object
    weak,
};

template <typename T, typename allocator_impl_t> struct variant_arc_t
{
    using ro_arc_t = ro_arc_t<T, allocator_impl_t>;
    using unique_rw_arc_t = unique_rw_arc_t<T, allocator_impl_t>;
    using weak_arc_t = weak_arc_t<T, allocator_impl_t>;

    constexpr variant_arc_t(unique_rw_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(other.m_payload),
          m_mode(arc_ownership::unique_rw)
    {
        other.m_payload = nullptr;
    }
    constexpr variant_arc_t(ro_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(other.m_payload),
          m_mode(arc_ownership::shared_ro)
    {
        other.m_payload = nullptr;
    }
    constexpr variant_arc_t(weak_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(other.m_payload),
          m_mode(arc_ownership::weak)
    {
        other.m_payload = nullptr;
    }

    [[nodiscard]] constexpr arc_ownership
    ownership_mode() const OKAYLIB_NOEXCEPT
    {
        __ok_assert(m_payload, "Use after move/consumption of variant_arc_t");
        return m_mode;
    }

    [[nodiscard]] constexpr weak_arc_t spawn_weak_arc()
    {
        if (!m_payload) {
            __ok_abort(
                "Attempt to spawn_weak_arc on used-up / null variant arc.");
        }

        switch (ownership_mode()) {
        case arc_ownership::unique_rw: {
            unique_rw_arc_t actual(m_payload);
            auto weak_out = actual.spawn_weak_arc();
            actual.m_payload = nullptr; // prevent destruction of self
            return weak_out;
        }
        case arc_ownership::shared_ro: {
            ro_arc_t actual(m_payload);
            auto weak_out = actual.spawn_weak_arc();
            actual.m_payload = nullptr; // prevent destruction of self
            return weak_out;
        }
        case arc_ownership::weak: {
            weak_arc_t actual(m_payload);
            auto weak_out = actual.duplicate();
            actual.m_payload = nullptr; // prevent destruction of self
            return weak_out;
        }
        }
        __ok_unreachable;
    }

    [[nodiscard]] constexpr opt_t<variant_arc_t>
    try_duplicate() const OKAYLIB_NOEXCEPT
    {
        if (!m_payload) [[unlikely]] {
            return nullopt;
        }
        switch (ownership_mode()) {
        case arc_ownership::unique_rw:
            return nullopt;
            break;
        case arc_ownership::shared_ro: {
            ro_arc_t actual(m_payload);
            auto out = actual.duplicate();
            actual.m_payload = nullptr; // prevent destruction of self
            return out;
        }
        case arc_ownership::weak: {
            weak_arc_t actual(m_payload);
            auto out = actual.duplicate();
            actual.m_payload = nullptr; // prevent destruction of self
            return out;
        }
        }
        __ok_unreachable;
    }

    [[nodiscard]] constexpr opt_t<T&> try_deref_nonconst() OKAYLIB_NOEXCEPT
    {
        if (m_mode != arc_ownership::unique_rw || !m_payload) [[unlikely]] {
            return nullopt;
        }
        unique_rw_arc_t actual(m_payload);
        auto& deref = actual.deref();
        actual.m_payload = nullptr;
        return deref;
    }

    [[nodiscard]] constexpr opt_t<const T&> try_deref() OKAYLIB_NOEXCEPT
    {
        if (!m_payload) [[unlikely]] {
            return nullopt;
        }
        switch (ownership_mode()) {
        case arc_ownership::unique_rw: {
            unique_rw_arc_t actual(m_payload);
            auto& deref = actual.deref();
            actual.m_payload = nullptr;
            return deref;
        }
        case arc_ownership::shared_ro: {
            ro_arc_t actual(m_payload);
            auto& deref = actual.deref();
            actual.m_payload = nullptr;
            return deref;
        }
        default:
            return nullopt;
        }
    }

    [[nodiscard]] constexpr opt_t<ro_arc_t>
    try_consume_into_contained_readonly_arc() OKAYLIB_NOEXCEPT
    {
        if (ownership_mode() != arc_ownership::shared_ro || !m_payload)
            [[unlikely]] {
            return nullopt;
        }
        return opt_t<ro_arc_t>(ro_arc_t(std::exchange(m_payload, nullptr)));
    }

    [[nodiscard]] constexpr opt_t<weak_arc_t>
    try_consume_into_contained_weak_arc() OKAYLIB_NOEXCEPT
    {
        if (ownership_mode() != arc_ownership::weak || !m_payload)
            [[unlikely]] {
            return nullopt;
        }
        return opt_t<weak_arc_t>(weak_arc_t(std::exchange(m_payload, nullptr)));
    }

    [[nodiscard]] constexpr opt_t<unique_rw_arc_t>
    try_consume_into_contained_unique_arc() OKAYLIB_NOEXCEPT
    {
        if (ownership_mode() != arc_ownership::unique_rw || !m_payload)
            [[unlikely]] {
            return nullopt;
        }
        return opt_t<unique_rw_arc_t>(
            unique_rw_arc_t(std::exchange(m_payload, nullptr)));
    }

    [[nodiscard]] constexpr opt_t<ro_arc_t>
    try_convert_and_consume_into_readonly_arc() OKAYLIB_NOEXCEPT
    {
        if (!m_payload) {
            return nullopt;
        }

        switch (ownership_mode()) {
        case arc_ownership::unique_rw:
            return unique_rw_arc_t(std::exchange(m_payload, nullptr))
                .demote_to_readonly();
        case arc_ownership::shared_ro:
            return ro_arc_t(std::exchange(m_payload, nullptr));
        case arc_ownership::weak:
            weak_arc_t actual(m_payload);
            if (auto result = actual.try_spawn_readonly()) {
                m_payload = nullptr;
                actual.m_payload = nullptr;
                return result;
            }
            actual.m_payload = nullptr;
            return nullopt;
        }
        __ok_unreachable;
        return nullopt;
    }

    [[nodiscard]] constexpr opt_t<unique_rw_arc_t>
    try_convert_and_consume_into_unique_arc() OKAYLIB_NOEXCEPT
    {
        if (!m_payload) [[unlikely]] {
            return nullopt;
        }

        switch (ownership_mode()) {
        case arc_ownership::unique_rw: {
            return unique_rw_arc_t(std::exchange(m_payload, nullptr));
        }
        case arc_ownership::shared_ro: {
            ro_arc_t actual(m_payload);

            auto promotion = actual.try_promote_and_consume_into_unique();

            if (promotion) {
                m_payload = nullptr;
                return promotion;
            }
            return nullopt;
        }
        case arc_ownership::weak: {
            weak_arc_t actual(m_payload);
            auto const_promotion = actual.try_spawn_readonly();
            if (!const_promotion) {
                actual.m_payload = nullptr;
                return nullopt;
            }
            ro_arc_t& const_arc = const_promotion.value();
            auto unique_promotion =
                const_arc.try_promote_and_consume_into_unique();
            if (!unique_promotion) {
                // perform operation to bring refcount back to weak, moving
                // const_arc into actual here
                actual = const_arc.demote_to_weak();
                // make sure actual is no longer owning- this variant arc
                // effectively transfers the weak ownership in actual back to
                // itself
                actual.m_payload = nullptr;
                return nullopt;
            }
            return unique_promotion;
        }
        }
        __ok_unreachable;
        return nullopt;
    }

    constexpr variant_arc_t(variant_arc_t&& other) OKAYLIB_NOEXCEPT
        : m_payload(std::exchange(other.m_payload, nullptr))
    {
    }
    constexpr variant_arc_t& operator=(variant_arc_t&& other) OKAYLIB_NOEXCEPT
    {
        destroy();
        m_payload = std::exchange(other.m_payload, nullptr);
        return *this;
    }

    ~variant_arc_t() { destroy(); }

  private:
    constexpr void destroy()
    {
        if (!m_payload) {
            return;
        }

        switch (ownership_mode()) {
        case arc_ownership::unique_rw: {
            unique_rw_arc_t destroyer(m_payload);
            break;
        }
        case arc_ownership::shared_ro: {
            ro_arc_t destroyer(m_payload);
            break;
        }
        case arc_ownership::weak: {
            weak_arc_t destroyer(m_payload);
            break;
        }
        }
        m_payload = nullptr;
    }

    variant_arc_t() = default;
    variant_arc_t(const variant_arc_t&) = default;
    variant_arc_t& operator=(const variant_arc_t&) = default;

    detail::arc_payload_t<T, allocator_impl_t>* m_payload;
    arc_ownership m_mode;
};

template <typename T, typename allocator_impl_t>
struct unique_rw_arc_t<T, allocator_impl_t>::make
{
    make() = delete;
    // TODO: have this support factory functions for T
    static constexpr auto with =
        [](status_t<alloc::error>& out_status, allocator_impl_t& allocator,
           auto&&... constructor_args) -> unique_rw_arc_t {
        static_assert(std::is_constructible_v<T, decltype(constructor_args)...>,
                      "Cannot make a unique_rw_arc_t with the given arguments- "
                      "there is no matching constructor for T.");
        using payload_t = detail::arc_payload_t<T, allocator_impl_t>;
        alloc::result_t<maybe_defined_memory_t> res =
            allocator.allocate(alloc::request_t{
                .num_bytes = sizeof(payload_t),
                .alignment = alignof(payload_t),
                .flags = alloc::flags::leave_nonzeroed,
            });

        if (!res.okay()) [[unlikely]] {
            out_status = res.err();
            return unique_rw_arc_t();
        }

        payload_t& payload = *reinterpret_cast<payload_t*>(
            res.release_ref().data_maybe_defined());

        // this unique ref holds the lock until it is destroyed. only when
        // unique ref is active is the strong refcount exactly equal to
        // lock_bit.
        payload.strong_refcount = detail::lock_bit;
        payload.weak_refcount = 1;
        payload.allocator = ok::addressof(allocator);
        new (ok::addressof(payload.object))
            T(std::forward<decltype(constructor_args)>(constructor_args)...);

        out_status = alloc::error::okay;

        return unique_rw_arc_t(ok::addressof(payload));
    };
};

template <typename T, typename allocator_impl_t = ok::allocator_t>
[[nodiscard]] alloc::result_t<unique_rw_arc_t<T, allocator_impl_t>>
into_arc(T&& item, allocator_impl_t& allocator) OKAYLIB_NOEXCEPT
{
    static_assert(detail::is_derived_from_v<allocator_impl_t, allocator_t>,
                  "Type given as allocator for into_arc is not derived "
                  "from allocator.");
    status_t<alloc::error> out_status;
    auto out = unique_rw_arc_t<T, allocator_impl_t>::make::with(
        out_status, allocator, std::forward<T>(item));
    if (!out_status.okay()) {
        return out_status.err();
    }
    return out;
}

} // namespace ok

#endif
