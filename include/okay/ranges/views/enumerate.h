#ifndef __OKAYLIB_RANGES_VIEWS_ENUMERATE_H__
#define __OKAYLIB_RANGES_VIEWS_ENUMERATE_H__

#include "okay/detail/get_best.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t> struct enumerated_view_t;

struct enumerate_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_range_v<range_t>,
                      "Cannot enumerate given type- it is not a range.");
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
    friend class ok::orderable_definition<enumerated_cursor_t>;

    constexpr size_t index() const OKAYLIB_NOEXCEPT { return m_index; }

  private:
    size_t m_index;
};

} // namespace detail

template <typename parent_range_t>
class ok::orderable_definition<detail::enumerated_cursor_t<parent_range_t>>
{
    using self_t = detail::enumerated_cursor_t<parent_range_t>;

  public:
    static constexpr bool is_strong_orderable =
        is_strong_fully_orderable_v<cursor_type_for<parent_range_t>>;

    static constexpr ordering cmp(const self_t& lhs,
                                  const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        const auto ordering = ok::cmp(lhs.inner(), rhs.inner());
        __ok_assert(ordering == ok::cmp(lhs.m_index, rhs.m_index),
                    "Comparing two enumerated cursors resulted in a different "
                    "comparison than the indices. This may indicate a broken "
                    "range implementation.");
        return ordering;
    }
};

// TODO: review const / ref correctness here, range_t input is allowed to be a
// reference or const but thats not really being removed before sending it to
// other templates
template <typename input_range_t>
struct range_definition<detail::enumerated_view_t<input_range_t>,
                        std::enable_if_t<!detail::range_is_arraylike_v<
                            detail::remove_cvref_t<input_range_t>>>>
    : public detail::propagate_sizedness_t<
          detail::enumerated_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>>,
      public detail::propagate_begin_t<
          detail::enumerated_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          detail::enumerated_cursor_t<input_range_t>>,
      public detail::propagate_boundscheck_t<
          detail::enumerated_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          detail::enumerated_cursor_t<input_range_t>>
{
    static constexpr bool is_view = true;

    using enumerated_t = detail::enumerated_view_t<input_range_t>;
    using cursor_t = detail::enumerated_cursor_t<input_range_t>;
    using range_t = std::remove_reference_t<input_range_t>;

    using pair_first_type = std::conditional_t<
        detail::range_can_get_ref_v<range_t>,
        std::conditional_t<detail::is_consuming_range_v<range_t>,
                           value_type_for<range_t>&,
                           const value_type_for<range_t&>>,
        value_type_for<range_t>>;

    using value_type = std::pair<pair_first_type, const size_t>;

    __ok_enable_if_static(range_t, detail::range_impls_increment_v<T>, void)
        increment(const enumerated_t& range, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        // perform parent's increment function
        ok::increment(range.template get_view_reference<enumerated_t, T>(),
                      c.inner());
        // also do our bit
        c.increment();
    }

    __ok_enable_if_static(range_t, detail::range_impls_decrement_v<T>, void)
        decrement(const enumerated_t& range, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        ok::decrement(
            range.template get_view_reference<enumerated_t, range_t>(),
            c.inner());
        c.decrement();
    }

    constexpr static auto get(const enumerated_t& range,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::range_definition_inner<range_t>;

        const range_t& parent_ref =
            range.template get_view_reference<enumerated_t, range_t>();
        // only const cast if the thing we are viewing is another view or a
        // nonconst reference to a range
        auto& casted_parent_ref = const_cast<std::conditional_t<
            detail::is_view_v<range_t> ||
                !std::is_const_v<std::remove_reference_t<input_range_t>>,
            range_t&, const range_t&>>(parent_ref);

        using pair_left =
            decltype(ok::detail::get_best(casted_parent_ref, c.inner()));
        return std::pair<pair_left, const size_t>{
            ok::detail::get_best(casted_parent_ref, c.inner()), c.index()};
    }
};

// in the case that the child is arraylike, we can just use the cursor as the
// index
template <typename input_range_t>
struct range_definition<detail::enumerated_view_t<input_range_t>,
                        std::enable_if_t<detail::range_is_arraylike_v<
                            detail::remove_cvref_t<input_range_t>>>>
    : public detail::propagate_sizedness_t<
          detail::enumerated_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>>
{
    constexpr static bool is_view = true;
    constexpr static bool is_arraylike = true;

    using range_t = std::remove_reference_t<input_range_t>;
    using enumerated_t = detail::enumerated_view_t<input_range_t>;

    constexpr static auto get(const enumerated_t& range,
                              size_t cursor) OKAYLIB_NOEXCEPT
    {
        const range_t& parent_ref =
            range.template get_view_reference<enumerated_t, range_t>();

        auto& casted_parent_ref = const_cast<std::conditional_t<
            detail::is_view_v<range_t> ||
                !std::is_const_v<std::remove_reference_t<input_range_t>>,
            range_t&, const range_t&>>(parent_ref);

        using pair_left =
            decltype(ok::detail::get_best(casted_parent_ref, cursor));
        return std::pair<pair_left, const size_t>{
            ok::detail::get_best(casted_parent_ref, cursor), cursor};
    }
};

constexpr detail::range_adaptor_closure_t<detail::enumerate_fn_t> enumerate;

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename range_t>
struct fmt::formatter<ok::detail::enumerated_view_t<range_t>>
{
    using formatted_type_t = ok::detail::enumerated_view_t<range_t>;
    static_assert(
        fmt::is_formattable<ok::detail::remove_cvref_t<range_t>>::value,
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
