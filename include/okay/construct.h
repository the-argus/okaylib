#ifndef __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__
#define __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/error.h"

namespace ok {
namespace detail {
struct is_success_fn_t
{
    template <ok::status_type_c status_t>
    constexpr bool operator()
        [[nodiscard]] (const status_t& status) const OKAYLIB_NOEXCEPT
    {
        if constexpr (ok::status_enum_c<status_t>) {
            return status == status_t::success;
        } else {
            return status.is_success();
        }
    }

    template <ok::detail::is_instance_c<ok::res> result_t>
    constexpr bool operator()
        [[nodiscard]] (const result_t& result) const OKAYLIB_NOEXCEPT
    {
        return result.is_success();
    }
};

template <typename T> struct make_into_uninitialized_fn_t
{
    template <typename... args_t>
    constexpr auto operator()
        [[nodiscard]] (T& uninitialized,
                       args_t&&... args) const OKAYLIB_NOEXCEPT
        requires is_inplace_constructible_or_move_makeable_c<T, args_t...>
    {
        if constexpr (is_std_constructible_c<T, args_t...>) {
            stdc::construct_at(ok::addressof(uninitialized),
                               stdc::forward<args_t>(args)...);
            return;
        } else {
            using analysis = detail::analyze_construction_t<args_t...>;
            static_assert(analysis::value);

            constexpr auto call_constructor =
                []<typename... inner_args_t>(
                    T& uninitialized, const auto& constructor,
                    inner_args_t&&... innerargs) noexcept {
                    if constexpr (analysis::has_inplace) {
                        return constructor.make_into_uninit(
                            uninitialized,
                            stdc::forward<inner_args_t>(innerargs)...);
                    } else {
                        // fall back to move constructor if no in-place
                        // construction is defined
                        ok::stdc::construct_at(
                            ok::addressof(uninitialized),
                            stdc::move(constructor.make(
                                stdc::forward<inner_args_t>(innerargs)...)));
                        return;
                    }
                };

            return call_constructor(uninitialized,
                                    stdc::forward<args_t>(args)...);
        }
    }
};

} // namespace detail

template <typename T>
constexpr inline detail::make_into_uninitialized_fn_t<T>
    make_into_uninitialized;

constexpr inline detail::is_success_fn_t is_success;

namespace detail {
struct deduced_t
{
    // user code and make_fn_t must not be able to instantiate this
    deduced_t() = delete;
};

template <typename...>
struct deduce_construction_with_constructor_arg : public std::false_type
{
    using type = void;
};

template <typename constructor_t, typename... args_t>
struct deduce_construction_with_constructor_arg<constructor_t, args_t...>
    : public stdc::true_type
{
    using type =
        typename detail::analyze_construction_t<constructor_t,
                                                args_t...>::associated_type;
};

template <typename T, typename... args_t>
using deduce_construction_t = stdc::conditional_t<
    stdc::is_same_v<T, deduced_t>,
    typename deduce_construction_with_constructor_arg<args_t...>::type, T>;

template <typename T, typename... args_t>
concept is_deduced_constructible_c = requires {
    requires is_constructible_c<detail::deduce_construction_t<T, args_t...>,
                                args_t...>;
    requires !stdc::is_void_v<deduce_construction_t<T, args_t...>>;
    requires !stdc::is_same_v<deduce_construction_t<T, args_t...>, deduced_t>;
};

} // namespace detail

/// ok::make() will automatically figure out what constructor to call and return
/// the resulting value on the stack, effectively serving as a wrapper for
/// situations where the constructor only provides a make_into_uninit()
/// function.
template <typename type_to_make_t = detail::deduced_t, typename... args_t>
    requires detail::is_deduced_constructible_c<type_to_make_t, args_t...>
[[nodiscard]] constexpr auto make(args_t&&... args) OKAYLIB_NOEXCEPT
{
    constexpr bool is_constructed_type_deduced =
        stdc::is_same_v<type_to_make_t, detail::deduced_t>;

    if constexpr (!is_constructed_type_deduced &&
                  is_std_constructible_c<type_to_make_t, args_t...>) {
        return T(stdc::forward<args_t>(args)...);
    } else {
        using analysis = detail::analyze_construction_t<args_t...>;
        using T = detail::deduce_construction_t<type_to_make_t, args_t...>;

        static_assert(analysis{});
        static_assert(
            !is_constructed_type_deduced || !stdc::is_void_v<T>,
            "Unable to deduce the type for given construction, "
            "you may need to pass the type like so: ok::make<MyType>(...), or "
            "the arguments may be incorrect.");
        static_assert(
            is_constructed_type_deduced ||
                stdc::is_void_v<typename analysis::associated_type> ||
                stdc::is_same_v<T, typename analysis::associated_type>,
            "Bad typehint provided to ok::make<...> which was able to "
            "deduce the type and found something else.");

        constexpr auto decomposed_output =
            []<typename constructor_t, typename... inner_args_t>(
                const constructor_t& constructor,
                inner_args_t&&... innerargs) -> decltype(auto) {
            if constexpr (analysis::has_rvo) {
                static_assert(!analysis::can_fail);
                return constructor.make(
                    stdc::forward<inner_args_t>(innerargs)...);
            } else {
                if constexpr (analysis::can_fail) {
                    using status_type = decltype(constructor.make_into_uninit(
                        stdc::declval<T&>(),
                        stdc::forward<inner_args_t>(innerargs)...));

                    using accessor = detail::res_accessor_t<T, status_type>;
                    auto out = accessor::construct_uninitialized_res();

                    auto& uninit =
                        accessor::get_result_payload_ref_unchecked(out);

                    // statuses have to be nothrow move constructible, so we
                    // can call make make_into_uninit and just move the
                    // result into the error of the output error. this
                    // initializes both the status and the payload in one
                    // line
                    accessor::emplace_error_nodestroy(
                        out, stdc::move(constructor.make_into_uninit(
                                 uninit,
                                 stdc::forward<inner_args_t>(innerargs)...)));

                    return out;
                } else {
                    // no rvo provided, so we have to make into something
                    // on this stack frame and then move it out
                    detail::uninitialized_storage_t<T> out;

                    constructor.make_into_uninit(
                        out.value, stdc::forward<inner_args_t>(innerargs)...);

                    static_assert(
                        stdc::is_void_v<decltype(constructor.make_into_uninit(
                            out.value,
                            stdc::forward<inner_args_t>(innerargs)...))>,
                        "Constructor analysis determined that given "
                        "constructor cannot fail, but its make_into_uninit "
                        "function returns something other than void.");

                    return stdc::move(out.value);
                }
            }
        };

        return decomposed_output(stdc::forward<args_t>(args)...);
    }
}

} // namespace ok

#endif
