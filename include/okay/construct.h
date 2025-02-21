#ifndef __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__
#define __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/res.h"

namespace ok {
namespace detail {

template <typename T, typename E>
void make_res_into_error_without_destruction(res_t<T, E>& res, E error)
{
    // tell res that it is an error even though its internal type is currently
    // valid
    __ok_internal_assert(res.get_error_payload() == 0);
    res.get_error_payload() = uint8_t(error);
}

template <typename T, typename constructor_tag_t> struct make_fn_t
{
    static_assert(
        std::is_trivially_default_constructible_v<constructor_tag_t> &&
            std::is_trivially_destructible_v<constructor_tag_t>,
        "Constructor tag type should be a trivial type. It must be "
        "constructed within try_construct.");

    template <typename... args_t>
    constexpr auto operator()
        [[nodiscard]] (args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        using error_type = typename detail::memberfunc_error_type<T>::type;
        static_assert(is_constructible_v<T, constructor_tag_t, args_t...>,
                      "No matching constructor found for the given arguments.");
        static_assert(
            is_fallible_constructible_v<T, constructor_tag_t, args_t...> !=
                is_infallible_constructible_v<T, constructor_tag_t, args_t...>,
            "Ambiguous constructor selection: there is both a fallible and "
            "nonfallible constructor taking the given arguments.");

        if constexpr (is_fallible_constructible_v<T, constructor_tag_t,
                                                  args_t...>) {
            error_type error;

            res_t<T, typename error_type::enum_type> out(
                constructor_tag_t{}, error, std::forward<args_t>(args)...);

            if (!error.okay()) [[unlikely]] {
                // prevent destructor from destroying the probably invalid type
                detail::make_res_into_error_without_destruction(out,
                                                                error.err());
            }
            return out;
        } else if constexpr (is_std_constructible_v<T, args_t...>) {
            return T(std::forward<args_t>(args)...);
        } else {
            return T(constructor_tag_t{}, std::forward<args_t>(args)...);
        }
    }
};

template <typename constructor_tag_t> struct make_into_uninitialized_fn_t
{
    static_assert(
        std::is_trivially_default_constructible_v<constructor_tag_t> &&
            std::is_trivially_destructible_v<constructor_tag_t>,
        "Constructor tag type should be a trivial type. It must be "
        "constructed within try_construct.");

    template <typename T, typename... args_t>
    constexpr decltype(auto) operator()
        [[nodiscard]] (T* uninitialized,
                       args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        using error_type = typename detail::memberfunc_error_type<T>::type;
        static_assert(is_constructible_v<T, constructor_tag_t, args_t...>,
                      "No matching fallible constructor found for the given "
                      "tag and arguments.");
        static_assert(
            is_infallible_constructible_v<T, constructor_tag_t, args_t...> !=
                is_fallible_constructible_v<T, constructor_tag_t, args_t...>,
            "Ambiguous constructor selection: there is both a fallible and "
            "nonfallible constructor taking the given arguments.");

        if constexpr (is_fallible_constructible_v<T, constructor_tag_t,
                                                  args_t...>) {
            error_type error;

            new (uninitialized)
                T(constructor_tag_t{}, error, std::forward<args_t>(args)...);

            using out_t = res_t<T&, typename error_type::enum_type>;

            if (!error.okay()) [[unlikely]] {
                return out_t(error.err());
            }

            return out_t(*uninitialized);
        } else {
            detail::construct_into_uninitialized_infallible(
                uninitialized, constructor_tag_t{},
                std::forward<args_t>(args)...);
            return *uninitialized;
        }
    }
};

} // namespace detail

template <typename T, typename constructor_tag_t = ok::default_constructor_tag>
constexpr detail::make_fn_t<T, constructor_tag_t> make;

template <typename constructor_tag_t = ok::default_constructor_tag>
constexpr detail::make_into_uninitialized_fn_t<constructor_tag_t>
    make_into_uninitialized;

} // namespace ok

#endif
