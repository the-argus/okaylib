#ifndef __OKAYLIB_TUPLE_H__
#define __OKAYLIB_TUPLE_H__

#include "okay/detail/invoke.h"
#include "okay/detail/no_unique_addr.h"
#include "okay/detail/template_util/remove_cvref.h"
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

template <typename... elements_t> class tuple;

namespace detail {
// c++20 no unique address is standard, so we can avoid EBO conditional
// inheritance and etc and just mark the type as no unique address
template <size_t index, typename payload_t> struct element_container_t
{
    constexpr element_container_t() = default;
    constexpr element_container_t(element_container_t&&) = default;
    constexpr element_container_t(const element_container_t&) = default;
    constexpr element_container_t& operator=(element_container_t&&) = default;
    constexpr element_container_t&
    operator=(const element_container_t&) = default;

    constexpr element_container_t(const payload_t& h) : m_head(h) {}

    template <typename other_t>
        requires(stdc::is_constructible_v<payload_t, other_t &&>)
    constexpr element_container_t(other_t&& other)
        : m_head(stdc::forward<other_t>(other))
    {
    }

    static constexpr payload_t& get_elem(element_container_t& base) noexcept
    {
        return base.m_head;
    }
    static constexpr const payload_t&
    get_elem(const element_container_t& base) noexcept
    {
        return base.m_head;
    }

    OKAYLIB_NO_UNIQUE_ADDR payload_t m_head;
};

template <size_t index, typename... elements_t> struct tuple_impl_t;

template <size_t index, typename element_t, typename... tail_t>
struct tuple_impl_t<index, element_t, tail_t...>
    : public tuple_impl_t<index + 1, tail_t...>,
      private element_container_t<index, element_t>
{
    template <size_t, typename...> friend struct tuple_impl_t;

    using parent_t = tuple_impl_t<index + 1, tail_t...>;
    using container_t = element_container_t<index, element_t>;

    static constexpr element_t& get_head(tuple_impl_t& self)
    {
        return container_t::get_elem(self);
    }
    static constexpr const element_t& get_head(const tuple_impl_t& self)
    {
        return container_t::get_elem(self);
    }

    static constexpr parent_t& get_tail(tuple_impl_t& t) { return t; }
    static constexpr const parent_t& get_tail(const tuple_impl_t& t)
    {
        return t;
    }

    explicit constexpr tuple_impl_t(const container_t& elem,
                                    const tail_t&... tail)
        : parent_t(tail...), container_t(elem)
    {
    }

    template <typename head_convertible_t, typename... tail_convertible_t>
        requires(sizeof...(tail_convertible_t) == sizeof...(tail_t))
    explicit constexpr tuple_impl_t(head_convertible_t&& head,
                                    tail_convertible_t&&... tail)
        : parent_t(stdc::forward<tail_convertible_t>(tail)...),
          container_t(stdc::forward<head_convertible_t>(head))
    {
    }

    constexpr tuple_impl_t() = default;
    constexpr tuple_impl_t(const tuple_impl_t&) = default;
    constexpr tuple_impl_t& operator=(const tuple_impl_t&) = default;
    constexpr tuple_impl_t(tuple_impl_t&&) = default;
    constexpr tuple_impl_t& operator=(tuple_impl_t&&) = default;

    template <typename... other_elements_t>
    constexpr tuple_impl_t(
        const tuple_impl_t<index, other_elements_t...>& other)
        : parent_t(tuple_impl_t<index, other_elements_t...>::get_tail(other)),
          container_t(tuple_impl_t<index, other_elements_t...>::get_head(other))
    {
    }

    template <typename other_head_t, typename... other_tail_t>
    constexpr tuple_impl_t(
        tuple_impl_t<index, other_head_t, other_tail_t...>&& other)
        : parent_t(stdc::move(
              tuple_impl_t<index, other_head_t, other_tail_t...>::get_tail(
                  other))),
          container_t(stdc::forward<other_head_t>(
              tuple_impl_t<index, other_head_t, other_tail_t...>::get_head(
                  other)))
    {
    }

    template <typename... other_elements_t>
    constexpr void assign(const tuple_impl_t<index, other_elements_t...>& other)
    {
        get_head(*this) =
            tuple_impl_t<index, other_elements_t...>::get_head(other);
        get_tail(*this).assign(
            tuple_impl_t<index, other_elements_t...>::get_tail(other));
    }

    template <typename other_head_t, typename... other_tail_t>
    constexpr void
    assign(tuple_impl_t<index, other_head_t, other_tail_t...>&& other)
    {
        get_head(*this) = stdc::forward<other_head_t>(
            tuple_impl_t<index, other_head_t, other_tail_t...>::get_head(
                other));
        get_tail(*this).assign(stdc::move(
            tuple_impl_t<index, other_head_t, other_tail_t...>::get_tail(
                other)));
    }
};

template <size_t index, typename head_t>
struct tuple_impl_t<index, head_t> : private element_container_t<index, head_t>
{
    template <size_t, typename...> friend struct tuple_impl_t;

