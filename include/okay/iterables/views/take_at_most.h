#ifndef __OKAYLIB_RANGES_VIEWS_TAKE_AT_MOST_H__
#define __OKAYLIB_RANGES_VIEWS_TAKE_AT_MOST_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t> struct take_at_most_view_t;

struct take_at_most_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range,
                                        size_t amount) const OKAYLIB_NOEXCEPT
    {
        return take_at_most_view_t<decltype(range)>{
            stdc::forward<range_t>(range), amount};
    }
};

template <typename input_parent_range_t>
struct take_at_most_cursor_t
    : public cursor_wrapper_t<take_at_most_cursor_t<input_parent_range_t>,
                              remove_cvref_t<input_parent_range_t>>
{
  private:
    using parent_range_t = stdc::remove_reference_t<input_parent_range_t>;
    using parent_cursor_t = cursor_type_for<parent_range_t>;
    using wrapper_t =
        cursor_wrapper_t<take_at_most_cursor_t<input_parent_range_t>,
                         parent_range_t>;
    using self_t = take_at_most_cursor_t;

    constexpr void increment() OKAYLIB_NOEXCEPT { ++m_consumed; }
    constexpr void decrement() OKAYLIB_NOEXCEPT { --m_consumed; }

    constexpr void plus_eql(size_t delta) OKAYLIB_NOEXCEPT
    {
        m_consumed += delta;
    }
    constexpr void minus_eql(size_t delta) OKAYLIB_NOEXCEPT
    {
        m_consumed -= delta;
    }

  public:
    explicit constexpr take_at_most_cursor_t(parent_cursor_t&& c)
        OKAYLIB_NOEXCEPT : m_consumed(0),
                           wrapper_t(stdc::move(c))
    {
    }

    constexpr operator parent_cursor_t() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const wrapper_t*>(this)->inner();
    }

    friend wrapper_t;
    friend class range_definition<
        detail::take_at_most_view_t<input_parent_range_t>>;

    constexpr size_t num_consumed() const OKAYLIB_NOEXCEPT
    {
        return m_consumed;
    }

    using wrapper_t::operator<=>;

  private:
    size_t m_consumed;
};

template <typename range_t>
struct take_at_most_view_t : public underlying_view_type<range_t>::type
{
  private:
    using cursor_t = take_at_most_cursor_t<remove_cvref_t<range_t>>;
    size_t m_amount;

  public:
    constexpr size_t amount() const noexcept { return m_amount; }

    take_at_most_view_t(const take_at_most_view_t&) = default;
    take_at_most_view_t& operator=(const take_at_most_view_t&) = default;
    take_at_most_view_t(take_at_most_view_t&&) = default;
    take_at_most_view_t& operator=(take_at_most_view_t&&) = default;

    constexpr take_at_most_view_t(range_t&& range,
                                  size_t amount) OKAYLIB_NOEXCEPT
        : underlying_view_type<range_t>::type(stdc::forward<range_t>(range))
    {
        if constexpr (detail::range_can_size_c<range_t>) {
            auto& parent_range =
                this->template get_view_reference<take_at_most_view_t,
                                                  range_t>();
            m_amount = ok::min(ok::size(parent_range), amount);
        } else {
            m_amount = amount;
        }
    }
};

template <typename input_range_t> struct sized_take_at_most_range_t
{
  private:
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;

  public:
    static constexpr size_t size(const take_at_most_t& i) { return i.amount(); }
};

// take_at_most_cursor_t but it is just the parent's cursor if there's no need
// to track number of items consumed
template <typename input_range_t>
using take_at_most_cursor_optimized_t = stdc::conditional_t<
    random_access_range_c<remove_cvref_t<input_range_t>> &&
        !detail::range_marked_finite_c<remove_cvref_t<input_range_t>> &&
        range_can_offset_c<remove_cvref_t<input_range_t>>,
    cursor_type_for<remove_cvref_t<input_range_t>>,
    take_at_most_cursor_t<input_range_t>>;

} // namespace detail

template <typename input_range_t>
    requires(!detail::range_marked_arraylike_c<input_range_t>)
