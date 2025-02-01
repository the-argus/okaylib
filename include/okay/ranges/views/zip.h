#ifndef __OKAYLIB_RANGES_VIEW_ZIP_H__
#define __OKAYLIB_RANGES_VIEW_ZIP_H__

#include "okay/detail/get_best.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {

template <typename... ranges_t> struct zipped_view_t;

struct zip_fn_t
{
    template <typename... ranges_t>
    constexpr auto operator()(ranges_t&&... ranges) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<sizeof...(ranges_t) >= 2, zipped_view_t<ranges_t...>>
    {
        static_assert((... && is_range_v<ranges_t>),
                      "Cannot zip given types- they are not all ranges");
        // zip puts the output of each range (the input from the view) into
        // tuples. if no get() or get_ref() is provided, then there's nothing to
        // put in the tuple.
        static_assert((... && is_input_range_v<ranges_t>),
                      "Cannot zip given types- they are not all input ranges");

        auto out =
            zipped_view_t<ranges_t...>(std::forward<ranges_t>(ranges)...);

        return out;
    }
};

template <typename... ranges_t> struct sized_zip_definition_t;

template <typename... ranges_t> struct zipped_view_t
{
    friend struct zip_fn_t;
    friend struct ok::range_definition<zipped_view_t>;
    friend struct sized_zip_definition_t<ranges_t...>;

    // NOTE: because we don't inherit from underlying_view_type with
    // zipped_view, we have to replicate this aspect of its API since normally
    // that is inherited by nested views
    template <typename derived_t, typename desired_reference_t>
    constexpr remove_cvref_t<desired_reference_t>&
    get_view_reference() & noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_v<derived_t, zipped_view_t>);
        static_assert(is_convertible_to_v<derived_t&, zipped_view_t&>);
        static_assert(std::is_convertible_v<derived_t&, desired_t&>);
        return *static_cast<derived_t*>(this);
    }
    template <typename derived_t, typename desired_reference_t>
    constexpr const remove_cvref_t<desired_reference_t>&
    get_view_reference() const& noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_v<derived_t, zipped_view_t>);
        static_assert(
            is_convertible_to_v<const derived_t&, const zipped_view_t&>);
        static_assert(
            std::is_convertible_v<const derived_t&, const desired_t&>);
        return *static_cast<const derived_t*>(this);
    }

  private:
    static constexpr bool num_bidirectional_ranges =
        (... + size_t(is_bidirectional_range_v<ranges_t>));

    static constexpr size_t num_sized_ranges =
        (... + size_t(range_can_size_v<ranges_t>));

    static constexpr size_t num_infinite_ranges =
        (... + size_t(range_marked_infinite_v<ranges_t>));

    static constexpr size_t num_finite_ranges =
        (... + size_t(range_marked_finite_v<ranges_t>));

    static constexpr size_t num_arraylike_ranges =
        (... + size_t(range_is_arraylike_v<ranges_t>));

    static constexpr size_t num_ranges = sizeof...(ranges_t);

    static constexpr bool all_sized = num_ranges == num_sized_ranges;
    static constexpr bool all_infinite = num_ranges == num_infinite_ranges;
    static constexpr bool all_arraylike = num_ranges == num_arraylike_ranges;
    static constexpr bool all_bidirectional =
        num_ranges == num_bidirectional_ranges;

    static_assert(num_ranges >= 2, "Cannot zip less than two ranges.");
    static_assert(all_infinite || num_infinite_ranges == 0,
                  "Cannot zip a set of ranges where some are infinite and "
                  "some are not.");

    template <size_t... indices>
    constexpr bool
    find_expected_size_impl(std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        bool mismatch = false;
        int64_t size = -1;
        (
            [&] {
                if constexpr (range_can_size_v<ranges_t>) {
                    const size_t actual_size =
                        ok::size(std::get<indices>(m_views));

                    // stop before overflow
                    if (actual_size > std::numeric_limits<int64_t>::max())
                        [[unlikely]] {
                        __ok_abort();
                    }

                    if (size == -1) {
                        size = actual_size;
                    } else if (actual_size != size) {
                        mismatch = true;
                    }
                }
            }(),
            ...);

        expected_size = size;

        return !mismatch;
    }

    // set expected_size to a positive number if there is a sized range,
    // otherwise it is negative (for example if all range sizes are unknown)
    // returns false if there is some kind of mismatch in sizes
    constexpr bool find_expected_size() OKAYLIB_NOEXCEPT
    {
        return find_expected_size_impl(std::make_index_sequence<num_ranges>());
    }

    constexpr zipped_view_t(ranges_t&&... ranges)
        : OKAYLIB_NOEXCEPT m_views(std::forward<ranges_t>(ranges)...)
    {
        if (!find_expected_size()) [[unlikely]] {
            // cannot zip ranges with mismatched sizes
            __ok_abort();
        }
    }

    // make views for all the ranges
    std::tuple<typename underlying_view_type<ranges_t>::type...> m_views;
    int64_t expected_size = -1;
};