    using container_t = element_container_t<index, head_t>;

    static constexpr head_t& get_head(tuple_impl_t& t) noexcept
    {
        return container_t::get_elem(t);
    }
    static constexpr const head_t& get_head(const tuple_impl_t& t) noexcept
    {
        return container_t::get_elem(t);
    }

    constexpr tuple_impl_t() = default;
    constexpr tuple_impl_t(const tuple_impl_t&) = default;
    constexpr tuple_impl_t& operator=(const tuple_impl_t&) = default;
    constexpr tuple_impl_t(tuple_impl_t&&) = default;
    constexpr tuple_impl_t& operator=(tuple_impl_t&&) = default;

    // construct directly from the stored element
    template <typename other_head_t>
    explicit constexpr tuple_impl_t(other_head_t&& other_head)
        requires stdc::is_constructible_v<container_t, decltype(other_head)>
        : container_t(stdc::forward<other_head_t>(other_head))
    {
    }

    template <typename other_head_t>
    constexpr void assign(const tuple_impl_t<index, other_head_t>& other)
    {
        get_head(*this) = tuple_impl_t<index, other_head_t>::get_head(other);
    }

    template <typename other_head_t>
    constexpr void assign(tuple_impl_t<index, other_head_t>&& other)
    {
        get_head(*this) = stdc::forward<other_head_t>(
            tuple_impl_t<index, other_head_t>::get_head(other));
    }
};

template <bool precondition, typename... types_t> struct tuple_constraints_t
{
    template <typename... other_types_t>
    constexpr static bool constructible =
        (stdc::is_constructible_v<types_t, other_types_t> && ...);
    template <typename... other_types_t>
    constexpr static bool convertible =
        (stdc::is_convertible_v<other_types_t, types_t> && ...);

    template <typename... other_types_t>
    constexpr static bool is_implicitly_constructible =
        constructible<other_types_t...> && convertible<other_types_t...>;
    template <typename... other_types_t>
    constexpr static bool is_explicitly_constructible =
        constructible<other_types_t...> && (!convertible<other_types_t...>);

    constexpr static bool is_implicitly_default_constructible =
        (stdc::is_implicitly_default_constructible_v<types_t> && ...);

    constexpr static bool is_explicitly_default_constructible =
        (stdc::is_default_constructible_v<types_t> && ...) &&
        !(stdc::is_implicitly_default_constructible_v<types_t> && ...);
};

template <typename... types_t> struct tuple_constraints_t<false, types_t...>
{
    template <typename... other_types_t>
    constexpr static bool constructible = false;
    template <typename... other_types_t>
    constexpr static bool convertible = false;

    template <typename... other_types_t>
    constexpr static bool is_implicitly_constructible = false;
    template <typename... other_types_t>
    constexpr static bool is_explicitly_constructible = false;

    constexpr static bool is_implicitly_default_constructible = false;

    constexpr static bool is_explicitly_default_constructible = false;
};

} // namespace detail

template <typename... elements_t>
class tuple : public detail::tuple_impl_t<0, elements_t...>
{
    static_assert(
        !(stdc::is_array_v<elements_t> || ...),
        "C style arrays cannot be tuple elements by value, if this behavior is "
        "needed consider an ok::array_t or, if by value storage is not needed, "
        "storing a const reference to the array.");
    using parent_t = detail::tuple_impl_t<0, elements_t...>;

    // TODO: can we remove the precondition on constraints in favor of just
    // doing precondition && constraint::thing<...>?
    template <bool cond>
    using constraints = detail::tuple_constraints_t<cond, elements_t...>;

    // check if a single argument is valid constructor arguments
    template <typename other_t> static constexpr bool valid_args()
    {
        return sizeof...(elements_t) == 1 &&
               !stdc::is_same_v<tuple, ok::detail::remove_cvref_t<other_t>>;
    }

