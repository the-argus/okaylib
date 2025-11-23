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
    {
        static_assert(is_constructible_c<T, args_t...>,
                      "No matching constructor for make_into_uninitialized.");

        if constexpr (is_std_constructible_c<T, args_t...>) {
            stdc::construct_at(ok::addressof(uninitialized),
                               stdc::forward<args_t>(args)...);
            return;
        } else {
            using analysis = decltype(analyze_construction<args_t...>());

            static_assert(
                analysis{},
                "Unable to find a constructor call for the given arguments.");

            static_assert(
                analysis::has_inplace || stdc::is_move_constructible_v<T>,
                "Attempt to call make_into_uninitialized but there is no known "
                "way to construct a T into an uninitialized spot of memory.");

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
} // namespace detail

/// ok::make() will automatically figure out what constructor to call and return
/// the resulting value on the stack, effectively serving as a wrapper for
/// situations where the constructor only provides a make_into_uninit()
/// function.
template <typename T = detail::deduced_t, typename... args_t>
[[nodiscard]] constexpr decltype(auto) make(args_t&&... args) OKAYLIB_NOEXCEPT
{
    constexpr bool is_constructed_type_deduced =
        stdc::is_same_v<T, detail::deduced_t>;

    if constexpr (!is_constructed_type_deduced &&
                  is_std_constructible_c<T, args_t...>) {
        return T(stdc::forward<args_t>(args)...);
    } else {
        using analysis = decltype(detail::analyze_construction<args_t...>());

        using actual_t =
            stdc::conditional_t<is_constructed_type_deduced,
                                typename analysis::associated_type, T>;

        static_assert(
            !is_constructed_type_deduced || !stdc::is_void_v<actual_t>,
            "Unable to deduce the type for given construction, "
            "you may need to pass the type like so: ok::make<MyType>(...), or "
            "the arguments may be incorrect.");
        static_assert(analysis{},
                      "No matching constructor for the given arguments.");
        static_assert(analysis{},
                      "No matching constructor for the given arguments.");
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
                static_assert(!analysis::can_fail,
                              "bad template analysis? found that something has "
                              ".make() but also it can fail");
                return constructor.make(
                    stdc::forward<inner_args_t>(innerargs)...);
            } else {
                if constexpr (analysis::can_fail) {
                    using status_type = decltype(constructor.make_into_uninit(
                        stdc::declval<actual_t&>(),
                        stdc::forward<inner_args_t>(innerargs)...));

                    using accessor =
                        detail::res_accessor_t<actual_t, status_type>;
                    auto out = accessor::construct_uninitialized_res();

                    auto& uninit =
                        accessor::get_result_payload_ref_unchecked(out);

                    // statuses have to be nothrow move constructible, so we
                    // can call make make_into_uninit and just move the
                    // result into the error of the output error. this
                    // initializes both the status and the payload in one
                    // move
                    accessor::emplace_error_nodestroy(
                        out, stdc::move(constructor.make_into_uninit(
                                 uninit,
                                 stdc::forward<inner_args_t>(innerargs)...)));

                    return out;
                } else {
                    // no rvo provided, so we have to make into something
                    // on this stack frame and then move it out
                    detail::uninitialized_storage_t<actual_t> out;

                    constructor.make_into_uninit(
                        out.value, stdc::forward<inner_args_t>(innerargs)...);

                    return stdc::move(out.value);
                }
            }
        };

        return decomposed_output(stdc::forward<args_t>(args)...);
    }
}

} // namespace ok

#endif
