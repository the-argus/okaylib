#ifndef __OKAYLIB_RANGES_VIEWS_TRANSFORM_H__
#define __OKAYLIB_RANGES_VIEWS_TRANSFORM_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t, typename callable_t> struct transformed_view_t;

struct transform_fn_t
{
    template <typename range_t, typename callable_t>
    constexpr decltype(auto)
    operator()(range_t&& range, callable_t&& callable) const OKAYLIB_NOEXCEPT
    {
        using T = detail::remove_cvref_t<range_t>;
        static_assert(ok::is_std_invocable_c<const callable_t&,
                                             decltype(ok::range_get_best(
                                                 range, ok::begin(range)))>,
                      "Given transformation function and given range do not "
                      "match up: there is no way to call a const reference to "
                      "the given callable with the result of range_get_best(). "
                      "Make sure the callable is not marked `mutable`.");
        static_assert(!ok::is_void_c<decltype(callable(
                          ok::range_get_best(range, ok::begin(range))))>,
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
    assignment_op_wrapper_t<ok::stdc::remove_reference_t<callable_t>>
        m_transformer_callable;

  public:
    transformed_view_t(const transformed_view_t&) = default;
    transformed_view_t& operator=(const transformed_view_t&) = default;
    transformed_view_t(transformed_view_t&&) = default;
    transformed_view_t& operator=(transformed_view_t&&) = default;

    // NOTE: we only allow callables whose operator() are marked const
    constexpr decltype(auto) transformer_callable() const OKAYLIB_NOEXCEPT
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
    : public detail::propagate_all_range_definition_functions_with_conversion_t<
          detail::transformed_view_t<input_range_t, callable_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>
{
  private:
    using range_t = std::remove_reference_t<input_range_t>;
    using transformed_t = detail::transformed_view_t<input_range_t, callable_t>;
    using cursor_t = cursor_type_for<range_t>;

    template <typename T>
    static constexpr decltype(auto)
    get_and_transform(T& t, const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::range_definition_inner_t<range_t>;
        return t.transformer_callable()(ok::range_get_best(
            t.template get_view_reference<T, range_t>(), cursor));
    }

    // NOTE: we only allow callables whose operator() are marked const, no need
    // to check return type when transformed_t is nonconst
    using transforming_callable_rettype = decltype(get_and_transform(
        std::declval<const transformed_t&>(), std::declval<const cursor_t&>()));

    static constexpr range_flags determine_flags()
    {
        auto outflags = range_definition<range_t>::flags;
        outflags |= range_flags::producing;
        outflags -= range_flags::consuming;
        outflags -= range_flags::implements_set;

        if constexpr (!ok::reference_c<transforming_callable_rettype>) {
            outflags -= range_flags::ref_wrapper;
        } else {
            outflags |= range_flags::ref_wrapper;
            if (!ok::is_const_c<ok::stdc::remove_reference_t<
                    transforming_callable_rettype>>) {
                outflags |= range_flags::consuming;
            }
        }
        return outflags;
    }

  public:
    static constexpr bool is_view = true;

    using value_type =
        ok::stdc::remove_reference_t<transforming_callable_rettype>;

    static constexpr range_flags flags = determine_flags();
    static constexpr range_strict_flags strict_flags =
        detail::get_strict_flags_for_range<range_t>();

    static_assert(detail::producing_range_c<range_t>,
                  "Cannot transform something which is not an input range.");

    constexpr static decltype(auto) get(const transformed_t& i,
                                        const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return get_and_transform(i, c);
    }

    constexpr static decltype(auto) get(transformed_t& i,
                                        const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return get_and_transform(i, c);
    }
};

constexpr detail::range_adaptor_t<detail::transform_fn_t> transform;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename range_t, typename callable_t>
struct fmt::formatter<ok::detail::transformed_view_t<range_t, callable_t>>
{
    static_assert(
        fmt::is_formattable<ok::detail::remove_cvref_t<range_t>>::value,
        "Attempt to format transformed_view_t whose inner range is not "
        "formattable.");

    using formatted_type_t =
        ok::detail::transformed_view_t<range_t, callable_t>;

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& transformed_view,
                                    format_context& ctx) const
    {
        return fmt::format_to(
            ctx.out(), "transformed_view_t< {} >",
            transformed_view
                .template get_view_reference<formatted_type_t, range_t>());
    }
};
#endif

#endif