struct range_definition<detail::take_at_most_view_t<input_range_t>>
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::take_at_most_view_t<input_range_t>,
          remove_cvref_t<input_range_t>,
          detail::take_at_most_cursor_optimized_t<input_range_t>>
{
    static constexpr bool is_view = true;

    using range_t = remove_cvref_t<input_range_t>;
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;
    using cursor_t = detail::take_at_most_cursor_optimized_t<input_range_t>;

    static constexpr bool uses_small_cursor_optimization =
        stdc::is_same_v<cursor_t, cursor_type_for<range_t>>;

    using value_type = value_type_for<range_t>;

  private:
    static constexpr range_strict_flags determine_strict_flags()
    {
        auto out = range_strict_flags::disallow_range_def_offset |
                   range_strict_flags::disallow_cursor_member_offset |
                   range_strict_flags::disallow_cursor_member_decrement |
                   range_strict_flags::disallow_cursor_member_increment;

        if constexpr (uses_small_cursor_optimization) {
            // if we use the same cursor as our parent, we might not be able to
            // disable cursor member increment/decrement, if the parent relies
            // on it.
            out -= range_strict_flags::disallow_cursor_member_decrement;
            out -= range_strict_flags::disallow_cursor_member_increment;
        }

        return out;
    }

  public:
    static constexpr range_flags flags = range_def_for<range_t>::flags;
    static constexpr range_strict_flags strict_flags = determine_strict_flags();

    static constexpr bool is_inbounds(const take_at_most_t& i,
                                      const cursor_t& c) OKAYLIB_NOEXCEPT
        requires detail::range_can_is_inbounds_c<range_t>
    {
        using parent_def = detail::range_definition_inner_t<range_t>;
        const range_t& parent_ref =
            i.template get_view_reference<take_at_most_t, range_t>();

        if constexpr (random_access_range_c<range_t> &&
                      !detail::range_marked_finite_c<range_t>) {
            static_assert(stdc::is_same_v<cursor_t, cursor_type_for<range_t>>,
                          "Cursor type has extra unneeded stuff in take() even "
                          "though it doesnt need it.");
            auto parent_begin = ok::begin(parent_ref);

            if (c < parent_begin) [[unlikely]] {
                return false;
            }

            auto advanced =
                ok::range_offset(parent_ref, parent_begin, i.amount());
            // no need to check if within parent bounds- size is constant
            // known, so when instantiating this view we already capped our
            // size
            return c < advanced;
        } else if constexpr (!detail::range_marked_finite_c<range_t>) {
            return c.num_consumed() < i.amount();
        } else {
            return c.num_consumed() < i.amount() &&
                   parent_def::is_inbounds(parent_ref, c.inner());
        }
    }

    static constexpr void increment(const take_at_most_t& i,
                                    cursor_t& c) OKAYLIB_NOEXCEPT
        requires(!uses_small_cursor_optimization)
    {
        const auto& parent_ref =
            i.template get_view_reference<take_at_most_t, range_t>();
        ok::increment(parent_ref, c.inner());
        c.increment();
    }

    static constexpr void decrement(const take_at_most_t& i,
                                    cursor_t& c) OKAYLIB_NOEXCEPT
        requires(!uses_small_cursor_optimization)
    {
        const auto& parent_ref =
            i.template get_view_reference<take_at_most_t, range_t>();
        ok::decrement(parent_ref, c.inner());
        c.decrement();
    }
};

// different definition if range is arraylike: just change what size() returns
template <typename input_range_t>
    requires detail::range_marked_arraylike_c<input_range_t>
struct range_definition<detail::take_at_most_view_t<input_range_t>>
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::take_at_most_view_t<input_range_t>,
          remove_cvref_t<input_range_t>,
          detail::take_at_most_cursor_optimized_t<input_range_t>>
{
    static constexpr bool is_view = true;

    using range_t = remove_cvref_t<input_range_t>;
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;

  private:
    static consteval range_flags determine_flags()
    {
        range_flags out = range_def_for<range_t>::flags;
        out -= range_flags::infinite;
        out |= range_flags::sized;
        return out;
    }

  public:
    static constexpr range_flags flags = determine_flags();
    /// We are arraylike and shouldn't implement is_inbounds
    static constexpr range_strict_flags strict_flags =
        range_strict_flags::disallow_is_inbounds;

    static constexpr bool is_ref_wrapper =
        !detail::range_impls_get_c<range_t> &&
        stdc::is_lvalue_reference_v<input_range_t>;

    using value_type = value_type_for<range_t>;

    static constexpr size_t size(const take_at_most_t& range)
    {
        return range.amount();
    }
};

constexpr detail::range_adaptor_t<detail::take_at_most_fn_t> take_at_most;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename range_t>
struct fmt::formatter<ok::detail::take_at_most_view_t<range_t>>
{
    using formatted_type_t = ok::detail::take_at_most_view_t<range_t>;
    static_assert(
        fmt::is_formattable<ok::remove_cvref_t<range_t>>::value,
        "Attempt to format take_at_most_view_t whose inner range is not "
        "formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& take_at_most_view,
                                    format_context& ctx) const
    {
        return fmt::format_to(
            ctx.out(), "take_at_most_view_t< {}, {} >",
            take_at_most_view.amount(),
            take_at_most_view
                .template get_view_reference<formatted_type_t, range_t>());
    }
};
#endif

#endif
