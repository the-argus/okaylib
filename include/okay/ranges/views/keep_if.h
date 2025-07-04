#ifndef __OKAYLIB_RANGES_VIEWS_KEEP_IF_H__
#define __OKAYLIB_RANGES_VIEWS_KEEP_IF_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t, typename predicate_t> struct keep_if_view_t;

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

struct keep_if_fn_t
{
    template <typename range_t, typename predicate_t>
    constexpr decltype(auto)
    operator()(range_t&& range,
               predicate_t&& filter_predicate) const OKAYLIB_NOEXCEPT
    {
        using T = std::remove_reference_t<range_t>;
        static_assert(range_c<T>,
                      "Cannot keep_if given type- it is not a range.");
        constexpr bool can_call_with_get_ref_const =
            std::is_invocable_v<predicate_t, const value_type_for<T>&> &&
            detail::range_can_get_ref_const_c<T>;
        constexpr bool can_call_with_copied_value =
            std::is_invocable_v<predicate_t, value_type_for<T>> &&
            (detail::range_impls_get_c<T> ||
             detail::range_can_get_ref_const_c<T>);
        static_assert(
            can_call_with_get_ref_const || can_call_with_copied_value,
            "Given keep_if predicate and given range do not match up: "
            "there is no way to call the function with the output of "
            "the range. This may also be caused by a lambda being "
            "marked \"mutable\".");
        constexpr bool const_ref_call_returns_bool =
            (detail::range_can_get_ref_const_c<T> &&
             returns_boolean<predicate_t, const value_type_for<T>&>::value);
        // TODO: check copied value is convertible?
        constexpr bool copied_value_call_returns_bool =
            ((detail::range_impls_get_c<T> ||
              detail::range_can_get_ref_const_c<T>) &&
             returns_boolean<predicate_t, value_type_for<T>>::value);
        static_assert(const_ref_call_returns_bool ||
                          copied_value_call_returns_bool,
                      "Given predicate accepts the correct arguments, "
                      "but doesn't return boolean.");
        return keep_if_view_t<decltype(range), predicate_t>{
            std::forward<range_t>(range),
            std::forward<predicate_t>(filter_predicate)};
    }
};

template <typename range_t, typename predicate_t>
struct keep_if_view_t : public underlying_view_type<range_t>::type
{
  private:
    assignment_op_wrapper_t<std::remove_reference_t<predicate_t>>
        m_filter_predicate;

  public:
    keep_if_view_t(const keep_if_view_t&) = default;
    keep_if_view_t& operator=(const keep_if_view_t&) = default;
    keep_if_view_t(keep_if_view_t&&) = default;
    keep_if_view_t& operator=(keep_if_view_t&&) = default;

    constexpr const predicate_t& filter_predicate() const OKAYLIB_NOEXCEPT
    {
        return m_filter_predicate.value();
    }

    constexpr keep_if_view_t(range_t&& range,
                             predicate_t&& filter_predicate) OKAYLIB_NOEXCEPT
        : m_filter_predicate(std::move(filter_predicate)),
          underlying_view_type<range_t>::type(std::forward<range_t>(range))
    {
    }
};
} // namespace detail

template <typename input_range_t, typename predicate_t>
struct range_definition<detail::keep_if_view_t<input_range_t, predicate_t>>
    : public detail::propagate_boundscheck_t<
          detail::keep_if_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public detail::propagate_get_set_t<
          detail::keep_if_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public detail::infinite_static_def_t<
          // finite if the range has size or if it is not marked infinite. never
          // propagate size() function because we cant know it until we traverse
          // the view
          !(detail::range_can_size_c<detail::remove_cvref_t<input_range_t>> ||
            !detail::range_marked_finite_c<
                detail::remove_cvref_t<input_range_t>>)>
// lose boundscheck optimization marker
{
    static constexpr bool is_view = true;
    // assures we are not random access because we dont define offset here.
    // dont want people offsetting cursors by random amounts with keep_if
    static constexpr bool disallow_cursor_member_offset = true;

    using value_type = value_type_for<input_range_t>;

    using range_t = std::remove_reference_t<input_range_t>;
    using keep_if_t = detail::keep_if_view_t<input_range_t, predicate_t>;
    using cursor_t = cursor_type_for<range_t>;

    static constexpr bool is_ref_wrapper =
        !detail::range_impls_get_c<range_t> &&
        std::is_lvalue_reference_v<input_range_t>;

    static_assert(detail::range_can_get_ref_const_c<range_t> ||
                      detail::range_impls_get_c<range_t>,
                  "Cannot keep_if a range which does not provide const "
                  "get_ref() or get().");

    constexpr static cursor_t begin(const keep_if_t& i) OKAYLIB_NOEXCEPT
    {
        const auto& parent =
            i.template get_view_reference<keep_if_t, range_t>();
        auto parent_cursor = ok::begin(parent);
        while (ok::is_inbounds(parent, parent_cursor) &&
               !i.filter_predicate()(
                   ok::iter_get_temporary_ref(parent, parent_cursor))) {
            ok::increment(parent, parent_cursor);
        }
        return cursor_t(parent_cursor);
    }

    // even if parent is random access range, convert to bidirectional range
    // by defining increment() and decrement() in range definition
    constexpr static void increment(const keep_if_t& i,
                                    cursor_t& c) OKAYLIB_NOEXCEPT
        requires detail::range_can_increment_c<range_t>
    {
        const auto& parent =
            i.template get_view_reference<keep_if_t, range_t>();
        do {
            ok::increment(parent, c);
        } while (ok::is_inbounds(parent, c) &&
                 !i.filter_predicate()(ok::iter_get_temporary_ref(parent, c)));
    }

    constexpr static void decrement(const keep_if_t& i,
                                    cursor_t& c) OKAYLIB_NOEXCEPT
        requires detail::range_can_decrement_c<range_t>
    {
        const auto& parent =
            i.template get_view_reference<keep_if_t, range_t>();
        do {
            ok::decrement(parent, c);
        } while (ok::is_inbounds(parent, c) &&
                 !i.filter_predicate()(ok::iter_get_temporary_ref(parent, c)));
    }
};

constexpr detail::range_adaptor_t<detail::keep_if_fn_t> keep_if;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename range_t, typename callable_t>
struct fmt::formatter<ok::detail::keep_if_view_t<range_t, callable_t>>
{
    using formatted_type_t = ok::detail::keep_if_view_t<range_t, callable_t>;
    static_assert(
        fmt::is_formattable<ok::detail::remove_cvref_t<range_t>>::value,
        "Attempt to format keep_if_view_t whose inner range type is not "
        "formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& keep_if_view,
                                    format_context& ctx) const
    {
        return fmt::format_to(
            ctx.out(), "keep_if_view_t< {} >",
            keep_if_view
                .template get_view_reference<formatted_type_t, range_t>());
    }
};
#endif

#endif