template <typename... ranges_t> struct zipped_cursor_t
{
    friend struct ok::range_definition<zipped_view_t<ranges_t...>>;

  private:
    constexpr zipped_cursor_t(cursor_type_for<ranges_t>&&... ranges)
        : OKAYLIB_NOEXCEPT m_cursors(
              std::forward<cursor_type_for<ranges_t>>(ranges)...)
    {
    }

    std::tuple<cursor_type_for<ranges_t>...> m_cursors;
};

template <typename... ranges_t> struct sized_zip_definition_t
{
    static constexpr size_t
    size(const zipped_view_t<ranges_t...>& view) OKAYLIB_NOEXCEPT
    {
        return view.expected_size;
    }
};

} // namespace detail

template <typename... ranges_t>
struct range_definition<detail::zipped_view_t<ranges_t...>> :
    // if all the ranges have constant time known size, then we will already
    // have found an expected size and that is our size
    public std::conditional_t<
        detail::zipped_view_t<ranges_t...>::all_sized,
        detail::sized_zip_definition_t<ranges_t...>,
        detail::infinite_static_def_t<
            detail::zipped_view_t<ranges_t...>::all_infinite>>
{
    static constexpr bool is_view = true;

    using zipped_t = detail::zipped_view_t<ranges_t...>;
    using cursor_t = detail::zipped_cursor_t<ranges_t...>;

  private:
    template <size_t... indices>
    static constexpr decltype(auto)
    begin_impl(const zipped_t& range,
               std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        return cursor_t(ok::begin(std::get<indices>(range.m_views))...);
    }

    template <size_t... indices>
    static constexpr bool
    is_inbounds_impl(const zipped_t& range, const cursor_t& cursor,
                     std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        // in debug mode, make sure that if one cursor is out of bounds, they
        // all are (zip expects ranges of the same length)
#ifndef NDEBUG
        int inbounds_status = -1;
        (
            [&] {
                bool is_inbounds =
                    ok::is_inbounds(std::get<indices>(range.m_views),
                                    std::get<indices>(cursor.m_cursors));
                if (inbounds_status == -1) {
                    inbounds_status = is_inbounds;
                } else {
                    // this assert will fire if some of the ranges are of
                    // different sizes
                    __ok_assert(inbounds_status == is_inbounds);
                }
            }(),
            ...);
#endif

        // NOTE: we only need to check the first view
        return (... && ok::is_inbounds(std::get<indices>(range.m_views),
                                       std::get<indices>(cursor.m_cursors)));
    }

    template <size_t... indices>
    static constexpr void
    increment_impl(const zipped_t& range, cursor_t& cursor,
                   std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        (
            [&] {
                ok::increment(std::get<indices>(range.m_views),
                              std::get<indices>(cursor.m_cursors));
            }(),
            ...);
    }

    template <size_t... indices>
    static constexpr void
    decrement_impl(const zipped_t& range, cursor_t& cursor,
                   std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        (
            [&] {
                ok::decrement(std::get<indices>(range.m_views),
                              std::get<indices>(cursor.m_cursors));
            }(),
            ...);
    }

    template <size_t... indices>
    static constexpr decltype(auto)
    get_impl(const zipped_t& range, const cursor_t& cursor,
             std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        // worlds largest and most evil const cast inside of a parameter
        // expansion. the idea is that we only cast to nonconst if the viewed
        // type is itself a view or it is a nonconst reference to a range.
        // This way we respect constness of stuff like std::array, but not
        // views like ok::reverse etc.
        return std::make_tuple(ok::detail::get_best(
            const_cast<std::conditional_t<
                detail::is_view_v<detail::remove_cvref_t<ranges_t>> ||
                    !std::is_const_v<std::remove_reference_t<ranges_t>>,
                detail::remove_cvref_t<
                    std::tuple_element_t<indices, decltype(range.m_views)>>&,
                const detail::remove_cvref_t<
                    std::tuple_element_t<indices, decltype(range.m_views)>>&>>(
                std::get<indices>(range.m_views)),
            std::get<indices>(cursor.m_cursors))...);
    }

  public:
    static constexpr cursor_t begin(const zipped_t& range) OKAYLIB_NOEXCEPT
    {
        return begin_impl(range,
                          std::make_index_sequence<zipped_t::num_ranges>());
    }

    static constexpr bool is_inbounds(const zipped_t& range,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return is_inbounds_impl(
            range, cursor, std::make_index_sequence<zipped_t::num_ranges>());
    }

    static constexpr void increment(const zipped_t& range,
                                    cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return increment_impl(range, cursor,
                              std::make_index_sequence<zipped_t::num_ranges>());
    }

    __ok_enable_if_static(zipped_t, T::all_bidirectional, void)
        decrement(const T& range, cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return decrement_impl(range, cursor,
                              std::make_index_sequence<zipped_t::num_ranges>());
    }

    constexpr static auto get(const zipped_t& range,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return get_impl(range, c,
                        std::make_index_sequence<zipped_t::num_ranges>());
    }
};

constexpr detail::range_adaptor_t<detail::zip_fn_t> zip;

} // namespace ok

#endif