    // check if multiple arguments are valid constructor arguments
    template <typename, typename, typename... tail_t>
    static constexpr bool valid_args()
    {
        return (sizeof...(tail_t) + 2) == sizeof...(elements_t);
    }

    template <typename other_t>
    constexpr static bool no_tuple_converting_constructor = requires {
        requires sizeof...(elements_t) == 1;
        requires stdc::is_convertible_v<tuple, elements_t...> ||
                     stdc::is_constructible_v<elements_t..., tuple> ||
                     stdc::is_same_v<elements_t..., other_t>;
    };

    template <typename... other_elements_t>
    using tuple_converting_constraints = constraints<
        (sizeof...(elements_t) == sizeof...(other_elements_t)) &&
        !no_tuple_converting_constructor<const tuple<other_elements_t...>&>>;

  public:
    constexpr tuple() OKAYLIB_NOEXCEPT
        requires constraints<true>::is_implicitly_default_constructible
    = default;
    explicit constexpr tuple() OKAYLIB_NOEXCEPT
        requires constraints<true>::is_explicitly_default_constructible
    = default;

    constexpr tuple(const elements_t&... elements) OKAYLIB_NOEXCEPT
        requires constraints<sizeof...(elements_t) >= 1>::template
    is_implicitly_constructible<const elements_t&...> : parent_t(elements...)
    {
    }
    explicit constexpr tuple(const elements_t&... elements) OKAYLIB_NOEXCEPT
        requires constraints<sizeof...(elements_t) >= 1>::template
    is_explicitly_constructible<const elements_t&...> : parent_t(elements...)
    {
    }

    template <typename... element_constructor_args_t>
    constexpr tuple(element_constructor_args_t&&... args) OKAYLIB_NOEXCEPT
        requires constraints<
            valid_args<element_constructor_args_t...>()>::template
    is_implicitly_constructible<element_constructor_args_t...>
        : parent_t(stdc::forward<element_constructor_args_t>(args)...)
    {
    }

    template <typename... element_constructor_args_t>
    explicit constexpr tuple(element_constructor_args_t&&... args)
        OKAYLIB_NOEXCEPT
        requires constraints<
            valid_args<element_constructor_args_t...>()>::template
    is_explicitly_constructible<element_constructor_args_t...>
        : parent_t(stdc::forward<element_constructor_args_t>(args)...)
    {
    }

    constexpr tuple(const tuple&) = default;
    constexpr tuple(tuple&&) = default;

    template <typename... other_elements_t>
        requires(tuple_converting_constraints<other_elements_t...>::
                     template is_implicitly_constructible<
                         const other_elements_t&...>)
    constexpr tuple(const tuple<other_elements_t...>& other) OKAYLIB_NOEXCEPT
        : parent_t(
              static_cast<const detail::tuple_impl_t<0, other_elements_t...>&>(
                  other))
    {
    }

    template <typename... other_elements_t>
        requires(tuple_converting_constraints<other_elements_t...>::
                     template is_explicitly_constructible<
                         const other_elements_t&...>)
    explicit constexpr tuple(const tuple<other_elements_t...>& other)
        OKAYLIB_NOEXCEPT
        : parent_t(
              static_cast<const detail::tuple_impl_t<0, other_elements_t...>&>(
                  other))
    {
    }

    template <typename... other_elements_t>
        requires(
            tuple_converting_constraints<other_elements_t...>::
                template is_implicitly_constructible<other_elements_t && ...>)
    constexpr tuple(tuple<other_elements_t...>&& other) OKAYLIB_NOEXCEPT
        : parent_t(static_cast<detail::tuple_impl_t<0, other_elements_t...>&&>(
              other))
    {
    }

    template <typename... other_elements_t>
        requires(
            tuple_converting_constraints<other_elements_t...>::
                template is_explicitly_constructible<other_elements_t && ...>)
    explicit constexpr tuple(tuple<other_elements_t...>&& other)
        OKAYLIB_NOEXCEPT
        : parent_t(static_cast<detail::tuple_impl_t<0, other_elements_t...>&&>(
              other))
    {
    }

    constexpr tuple& operator=(const tuple&) = default;
    constexpr tuple& operator=(tuple&&) = default;

    // copy assignment only enabled if all elements are copy assignable, and not
    // all the elements are trivially copy assignable (in which case generated
    // implementation is fine)
    constexpr tuple& operator=(const tuple& other)
        requires((stdc::is_copy_assignable_v<elements_t> && ...) &&
                 !(stdc::is_trivially_copy_assignable_v<elements_t> && ...))
    {
        this->assign(other);
        return *this;
    }

