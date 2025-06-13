#ifndef __OKAYLIB_TRAITS_IS_CONTAINER_H__
#define __OKAYLIB_TRAITS_IS_CONTAINER_H__

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace ok::detail {

template <typename T>
concept pointer = requires { std::is_pointer_v<T>; };
template <typename T>
concept const_c = requires { std::is_const_v<T>; };
template <typename T>
concept const_pointer = requires {
    pointer<T>;
    std::is_const_v<std::remove_pointer_t<T>>;
};
template <typename T>
concept nonconst_pointer = requires {
    pointer<T>;
    !std::is_const_v<std::remove_pointer_t<T>>;
};

template <typename T, typename target_t>
concept pointer_to = requires {
    pointer<T>;
    std::is_same_v<std::remove_pointer_t<T>, target_t>;
};

template <typename T, typename target_t>
concept nonconst_or_const_pointer_to = requires {
    pointer<T>;
    std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,
                   std::remove_cv_t<target_t>>;
};

template <typename T>
concept has_rvalue_data = requires() {
    { std::declval<T&&>().data() };
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
    std::is_same_v<std::remove_pointer_t<decltype(nc.data())>,
                   std::remove_cv_t<std::remove_pointer_t<decltype(c.data())>>>;

    !has_rvalue_data<T>;

    { c.size() } -> std::same_as<size_t>;
    { nc.size() } -> std::same_as<size_t>;
    { std::declval<T&&>().size() } -> std::same_as<size_t>;
};

template <typename T, typename contents_t>
concept std_arraylike_container_of =
    requires(const T& c, std::remove_const_t<T>& nc) {
        std_arraylike_container<T>;
        { c.data() } -> std::same_as<const contents_t*>;
        { nc.data() } -> std::same_as<contents_t*>;
    };

template <typename T, typename contents_t>
concept std_arraylike_container_of_nonconst_or_const =
    requires(const T& c, std::remove_const_t<T>& nc) {
        std_arraylike_container<T>;
        std::is_same_v<
            std::remove_const_t<std::remove_pointer_t<decltype(c.data())>>,
            contents_t>;
        std::is_same_v<
            std::remove_const_t<std::remove_pointer_t<decltype(nc.data())>>,
            contents_t>;
    };

} // namespace ok::detail

#endif
