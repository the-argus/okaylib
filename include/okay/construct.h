#ifndef __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__
#define __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/res.h"

namespace ok {
namespace detail {

template <typename T, typename E> struct res_accessor_t
{
    static constexpr void set_result_error_byte(res<T, E>& res,
                                                uint8_t byte) noexcept
    {
        res.get_error_payload() = byte;
    }

    static constexpr T&
    get_result_payload_ref_unchecked(res<T, E>& res) noexcept
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
    constexpr auto operator()
        [[nodiscard]] (T& uninitialized,
                       args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_constructible_v<T, args_t...>,
                      "No matching constructor for make_into_uninitialized.");
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
            new (ok::addressof(uninitialized)) T(std::forward<args_t>(args)...);
            return;
        } else if constexpr (is_fallible_constructible) {
            status out = detail::
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
  private:
    template <typename constructor_t, typename fallback_t, typename = void>
    struct deduce_by_explicit_using_statement : public std::false_type
    {
        using type = fallback_t;
    };

    template <typename constructor_t, typename fallback_t>
    struct deduce_by_explicit_using_statement<
        constructor_t, fallback_t,
        std::void_t<
            typename constructor_t::template associated_type<args_t...>>>
        : public std::true_type
    {
        using type =
            typename constructor_t::template associated_type<args_t...>;
    };

    template <typename constructor_t, typename fallback_t, typename = void>
    struct deduce_by_return_type : public std::false_type
    {
        using type = fallback_t;
    };

    template <typename constructor_t, typename fallback_t>
    struct deduce_by_return_type<
        constructor_t, fallback_t,
        std::void_t<decltype(std::declval<const constructor_t&>()(
            std::declval<args_t>()...))>> : public std::true_type
    {
        using type = decltype(std::declval<const constructor_t&>()(
            std::declval<args_t>()...));
    };

  public:
    template <typename constructor_t, typename fallback_t, typename = void>
    struct inner
    {
        using type =
            typename deduce_by_explicit_using_statement<constructor_t,
                                                        fallback_t>::type;
    };

    template <typename constructor_t, typename fallback_t>
    struct inner<constructor_t, fallback_t,
                 std::enable_if_t<
                     deduce_by_return_type<constructor_t, fallback_t>::value>>
    {
        using type =
            typename deduce_by_return_type<constructor_t, fallback_t>::type;
    };
};

template <typename fallback_t> struct factory_function_invocable_t
{
    // if sizeof...(args) > 0, and first arg can be const ref, we can split into
    // constructor function and rest of args and then try to call the
    // constructor function with the rest of the args.
    template <typename constructor_t, typename... args_t>
    constexpr auto operator()(const constructor_t& constructor,
                              args_t&&... innerargs) ->
        typename detail::associated_type_or_fallback_t<decltype(innerargs)...>::
            template inner<constructor_t, fallback_t>::type;

    constexpr void operator()();
};

/// Return a deduced type T, or void if the type was unable to be deduced.
// clang-format off
template <typename T, typename... args_t>
using deduced_make_type_t = std::conditional_t<
!std::is_same_v<T, detail::deduced_t>,
    std::conditional_t<is_std_constructible_v<T, args_t...>,
        T,
        void>,
    std::conditional_t<std::is_invocable_v<factory_function_invocable_t<T>, args_t...>,
        decltype(std::declval<factory_function_invocable_t<T>>()(std::declval<args_t>()...)),
        void>
      >;
// clang-format on

} // namespace detail

/// ok::make(), which takes variable arguments and will do exactly one of the
/// following (if the overload selection is ambiguous, a static assert will
/// fire):
/// - Construct type T with the given arguments. ex: `ok::make<int>(1)` is the
/// same as `int(1)`
/// - Call the first argument with the rest of the arguments, like std::invoke,
///   directly returning the value. ex: `ok::make(ok::arraylist_t<int>::empty,
///   my_allocator)` which is identical to
///   `ok::arraylist_t<int>::empty(my_allocator)`
/// - Call the first argument with a reference to T, followed by the rest of the
///   arguments to ok::make. Useful when the first argument (the constructor
///   function object) takes in a `T& uninitialized` as its first parameter and
///   does initialization in-place over that.
///   - In this case, ok::make() will also check if the constructor function
///     returns a status. If so, it will create a result_t<T, error_enum>,
///     binding together the in-place constructed target of the constructor
///     function and the returned status into one object.
///   - Otherwise, ok::make() will just return the in-place constructed object.
template <typename T = detail::deduced_t, typename... args_t>
[[nodiscard]] constexpr auto make(args_t&&... args) OKAYLIB_NOEXCEPT
{
    constexpr bool is_constructed_type_deduced =
        std::is_same_v<T, detail::deduced_t>;

    /// analyze call to see if deduction is working as expected IF the first
    /// argument is a constructor which can be invoked with the remaining
    /// arguments
    constexpr auto factory_function_construct =
        [](const auto& constructor, auto&&... innerargs) noexcept {
            using constructor_t = detail::remove_cvref_t<decltype(constructor)>;

            // try to get constructor_t::associated_type, otherwise use type T
            using actual_deduced_t =
                typename detail::associated_type_or_fallback_t<
                    decltype(innerargs)...>::template inner<constructor_t,
                                                            T>::type;

            // deduction may have failed completely and no hint was given
            static_assert(
                !std::is_same_v<actual_deduced_t, detail::deduced_t>,
                "Unable to deduce the type that should be constructed by "
                "ok::make(). You may need to explicitly provide it in "
                "angled brackets: `ok::make<...>()`.");

            // its possible we deduced a type but the user also gave a type
            // which doesn't match. but this is acceptable if std-style
            // construction is a fallback.
            static_assert(
                std::is_same_v<actual_deduced_t, T> ||
                    std::is_same_v<detail::deduced_t, T> ||
                    is_std_constructible_v<T, decltype(innerargs)...>,
                "Type provided to ok::make<...>() was different from the "
                "type deduced from the given arguments.");

            if constexpr (is_std_invocable_v<constructor_t,
                                             decltype(innerargs)...>) {
                // this is intended to be an infallible constructor call of some
                // kind which returns actual_deduced_t
                static_assert(
                    is_std_invocable_r_v<constructor_t, actual_deduced_t,
                                         decltype(innerargs)...>,
                    "Given constructor function does not return its "
                    "associated_type.");
                static_assert(
                    !is_std_constructible_v<T, decltype(args)...>,
                    "Ambiguous constructor selection: ok::make() found a "
                    "standard constructor and a factory constructor to both be "
                    "valid ways of constructing an object with the given "
                    "arguments.");
                return constructor(
                    std::forward<decltype(innerargs)>(innerargs)...);
            } else if constexpr (is_std_invocable_v<constructor_t,
                                                    actual_deduced_t&,
                                                    decltype(innerargs)...>) {
                // this is an in-place constructor call of some kind
                static_assert(
                    !is_std_constructible_v<T, decltype(args)...>,
                    "Ambiguous constructor selection: ok::make() found a "
                    "standard constructor and a factory constructor to both be "
                    "valid ways of constructing an object with the given "
                    "arguments.");
                using return_type = decltype(constructor(
                    std::declval<actual_deduced_t&>(),
                    std::forward<decltype(innerargs)>(innerargs)...));
                if constexpr (std::is_void_v<return_type>) {
                    // infallible in place constructor
                    detail::uninitialized_storage_t<actual_deduced_t> out;
                    constructor(out.value, std::forward<decltype(innerargs)>(
                                               innerargs)...);
                    return out.value;
                } else {
                    static_assert(detail::is_instance_v<return_type, status>,
                                  "Type returned from an in-place constructor "
                                  "must either be void (if it cannot error) or "
                                  "an ok::status.");
                    // fallible in-place constructor
                    res<actual_deduced_t, typename return_type::enum_type>
                        out;

                    using accessor =
                        detail::res_accessor_t<actual_deduced_t,
                                               typename return_type::enum_type>;
                    accessor::set_result_error_byte(
                        out,
                        uint8_t(
                            constructor(
                                accessor::get_result_payload_ref_unchecked(out),
                                std::forward<decltype(innerargs)>(innerargs)...)
                                .err()));

                    return out;
                }
            } else {
#define __ok_asked_to_deduce_stdstyle_construction_errmsg                     \
    "Given arguments do not seem to start with a factory construction "       \
    "function, but ok::make() is being asked to deduce the type to "          \
    "construct. You may need to explicitly specify the type, if you mean to " \
    "call one of its constructors: `ok::make<MyType>(args...)`"

                // NOTE: this branch is duplicated at the bottom of the
                // outermost function here

                static_assert(
                    !is_constructed_type_deduced,
                    __ok_asked_to_deduce_stdstyle_construction_errmsg);

                static_assert(is_std_constructible_v<T, decltype(innerargs)...>,
                              "Given type is not constructible with the given "
                              "arguments.");

                return T(std::forward<decltype(innerargs)>(innerargs)...);
            }
        };

    if constexpr (is_std_invocable_v<decltype(factory_function_construct),
                                     decltype(args)...>) {
        return factory_function_construct(
            std::forward<decltype(args)>(args)...);
    } else {
        // doesnt have a first argument which looks like a factory function
        static_assert(!is_constructed_type_deduced,
                      __ok_asked_to_deduce_stdstyle_construction_errmsg);

#undef __ok_asked_to_deduce_stdstyle_construction_errmsg

        static_assert(
            is_std_constructible_v<T, decltype(args)...>,
            "Given type is not constructible with the given arguments.");

        return T(std::forward<decltype(args)>(args)...);
    }
}

} // namespace ok

#endif
