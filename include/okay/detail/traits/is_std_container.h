#ifndef __OKAYLIB_TRAITS_IS_CONTAINER_H__
#define __OKAYLIB_TRAITS_IS_CONTAINER_H__

#include "okay/detail/type_traits.h"
#include <cstddef>

namespace ok::detail {

template <typename T>
concept pointer = requires { requires stdc::is_pointer_v<T>; };
template <typename T>
concept const_pointer = requires {
    requires pointer<T>;
    requires is_const_c<stdc::remove_pointer_t<T>>;
};
template <typename T>
concept nonconst_pointer = requires {
    requires pointer<T>;
    requires !is_const_c<stdc::remove_pointer_t<T>>;
};

template <typename T, typename target_t>
concept pointer_to = requires {
    requires pointer<T>;
    requires stdc::is_same_v<stdc::remove_pointer_t<T>, target_t>;
};

template <typename T, typename target_t>
concept nonconst_or_const_pointer_to = requires {
    requires pointer<T>;
    requires stdc::is_same_v<stdc::remove_cv_t<stdc::remove_pointer_t<T>>,
                             stdc::remove_cv_t<target_t>>;
};

template <typename T>
concept std_arraylike_container = requires(const T& c, T& nc) {
    {
        c.data()
    } -> const_pointer; // owning container: const should always mean
                        // const interior
    {
        nc.data()
    } -> pointer; // allowed to be const, too (std::array<const int>)

    { c.size() } -> stdc::same_as<size_t>;
    { nc.size() } -> stdc::same_as<size_t>;
    { stdc::declval<T&&>().size() } -> stdc::same_as<size_t>;
};

template <typename T, typename contents_t>
concept std_arraylike_container_of =
    requires(const T& c, stdc::remove_const_t<T>& nc) {
        requires std_arraylike_container<T>;
        { c.data() } -> stdc::same_as<const contents_t*>;
        { nc.data() } -> stdc::same_as<contents_t*>;
    };

template <typename T, typename contents_t>
concept std_arraylike_container_of_nonconst_or_const =
    requires(const T& c, stdc::remove_const_t<T>& nc) {
        requires std_arraylike_container<T>;
        requires stdc::is_same_v<
            stdc::remove_const_t<stdc::remove_pointer_t<decltype(c.data())>>,
            contents_t>;
        requires stdc::is_same_v<
            stdc::remove_const_t<stdc::remove_pointer_t<decltype(nc.data())>>,
            contents_t>;
    };

} // namespace ok::detail

#endif
