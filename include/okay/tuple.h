#ifndef __OKAYLIB_TUPLE_H__
#define __OKAYLIB_TUPLE_H__

#include "okay/detail/invoke.h"
#include "okay/detail/type_traits.h"
#include "okay/detail/utility.h"
#include <cstddef>

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
#include <tuple>
#elif defined(OKAYLIB_COMPAT_STRATEGY_NO_STD) || \
    defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
namespace std {
template <typename> struct tuple_size;
template <size_t, typename> struct tuple_element;
} // namespace std
#endif

namespace ok {

template <size_t N, typename head_t, typename... tail_t>
struct tuple_element_impl
{
    using type = typename tuple_element_impl<N - 1, tail_t...>::type;
};

template <typename head_t, typename... tail_t>
struct tuple_element_impl<0, head_t, tail_t...>
{
    using type = head_t;
};

template <typename... types_t> class tuple;

template <size_t N, typename T> struct tuple_element;

template <size_t N, typename... types_t>
struct tuple_element<N, tuple<types_t...>>
{
    static_assert(N < sizeof...(types_t), "tuple_element index out of bounds");
    using type = typename tuple_element_impl<N, types_t...>::type;
};

template <size_t N, typename T,
          bool is_empty = stdc::is_empty<T>::value && !stdc::is_final<T>::value>
struct tuple_leaf
{
    T value;
    constexpr tuple_leaf() = default;
    template <typename u_t>
    constexpr tuple_leaf(u_t&& val) : value(stdc::forward<u_t>(val))
    {
    }

    template <size_t N_get, typename... args_t_get>
    friend constexpr typename tuple_element<N_get, tuple<args_t_get...>>::type&
    get(tuple<args_t_get...>& t) noexcept;
    template <size_t N_get, typename... args_t_get>
    friend constexpr const typename tuple_element<N_get,
                                                  tuple<args_t_get...>>::type&
    get(const tuple<args_t_get...>& t) noexcept;
    template <size_t N_get, typename... args_t_get>
    friend constexpr typename tuple_element<N_get, tuple<args_t_get...>>::type&&
    get(tuple<args_t_get...>&& t) noexcept;
    template <typename T_get, typename... args_t_get>
    friend constexpr T_get& get(tuple<args_t_get...>& t) noexcept;
    template <typename T_get, typename... args_t_get>
    friend constexpr const T_get& get(const tuple<args_t_get...>& t) noexcept;
    template <typename T_get, typename... args_t_get>
    friend constexpr T_get&& get(tuple<args_t_get...>&& t) noexcept;