    constexpr tuple& operator=(tuple&& other)
        requires((stdc::is_move_assignable_v<elements_t> && ...) &&
                 !(stdc::is_trivially_move_assignable_v<elements_t> && ...))
    {
        this->assign(stdc::move(other));
        return *this;
    }

    template <typename... other_elements_t>
    constexpr tuple& operator=(const tuple<other_elements_t...>& other)
        requires(sizeof...(elements_t) == sizeof...(other_elements_t)) &&
                (stdc::is_assignable_v<elements_t&, const other_elements_t&> &&
                 ...) &&
                (!(stdc::is_trivially_assignable_v<elements_t&,
                                                   const other_elements_t&> &&
                   ...))
    {
        this->assign(other);
        return *this;
    }

    template <typename... other_elements_t>
    constexpr tuple& operator=(tuple<other_elements_t...>&& other)
        requires(sizeof...(elements_t) == sizeof...(other_elements_t)) &&
                (stdc::is_assignable_v<elements_t&, other_elements_t &&> &&
                 ...) &&
                (!(stdc::is_trivially_assignable_v<elements_t&,
                                                   other_elements_t &&> &&
                   ...))
    {
        this->assign(stdc::move(other));
        return *this;
    }
};

#if __cpp_deduction_guides >= 201606
template <typename... other_types_t>
tuple(other_types_t...) -> tuple<other_types_t...>;
#endif

template <> class tuple<>
{
  public:
    tuple() = default;
};

namespace detail {
/// helper for getting the nth type out of a parameter pack
template <size_t N, typename... types_t> struct nth_type
{};
template <typename T0, typename... tail_t> struct nth_type<0, T0, tail_t...>
{
    using type = T0;
};
template <typename T0, typename T1, typename... tail_t>
struct nth_type<1, T0, T1, tail_t...>
{
    using type = T1;
};
template <typename T0, typename T1, typename T2, typename... tail_t>
struct nth_type<2, T0, T1, T2, tail_t...>
{
    using type = T2;
};
template <size_t N, typename T0, typename T1, typename T2, typename... tail_t>
    requires(N >= 3)
struct nth_type<N, T0, T1, T2, tail_t...> : nth_type<N - 3, tail_t...>
{};
} // namespace detail

template <size_t index, typename T> struct tuple_element;
template <size_t index, typename T>
using tuple_element_t = typename tuple_element<index, T>::type;
template <size_t index, typename T> struct tuple_element<index, const T>
{
    using type = const tuple_element_t<index, T>;
};
template <size_t index, typename T> struct tuple_element<index, volatile T>
{
    using type = volatile tuple_element_t<index, T>;
};
template <size_t index, typename T>
struct tuple_element<index, const volatile T>
{
    using type = const volatile tuple_element_t<index, T>;
};
template <size_t index, typename... types_t>
struct tuple_element<index, tuple<types_t...>>
{
    static_assert(index < sizeof...(types_t), "tuple index must be in range");
    using type = typename detail::nth_type<index, types_t...>::type;
};

namespace detail {
template <size_t index, typename head_t, typename... tail_t>
constexpr head_t&
tuple_get_impl(tuple_impl_t<index, head_t, tail_t...>& t) noexcept
{
    return tuple_impl_t<index, head_t, tail_t...>::get_head(t);
}

template <size_t index, typename head_t, typename... tail_t>
constexpr const head_t&
tuple_get_impl(const tuple_impl_t<index, head_t, tail_t...>& t) noexcept
{
    return tuple_impl_t<index, head_t, tail_t...>::get_head(t);
}
template <size_t index, typename... types_t>
void tuple_get_impl(const tuple<types_t...>&)
    requires(index >= sizeof...(types_t))
= delete;

// Return the index of T in types_t, if it occurs exactly once.
// Otherwise, return sizeof...(types_t).
// GCC style implementation
template <typename T, typename... types_t>
constexpr size_t find_unique_type_in_pack()
{
    constexpr size_t size = sizeof...(types_t);
    constexpr bool found[size] = {stdc::is_same_v<T, types_t>...};
    size_t n = size;
    for (size_t i = 0; i < size; ++i) {
        if (found[i]) {
            if (n < size) {
                return size;
            }
            n = i;
        }
    }
    return n;
}
} // namespace detail

