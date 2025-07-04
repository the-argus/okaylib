#ifndef __OKAYLIB_DETAIL_UTILITY_H__
#define __OKAYLIB_DETAIL_UTILITY_H__

#include "okay/detail/type_traits.h"
#include <cstddef>

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

} // namespace ok::stdc

#endif