  private:
    T& get_element_ref() noexcept { return value; }
    const T& get_element_ref() const noexcept { return value; }
    T&& get_element_rref() noexcept { return stdc::move(value); }
};

template <size_t N, typename T> struct tuple_leaf<N, T, true> : T
{ // inherit from T for EBO
    constexpr tuple_leaf() = default;
    template <typename u_t>
    constexpr tuple_leaf(u_t&& val) : T(stdc::forward<u_t>(val))
    {
    }

    template <size_t N_get, typename... args_t_get>
    friend constexpr typename tuple_element<N_get, tuple<args_t_get...>>::type&
    get(tuple<args_t_get...>& t) noexcept;
    template <size_t N_get, typename... args_t_get>
    friend constexpr const typename tuple_element<N_get,
                                                  tuple<args_t_get...>>::type&
    get(const tuple<args_t_get...>& t) noexcept;
    template <size_t N_get, typename... args_t_get>
    friend constexpr typename tuple_element<N_get, tuple<args_t_get...>>::type&&
    get(tuple<args_t_get...>&& t) noexcept;
    template <typename T_get, typename... args_t_get>
    friend constexpr T_get& get(tuple<args_t_get...>& t) noexcept;
    template <typename T_get, typename... args_t_get>
    friend constexpr const T_get& get(const tuple<args_t_get...>& t) noexcept;
    template <typename T_get, typename... args_t_get>
    friend constexpr T_get&& get(tuple<args_t_get...>&& t) noexcept;

  private:
    T& get_element_ref() noexcept { return *this; }
    const T& get_element_ref() const noexcept { return *this; }
    T&& get_element_rref() noexcept
    {
        return stdc::move(*static_cast<T*>(this));
    }
};

template <typename indices, typename... types_t> struct tuple_base;

// template recursion base case
template <size_t... indices> struct tuple_base<stdc::index_sequence<indices...>>
{
    constexpr tuple_base() = default;
    void swap(tuple_base&) noexcept {}
};

// recursive case
template <size_t index, size_t... remaining_indices, typename head_t,
          typename... tail_t>
struct tuple_base<stdc::index_sequence<index, remaining_indices...>, head_t,
                  tail_t...>
    : tuple_leaf<index, head_t>,
      tuple_base<stdc::index_sequence<remaining_indices...>, tail_t...>
{
  private:
    using current_leaf = tuple_leaf<index, head_t>;
    using next_base =
        tuple_base<stdc::index_sequence<remaining_indices...>, tail_t...>;

  public:
    constexpr tuple_base() = default;

    template <typename h1_t, typename... t1_t>
        requires stdc::is_constructible<head_t, h1_t&&>::value
    constexpr tuple_base(h1_t&& h, t1_t&&... t)
        : current_leaf(stdc::forward<h1_t>(h)),
          next_base(stdc::forward<t1_t>(t)...)
    {
    }

    constexpr tuple_base(const tuple_base& other) = default;
    constexpr tuple_base(tuple_base&& other) = default;
    tuple_base& operator=(const tuple_base& other) = default;
    tuple_base& operator=(tuple_base&& other) = default;

    template <typename h1_t, typename... t1_t>
        requires stdc::is_constructible<head_t, const h1_t&>::value
    constexpr tuple_base(
        const tuple_base<stdc::index_sequence<index, remaining_indices...>,
                         h1_t, t1_t...>& other)
        : current_leaf(static_cast<const tuple_leaf<index, h1_t>&>(other)
                           .get_element_ref()),
          next_base(
              static_cast<const tuple_base<
                  stdc::index_sequence<remaining_indices...>, t1_t...>&>(other))
    {
    }

    template <typename h1_t, typename... t1_t>
        requires stdc::is_constructible<head_t, h1_t&&>::value
    constexpr tuple_base(
        tuple_base<stdc::index_sequence<index, remaining_indices...>, h1_t,
                   t1_t...>&& other)
        : current_leaf(
              static_cast<tuple_leaf<index, h1_t>&&>(other).get_element_rref()),
          next_base(
              static_cast<tuple_base<stdc::index_sequence<remaining_indices...>,
                                     t1_t...>&&>(other))
    {
    }

    template <typename h1_t, typename... t1_t>
        requires stdc::is_assignable<head_t&, const h1_t&>::value
    tuple_base& operator=(
        const tuple_base<stdc::index_sequence<index, remaining_indices...>,
                         h1_t, t1_t...>& other)
    {
        static_cast<current_leaf&>(*this).get_element_ref() =
            static_cast<const tuple_leaf<index, h1_t>&>(other)
                .get_element_rref();
        static_cast<next_base&>(*this).operator=(
            static_cast<const next_base&>(other));
        return *this;
    }

    template <typename h1_t, typename... t1_t>
        requires stdc::is_assignable<head_t&, h1_t&&>::value
    tuple_base&
    operator=(tuple_base<stdc::index_sequence<index, remaining_indices...>,
                         h1_t, t1_t...>&& other)
    {
        static_cast<current_leaf&>(*this).get_element_ref() =
            static_cast<tuple_leaf<index, h1_t>&&>(other).get_element_ref();
        static_cast<next_base&>(*this).operator=(
            static_cast<next_base&&>(other));
        return *this;
    }

    void swap(tuple_base& other) noexcept(
        noexcept(stdc::swap(stdc::declval<head_t&>(),
                            stdc::declval<head_t&>())) &&
        noexcept(stdc::declval<next_base&>().swap(stdc::declval<next_base&>())))
    {
        stdc::swap(static_cast<current_leaf&>(*this).get_element_ref(),
                   static_cast<current_leaf&>(other).get_element_ref());
        static_cast<next_base&>(*this).swap(static_cast<next_base&>(other));
    }
};

template <typename... types_t>
class tuple : public tuple_base<stdc::make_index_sequence<sizeof...(types_t)>,
                                types_t...>
{
  private:
    using base =
        tuple_base<stdc::make_index_sequence<sizeof...(types_t)>, types_t...>;

    template <size_t N_get, typename... args_t_get>
    friend constexpr typename tuple_element<N_get, tuple<args_t_get...>>::type&
    get(tuple<args_t_get...>& t) noexcept;

    template <size_t N_get, typename... args_t_get>
    friend constexpr const typename tuple_element<N_get,
                                                  tuple<args_t_get...>>::type&
    get(const tuple<args_t_get...>& t) noexcept;

    template <size_t N_get, typename... args_t_get>
    friend constexpr typename tuple_element<N_get, tuple<args_t_get...>>::type&&
    get(tuple<args_t_get...>&& t) noexcept;

    template <typename T_get, typename... args_t_get>
    friend constexpr T_get& get(tuple<args_t_get...>& t) noexcept;

    template <typename T_get, typename... args_t_get>
    friend constexpr const T_get& get(const tuple<args_t_get...>& t) noexcept;

    template <typename T_get, typename... args_t_get>
    friend constexpr T_get&& get(tuple<args_t_get...>&& t) noexcept;

  public:
    constexpr tuple() = default;
    constexpr tuple(const tuple& other) = default;
    constexpr tuple(tuple&& other) = default;
    tuple& operator=(const tuple& other) = default;
    tuple& operator=(tuple&& other) = default;

    template <typename... args_t>
        requires(sizeof...(args_t) == sizeof...(types_t) &&
                 stdc::conjunction<
                     stdc::is_constructible<types_t, args_t &&>...>::value)
    constexpr tuple(args_t&&... args) : base(stdc::forward<args_t>(args)...)
    {
    }

    // conversion copy constructor from another tuple with potentially different
    // types
    template <typename... other_types_t>
        requires(sizeof...(other_types_t) == sizeof...(types_t) &&
                 stdc::conjunction<stdc::is_constructible<
                     types_t, const other_types_t&>...>::value)
    constexpr tuple(const tuple<other_types_t...>& other) : base(other)
    {
    }

    // conversion move constructor from another tuple with potentially different
    // types
    template <typename... other_types_t>
        requires(
            sizeof...(other_types_t) == sizeof...(types_t) &&
            stdc::conjunction<
                stdc::is_constructible<types_t, other_types_t &&>...>::value)
    constexpr tuple(tuple<other_types_t...>&& other) : base(stdc::move(other))
    {
    }

    template <typename... other_types_t>
        requires(
            sizeof...(other_types_t) == sizeof...(types_t) &&
            stdc::conjunction<
                stdc::is_assignable<types_t&, const other_types_t&>...>::value)
    tuple& operator=(const tuple<other_types_t...>& other)
    {
        base::operator=(other);
        return *this;
    }

    template <typename... other_types_t>
        requires(sizeof...(other_types_t) == sizeof...(types_t) &&
                 stdc::conjunction<
                     stdc::is_assignable<types_t&, other_types_t &&>...>::value)
    tuple& operator=(tuple<other_types_t...>&& other)
    {
        base::operator=(stdc::move(other));
        return *this;
    }

    void swap(tuple& other) noexcept(
        noexcept(stdc::declval<base&>().swap(stdc::declval<base&>())))
    {
        base::swap(other);
    }
};

// TODO: ok::swap customization point
// template<typename... types_t>
// void swap(tuple<types_t...>& lhs, tuple<types_t...>& rhs)
// noexcept(noexcept(lhs.swap(rhs))) {
//     lhs.swap(rhs);
// }

template <typename T, typename... types_t> struct type_index_finder;

template <typename T, typename head_t, typename... tail_t>
struct type_index_finder<T, head_t, tail_t...>
{
    static constexpr size_t value = 1 + type_index_finder<T, tail_t...>::value;
};

template <typename T, typename... tail_t>
struct type_index_finder<T, T, tail_t...>
{
    static constexpr size_t value = 0;
};

template <typename T, typename tuple_type> struct count_type;

template <typename T, typename... types_t>
struct count_type<T, tuple<types_t...>>
{
    static constexpr size_t value =
        (static_cast<size_t>(stdc::is_same<T, types_t>::value) + ... + 0);
};

template <size_t N, typename... types_t>
constexpr typename tuple_element<N, tuple<types_t...>>::type&
get(tuple<types_t...>& t) noexcept
{
    using element_type = typename tuple_element<N, tuple<types_t...>>::type;
    return static_cast<tuple_leaf<N, element_type>&>(t).get_element_ref();
}

template <size_t N, typename... types_t>
constexpr const typename tuple_element<N, tuple<types_t...>>::type&
get(const tuple<types_t...>& t) noexcept
{
    using element_type = typename tuple_element<N, tuple<types_t...>>::type;
    return static_cast<const tuple_leaf<N, element_type>&>(t).get_element_ref();
}

template <size_t N, typename... types_t>
constexpr typename tuple_element<N, tuple<types_t...>>::type&&
get(tuple<types_t...>&& t) noexcept
{
    using element_type = typename tuple_element<N, tuple<types_t...>>::type;
    return static_cast<element_type&&>(
        static_cast<tuple_leaf<N, element_type>&&>(t).get_element_rref());
}

template <typename T, typename... types_t>
    requires(count_type<T, tuple<types_t...>>::value == 1)
constexpr T& get(tuple<types_t...>& t) noexcept
{
    constexpr size_t N = type_index_finder<T, types_t...>::value;
    return static_cast<tuple_leaf<N, T>&>(t).get_element_ref();
}

template <typename T, typename... types_t>
    requires(count_type<T, tuple<types_t...>>::value == 1)
constexpr const T& get(const tuple<types_t...>& t) noexcept
{
    constexpr size_t N = type_index_finder<T, types_t...>::value;
    return static_cast<const tuple_leaf<N, T>&>(t).get_element_ref();
}

template <typename T, typename... types_t>
    requires(count_type<T, tuple<types_t...>>::value == 1)
constexpr T&& get(tuple<types_t...>&& t) noexcept
{
    constexpr size_t N = type_index_finder<T, types_t...>::value;
    return static_cast<T&&>(
        static_cast<tuple_leaf<N, T>&&>(t).get_element_rref());
}

template <typename... args_t>
constexpr tuple<typename stdc::decay<args_t>::type...>
make_tuple(args_t&&... args)
{
    return tuple<typename stdc::decay<args_t>::type...>(
        stdc::forward<args_t>(args)...);
}

template <typename... args_t>
constexpr tuple<args_t&&...> forward_as_tuple(args_t&&... args) noexcept
{
    return tuple<args_t&&...>(stdc::forward<args_t>(args)...);
}
} // namespace ok

