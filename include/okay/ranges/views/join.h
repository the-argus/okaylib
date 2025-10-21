#ifndef __OKAYLIB_RANGES_VIEWS_JOIN_H__
#define __OKAYLIB_RANGES_VIEWS_JOIN_H__

#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/opt.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename range_t> struct joined_view_t;

struct join_fn_t
{
    template <typename input_range_t>
    constexpr decltype(auto)
    operator()(input_range_t&& range) const OKAYLIB_NOEXCEPT
    {
        using range_t = std::remove_reference_t<input_range_t>;
        static_assert(range_c<range_t>,
                      "Cannot join given type- it is not a range.");
        static_assert(range_c<value_type_for<range_t>>,
                      "Cannot join given type- it's a range, but it does not "
                      "view other ranges.");
        static_assert(producing_range_c<range_t> &&
                          producing_range_c<value_type_for<range_t>>,
                      "Cannot join given range- it is not a producing range "
                      "which views other producing ranges.");
        return joined_view_t<decltype(range)>{
            std::forward<input_range_t>(range)};
    }
};

template <typename range_t>
struct joined_view_t : public underlying_view_type<range_t>::type
{
    friend class ok::range_definition<joined_view_t>;
};

/// input_inner_range_t refers to the inner range given to this view. if the
/// outer range of the joined view just provides get(), then we take ownership
/// of that value inside of this cursor, and input_inner_range_t is an rvalue
/// reference. otherwise its an lvalue reference and we just use ref_view
template <typename input_outer_range_t, typename input_inner_range_t>
struct joined_cursor_t
{
  private:
    using outer_range_t = detail::remove_cvref_t<input_outer_range_t>;
    using inner_range_t = value_type_for<outer_range_t>;
    static_assert(!stdc::is_reference_c<outer_range_t> &&
                  !is_const_c<std::remove_reference_t<outer_range_t>>);
    static_assert(std::is_same_v<detail::remove_cvref_t<input_inner_range_t>,
                                 inner_range_t>,
                  "something broken with join view implementation");
    using outer_cursor_t = cursor_type_for<outer_range_t>;
    using inner_cursor_t = cursor_type_for<inner_range_t>;
    using self_t = joined_cursor_t;

    using view_t =
        typename detail::underlying_view_type<input_inner_range_t>::type;

  public:
    constexpr const inner_cursor_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m.ref_or_panic().inner;
    }
    constexpr inner_cursor_t& inner() OKAYLIB_NOEXCEPT
    {
        return m.ref_or_panic().inner;
    }

    constexpr const outer_cursor_t& outer() const OKAYLIB_NOEXCEPT
    {
        return m.ref_or_panic().outer;
    }
    constexpr outer_cursor_t& outer() OKAYLIB_NOEXCEPT
    {
        return m.ref_or_panic().outer;
    }

    template <
        typename T,
        std::enable_if_t<std::is_same_v<T, input_inner_range_t>, bool> = true>
    constexpr joined_cursor_t(outer_cursor_t&& outer_cursor,
                              T&& inner_range) OKAYLIB_NOEXCEPT
        : m(ok::in_place, std::move(outer_cursor), std::forward<T>(inner_range))
    {
    }

    constexpr bool has_value() const OKAYLIB_NOEXCEPT { return m.has_value(); }

    constexpr joined_cursor_t() = default;

    friend class range_definition<detail::joined_view_t<input_outer_range_t>>;

  private:
    // can always get mutable internal view from range friend impl
    constexpr view_t& view() const OKAYLIB_NOEXCEPT
    {
        return m.ref_or_panic().inner_view;
    }

    struct members_t
    {
        mutable view_t inner_view;
        outer_cursor_t outer;
        cursor_type_for<view_t> inner;

        template <typename T,
                  std::enable_if_t<std::is_same_v<T, input_inner_range_t>,
                                   bool> = true>
        constexpr members_t(outer_cursor_t&& _outer, T&& range) OKAYLIB_NOEXCEPT
            : outer(std::move(_outer)),
              inner_view(std::forward<T>(range)),
              inner(ok::begin(inner_view))
        {
            static_assert(stdc::is_reference_c<decltype(range)>);
        }
    };

    opt<members_t> m;
};

} // namespace detail

template <typename input_range_t>
struct range_definition<detail::joined_view_t<input_range_t>>
{
  private:
    using outer_range_t = detail::remove_cvref_t<input_range_t>;
    using inner_range_t = value_type_for<outer_range_t>;
    static_assert(!detail::range_marked_infinite_c<inner_range_t>,
                  "Cannot join infinite ranges.");

    static constexpr range_flags determine_flags()
    {
        const auto parents_flags = range_def_for<inner_range_t>::flags;
        auto nosize_flags =
            parents_flags -
            (range_flags::sized | range_flags::finite | range_flags::infinite);
        if (!detail::range_gets_by_value_c<inner_range_t> &&
            (std::is_lvalue_reference_v<input_range_t> ||
             detail::range_marked_ref_wrapper_c<input_range_t>)) {
            nosize_flags = nosize_flags | range_flags::ref_wrapper;
        }

        // inherit sizedness from outer range
        const auto outer_flags = range_def_for<outer_range_t>::flags;
        if (outer_flags & range_flags::sized &&
            parents_flags & range_flags::sized) {
            return parents_flags;
        }
        if (outer_flags & range_flags::infinite) {
            return nosize_flags & range_flags::infinite;
        }

        static_assert(!(parents_flags & range_flags::infinite));

        // if the outer range is not infinite, and they're not both sized, the
        // inner cannot be infinite, then at least one must be finite, therefore
        // the whole thing is finite
        return nosize_flags & range_flags::finite;
    }