// lvalue get
template <size_t N, typename... types_t>
constexpr tuple_element_t<N, tuple<types_t...>>&
get(tuple<types_t...>& t) noexcept
{
    return detail::tuple_get_impl<N>(t);
}
// const lvalue get
template <size_t N, typename... types_t>
constexpr const tuple_element_t<N, tuple<types_t...>>&
get(const tuple<types_t...>& t) noexcept
{
    return detail::tuple_get_impl<N>(t);
}
// rvalue get
template <size_t N, typename... types_t>
constexpr tuple_element_t<N, tuple<types_t...>>&&
get(tuple<types_t...>&& t) noexcept
{
    using element_type = tuple_element_t<N, tuple<types_t...>>;
    return stdc::forward<element_type>(detail::tuple_get_impl<N>(t));
}
// const rvalue get, for completeness i guess
template <size_t N, typename... types_t>
constexpr const tuple_element_t<N, tuple<types_t...>>&&
get(const tuple<types_t...>&& t) noexcept
{
    using element_type = tuple_element_t<N, tuple<types_t...>>;
    return stdc::forward<const element_type>(detail::tuple_get_impl<N>(t));
}

// deleted overload for going out of bounds of tuple
template <size_t index, typename... types_t>
constexpr void get(const tuple<types_t...>&)
    requires(index > sizeof...(types_t))
= delete;

// get item from tuple by type, provided it is unique
template <typename T, typename... types_t>
constexpr T& get(tuple<types_t...>& t) noexcept
{
    constexpr size_t index = detail::find_unique_type_in_pack<T, types_t...>();
    static_assert(
        index < sizeof...(types_t),
        "the type T in std::get<T> must occur exactly once in the tuple");
    return detail::tuple_get_impl<index>(t);
}

template <typename T, typename... types_t>
constexpr T&& get(tuple<types_t...>&& t) noexcept
{
    constexpr size_t index = detail::find_unique_type_in_pack<T, types_t...>();
    static_assert(
        index < sizeof...(types_t),
        "the type T in std::get<T> must occur exactly once in the tuple");
    return stdc::forward<T>(detail::tuple_get_impl<index>(t));
}

template <typename T, typename... types_t>
constexpr const T& get(const tuple<types_t...>& t) noexcept
{
    constexpr size_t index = detail::find_unique_type_in_pack<T, types_t...>();
    static_assert(
        index < sizeof...(types_t),
        "the type T in std::get<T> must occur exactly once in the tuple");
    return detail::tuple_get_impl<index>(t);
}

template <typename T, typename... types_t>
constexpr const T&& get(const tuple<types_t...>&& t) noexcept
{
    constexpr size_t index = detail::find_unique_type_in_pack<T, types_t...>();
    static_assert(
        index < sizeof...(types_t),
        "the type T in std::get<T> must occur exactly once in the tuple");
    return stdc::forward<const T>(detail::tuple_get_impl<index>(t));
}

namespace detail {
/// Recursive template for comparing all the elements of a tuple from left to
/// right
template <typename lhs_tuple_t, typename rhs_tuple_t, size_t idx, size_t size>
struct tuple_compare
{
    static constexpr bool eq(const lhs_tuple_t& lhs_tuple,
                             const rhs_tuple_t& rhs_tuple)
    {
        return bool(ok::get<idx>(lhs_tuple) == ok::get<idx>(rhs_tuple)) &&
               tuple_compare<lhs_tuple_t, rhs_tuple_t, idx + 1, size>::eq(
                   lhs_tuple, rhs_tuple);
    }
};

template <typename lhs_tuple_t, typename rhs_tuple_t, size_t size>
struct tuple_compare<lhs_tuple_t, rhs_tuple_t, size, size>
{
    static constexpr bool eq(const lhs_tuple_t&, const rhs_tuple_t&)
    {
        return true;
    }
};
} // namespace detail

template <typename... lhs_elements_t, typename... rhs_elements_t>
constexpr bool operator==(const tuple<lhs_elements_t...>& lhs_tuple,
                          const tuple<rhs_elements_t...>& rhs_tuple)
{
    static_assert(
        sizeof...(lhs_elements_t) == sizeof...(rhs_elements_t),
        "tuple objects can only be compared if they have equal sizes.");
    using compare = detail::tuple_compare<tuple<lhs_elements_t...>,
                                          tuple<rhs_elements_t...>, 0,
                                          sizeof...(lhs_elements_t)>;
    return compare::eq(lhs_tuple, rhs_tuple);
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
