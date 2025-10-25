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

struct keep_if_fn_t
{
    template <range_c range_t, typename predicate_t>
    constexpr decltype(auto)
    operator()(range_t&& range,
               predicate_t&& filter_predicate) const OKAYLIB_NOEXCEPT
    {
        using T = std::remove_reference_t<range_t>;
        static_assert(
            requires(const cursor_type_for<T>& c) {
                {
                    filter_predicate(ok::range_get_best(range, c))
                } -> same_as_c<bool>;
            }, "Given keep_if predicate and given range do not match up: "
               "there is no way to call the function with the output of "
               "the range. This may also be caused by a lambda being "
               "marked \"mutable\", or the lambda not returning bool.");
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
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::keep_if_view_t<input_range_t, predicate_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>
{
    static constexpr bool is_view = true;

    using value_type = value_type_for<input_range_t>;

    using range_t = std::remove_reference_t<input_range_t>;
    using keep_if_t = detail::keep_if_view_t<input_range_t, predicate_t>;
    using cursor_t = cursor_type_for<range_t>;

  private:
    static constexpr range_flags determine_flags() noexcept
    {
        using flags = range_flags;
        flags f = flags::producing;

        constexpr flags parent_flags = range_def_for<range_t>::flags;

        if (parent_flags & flags::consuming)
            f |= flags::consuming;

        if (parent_flags & flags::implements_set)
            f |= flags::implements_set;

        if (parent_flags & flags::infinite)
            f |= flags::infinite;
        else
            f |= flags::finite;

        if (parent_flags & flags::ref_wrapper ||
            stdc::is_reference_c<input_range_t>)
            f |= flags::ref_wrapper;

        return f;
    }

  public:
    static constexpr range_flags flags = determine_flags();
    static constexpr range_strict_flags strict_flags =
        range_strict_flags::disallow_cursor_member_offset |
        range_strict_flags::disallow_range_def_offset |
        range_strict_flags::disallow_cursor_member_increment |
        range_strict_flags::disallow_cursor_member_decrement;

    constexpr static cursor_t begin(const keep_if_t& i) OKAYLIB_NOEXCEPT
    {
        const auto& parent =
            i.template get_view_reference<keep_if_t, range_t>();
        auto parent_cursor = ok::begin(parent);
        while (
            ok::is_inbounds(parent, parent_cursor) &&
            !i.filter_predicate()(ok::range_get_best(parent, parent_cursor))) {
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
                 !i.filter_predicate()(ok::range_get_best(parent, c)));
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
                 !i.filter_predicate()(ok::range_get_best(parent, c)));
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
