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

template <typename T> struct make_into_uninitialized_fn_t
{
    template <typename... args_t>
    constexpr auto operator()
        [[nodiscard]] (T& uninitialized,
                       args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_constructible_v<T, args_t...>,
                      "No matching constructor for make_into_uninitialized.");

        if constexpr (is_std_constructible_v<T, args_t...>) {
            new (ok::addressof(uninitialized)) T(std::forward<args_t>(args)...);
            return;
        } else {
            using analysis = decltype(analyze_construction<args_t...>());

            static_assert(
                analysis{},
                "Unable to find a constructor call for the given arguments.");

            static_assert(
                analysis::has_inplace || std::is_move_constructible_v<T>,
                "Attempt to call make_into_uninitialized but there is no known "
                "way to construct a T into an uninitialized spot of memory.");

            constexpr auto call_constructor = [](T& uninitialized,
                                                 const auto& constructor,
                                                 auto&&... innerargs) noexcept {
                if constexpr (analysis::has_inplace) {
                    return constructor.make_into_uninit(
                        uninitialized,
                        std::forward<decltype(innerargs)>(innerargs)...);
                } else {
                    // fall back to move constructor if no in-place construction
                    // is defined
                    new (ok::addressof(uninitialized))
                        T(std::move(constructor.make(
                            std::forward<decltype(innerargs)>(innerargs)...)));
                    return;
                }
            };

            return call_constructor(uninitialized,
                                    std::forward<args_t>(args)...);
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
} // namespace detail

/// ok::make() will automatically figure out what constructor to call and return
/// the resulting value on the stack, effectively serving as a wrapper for
/// situations where the constructor only provides a make_into_uninit()
/// function.
template <typename T = detail::deduced_t, typename... args_t>
[[nodiscard]] constexpr auto make(args_t&&... args) OKAYLIB_NOEXCEPT
{
    constexpr bool is_constructed_type_deduced =
        std::is_same_v<T, detail::deduced_t>;

    if constexpr (!is_constructed_type_deduced &&
                  is_std_constructible_v<T, args_t...>) {
        return T(std::forward<args_t>(args)...);
    } else {
        using analysis = decltype(detail::analyze_construction<args_t...>());

        using actual_t =
            std::conditional_t<is_constructed_type_deduced,
                               typename analysis::associated_type, T>;

        static_assert(
            !is_constructed_type_deduced || !std::is_void_v<actual_t>,
            "Unable to deduce the type for given construction, "
            "you may need to pass the type like so: ok::make<MyType>(...), or "
            "the arguments may be incorrect.");
        static_assert(analysis{},
                      "No matching constructor for the given arguments.");
        static_assert(analysis{},
                      "No matching constructor for the given arguments.");
        static_assert(
            is_constructed_type_deduced ||
                std::is_void_v<typename analysis::associated_type> ||
                std::is_same_v<T, typename analysis::associated_type>,
            "Bad typehint provided to ok::make<...> which was able to "
            "deduce the type and found something else.");

        constexpr auto decomposed_output = [](const auto& constructor,
                                              auto&&... innerargs) noexcept {
            if constexpr (analysis::has_rvo) {
                static_assert(!analysis::can_fail,
                              "bad template analysis? found that something has "
                              ".make() but also it can fail");
                return constructor.make(
                    std::forward<decltype(innerargs)>(innerargs)...);
            } else {
                if constexpr (analysis::can_fail) {
                    using enum_type =
                        typename decltype(constructor.make_into_uninit(
                            std::declval<actual_t&>(),
                            std::forward<decltype(innerargs)>(
                                innerargs)...))::enum_type;
                    ok::res<actual_t, enum_type> out;

                    auto& uninit = detail::res_accessor_t<actual_t, enum_type>::
                        get_result_payload_ref_unchecked(out);

                    detail::res_accessor_t<actual_t, enum_type>::
                        set_result_error_byte(
                            out,
                            uint8_t(constructor
                                        .make_into_uninit(
                                            uninit,
                                            std::forward<decltype(innerargs)>(
                                                innerargs)...)
                                        .err()));

                    return out;
                } else {
                    // no rvo provided, so we have to make into something
                    // on this stack frame and then move it out
                    detail::uninitialized_storage_t<actual_t> out;

                    constructor.make_into_uninit(
                        out.value,
                        std::forward<decltype(innerargs)>(innerargs)...);

                    // TODO: what is codegen like for this? if constructor is
                    // constexpr, is not doing a std::move better?
                    return std::move(out.value);
                }
            }
        };

        return decomposed_output(std::forward<args_t>(args)...);
    }
}

} // namespace ok

#endif
