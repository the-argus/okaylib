#ifndef __OKAYLIB_RANGES_VIEWS_TRANSFORM_H__
#define __OKAYLIB_RANGES_VIEWS_TRANSFORM_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t, typename callable_t> struct transformed_view_t;

template <typename callable_t, typename param_t, typename = void>
struct returns_transformed_type : public std::false_type
{};
template <typename callable_t, typename param_t>
struct returns_transformed_type<
    callable_t, param_t,
    // requires that callable does not return void
    std::enable_if_t<!std::is_same_v<void, decltype(std::declval<callable_t>()(
                                               std::declval<param_t>()))>>>
    : public std::true_type
{};

struct transform_fn_t
{
    template <typename range_t, typename callable_t>
    constexpr decltype(auto)
    operator()(range_t&& range, callable_t&& callable) const OKAYLIB_NOEXCEPT
    {
        using T = remove_cvref_t<range_t>;
        static_assert(is_range_v<T>,
                      "Cannot transform given type- it is not a range.");
        constexpr bool can_call_with_get_ref_const =
            std::is_invocable_v<callable_t, const value_type_for<T>&> &&
            detail::range_has_get_ref_const_v<T>;
        constexpr bool can_call_with_copied_value =
            std::is_invocable_v<callable_t, value_type_for<T>> &&
            (detail::range_has_get_v<T> ||
             detail::range_has_get_ref_const_v<T>);
        static_assert(
            can_call_with_get_ref_const || can_call_with_copied_value,
            "Given transformation function and given range do not match up: "
            "there is no way to do `func(ok::iter_get_ref(const range))` "
            "nor `func(ok::iter_copyout(const range))`");
        constexpr bool const_ref_call_doesnt_return_void =
            (detail::range_has_get_ref_const_v<T> &&
             returns_transformed_type<callable_t,
                                      const value_type_for<T>&>::value);
        constexpr bool copied_value_call_doesnt_return_void =
            ((detail::range_has_get_v<T> ||
              detail::range_has_get_ref_const_v<T>) &&
             returns_transformed_type<callable_t, value_type_for<T>>::value);
        static_assert(const_ref_call_doesnt_return_void ||
                          copied_value_call_doesnt_return_void,
                      "Object given as callable accepts the correct arguments, "
                      "but always returns void.");
        return transformed_view_t<decltype(range), callable_t>{
            std::forward<range_t>(range), std::forward<callable_t>(callable)};
    }
};

template <typename range_t, typename callable_t>
struct transformed_view_t : public underlying_view_type<range_t>::type
{
  private:
    std::remove_reference_t<callable_t> m_callable;

  public:
    constexpr const callable_t& callable() const OKAYLIB_NOEXCEPT
    {
        return m_callable;
    }

    constexpr transformed_view_t(range_t&& range, callable_t&& callable)
        : OKAYLIB_NOEXCEPT m_callable(std::move(callable)),
          underlying_view_type<range_t>::type(std::forward<range_t>(range))
    {
    }
};
} // namespace detail

template <typename input_range_t, typename callable_t>
struct range_definition<detail::transformed_view_t<input_range_t, callable_t>>
    : public detail::propagate_sizedness_t<
          detail::transformed_view_t<input_range_t, callable_t>,
          detail::remove_cvref_t<input_range_t>>,
      public detail::propagate_begin_t<
          detail::transformed_view_t<input_range_t, callable_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public detail::propagate_boundscheck_t<
          detail::transformed_view_t<input_range_t, callable_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using transformed_t = detail::transformed_view_t<input_range_t, callable_t>;
    using cursor_t = cursor_type_for<range_t>;

    constexpr static decltype(auto) get(const transformed_t& i,
                                        const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        static_assert(detail::is_input_range_v<range_t>);
        using inner_def = detail::range_definition_inner<range_t>;
        if constexpr (detail::range_has_get_v<range_t>) {
            using rettype = decltype(inner_def::get(
                i.template get_view_reference<transformed_t, range_t>(), c));

            return i.callable()(
                std::forward<std::remove_reference_t<rettype>>(inner_def::get(
                    i.template get_view_reference<transformed_t, range_t>(),
                    c)));
        } else {
            static_assert(detail::range_has_get_ref_const_v<range_t>);

            using rettype = decltype(inner_def::get_ref(
                i.template get_view_reference<transformed_t, range_t>(), c));

            return i.callable()(std::forward<rettype>(inner_def::get_ref(
                i.template get_view_reference<transformed_t, range_t>(), c)));
        }
    }
};

constexpr detail::range_adaptor_t<detail::transform_fn_t> transform;

} // namespace ok

#endif
