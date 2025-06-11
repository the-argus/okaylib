#ifndef __OKAYLIB_DETAIL_TRAITS_CLONE_H__
#define __OKAYLIB_DETAIL_TRAITS_CLONE_H__

#include "okay/detail/traits/error_traits.h"
#include "okay/detail/traits/is_instance.h"
#include <concepts>
namespace ok {
// TODO: allow cloneable things to use a customization point object instead of
// member functions

namespace detail {
template <typename T>
concept cloneable_member_impl = requires(const T& t, T& nonconst) {
    // has a .clone() method which should either return T or a result with T on
    // success
    (std::is_same_v<decltype(t.clone()), T> &&
     std::is_void_v<decltype(t.clone_into(nonconst))>) ||
        (detail::is_instance_v<decltype(t.clone()), res> &&
         std::is_same_v<typename decltype(t.clone())::success_type, T> &&
         status_type<decltype(t.clone_into(nonconst))> &&
         // the res returned by t.clone() should have the same status type as
         // clone_into()
         std::is_same_v<typename decltype(t.clone())::status_type,
                        decltype(t.clone_into(nonconst))>);
};
template <typename T>
concept cloneable_copy_derive =
    requires(const T& t, T& nonconst) { std::is_constructible_v<T, const T&>; };

} // namespace detail

/// Cloneable is a concept which means that a type can be copied. This copy may
/// or may not be expensive. It is the most general form of copying and since
/// that means it may sometimes invoke expensive operations, it is very explicit
/// (unlike the implicit copy constructor).
template <typename T>
concept cloneable = requires {
    detail::cloneable_member_impl<T> || detail::cloneable_copy_derive<T>;
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

template <cloneable C>
inline constexpr bool cloneable_can_error_v =
    !std::is_same_v<decltype(ok::clone(std::declval<C>())), C>;

namespace detail {
template <cloneable C, typename = void> struct cloneable_status_type
{
    using type = void;
};
template <cloneable C>
struct cloneable_status_type<C, std::enable_if_t<cloneable_can_error_v<C>>>
{
    using type =
        decltype(std::declval<const C&>().clone_into(std::declval<C&>()));
};

} // namespace detail

template <cloneable C>
using cloneable_status_type_t = detail::cloneable_status_type<C>::type;

} // namespace ok

#endif
