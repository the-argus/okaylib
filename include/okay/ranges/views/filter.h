#ifndef __OKAYLIB_RANGES_VIEWS_FILTER_H__
#define __OKAYLIB_RANGES_VIEWS_FILTER_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t, typename predicate_t> struct filtered_view_t;

template <typename predicate_t, typename param_t, typename = void>
struct returns_boolean : public std::false_type
{};
template <typename predicate_t, typename param_t>
struct returns_boolean<
    predicate_t, param_t,
    std::enable_if_t<std::is_same_v<bool, decltype(std::declval<predicate_t>()(
                                              std::declval<param_t>()))>>>
    : public std::true_type
{};

struct filter_fn_t
{
    template <typename range_t, typename predicate_t>
    constexpr decltype(auto)
    operator()(range_t&& range,
               predicate_t&& filter_predicate) const OKAYLIB_NOEXCEPT
    {
        using T = remove_cvref_t<range_t>;
        static_assert(is_range_v<T>,
                      "Cannot filter given type- it is not a range.");
        constexpr bool can_call_with_get_ref_const =
            std::is_invocable_v<predicate_t, const value_type_for<T>&> &&
            detail::range_has_get_ref_const_v<T>;
        constexpr bool can_call_with_copied_value =
            std::is_invocable_v<predicate_t, value_type_for<T>> &&
            (detail::range_has_get_v<T> ||
             detail::range_has_get_ref_const_v<T>);
        static_assert(can_call_with_get_ref_const || can_call_with_copied_value,
                      "Given filter predicate and given range do not match up: "
                      "there is no way to call the function with the output of "
                      "the range. This may also be caused by a lambda being "
                      "marked \"mutable\".");
        constexpr bool const_ref_call_returns_bool =
            (detail::range_has_get_ref_const_v<T> &&
             returns_boolean<predicate_t, const value_type_for<T>&>::value);
        // TODO: check copied value is convertible?
        constexpr bool copied_value_call_returns_bool =
            ((detail::range_has_get_v<T> ||
              detail::range_has_get_ref_const_v<T>) &&
             returns_boolean<predicate_t, value_type_for<T>>::value);
        static_assert(const_ref_call_returns_bool ||
                          copied_value_call_returns_bool,
                      "Given predicate accepts the correct arguments, "
                      "but doesn't return boolean.");
        return filtered_view_t<decltype(range), predicate_t>{
            std::forward<range_t>(range),
            std::forward<predicate_t>(filter_predicate)};
    }
};

template <typename range_t, typename predicate_t>
struct filtered_view_t : public underlying_view_type<range_t>::type
{
  private:
    assignment_op_wrapper_t<std::remove_reference_t<predicate_t>> m_filter_predicate;

  public:
    filtered_view_t(const filtered_view_t&) = default;
    filtered_view_t& operator=(const filtered_view_t&) = default;
    filtered_view_t(filtered_view_t&&) = default;
    filtered_view_t& operator=(filtered_view_t&&) = default;

    constexpr const predicate_t& filter_predicate() const OKAYLIB_NOEXCEPT
    {
        return m_filter_predicate.value();
    }

    constexpr filtered_view_t(range_t&& range, predicate_t&& filter_predicate)
        : OKAYLIB_NOEXCEPT m_filter_predicate(std::move(filter_predicate)),
          underlying_view_type<range_t>::type(std::forward<range_t>(range))
    {
    }
};
} // namespace detail

template <typename input_range_t, typename predicate_t>
struct range_definition<detail::filtered_view_t<input_range_t, predicate_t>>
    : public detail::propagate_sizedness_t<
          detail::filtered_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>>,
      public detail::propagate_begin_t<
          detail::filtered_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public detail::propagate_boundscheck_t<
          detail::filtered_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public detail::propagate_get_set_t<
          detail::filtered_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using filtered_t = detail::filtered_view_t<input_range_t, predicate_t>;
    using cursor_t = cursor_type_for<range_t>;

    static_assert(detail::range_has_get_ref_const_v<range_t> ||
                      detail::range_has_get_v<range_t>,
                  "Cannot filter a range which does not provide const "
                  "get_ref() or get().");

    template <typename T = range_t>
    constexpr static auto increment(const filtered_t& i, cursor_t& c)
        -> std::void_t<decltype(ok::increment(std::declval<const range_t&>(),
                                              std::declval<cursor_t&>()))>
    {
        do {
            ok::increment(i.template get_view_reference<filtered_t, range_t>(),
                          c);
        } while (!i.filter_predicate()(ok::iter_get_temporary_ref(i, c)));
    }

    template <typename T = range_t>
    constexpr static auto decrement(const filtered_t& i, cursor_t& c)
        -> std::void_t<decltype(ok::decrement(std::declval<const range_t&>(),
                                              std::declval<cursor_t&>()))>
    {
        do {
            ok::decrement(i.template get_view_reference<filtered_t, range_t>(),
                          c);
        } while (!i.filter_predicate()(ok::iter_get_temporary_ref(i, c)));
    }
};

constexpr detail::range_adaptor_t<detail::filter_fn_t> filter;

} // namespace ok

#endif
