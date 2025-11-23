#ifndef __OKAYLIB_RANGES_VIEWS_ENUMERATE_H__
#define __OKAYLIB_RANGES_VIEWS_ENUMERATE_H__

#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t> struct enumerated_view_t;

struct enumerate_fn_t
{
    template <range_c range_t>
    constexpr decltype(auto) operator()(range_t&& range) const OKAYLIB_NOEXCEPT
    {
        return enumerated_view_t<decltype(range)>{std::forward<range_t>(range)};
    }
};

// conditionally either a ref view wrapper or a owned view wrapper or just
// straight up inherits from the range_t if it's a view
template <typename range_t>
struct enumerated_view_t : public underlying_view_type<range_t>::type
{};

template <typename parent_range_t>
struct enumerated_cursor_t
    : public cursor_wrapper_t<enumerated_cursor_t<parent_range_t>,
                              parent_range_t>
{
  private:
    using parent_cursor_t = cursor_type_for<parent_range_t>;
    using wrapper_t =
        cursor_wrapper_t<enumerated_cursor_t<parent_range_t>, parent_range_t>;
    using self_t = enumerated_cursor_t;

    constexpr void increment() OKAYLIB_NOEXCEPT { ++m_index; }
    constexpr void decrement() OKAYLIB_NOEXCEPT { --m_index; }

    constexpr void plus_eql(size_t delta) OKAYLIB_NOEXCEPT { m_index += delta; }
    constexpr void minus_eql(size_t delta) OKAYLIB_NOEXCEPT
    {
        m_index -= delta;
    }

  public:
    explicit constexpr enumerated_cursor_t(parent_cursor_t&& c) OKAYLIB_NOEXCEPT
        : m_index(0),
          wrapper_t(std::move(c))
    {
    }

    friend wrapper_t;
    friend class ok::range_definition<
        detail::enumerated_view_t<parent_range_t>>;

    [[nodiscard]] constexpr size_t index() const OKAYLIB_NOEXCEPT
    {
        return m_index;
    }

    [[nodiscard]] constexpr friend auto
    operator<=>(const enumerated_cursor_t& lhs, const enumerated_cursor_t& rhs)
    {
        const auto ordering = ok::cmp(lhs.inner(), rhs.inner());
        __ok_assert(ordering == ok::cmp(lhs.m_index, rhs.m_index),
                    "Comparing two enumerated cursors resulted in a different "
                    "comparison than the indices. This may indicate a broken "
                    "range implementation.");
        return ordering;
    }

  private:
    size_t m_index;
};

} // namespace detail

template <typename input_range_t>
    requires(!detail::range_marked_arraylike_c<input_range_t>)
