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
        using T = detail::remove_cvref_t<range_t>;
        static_assert(is_range_v<T>,
                      "Cannot transform given type- it is not a range.");
        constexpr bool can_call_with_get_ref_const =
            std::is_invocable_v<callable_t, const value_type_for<T>&> &&
            detail::range_can_get_ref_const_v<range_t>;
        constexpr bool can_call_with_copyout =
            std::is_invocable_v<callable_t, value_type_for<T>> &&
            (detail::range_impls_get_v<T> ||
             detail::range_can_get_ref_const_v<T>);
        static_assert(
            can_call_with_get_ref_const || can_call_with_copyout,
            "Given transformation function and given range do not match up: "
            "there is no way to do `func(ok::iter_get_ref(const range))` "
            "nor `func(ok::iter_copyout(const range))`. This may also be "
            "caused by a lambda being marked \"mutable\".");
        constexpr bool const_ref_call_doesnt_return_void =
            (detail::range_can_get_ref_const_v<T> &&
             returns_transformed_type<callable_t,
                                      const value_type_for<T>&>::value);
        constexpr bool copied_value_call_doesnt_return_void =
            ((detail::range_impls_get_v<T> ||
              detail::range_can_get_ref_const_v<T>) &&
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
    assignment_op_wrapper_t<std::remove_reference_t<callable_t>>
        m_transformer_callable;

  public:
    transformed_view_t(const transformed_view_t&) = default;
    transformed_view_t& operator=(const transformed_view_t&) = default;
    transformed_view_t(transformed_view_t&&) = default;
    transformed_view_t& operator=(transformed_view_t&&) = default;

    constexpr const callable_t& transformer_callable() const OKAYLIB_NOEXCEPT
    {
        return m_transformer_callable.value();
    }

    constexpr transformed_view_t(range_t&& range,
                                 callable_t&& callable) OKAYLIB_NOEXCEPT
        : m_transformer_callable(std::move(callable)),
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
          cursor_type_for<detail::remove_cvref_t<input_range_t>>, true>,
      public detail::propagate_boundscheck_t<
          detail::transformed_view_t<input_range_t, callable_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>, true>,
      public detail::propagate_increment_decrement_t<
          detail::transformed_view_t<input_range_t, callable_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>
{
    static constexpr bool is_view = true;

    using range_t = std::remove_reference_t<input_range_t>;
    using transformed_t = detail::transformed_view_t<input_range_t, callable_t>;
    using cursor_t = cursor_type_for<range_t>;

    static_assert(detail::is_producing_range_v<range_t>,
                  "Cannot transform something which is not an input range.");

    template <typename T>
    static constexpr decltype(auto)
    get_and_transform(T& t, const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::range_definition_inner<range_t>;
        if constexpr (detail::range_can_get_ref_v<range_t> ||
                      detail::range_can_get_ref_const_v<range_t>) {
            return t.transformer_callable()(inner_def::get_ref(
                t.template get_view_reference<T, range_t>(), cursor));
        } else {
            static_assert(detail::range_impls_get_v<range_t>);
            return t.transformer_callable()(inner_def::get(
                t.template get_view_reference<T, range_t>(), cursor));
        }
    }
    using transforming_callable_rettype = decltype(get_and_transform(
        std::declval<transformed_t&>(), std::declval<const cursor_t&>()));

    static constexpr bool is_arraylike = detail::range_is_arraylike_v<range_t>;

    template <typename T = transforming_callable_rettype>
    constexpr static auto get(const transformed_t& i,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, transforming_callable_rettype> &&
                                // should only use get() if the transformation
                                // doesnt seem to create a ref
                                !std::is_lvalue_reference_v<T>,
                            T>
    {
        return get_and_transform(i, c);
    }

    template <typename T = transforming_callable_rettype>
    constexpr static auto get_ref(const transformed_t& i,
                                  const cursor_t& c) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, transforming_callable_rettype> &&
                                // should only use get() if the transformation
                                // doesnt seem to create a ref
                                std::is_lvalue_reference_v<T>,
                            const std::remove_reference_t<T>&>
    {
        return get_and_transform(i, c);
    }

    template <typename T = transforming_callable_rettype>
    constexpr static auto get_ref(transformed_t& i,
                                  const cursor_t& c) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, transforming_callable_rettype> &&
                                // should only use get() if the transformation
                                // doesnt seem to create a ref
                                std::is_lvalue_reference_v<T>,
                            T>
    {
        return get_and_transform(i, c);
    }
};

constexpr detail::range_adaptor_t<detail::transform_fn_t> transform;

} // namespace ok

#endif
