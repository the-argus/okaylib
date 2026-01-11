#ifndef __OKAYLIB_DETAIL_UTILITY_H__
#define __OKAYLIB_DETAIL_UTILITY_H__

#include "okay/detail/type_traits.h"
#include <cstddef>

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
#include <bit> // this header defines stdc::bit_cast
#elif defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
#include <cstring> // uses memcpy to polyfill stdc::bit_cast
#endif

namespace ok::stdc {

template <typename T>
[[nodiscard]] constexpr stdc::remove_reference_t<T>&& move(T&& t) noexcept
{
    return static_cast<stdc::remove_reference_t<T>&&>(t);
}

template <typename T>
[[nodiscard]] constexpr T&& forward(stdc::remove_reference_t<T>& t) noexcept
{
    return static_cast<T&&>(t);
}

template <typename T>
[[nodiscard]] constexpr T&& forward(stdc::remove_reference_t<T>&& t) noexcept
{
    static_assert(!stdc::is_lvalue_reference_v<T>,
                  "Bad forward call: cannot forward an rvalue as an lvalue.");
    return static_cast<T&&>(t);
}

template <typename T> stdc::add_rvalue_reference_t<T> declval() noexcept;

template <typename T>
void swap(T& a, T& b) noexcept(stdc::is_nothrow_move_constructible_v<T> &&
                               stdc::is_nothrow_move_assignable_v<T>)
{
    T temp = ok::stdc::move(a);
    a = ok::stdc::move(b);
    b = ok::stdc::move(temp);
}

template <size_t... Is> struct index_sequence
{};

template <size_t N, size_t... Is>
struct make_index_sequence_helper
    : make_index_sequence_helper<N - 1, N - 1, Is...>
{};

template <size_t... Is> struct make_index_sequence_helper<0, Is...>
{
    using type = index_sequence<Is...>;
};

template <size_t N>
using make_index_sequence = typename make_index_sequence_helper<N>::type;

template <typename...> struct conjunction : stdc::true_type
{};

template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : stdc::conditional_t<static_cast<bool>(B1::value), conjunction<Bn...>, B1>
{};

template <typename T, typename U = T>
[[nodiscard]] constexpr decltype(auto) exchange(T& item, U&& newvalue)
{
    T old_value = stdc::move(item);
    item = stdc::forward<U>(newvalue);
    return old_value;
}

[[nodiscard]] constexpr bool is_constant_evaluated() noexcept
{
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
    return ::std::is_constant_evaluated();
#elif defined(OKAYLIB_COMPAT_STRATEGY_NO_STD)
    return __builtin_is_constant_evaluated();
#elif defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
    return false;
#endif
}

template <typename to_t, typename from_t>
[[nodiscard]]
constexpr to_t bit_cast(const from_t& from) noexcept
    requires(sizeof(to_t) == sizeof(from_t)) &&
            stdc::is_trivially_copyable_v<to_t> &&
            stdc::is_trivially_copyable_v<from_t>
{
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
    return ::std::bit_cast<to_t>(from);
#elif defined(OKAYLIB_COMPAT_STRATEGY_NO_STD)
    return __builtin_bit_cast(to_t, from);
#elif defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
    static_assert(
        stdc::is_trivially_constructible_v<to_t>,
        "pure cpp implementation of bit_cast additionally requires that the "
        "type being cast to is trivially default constructible.");

    to_t dst;
    ::memcpy(&dst, &from, sizeof(to_t));
    return dst;
#endif
}

} // namespace ok::stdc

#endif
