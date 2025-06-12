#ifndef __OKAYLIB_DETAIL_TRAITS_CLONE_H__
#define __OKAYLIB_DETAIL_TRAITS_CLONE_H__

#include "okay/detail/traits/error_traits.h"
#include "okay/detail/traits/is_instance.h"

namespace ok {
// TODO: allow cloneable things to use a customization point object instead of
// member functions

namespace detail {
template <typename T>
concept cloneable_member_impl = requires(const T& t, T& nonconst) {
    { t.clone() };
    { t.clone_into() };
    std::is_same_v<decltype(t.clone()), T>&&
        std::is_void_v<decltype(t.clone_into(nonconst))>;
    noexcept(t.clone());
    noexcept(t.clone_into(nonconst));
};
template <typename T>
concept cloneable_member_impl_fallible = requires(const T& t, T& nonconst) {
    { t.try_clone() };
    { t.try_clone_into() };
    detail::is_instance_v<decltype(t.try_clone()), res>&&
        std::is_same_v<typename decltype(t.try_clone())::success_type,
                       T>&& status_type<decltype(t.try_clone_into(nonconst))>&&
            // the res returned by t.clone() should have the same status type as
            // clone_into()
            std::is_same_v<typename decltype(t.try_clone())::status_type,
                           decltype(t.try_clone_into(nonconst))>;
};
template <typename T>
concept cloneable_copy_derive = requires(const T& t, T& nonconst) {
    std::is_copy_constructible_v<T>;
    std::is_copy_assignable_v<T>;
    !cloneable_member_impl_fallible<T>;
};

} // namespace detail

/// Cloneable is a concept which means that a type can be copied. This copy may
/// or may not be expensive. It is the most general form of copying and since
/// that means it may sometimes invoke expensive operations, it is very explicit
/// (unlike the implicit copy constructor).
template <typename T>
concept cloneable = requires {
    detail::cloneable_member_impl<T> || detail::cloneable_copy_derive<T>;
};

template <typename T>
concept try_cloneable = requires {
    detail::cloneable_member_impl_fallible<T>;
    // if something is both cloneable and try cloneable, then the try cloneable
    // implementation is ignored. the two concepts are meant to be mutally
    // exclusive
    !cloneable<T>;
};

// can be customized later to use CPO
template <cloneable C> auto clone(const C& c)
{
    if constexpr (detail::cloneable_copy_derive<C>) {
        return C(c);
    } else {
        return c.clone();
    }
}
template <cloneable C> auto clone_into(const C& src, C& dest)
{
    if constexpr (detail::cloneable_copy_derive<C>) {
        dest = src;
        return;
    } else {
        return src.clone_into(dest);
    }
}
template <try_cloneable C> auto try_clone(const C& c) { return c.clone(); }
template <try_cloneable C> auto try_clone_into(const C& src, C& dest)
{
    return src.clone_into(dest);
}

namespace detail {
template <typename T, typename = void> struct try_clone_status
{
    using type = void;
};

template <try_cloneable T>
struct try_clone_status<T, std::void_t<decltype(std::declval<T>().try_clone())>>
{
    using type = decltype(std::declval<const T&>().try_clone())::status_type;
};
} // namespace detail

template <try_cloneable C>
using try_clone_status_t = detail::try_clone_status<C>::type;

} // namespace ok

#endif