// std specializations so structured bindings work
namespace std {

template <typename... types_t>
struct tuple_size<ok::tuple<types_t...>>
    : public ok::stdc::integral_constant<size_t, sizeof...(types_t)>
{};

template <size_t N, typename... types_t>
struct tuple_element<N, ok::tuple<types_t...>>
    : public ok::tuple_element<N, ok::tuple<types_t...>>
{};

template <size_t N, typename... types_t>
constexpr typename ok::tuple_element<N, ok::tuple<types_t...>>::type&
get(ok::tuple<types_t...>& t) noexcept
{
    return ok::get<N>(t);
}

template <size_t N, typename... types_t>
constexpr const typename ok::tuple_element<N, ok::tuple<types_t...>>::type&
get(const ok::tuple<types_t...>& t) noexcept
{
    return ok::get<N>(t);
}

template <size_t N, typename... types_t>
constexpr typename ok::tuple_element<N, ok::tuple<types_t...>>::type&&
get(ok::tuple<types_t...>&& t) noexcept
{
    return ok::get<N>(ok::stdc::move(t));
}

} // namespace std

namespace ok::detail {
template <class callable_t, class tuple_t, size_t... I>
constexpr decltype(auto) apply_impl(callable_t&& c, tuple_t&& t,
                                    stdc::index_sequence<I...>)
{
    return ok::invoke(stdc::forward<callable_t>(c),
                      std::get<I>(stdc::forward<tuple_t>(t))...);
}
} // namespace ok::detail

namespace ok {
template <typename callable_t, typename tuple_t>
constexpr decltype(auto) apply(callable_t&& c, tuple_t&& tuple)
{
    return detail::apply_impl(
        stdc::forward<callable_t>(c), stdc::forward<tuple_t>(tuple),
        // NOTE: using std::tuple_size here is okay, we declared our
        // specialization for ok::tuple already
        stdc::make_index_sequence<
            std::tuple_size<std::decay_t<tuple_t>>::value>{});
}
} // namespace ok

#endif
