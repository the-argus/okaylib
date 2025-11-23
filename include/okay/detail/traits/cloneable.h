#ifndef __OKAYLIB_DETAIL_TRAITS_CLONE_H__
#define __OKAYLIB_DETAIL_TRAITS_CLONE_H__

#include "okay/detail/traits/error_traits.h"
#include "okay/detail/traits/is_instance.h"

namespace ok {
// TODO: allow cloneable things to use a customization point object instead of
// member functions

namespace detail {
template <typename T>
concept cloneable_member_impl_c = requires(const T& t, T& nonconst) {
    { t.clone() };
    { t.clone_into() };
    requires stdc::is_same_v<decltype(t.clone()), T> &&
                 stdc::is_void_v<decltype(t.clone_into(nonconst))>;
    requires noexcept(t.clone());
    requires noexcept(t.clone_into(nonconst));
};
template <typename T>
concept cloneable_member_impl_fallible_c = requires(const T& t, T& nonconst) {
    { t.try_clone() };
    { t.try_clone_into() };
    requires detail::is_instance_c<decltype(t.try_clone()), res> &&
                 stdc::is_same_v<typename decltype(t.try_clone())::success_type,
                                 T> &&
                 status_type_c<decltype(t.try_clone_into(nonconst))> &&
                 // the res returned by t.clone() should have the same status
                 // type as clone_into()
                 stdc::is_same_v<typename decltype(t.try_clone())::status_type,
                                 decltype(t.try_clone_into(nonconst))>;
};
template <typename T>
concept cloneable_copy_derive_c = requires(const T& t, T& nonconst) {
    requires stdc::is_copy_constructible_v<T>;
    requires stdc::is_copy_assignable_v<T>;
    requires !cloneable_member_impl_fallible_c<T>;
};

} // namespace detail

/// Cloneable is a concept which means that a type can be copied. This copy may
/// or may not be expensive. It is the most general form of copying and since
/// that means it may sometimes invoke expensive operations, it is very explicit
/// (unlike the implicit copy constructor).
template <typename T>
concept cloneable_c = requires {
    requires detail::cloneable_member_impl_c<T> ||
                 detail::cloneable_copy_derive_c<T>;
};

template <typename T>
concept try_cloneable_c = requires {
    requires detail::cloneable_member_impl_fallible_c<T>;
    // if something is both cloneable and try cloneable, then the try cloneable
    // implementation is ignored. the two concepts are meant to be mutally
    // exclusive
    requires !cloneable_c<T>;
};

// can be customized later to use CPO
template <cloneable_c C> auto clone(const C& c)
{
    if constexpr (detail::cloneable_copy_derive_c<C>) {
        return C(c);
    } else {
        return c.clone();
    }
}
template <cloneable_c C> auto clone_into(const C& src, C& dest)
{
    if constexpr (detail::cloneable_copy_derive_c<C>) {
        dest = src;
        return;
    } else {
        return src.clone_into(dest);
    }
}
template <try_cloneable_c C> auto try_clone(const C& c) { return c.clone(); }
template <try_cloneable_c C> auto try_clone_into(const C& src, C& dest)
{
    return src.clone_into(dest);
}

namespace detail {
template <typename T, typename = void> struct try_clone_status
{
    using type = void;
};

template <try_cloneable_c T>
struct try_clone_status<T,
                        stdc::void_t<decltype(stdc::declval<T>().try_clone())>>
{
    using type = decltype(stdc::declval<const T&>().try_clone())::status_type;
};
} // namespace detail

template <try_cloneable_c C>
using try_clone_status_t = detail::try_clone_status<C>::type;

} // namespace ok

#endif
