#ifndef __OKAYLIB_ITERABLE_TRANSFORM_H__
#define __OKAYLIB_ITERABLE_TRANSFORM_H__

#include "okay/detail/view_common.h"
#include "okay/iterable/iterable.h"
#include "okay/iterable/ranges.h"

namespace ok {
namespace detail {
template <typename iterable_t, typename callable_t> struct transformed_view_t;

template <typename callable_t, typename param_t, typename = void>
struct returns_transformed_type : std::false_type
{};
template <typename callable_t, typename param_t>
struct returns_transformed_type<
    callable_t, param_t,
    // requires that callable does not return void
    std::enable_if_t<!std::is_same_v<void, decltype(std::declval<callable_t>()(
                                               std::declval<param_t>()))>>>
    : std::true_type
{};

struct transform_fn_t
{
    template <typename iterable_t, typename callable_t>
    constexpr decltype(auto)
    operator()(iterable_t&& iterable,
               callable_t&& callable) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_iterable_v<iterable_t>,
                      "Cannot transform given type- it is not iterable.");
        constexpr bool can_call_with_get_ref_const =
            std::is_invocable_v<callable_t,
                                const value_type_for<iterable_t>&> &&
            detail::iterable_has_get_ref_const_v<iterable_t>;
        constexpr bool can_call_with_get =
            std::is_invocable_v<callable_t, value_type_for<iterable_t>> &&
            detail::iterable_has_get_v<iterable_t>;
        static_assert(can_call_with_get_ref_const || can_call_with_get,
                      "Object given as callable is not able to accept "
                      "const-qualified input from the given iterable.");
        constexpr bool doesnt_return_void =
            (detail::iterable_has_get_ref_const_v<iterable_t> &&
             returns_transformed_type<
                 callable_t, const value_type_for<iterable_t>&>::value) ||
            (detail::iterable_has_get_v<iterable_t> &&
             returns_transformed_type<callable_t,
                                      value_type_for<iterable_t>>::value);
        static_assert(doesnt_return_void,
                      "Object given as callable accepts the correct arguments, "
                      "but returns void.");
        return transformed_view_t<decltype(iterable), callable_t>{
            std::forward<iterable_t>(iterable),
            std::forward<callable_t>(callable)};
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

template <typename iterable_t, typename callable_t>
struct iterable_definition<detail::transformed_view_t<iterable_t, callable_t>> :
    // inherit size() or infinite marking
    public std::conditional_t<
        detail::iterable_has_size_v<iterable_t>,
        detail::sized_iterable_t<
            detail::transformed_view_t<iterable_t, callable_t>>,
        std::conditional<detail::iterable_marked_infinite_v<iterable_t>,
                         detail::infinite_iterable_t,
                         detail::finite_iterable_t>>
{
    static constexpr bool is_view = true;

    using transformed_t = detail::transformed_view_t<iterable_t, callable_t>;
    using cursor_t = cursor_type_for<iterable_t>;

    constexpr static decltype(auto) begin(const transformed_t& i)
    {
        return ok::begin(i.base());
    }

    template <typename T = iterable_t>
    constexpr static std::enable_if_t<
        std::is_same_v<iterable_t, T> &&
            detail::iterable_has_is_inbounds_v<iterable_t>,
        bool>
    is_inbounds(const transformed_t& i, const cursor_t& c)
    {
        return detail::iterable_definition_inner<T>::is_inbounds(i.base(),
                                                                 c.inner());
    }

    template <typename T = iterable_t>
    constexpr static std::enable_if_t<
        std::is_same_v<iterable_t, T> &&
            detail::iterable_has_is_after_bounds_v<iterable_t>,
        bool>
    is_after_bounds(const transformed_t& i, const cursor_t& c)
    {
        return detail::iterable_definition_inner<T>::is_after_bounds(i.base(),
                                                                     c.inner());
    }

    template <typename T = iterable_t>
    constexpr static bool is_before_bounds(const transformed_t& i,
                                           const cursor_t& c)
    {
        return detail::iterable_definition_inner<T>::is_before_bounds(
            i.base(), c.inner());
    }

    constexpr static decltype(auto) get(const transformed_t& i,
                                        const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        static_assert(detail::is_input_iterable_v<iterable_t>);
        using inner_def = detail::iterable_definition_inner<iterable_t>;
        if constexpr (detail::iterable_has_get_v<iterable_t>) {
            using rettype =
                decltype(inner_def::get(std::declval<const iterable_t&>(),
                                        std::declval<const cursor_t&>()));

            return i.callable()(std::forward<std::remove_reference_t<rettype>>(
                inner_def::get(c)));
        } else {
            static_assert(detail::iterable_has_get_ref_const_v<iterable_t>);

            using rettype =
                decltype(inner_def::get_ref(std::declval<const iterable_t&>(),
                                            std::declval<const cursor_t&>()));

            return i.callable()(std::forward<rettype>(inner_def::get(c)));
        }
    }
};

constexpr detail::range_adaptor_t<detail::transform_fn_t> transform;

} // namespace ok

#endif