  public:
    static constexpr range_flags flags = determine_flags();

    using joined_t = detail::joined_view_t<input_range_t>;
    using cursor_t = detail::joined_cursor_t<
        input_range_t, decltype(range_get_best(
                           std::declval<const outer_range_t&>(),
                           std::declval<cursor_type_for<outer_range_t>>()))>;
    using value_type = value_type_for<inner_range_t>;

    using cursor_view_t = typename cursor_t::view_t;

    constexpr static cursor_t begin(const joined_t& joined) OKAYLIB_NOEXCEPT
    {
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        auto outer_cursor = ok::begin(outer_ref);

        while (ok::is_inbounds(outer_ref, outer_cursor)) {
            auto&& inner = ok::range_get(outer_ref, outer_cursor);
            cursor_type_for<inner_range_t> inner_cursor = ok::begin(inner);

            // make sure we're not empty
            if (ok::is_inbounds(inner, inner_cursor)) {
                if constexpr (stdc::is_lvalue_reference_v<decltype(inner)>) {
                    return cursor_t(std::move(outer_cursor), inner);
                } else {
                    return cursor_t(std::move(outer_cursor), std::move(inner));
                }
            }

            ok::increment(outer_ref, outer_cursor);
        }

        // out of bounds of outer
        return {};
    }

    constexpr static void increment(const joined_t& joined,
                                    cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        if (!cursor.has_value()) [[unlikely]]
            return;

        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();
        auto& outer_cursor = cursor.outer();

        // cant increment further
        if (!ok::is_inbounds(outer_ref, outer_cursor))
            return;

        auto& inner_cursor = cursor.inner();
        auto& inner_view = cursor.view();
        __ok_internal_assert(ok::is_inbounds(inner_view, inner_cursor));
        ok::increment(inner_view, inner_cursor);

        // if good after increment, then we just did a valid increment and
        // we're done
        if (ok::is_inbounds(inner_view, inner_cursor)) {
            return;
        }

        // ran out of items on the current range, increment outer cursor until
        // we find a non empty subrange
        ok::increment(outer_ref, outer_cursor);
        while (ok::is_inbounds(outer_ref, outer_cursor)) {

            inner_view =
                cursor_view_t(ok::range_get_best(outer_ref, outer_cursor));

            cursor.inner() = ok::begin(inner_view);

            if (ok::is_inbounds(inner_view, cursor.inner())) {
                return;
            }

            ok::increment(outer_ref, outer_cursor);
        }
    }

    constexpr static bool is_inbounds(const joined_t& joined,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        if (!cursor.has_value())
            return false;
        // increment will always increment outer when going out of bounds on
        // inner, so we only have to check if the outer is out of bounds and we
        // should be good. NOTE: update this check if bidirectional or random
        // access is added
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();
        return ok::is_inbounds(outer_ref, cursor.outer());
    }

    constexpr static decltype(auto) get(const joined_t& joined,
                                        const cursor_t& cursor) OKAYLIB_NOEXCEPT
        requires detail::range_impls_get_c<cursor_view_t>
    {
        __ok_assert(cursor.has_value(), "Invalid cursor passed to join view, "
                                        "it seems to be uninitialized.");

        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        __ok_assert(ok::is_inbounds(outer_ref, cursor.outer()),
                    "Out of bounds cursor passed to join_view's get method.");

        return ok::range_get(cursor.view(), cursor.inner());
    }

    constexpr static decltype(auto) get(joined_t& joined,
                                        const cursor_t& cursor) OKAYLIB_NOEXCEPT
        requires detail::range_impls_get_c<cursor_view_t>
    {
        __ok_assert(cursor.has_value(), "Invalid cursor passed to join view, "
                                        "it seems to be uninitialized.");

        auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        __ok_assert(ok::is_inbounds(outer_ref, cursor.outer()),
                    "Out of bounds cursor passed to join_view's get method.");

        return ok::range_get(cursor.view(), cursor.inner());
    }

    template <typename... args_t>
    constexpr static void set(joined_t& joined, const cursor_t& cursor,
                              args_t&&... constructor_args) OKAYLIB_NOEXCEPT
        requires detail::range_impls_construction_set_c<inner_range_t,
                                                        args_t...>
    {
        __ok_assert(cursor.has_value(), "Invalid cursor passed to join view, "
                                        "it seems to be uninitialized.");

        auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        __ok_assert(ok::is_inbounds(outer_ref, cursor.outer()),
                    "Out of bounds cursor passed to join_view's get method.");

        return ok::range_set(cursor.view(), cursor.inner(),
                             stdc::forward<args_t>(constructor_args)...);
    }
};

constexpr detail::range_adaptor_closure_t<detail::join_fn_t> join;

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename range_t>
struct fmt::formatter<ok::detail::joined_view_t<range_t>>
{
    using formatted_type_t = ok::detail::joined_view_t<range_t>;
    static_assert(
        fmt::is_formattable<ok::detail::remove_cvref_t<range_t>>::value,
        "Attempt to format joined_view_t whose inner range is not "
        "formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const formatted_type_t& joined_view_t,
                                    format_context& ctx) const
    {
        return fmt::format_to(
            ctx.out(), "joined_view_t< {} >",
            joined_view_t
                .template get_view_reference<formatted_type_t, range_t>());
    }
};
#endif

#endif
