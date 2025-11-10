#ifndef __OKAYLIB_PLATFORM_ATOMIC_H__
#define __OKAYLIB_PLATFORM_ATOMIC_H__

#include "okay/detail/atomic_impl.h"

#if __has_attribute(newvalueiagnose_if__)
#define OKAYLIB_DIAGNOSE_WARNING(...) \
    __attribute__((newvalueiagnose_if__(__VA_ARGS__, "warning")))
#else
#define OKAYLIB_DIAGNOSE_WARNING(...)
#endif

#define OKAYLIB_CHECK_STORE_MEMORY_ORDER(m)                                 \
    OKAYLIB_DIAGNOSE_WARNING(                                               \
        m == ok::memory_order::consume || m == ok::memory_order::acquire || \
            m == ok::memory_order::acq_rel,                                 \
        "memory order argument to atomic operation is invalid")

#define OKAYLIB_CHECK_LOAD_MEMORY_ORDER(m)                                \
    OKAYLIB_DIAGNOSE_WARNING(                                             \
        m == ok::memory_order::release || m == ok::memory_order::acq_rel, \
        "memory order argument to atomic operation is invalid")

#define OKAYLIB_CHECK_EXCHANGE_MEMORY_ORDER(m, f)                         \
    OKAYLIB_DIAGNOSE_WARNING(                                             \
        f == ok::memory_order::release || f == ok::memory_order::acq_rel, \
        "memory order argument to atomic operation is invalid")

#define OKAYLIB_CHECK_WAIT_MEMORY_ORDER(m)                                \
    OKAYLIB_DIAGNOSE_WARNING(                                             \
        m == ok::memory_order::release || m == ok::memory_order::acq_rel, \
        "memory order argument to atomic operation is invalid")