struct range_definition<detail::enumerated_view_t<input_range_t>>
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::enumerated_view_t<input_range_t>,
          remove_cvref_t<input_range_t>,
          detail::enumerated_cursor_t<input_range_t>>
{
  private:
    static consteval range_flags determine_flags() noexcept
    {
        using flags = range_flags;
        flags f = flags::producing;

        constexpr flags parent_flags = range_def_for<input_range_t>::flags;
        if (parent_flags & flags::infinite)
            f = f | flags::infinite;
        if (parent_flags & flags::finite)
            f = f | flags::finite;
        if (parent_flags & flags::sized)
            f = f | flags::sized;

        return f;
    };

  public:
    static constexpr bool is_view = true;

    static constexpr range_flags flags = determine_flags();
    static constexpr range_strict_flags strict_flags =
        range_strict_flags::disallow_cursor_member_decrement |
        range_strict_flags::disallow_cursor_member_compare |
        range_strict_flags::disallow_cursor_member_offset;

    using enumerated_t = detail::enumerated_view_t<input_range_t>;
    using cursor_t = detail::enumerated_cursor_t<input_range_t>;
    using range_t = std::remove_reference_t<input_range_t>;

    using parent_get_return_type = decltype(ok::range_get_best(
        stdc::declval<input_range_t>(),
        stdc::declval<const cursor_type_for<input_range_t>&>()));

    using value_type = ok::tuple<parent_get_return_type, const size_t>;

    constexpr static void increment(const enumerated_t& range,
                                    cursor_t& c) OKAYLIB_NOEXCEPT
        requires detail::range_impls_increment_c<range_t>
    {
        // perform parent's increment function
        ok::increment(
            range.template get_view_reference<enumerated_t, range_t>(),
            c.inner());
        // also do our bit
        c.increment();
    }

    constexpr static void decrement(const enumerated_t& range,
                                    cursor_t& c) OKAYLIB_NOEXCEPT
        requires detail::range_impls_decrement_c<range_t>
    {
        ok::decrement(
            range.template get_view_reference<enumerated_t, range_t>(),
            c.inner());
        c.decrement();
    }

    constexpr static value_type get(const enumerated_t& range,
                                    const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::range_definition_inner_t<range_t>;

        const range_t& parent_ref =
            range.template get_view_reference<enumerated_t, range_t>();
        // only const cast if the thing we are viewing is another view or a
        // nonconst reference to a range
        auto& casted_parent_ref = const_cast<std::conditional_t<
            detail::is_view_v<range_t> ||
                !is_const_c<std::remove_reference_t<input_range_t>>,
            range_t&, const range_t&>>(parent_ref);

        return value_type{ok::range_get_best(casted_parent_ref, c.inner()),
                          c.index()};
    }
};

// in the case that the child is arraylike, we can just use the cursor as the
// index
template <typename input_range_t>
    requires detail::range_marked_arraylike_c<input_range_t>
struct range_definition<detail::enumerated_view_t<input_range_t>>
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::enumerated_view_t<input_range_t>,
          remove_cvref_t<input_range_t>,
          detail::enumerated_cursor_t<input_range_t>>
{
    constexpr static bool is_view = true;

    constexpr static range_flags flags =
        range_flags::producing | range_flags::arraylike | range_flags::sized;
    constexpr static range_strict_flags strict_flags =
        range_strict_flags::disallow_range_def_compare |
        range_strict_flags::disallow_range_def_decrement |
        range_strict_flags::disallow_range_def_increment |
        range_strict_flags::disallow_range_def_offset;

    using range_t = std::remove_reference_t<input_range_t>;
    using enumerated_t = detail::enumerated_view_t<input_range_t>;
    using parent_get_return_type = decltype(ok::range_get_best(
        stdc::declval<input_range_t>(),
        stdc::declval<const cursor_type_for<input_range_t>&>()));

    using value_type = ok::tuple<parent_get_return_type, const size_t>;

    constexpr static value_type get(const enumerated_t& range,
                                    size_t cursor) OKAYLIB_NOEXCEPT
    {
        const range_t& parent_ref =
            range.template get_view_reference<enumerated_t, range_t>();

        auto& casted_parent_ref = const_cast<std::conditional_t<
            detail::is_view_v<range_t> ||
                !is_const_c<std::remove_reference_t<input_range_t>>,
            range_t&, const range_t&>>(parent_ref);

        return value_type{ok::range_get_best(casted_parent_ref, cursor),
                          cursor};
    }
};

constexpr detail::range_adaptor_closure_t<detail::enumerate_fn_t> enumerate;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename range_t>
struct fmt::formatter<ok::detail::enumerated_view_t<range_t>>
{
    using formatted_type_t = ok::detail::enumerated_view_t<range_t>;
    static_assert(
        fmt::is_formattable<ok::remove_cvref_t<range_t>>::value,
        "Attempt to format enumerated_view_t whose inner range is not "
        "formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& enumerated,
                                    format_context& ctx) const
    {
        return fmt::format_to(
            ctx.out(), "joined_view_t< {} >",
            enumerated
                .template get_view_reference<formatted_type_t, range_t>());
    }
};
#endif

#endif
