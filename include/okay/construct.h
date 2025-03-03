#ifndef __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__
#define __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/res.h"

namespace ok {
namespace detail {

template <typename T, typename E> struct res_accessor_t
{
    static constexpr void set_result_error_byte(res_t<T, E>& res,
                                                uint8_t byte) noexcept
    {
        res.get_error_payload() = byte;
    }

    static constexpr T&
    get_result_payload_ref_unchecked(res_t<T, E>& res) noexcept
    {
        return res.get_value_unchecked_payload();
    }
};

template <typename callable_t, typename... pack_t>
constexpr decltype(auto)
call_first_argument_with_the_rest_of_the_arguments(callable_t&& callable,
                                                   pack_t&&... pack)
{
    return callable(std::forward<pack_t>(pack)...);
}

template <typename T, typename callable_t, typename... pack_t>
constexpr decltype(auto)
call_first_argument_with_T_ref_and_the_rest_of_the_arguments(
    T& item, callable_t&& callable, pack_t&&... pack)
{
    return callable(item, std::forward<pack_t>(pack)...);
}

template <typename T> struct make_into_uninitialized_fn_t
{
    template <typename... args_t>
    constexpr auto operator() [[nodiscard]] (
        std::enable_if_t<is_constructible_v<T, args_t...>, T&> uninitialized,
        args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        constexpr bool is_infallible_constructible =
            decltype(detail::is_valid_infallible_construction_safe_args<
                     T, args_t...>())::value;
        constexpr bool is_fallible_constructible =
            decltype(detail::is_valid_fallible_construction_safe_args<
                     T, args_t...>())::value;
        constexpr bool is_std = is_std_constructible_v<T, args_t...>;

        static_assert(int(is_infallible_constructible) +
                              int(is_fallible_constructible) + int(is_std) ==
                          1,
                      "Ambiguous constructor selection. the given args "
                      "could address multiple functions.");

        if constexpr (is_std) {
            new (ok::addressof(uninitialized)) T(std::forward<args_t>()...);
            return;
        } else if constexpr (is_fallible_constructible) {
            status_t out = detail::
                call_first_argument_with_T_ref_and_the_rest_of_the_arguments(
                    uninitialized, std::forward<args_t>(args)...);
            return out;
        } else if constexpr (is_infallible_constructible) {
            if constexpr (is_inplace_constructible_v<T, args_t...>) {
                detail::
                    call_first_argument_with_T_ref_and_the_rest_of_the_arguments(
                        uninitialized, std::forward<args_t>(args)...);
            } else {
                new (ok::addressof(uninitialized)) T(
                    detail::call_first_argument_with_the_rest_of_the_arguments(
                        std::forward<args_t>(args)...));
            }
            return;
        }
    }
};

} // namespace detail

template <typename T>
constexpr inline detail::make_into_uninitialized_fn_t<T>
    make_into_uninitialized;

namespace detail {

struct deduced_t
{
    // user code and make_fn_t must not be able to instantiate this
    deduced_t() = delete;
};

template <typename... args_t> struct associated_type_or_fallback_t
{
    template <typename constructor_t, typename fallback_t, typename = void>
    struct inner
    {
        using type = fallback_t;
    };

    template <typename constructor_t, typename fallback_t>
    struct inner<constructor_t, fallback_t,
                 std::void_t<typename constructor_t::template associated_type_t<
                     args_t...>>>
    {
        using type = fallback_t;
    };
};

template <typename T> struct make_fn_t
{
  private:
    template <typename actual_deduced_t, typename constructor_t,
              typename... args_t>
    inline static constexpr bool
        is_inplace_infallible_constructor_with_deduction_v =
            is_valid_infallible_construction_v<actual_deduced_t, constructor_t,
                                               args_t...> &&
            is_inplace_factory_constructible<constructor_t, args_t...>::
                template inner<actual_deduced_t>::value;

  public:
    template <typename constructor_t, typename... args_t>
    constexpr auto operator()(const constructor_t& constructor,
                              args_t&&... args) const OKAYLIB_NOEXCEPT
        // only enable if this is an inplace infallible constructor
        -> std::enable_if_t<
            is_std_invocable_v<constructor_t, args_t...> &&
                is_inplace_infallible_constructor_with_deduction_v<
                    typename associated_type_or_fallback_t<
                        args_t...>::template inner<constructor_t, T>,
                    constructor_t, args_t...>,
            typename associated_type_or_fallback_t<args_t...>::template inner<
                constructor_t, T>>
    {
        using actual_deduced_t = typename associated_type_or_fallback_t<
            args_t...>::template inner<constructor_t, T>;
        static_assert(
            !std::is_same_v<actual_deduced_t, deduced_t>,
            "Invalid callable which is associated with ok::detail::deduced_t. "
            "Why are you doing that, anyways?");

        uninitialized_storage_t<actual_deduced_t> out;

        constructor(out.value, std::forward<args_t>(args)...);

        return out.value;
    }

    // only enabled if the constructor can be directly called with args, in this
    // case ok::make is similar to std::invoke. infallible, returning
    // constructors
    template <typename constructor_t, typename... args_t,
              typename actual_deduced_t =
                  decltype(std::declval<const constructor_t&>()(
                      std::declval<args_t>()...))>
    constexpr auto operator()(const constructor_t& constructor,
                              args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        static_assert(
            !std::is_same_v<actual_deduced_t, deduced_t>,
            "Invalid callable which returns an ok::detail::deduced_t. Why are "
            "you doing that, anyways?");
        return constructor(std::forward<args_t>(args)...);
    }

    // overload for std style constructors
    template <typename... args_t>
    constexpr auto operator()(args_t&&... args) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<is_std_constructible_v<T, args_t...>, T>
    {
        return T(std::forward<args_t>(args)...);
    }

    // fallible constructor overload
    template <typename constructor_t, typename... args_t,
              std::enable_if_t<is_valid_fallible_construction_v<
                  typename associated_type_or_fallback_t<
                      args_t...>::template inner<constructor_t, T>,
                  constructor_t, args_t...>> = true>
    constexpr auto operator()(const constructor_t& constructor,
                              args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        using actual_deduced_t = typename associated_type_or_fallback_t<
            args_t...>::template inner<constructor_t, T>;
        static_assert(
            !std::is_same_v<actual_deduced_t, deduced_t>,
            "Invalid callable which is associated with ok::detail::deduced_t. "
            "Why are you doing that, anyways?");
        using return_type = decltype(constructor(
            std::declval<T&>(), std::forward<args_t>(args)...));

        res_t<actual_deduced_t, typename return_type::enum_type> out;

        using accessor =
            res_accessor_t<actual_deduced_t, typename return_type::enum_type>;
        accessor::set_result_error_byte(
            out, constructor(accessor::get_result_payload_ref_unchecked(out),
                             std::forward<args_t>(args)...)
                     .err());

        return out;
    }
};
} // namespace detail

/// ok::make(), which takes variable arguments and will do exactly one of the
/// following (if the overload selection is ambiguous, a static assert will
/// fire):
/// - Construct type T with the given arguments. ex: `ok::make<int>(1)` is the
/// same as `int(1)`
/// - Call the first argument with the rest of the arguments, like std::invoke,
///   directly returning the value. ex: `ok::make(ok::arraylist_t<int>::empty,
///   my_allocator)`
/// - Call the first argument with a reference to T, followed by the rest of the
///   arguments to ok::make. Useful when the first argument (the constructor
///   function object) takes in a `T& uninitialized` as its first parameter and
///   does initialization in-place over that.
///   - In this case, ok::make() will also check if the constructor function
///     returns a status_t. If so, it will create a result_t<T, error_enum>,
///     binding together the in-place constructed target of the constructor
///     function and the returned status into one object.
///   - Otherwise, ok::make() will just return the in-place constructed object.
template <typename T = detail::deduced_t>
inline constexpr detail::make_fn_t<T> make;

} // namespace ok

#endif