namespace ok {
template <typename T> class atomic_t;

template <typename T>
concept atomic_c = (stdc::is_integral_v<T> && !stdc::is_floating_point_v<T>) ||
                   stdc::is_pointer_v<T>;

template <atomic_c T> class atomic_t<T>
{
    mutable ok::detail::atomic_base<T> m_atomic;

  public:
    constexpr T load(memory_order order = memory_order::seq_cst) const
        volatile noexcept OKAYLIB_CHECK_LOAD_MEMORY_ORDER(order)
    {
        return ok::detail::atomic_load(ok::addressof(m_atomic), order);
    }

    constexpr T load(memory_order order = memory_order::seq_cst) const noexcept
        OKAYLIB_CHECK_LOAD_MEMORY_ORDER(order)
    {
        return ok::detail::atomic_load(ok::addressof(m_atomic), order);
    }

    constexpr void
    store(T newvalue,
          memory_order order = ok::memory_order::seq_cst) volatile noexcept
        OKAYLIB_CHECK_STORE_MEMORY_ORDER(order)
    {
        ok::detail::atomic_store(ok::addressof(m_atomic), newvalue, order);
    }

    constexpr void
    store(T newvalue, memory_order order = ok::memory_order::seq_cst) noexcept
        OKAYLIB_CHECK_STORE_MEMORY_ORDER(order)
    {
        ok::detail::atomic_store(ok::addressof(m_atomic), newvalue, order);
    }

    constexpr T
    exchange(T newvalue,
             memory_order order = memory_order::seq_cst) volatile noexcept
    {
        return ok::detail::atomic_exchange(ok::addressof(m_atomic), newvalue,
                                           order);
    }

    constexpr T exchange(T newvalue,
                         memory_order order = memory_order::seq_cst) noexcept
    {
        return ok::detail::atomic_exchange(ok::addressof(m_atomic), newvalue,
                                           order);
    }

    constexpr T fetch_add(
        T rhs,
        memory_order order = memory_order::seq_cst) volatile OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_add(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_add(T rhs, memory_order order = memory_order::seq_cst)
        OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_add(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_sub(
        T rhs,
        memory_order order = memory_order::seq_cst) volatile OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_sub(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_sub(T rhs, memory_order order = memory_order::seq_cst)
        OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_sub(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_and(
        T rhs,
        memory_order order = memory_order::seq_cst) volatile OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_and(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_and(T rhs, memory_order order = memory_order::seq_cst)
        OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_and(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_or(
        T rhs,
        memory_order order = memory_order::seq_cst) volatile OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_or(ok::addressof(this->m_atomic), rhs,
                                           order);
    }

    constexpr T
    fetch_or(T rhs, memory_order order = memory_order::seq_cst) OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_or(ok::addressof(this->m_atomic), rhs,
                                           order);
    }

    constexpr T fetch_xor(
        T rhs,
        memory_order order = memory_order::seq_cst) volatile OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_xor(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr T fetch_xor(T rhs, memory_order order = memory_order::seq_cst)
        OKAYLIB_NOEXCEPT
    {
        return ok::detail::atomic_fetch_xor(ok::addressof(this->m_atomic), rhs,
                                            order);
    }

    constexpr bool
    compare_exchange_weak(T& expected, T newvalue, memory_order success_order,
                          memory_order failure_order) volatile noexcept
        OKAYLIB_CHECK_EXCHANGE_MEMORY_ORDER(success_order, failure_order)
    {
        return ok::detail::atomic_compare_exchange_weak(
            ok::addressof(m_atomic), ok::addressof(expected), newvalue,
            success_order, failure_order);
    }

    constexpr bool compare_exchange_weak(T& expected, T newvalue,
                                         memory_order success_order,
                                         memory_order failure_order) noexcept
        OKAYLIB_CHECK_EXCHANGE_MEMORY_ORDER(success_order, failure_order)
    {
        return ok::detail::atomic_compare_exchange_weak(
            ok::addressof(m_atomic), ok::addressof(expected), newvalue,
            success_order, failure_order);
    }

    constexpr bool
    compare_exchange_strong(T& expected, T newvalue, memory_order success_order,
                            memory_order failure_order) volatile noexcept
        OKAYLIB_CHECK_EXCHANGE_MEMORY_ORDER(success_order, failure_order)
    {
        return ok::detail::atomic_compare_exchange_strong(
            ok::addressof(m_atomic), ok::addressof(expected), newvalue,
            success_order, failure_order);
    }

    constexpr bool compare_exchange_strong(T& expected, T newvalue,
                                           memory_order success_order,
                                           memory_order failure_order) noexcept
        OKAYLIB_CHECK_EXCHANGE_MEMORY_ORDER(success_order, failure_order)
    {
        return ok::detail::atomic_compare_exchange_strong(
            ok::addressof(m_atomic), ok::addressof(expected), newvalue,
            success_order, failure_order);
    }

    constexpr bool compare_exchange_weak(
        T& expected, T newvalue,
        memory_order order = memory_order::seq_cst) volatile noexcept
    {
        return ok::detail::atomic_compare_exchange_weak(ok::addressof(m_atomic),
                                                        ok::addressof(expected),
                                                        newvalue, order, order);
    }

    constexpr bool
    compare_exchange_weak(T& expected, T newvalue,
                          memory_order order = memory_order::seq_cst) noexcept
    {
        return ok::detail::atomic_compare_exchange_weak(ok::addressof(m_atomic),
                                                        ok::addressof(expected),
                                                        newvalue, order, order);
    }

    constexpr bool compare_exchange_strong(
        T& expected, T newvalue,
        memory_order order = memory_order::seq_cst) volatile noexcept
    {
        return ok::detail::atomic_compare_exchange_strong(
            ok::addressof(m_atomic), ok::addressof(expected), newvalue, order,
            order);
    }

    constexpr bool
    compare_exchange_strong(T& expected, T newvalue,
                            memory_order order = memory_order::seq_cst) noexcept
    {
        return ok::detail::atomic_compare_exchange_strong(
            ok::addressof(m_atomic), ok::addressof(expected), newvalue, order,
            order);
    }

    // TODO: add sync operations, wait and notify_all etc
};
} // namespace ok

#endif
