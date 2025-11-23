#ifndef __OKAYLIB_RANGES_VIEWS_REVERSE_H__
#define __OKAYLIB_RANGES_VIEWS_REVERSE_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t> struct reversed_view_t;

struct reverse_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(range_c<range_t>,
                      "Cannot reverse given type- it is not a range.");
        static_assert(random_access_range_c<range_t> &&
                          range_can_size_c<range_t>,
                      "Cannot reverse given type- it is either not random "
                      "access, or its size is not finite or cannot be known in "
                      "constant time.");
        return reversed_view_t<decltype(range)>{stdc::forward<range_t>(range)};
    }
};

template <typename input_parent_range_t> struct reversed_cursor_t
{
  private:
    using parent_range_t = stdc::remove_reference_t<input_parent_range_t>;
    using parent_cursor_t = cursor_type_for<parent_range_t>;
    using self_t = reversed_cursor_t;

  public:
    explicit constexpr reversed_cursor_t(parent_cursor_t&& c) OKAYLIB_NOEXCEPT
        : m_inner(stdc::move(c))
    {
    }

    friend class ok::range_definition<reversed_view_t<input_parent_range_t>>;

    reversed_cursor_t(const reversed_cursor_t&) = default;
    reversed_cursor_t& operator=(const reversed_cursor_t&) = default;
    reversed_cursor_t(reversed_cursor_t&&) = default;
    reversed_cursor_t& operator=(reversed_cursor_t&&) = default;

    constexpr const parent_cursor_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m_inner;
    }
    constexpr parent_cursor_t& inner() OKAYLIB_NOEXCEPT { return m_inner; }

    constexpr operator parent_cursor_t() const noexcept { return inner(); }

    constexpr self_t& operator++() OKAYLIB_NOEXCEPT
    {
        --m_inner;
        return *this;
    }

    constexpr self_t& operator--() OKAYLIB_NOEXCEPT
    {
        ++m_inner;
        return *this;
    }

    constexpr self_t& operator+=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_inner -= rhs;
        return *this;
    }

    constexpr self_t& operator-=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_inner += rhs;
        return *this;
    }

    constexpr self_t operator+(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        return self_t(m_inner - rhs);
    }

    constexpr self_t operator-(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        return self_t(m_inner + rhs);
    }

    constexpr auto operator<=>(const self_t& rhs) const
    {
        return rhs <=> *this; // backwards comparison for reversed view
    }

  private:
    parent_cursor_t m_inner;
};

template <typename range_t>
struct reversed_view_t : public underlying_view_type<range_t>::type
{};
} // namespace detail

template <typename input_range_t>
    requires(!detail::range_marked_arraylike_c<remove_cvref_t<input_range_t>>)
struct range_definition<detail::reversed_view_t<input_range_t>>
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::reversed_view_t<input_range_t>, remove_cvref_t<input_range_t>,
          detail::reversed_cursor_t<input_range_t>>
{
    static constexpr bool is_view = true;

    using range_t = stdc::remove_reference_t<input_range_t>;
    using reverse_t = detail::reversed_view_t<input_range_t>;
    using cursor_t = detail::reversed_cursor_t<input_range_t>;

    using value_type = value_type_for<range_t>;

    static constexpr range_flags flags = detail::get_flags_for_range<range_t>();
    static constexpr range_strict_flags strict_flags =
        detail::get_strict_flags_for_range<range_t>();

    static_assert(
        random_access_range_c<range_t> && detail::range_can_size_c<range_t>,
        "Cannot reverse a range which is not both random access and sized.");

    constexpr static cursor_t begin(const reverse_t& i) OKAYLIB_NOEXCEPT
    {
        const auto& parent =
            i.template get_view_reference<reverse_t, range_t>();
        auto parent_cursor = ok::begin(parent);
        auto size = ok::size(parent);
        return cursor_t(parent_cursor + (size == 0 ? 0 : size - 1));
    }
};

// in the case that something is arraylike, just subtract size when doing gets
// and sets
template <typename input_range_t>
    requires detail::range_marked_arraylike_c<remove_cvref_t<input_range_t>>
struct range_definition<detail::reversed_view_t<input_range_t>>
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::reversed_view_t<input_range_t>, remove_cvref_t<input_range_t>,
          size_t>
{
    using range_t = remove_cvref_t<input_range_t>;
    using reversed_t = detail::reversed_view_t<input_range_t>;

    using value_type = value_type_for<range_t>;

    static constexpr bool is_view = true;

    static constexpr range_flags flags = detail::get_flags_for_range<range_t>();
    static constexpr range_strict_flags strict_flags =
        detail::get_strict_flags_for_range<range_t>();

    constexpr static auto get(const reversed_t& range,
                              size_t cursor) OKAYLIB_NOEXCEPT
        requires detail::range_impls_get_c<range_t>
    {
        const auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(
            cursor < size,
            "Bad cursor passed to reverse_view_t::get(), overflow will occur");
        return ok::range_get_best(parent_ref, size - (cursor + 1));
    }

    constexpr static auto get(reversed_t& range, size_t cursor) OKAYLIB_NOEXCEPT
        requires detail::range_impls_get_c<range_t>
    {
        auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(
            cursor < size,
            "Bad cursor passed to reverse_view_t::get(), overflow will occur");
        return ok::range_get_best(parent_ref, size - (cursor + 1));
    }

    template <typename... construction_args_t>
        requires detail::range_impls_construction_set_c<range_t,
                                                        construction_args_t...>
    static constexpr void set(reversed_t& range, size_t cursor,
                              construction_args_t&&... args) OKAYLIB_NOEXCEPT
    {
        const auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(
            cursor < size,
            "Bad cursor passed to reverse_view_t::set(), overflow will occur");
        ok::range_set(parent_ref, size - (cursor + 1),
                      stdc::forward<construction_args_t>(args)...);
    }
};

constexpr detail::range_adaptor_closure_t<detail::reverse_fn_t> reverse;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename range_t>
struct fmt::formatter<ok::detail::reversed_view_t<range_t>>
{
    using formatted_type_t = ok::detail::reversed_view_t<range_t>;
    static_assert(
        fmt::is_formattable<ok::remove_cvref_t<range_t>>::value,
        "Attempt to format reversed_view_t whose inner range type is not "
        "formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& reversed,
                                    format_context& ctx) const
    {
        return fmt::format_to(
            ctx.out(), "reverse_view_t< {} >",
            reversed.template get_view_reference<formatted_type_t, range_t>());
    }
};
#endif

#endif
