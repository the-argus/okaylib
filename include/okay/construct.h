#ifndef __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__
#define __OKAYLIB_NOEXCEPT_SPECIAL_MEMBER_FUNCTIONS_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/res.h"

namespace ok {
namespace detail {

template <typename T, typename E>
constexpr void set_result_error_byte(res_t<T, E>& res, uint8_t byte) noexcept
{
    res.get_error_payload() = byte;
}

template <typename T> struct make_inner_fn_t
{
  private:
    template <typename constructor_args_t>
    constexpr auto impl(const constructor_args_t& args) const OKAYLIB_NOEXCEPT
    {
        using has_infallible =
            has_infallible_construct_for_args<T, constructor_args_t>;
        using has_fallible =
            has_fallible_construct_for_args<T, constructor_args_t>;
        if constexpr (has_fallible::value) {
            static_assert(
                has_fallible::is_return_type_valid,
                "Invalid return type from construct() call. `construct()` must "
                "return an instance of the res_t template, if the first "
                "argument is an uninitialized_storage_t. If you intended to "
                "make a nonfailing `construct()`, then remove the "
                "`uninitialized_storage_t` from the arguments and return the "
                "instance of the associated type directly.");

            using inner_return_type = typename has_fallible::return_type;

            static_assert(is_type_res_and_contains<inner_return_type,
                                                   owning_ref<T>>::value);

            res_t<T, typename inner_return_type::enum_type> out(
                ok::detail::uninitialized_result_tag{});

            inner_return_type result = T::construct(
                *reinterpret_cast<uninitialized_storage_t<T>*>(
                    ok::addressof(out.get_value_unchecked_payload())),
                args);

            if (!result.okay()) [[unlikely]] {
                // result.err() and the enum_type in the res should be the same
                set_result_error_byte(out, uint8_t(result.err()));
            } else {
                // mark as initialized so the result will call the payload's
                // destructor and allow access to the now-initialized memory
                set_result_error_byte(out, 0);
                // prevent destruction of the returned owning_ref, ownership has
                // effectively been moved into the result returned from this
                // function
                set_result_error_byte(result, 1);
            }

            return out;
        } else {
            static_assert(has_infallible::value);
            static_assert(
                has_infallible::is_return_type_valid,
                "Invalid return type from construct() call. `construct()` "
                "functions that cannot fail must directly return an instance "
                "of the associated type.");

            return T::construct(args);
        }
    }

  public:
    template <typename constructor_args_t>
    constexpr auto operator()
        [[nodiscard]] (const constructor_args_t& args) const OKAYLIB_NOEXCEPT
            ->std::enable_if_t<has_construct_for_args_v<T, constructor_args_t>,
                               decltype(this->impl(args))>
    {
        return this->impl(args);
    }
};

struct make_fn_t
{
    template <typename constructor_args_t,
              typename T = typename constructor_args_t::associated_type>
    constexpr auto operator()
        [[nodiscard]] (const constructor_args_t& args) const
        OKAYLIB_NOEXCEPT->std::enable_if_t<
            has_construct_for_args_v<T, constructor_args_t>,
            decltype(std::declval<const make_inner_fn_t<T>&>()(args))>
    {
        constexpr detail::make_inner_fn_t<T> make_inner;
        return make_inner(args);
    }
};

} // namespace detail

inline constexpr detail::make_fn_t make;

} // namespace ok

#endif
