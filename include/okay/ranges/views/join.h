#ifndef __OKAYLIB_RANGES_VIEWS_JOIN_H__
#define __OKAYLIB_RANGES_VIEWS_JOIN_H__

#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t> struct joined_view_t;

struct join_fn_t
{
    template <typename input_range_t>
    constexpr decltype(auto)
    operator()(input_range_t&& range) const OKAYLIB_NOEXCEPT
    {
        using range_t = detail::remove_cvref_t<input_range_t>;
        static_assert(is_range_v<range_t>,
                      "Cannot join given type- it is not a range.");
        static_assert(is_range_v<value_type_for<range_t>>,
                      "Cannot join given type- it's a range, but it does not "
                      "view other ranges.");
        // TODO: add caching of most recently accessed range to get this?
        static_assert(range_has_get_ref_const_v<range_t>,
                      "Cannot join given range- it does not provide a way to "
                      "get const reference to its internal ranges.");
        return joined_view_t<decltype(range)>{
            std::forward<input_range_t>(range)};
    }
};

template <typename range_t>
struct joined_view_t : public underlying_view_type<range_t>::type
{
    friend class ok::range_definition<joined_view_t>;
};

template <typename input_range_t> struct joined_cursor_t
{
  private:
    using outer_range_t = detail::remove_cvref_t<input_range_t>;
    using inner_range_t = value_type_for<outer_range_t>;
    using outer_cursor_t = cursor_type_for<outer_range_t>;
    using inner_cursor_t = cursor_type_for<inner_range_t>;
    using self_t = joined_cursor_t;

  public:
    constexpr const inner_cursor_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m_inner;
    }
    constexpr inner_cursor_t& inner() OKAYLIB_NOEXCEPT { return m_inner; }

    constexpr const outer_cursor_t& outer() const OKAYLIB_NOEXCEPT
    {
        return m_outer;
    }
    constexpr outer_cursor_t& outer() OKAYLIB_NOEXCEPT { return m_outer; }

    explicit constexpr joined_cursor_t(outer_cursor_t&& outer_cursor,
                                       inner_cursor_t&& inner_cursor_t)
        : OKAYLIB_NOEXCEPT m_outer(std::move(outer_cursor)),
          m_inner(std::move(inner_cursor_t))
    {
    }

    friend class range_definition<detail::joined_view_t<input_range_t>>;

  private:
    outer_cursor_t m_outer;
    inner_cursor_t m_inner;
};

} // namespace detail

template <typename input_range_t>
struct range_definition<detail::joined_view_t<input_range_t>>
{
    using outer_range_t = detail::remove_cvref_t<input_range_t>;
    using inner_range_t = value_type_for<outer_range_t>;

    static constexpr bool is_view = true;
    static constexpr bool infinite =
        detail::range_marked_infinite_v<input_range_t>;

    using joined_t = detail::joined_view_t<input_range_t>;
    using cursor_t = detail::joined_cursor_t<input_range_t>;

    static constexpr cursor_t begin(const joined_t& joined) OKAYLIB_NOEXCEPT
    {
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();
        auto outer_begin = ok::begin(outer_ref);
        const auto& inner = ok::iter_get_ref(outer_ref, outer_begin);
        auto inner_begin = ok::begin(inner);
        return cursor_t(std::move(outer_begin), std::move(inner_begin));
    }

    static constexpr void increment(const joined_t& joined,
                                    cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();
        const auto& inner_ref = ok::iter_get_ref(outer_ref, cursor.outer());

        ok::increment(inner_ref, cursor.inner());

        // if inner goes out of the current range, go to the next range
        if (!ok::is_inbounds(inner_ref, cursor.inner())) [[unlikely]] {
            ok::increment(outer_ref, cursor.outer());
            if (ok::is_inbounds(outer_ref, cursor.outer())) [[likely]] {
                cursor.inner() =
                    ok::begin(ok::iter_get_ref(outer_ref, cursor.outer()));
            }
        }
    }

    static constexpr bool is_inbounds(const joined_t& joined,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        // increment will always increment outer when going out of bounds on
        // inner, so we only have to check if the outer is out of bounds and we
        // should be good
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();
        return ok::is_inbounds(outer_ref, cursor.outer());
    }

    __ok_enable_if_static(joined_t, detail::range_has_get_v<inner_range_t>,
                          value_type_for<inner_range_t>)
        get(const T& joined, const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        __ok_assert(ok::is_inbounds(outer_ref, cursor.outer()));

        const auto& inner_ref = ok::iter_get_ref(outer_ref, cursor.outer());

        __ok_assert(ok::is_inbounds(inner_ref, cursor.inner()));

        return detail::range_definition_inner<inner_range_t>::get(
            inner_ref, cursor.inner());
    }

    __ok_enable_if_static(joined_t, detail::range_has_get_ref_v<inner_range_t>,
                          value_type_for<inner_range_t>&)
        get_ref(T& joined, const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        __ok_assert(ok::is_inbounds(outer_ref, cursor.outer()));

        auto& inner_ref = ok::iter_get_ref(outer_ref, cursor.outer());

        __ok_assert(ok::is_inbounds(inner_ref, cursor.inner()));

        return detail::range_definition_inner<inner_range_t>::get_ref(
            inner_ref, cursor.inner());
    }

    __ok_enable_if_static(joined_t,
                          detail::range_has_get_ref_const_v<inner_range_t>,
                          const value_type_for<inner_range_t>&)
        get_ref(const T& joined, const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        const auto& outer_ref =
            joined.template get_view_reference<joined_t, outer_range_t>();

        __ok_assert(ok::is_inbounds(outer_ref, cursor.outer()));

        const auto& inner_ref = ok::iter_get_ref(outer_ref, cursor.outer());

        __ok_assert(ok::is_inbounds(inner_ref, cursor.inner()));

        return detail::range_definition_inner<inner_range_t>::get_ref(
            inner_ref, cursor.inner());
    }
};

constexpr detail::range_adaptor_closure_t<detail::join_fn_t> join;

} // namespace ok

#endif
