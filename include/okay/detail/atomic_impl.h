#ifndef __OKAYLIB_DETAIL_ATOMIC_IMPL_H__
#define __OKAYLIB_DETAIL_ATOMIC_IMPL_H__

#include "okay/detail/addressof.h"
#include "okay/detail/compiler_config.h"
#include "okay/detail/type_traits.h"

// TODO: msvc, sorry

#if __has_feature(cxx_atomic) || __has_extension(c_atomic) || \
    __ok_has_keyword(_Atomic)
#define OKAYLIB_HAS_C_ATOMIC_IMPL
#elif defined(OKAYLIB_COMPILER_GCC)
#define OKAYLIB_HAS_GCC_ATOMIC_IMPL
#endif

#if defined(OKAYLIB_HAS_C_ATOMIC_IMPL)

namespace ok {
namespace detail {
// NOTE: diabolical stuff going on here to make sure we are binary compatible
// with underlying c memory
enum legacy_memory_order
{
    __mo_relaxed,
    __mo_consume,
    __mo_acquire,
    __mo_release,
    __mo_acq_rel,
    __mo_seq_cst
};
using memory_order_underlying_t =
    ok::stdc::underlying_type_t<legacy_memory_order>;

template <typename T> struct atomic_base
{
    // NOTE: unlike std atomics, default constructor zeroes value
    constexpr atomic_base() noexcept : value({}) {}
    constexpr explicit atomic_base(T v) noexcept : value(v) {}

    __extension__ _Atomic(T) value;
};

} // namespace detail

enum class memory_order : ok::detail::memory_order_underlying_t
{
    relaxed = detail::__mo_relaxed,
    consume = detail::__mo_consume,
    acquire = detail::__mo_acquire,
    release = detail::__mo_release,
    acq_rel = detail::__mo_acq_rel,
    seq_cst = detail::__mo_seq_cst
};

static_assert(ok::stdc::is_same_v<ok::stdc::underlying_type_t<memory_order>,
                                  detail::memory_order_underlying_t>,
              "unexpected underlying type for ok::memory_order");

namespace detail {
inline void atomic_thread_fence(memory_order order) noexcept
{
    __c11_atomic_thread_fence(static_cast<memory_order_underlying_t>(order));
}

inline void atomic_signal_fence(memory_order order) noexcept
{
    __c11_atomic_signal_fence(static_cast<memory_order_underlying_t>(order));
}

template <class T> void atomic_init(atomic_base<T> volatile* a, T val) noexcept
{
    __c11_atomic_init(ok::addressof(a->value), val);
}
template <class T> void atomic_init(atomic_base<T>* a, T val) noexcept
{
    __c11_atomic_init(ok::addressof(a->value), val);
}

template <class T>
void atomic_store(atomic_base<T> volatile* a, T val,
                  memory_order order) noexcept
{
    __c11_atomic_store(ok::addressof(a->value), val,
                       static_cast<memory_order_underlying_t>(order));
}
template <class T>
void atomic_store(atomic_base<T>* a, T val, memory_order order) noexcept
{
    __c11_atomic_store(ok::addressof(a->value), val,
                       static_cast<memory_order_underlying_t>(order));
}

template <class T>
T atomic_load(atomic_base<T> const volatile* a, memory_order order) noexcept
{
    using ptr_type = ok::stdc::remove_const_t<decltype(a->value)>*;
    return __c11_atomic_load(const_cast<ptr_type>(ok::addressof(a->value)),
                             static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_load(atomic_base<T> const* a, memory_order order) noexcept
{
    using ptr_type = ok::stdc::remove_const_t<decltype(a->value)>*;
    return __c11_atomic_load(const_cast<ptr_type>(ok::addressof(a->value)),
                             static_cast<memory_order_underlying_t>(order));
}

template <class T>
void atomic_load_inplace(atomic_base<T> const volatile* a, T* dest,
                         memory_order order) noexcept
{
    using ptr_type = ok::stdc::remove_const_t<decltype(a->value)>*;
    *dest = __c11_atomic_load(const_cast<ptr_type>(ok::addressof(a->value)),
                              static_cast<memory_order_underlying_t>(order));
}
template <class T>
void atomic_load_inplace(atomic_base<T> const* a, T* dest,
                         memory_order order) noexcept
{
    using ptr_type = ok::stdc::remove_const_t<decltype(a->value)>*;
    *dest = __c11_atomic_load(const_cast<ptr_type>(ok::addressof(a->value)),
                              static_cast<memory_order_underlying_t>(order));
}

template <class T>
T atomic_exchange(atomic_base<T> volatile* a, T value,
                  memory_order order) noexcept
{
    return __c11_atomic_exchange(ok::addressof(a->value), value,
                                 static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_exchange(atomic_base<T>* a, T value, memory_order order) noexcept
{
    return __c11_atomic_exchange(ok::addressof(a->value), value,
                                 static_cast<memory_order_underlying_t>(order));
}

inline constexpr memory_order to_failure_order(memory_order order)
{
    return order == memory_order::release
               ? memory_order::relaxed
               : (order == memory_order::acq_rel ? memory_order::acquire
                                                 : order);
}

template <class T>
bool atomic_compare_exchange_strong(atomic_base<T> volatile* a, T* expected,
                                    T value, memory_order success,
                                    memory_order failure) noexcept
{
    return __c11_atomic_compare_exchange_strong(
        ok::addressof(a->value), expected, value,
        static_cast<memory_order_underlying_t>(success),
        static_cast<memory_order_underlying_t>(to_failure_order(failure)));
}
template <class T>
bool atomic_compare_exchange_strong(atomic_base<T>* a, T* expected, T value,
                                    memory_order success,
                                    memory_order failure) noexcept
{
    return __c11_atomic_compare_exchange_strong(
        ok::addressof(a->value), expected, value,
        static_cast<memory_order_underlying_t>(success),
        static_cast<memory_order_underlying_t>(to_failure_order(failure)));
}

template <class T>
bool atomic_compare_exchange_weak(atomic_base<T> volatile* a, T* expected,
                                  T value, memory_order success,
                                  memory_order failure) noexcept
{
    return __c11_atomic_compare_exchange_weak(
        ok::addressof(a->value), expected, value,
        static_cast<memory_order_underlying_t>(success),
        static_cast<memory_order_underlying_t>(to_failure_order(failure)));
}
template <class T>
bool atomic_compare_exchange_weak(atomic_base<T>* a, T* expected, T value,
                                  memory_order success,
                                  memory_order failure) noexcept
{
    return __c11_atomic_compare_exchange_weak(
        ok::addressof(a->value), expected, value,
        static_cast<memory_order_underlying_t>(success),
        static_cast<memory_order_underlying_t>(to_failure_order(failure)));
}

template <class T>
T atomic_fetch_add(atomic_base<T> volatile* a, T delta,
                   memory_order order) noexcept
{
    return __c11_atomic_fetch_add(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_fetch_add(atomic_base<T>* a, T delta, memory_order order) noexcept
{
    return __c11_atomic_fetch_add(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}

template <class T>
T* atomic_fetch_add(atomic_base<T*> volatile* a, ptrdiff_t delta,
                    memory_order order) noexcept
{
    return __c11_atomic_fetch_add(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T* atomic_fetch_add(atomic_base<T*>* a, ptrdiff_t delta,
                    memory_order order) noexcept
{
    return __c11_atomic_fetch_add(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}

template <class T>
T atomic_fetch_sub(atomic_base<T> volatile* a, T delta,
                   memory_order order) noexcept
{
    return __c11_atomic_fetch_sub(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_fetch_sub(atomic_base<T>* a, T delta, memory_order order) noexcept
{
    return __c11_atomic_fetch_sub(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T* atomic_fetch_sub(atomic_base<T*> volatile* a, ptrdiff_t delta,
                    memory_order order) noexcept
{
    return __c11_atomic_fetch_sub(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T* atomic_fetch_sub(atomic_base<T*>* a, ptrdiff_t delta,
                    memory_order order) noexcept
{
    return __c11_atomic_fetch_sub(
        ok::addressof(a->value), delta,
        static_cast<memory_order_underlying_t>(order));
}

template <class T>
T atomic_fetch_and(atomic_base<T> volatile* a, T pattern,
                   memory_order order) noexcept
{
    return __c11_atomic_fetch_and(
        ok::addressof(a->value), pattern,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_fetch_and(atomic_base<T>* a, T pattern, memory_order order) noexcept
{
    return __c11_atomic_fetch_and(
        ok::addressof(a->value), pattern,
        static_cast<memory_order_underlying_t>(order));
}

template <class T>
T atomic_fetch_or(atomic_base<T> volatile* a, T pattern,
                  memory_order order) noexcept
{
    return __c11_atomic_fetch_or(ok::addressof(a->value), pattern,
                                 static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_fetch_or(atomic_base<T>* a, T pattern, memory_order order) noexcept
{
    return __c11_atomic_fetch_or(ok::addressof(a->value), pattern,
                                 static_cast<memory_order_underlying_t>(order));
}

template <class T>
T atomic_fetch_xor(atomic_base<T> volatile* a, T pattern,
                   memory_order order) noexcept
{
    return __c11_atomic_fetch_xor(
        ok::addressof(a->value), pattern,
        static_cast<memory_order_underlying_t>(order));
}
template <class T>
T atomic_fetch_xor(atomic_base<T>* a, T pattern, memory_order order) noexcept
{
    return __c11_atomic_fetch_xor(
        ok::addressof(a->value), pattern,
        static_cast<memory_order_underlying_t>(order));
}
} // namespace detail

} // namespace ok

#elif defined(OKAYLIB_HAS_GCC_ATOMIC_IMPL)
#error "okaylib does not yet support gcc atomic implementation"
#endif

#endif
