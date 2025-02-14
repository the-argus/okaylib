#ifndef __OKAYLIB_RANGES_VIEW_ZIP_H__
#define __OKAYLIB_RANGES_VIEW_ZIP_H__

#include "okay/detail/get_best.h"
#include "okay/detail/template_util/first_type_in_pack.h"
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
    friend struct ok::range_definition<zipped_view_t, void>;
    friend struct sized_zip_definition_t<ranges_t...>;

    // NOTE: because we don't inherit from underlying_view_type with
    // zipped_view, we have to replicate this aspect of its API since normally
    // that is inherited by nested views
    template <typename derived_t, typename desired_reference_t>
    constexpr remove_cvref_t<desired_reference_t>&
    get_view_reference() & noexcept
    {
        // in practice desired_t is zipped_view_t, could just be return *this;
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

  public:
    static constexpr size_t num_ranges = sizeof...(ranges_t);
    static_assert(num_ranges >= 2, "Cannot zip less than two ranges.");

    using first_range_t = first_type_in_pack_t<ranges_t...>;
    static_assert(is_range_v<first_range_t>);

    static constexpr size_t num_sized_ranges =
        (... + size_t(range_can_size_v<ranges_t>));

    static constexpr size_t num_infinite_ranges =
        (... + size_t(range_marked_infinite_v<ranges_t>));

    static constexpr size_t num_finite_ranges =
        (... + size_t(range_marked_finite_v<ranges_t>));

    // mutually exclusive stuff should add up to total
    static_assert(num_sized_ranges + num_infinite_ranges + num_finite_ranges ==
                  num_ranges);

    static constexpr size_t num_arraylike_ranges =
        (... + size_t(range_is_arraylike_v<ranges_t>));

    static constexpr bool all_infinite = num_ranges == num_infinite_ranges;
    static constexpr bool all_bidirectional =
        (... && is_bidirectional_range_v<ranges_t>);
    static constexpr bool all_random_access =
        (... && is_random_access_range_v<ranges_t>);

    static constexpr bool is_sized = range_can_size_v<first_range_t>;
    static constexpr bool is_infinite = all_infinite;
    static constexpr bool is_finite = range_marked_finite_v<first_range_t>;

    static_assert(
        all_infinite || !range_marked_infinite_v<first_range_t>,
        "Cannot zip differently sized ranges where the first range is "
        "infinite. The first range determines the length of the whole view, "
        "so it being infinite will cause an out-of-bounds panic when viewing "
        "the other, shorter ranges.");

    // guarantee only one is selected
    static_assert(int(is_sized) + int(is_infinite) + int(is_finite) == 1);

    template <size_t... indices>
    constexpr bool
    find_expected_size_impl(std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        bool shorter_sized_range = false;
        int64_t size = -1;
        (
            [&] {
                if constexpr (range_can_size_v<ranges_t>) {
                    const size_t actual_size =
                        ok::size(std::get<indices>(m_views));

                    // stop before overflow
                    if (actual_size > std::numeric_limits<int64_t>::max())
                        [[unlikely]] {
                        __ok_abort("Integer overflow in zip view");
                    }

                    if (size == -1) {
                        size = actual_size;
                    } else if (actual_size < size) {
                        shorter_sized_range = true;
                    }
                }
            }(),
            ...);

        expected_size = size;

        return !shorter_sized_range;
    }

    // set expected_size to a positive number if there is a sized range,
    // otherwise it is negative (for example if all range sizes are unknown)
    // returns false if there is some kind of mismatch in sizes
    constexpr bool find_expected_size() OKAYLIB_NOEXCEPT
    {
        return find_expected_size_impl(std::make_index_sequence<num_ranges>());
    }

    constexpr zipped_view_t(ranges_t&&... ranges) OKAYLIB_NOEXCEPT
        : m_views(std::forward<ranges_t>(ranges)...)
    {
        if constexpr (is_sized) {
            if (!find_expected_size()) [[unlikely]] {
                // cannot zip these ranges- one of the sized ranges is shorter
                // than the starting range
                __ok_abort(
                    "Attempt to zip some ranges of known size, and one of them "
                    "seems to be shorter than the first range (the first range "
                    "in the zip determines the length of the whole zip)");
            }
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
        OKAYLIB_NOEXCEPT
        : m_cursors(std::forward<cursor_type_for<ranges_t>>(ranges)...)
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
struct range_definition<
    detail::zipped_view_t<ranges_t...>,
    // only enable this if not all the views are arraylike
    std::enable_if_t<(... + size_t(detail::range_is_arraylike_v<ranges_t>)) !=
                     sizeof...(ranges_t)>>
    : public std::conditional_t<
          detail::zipped_view_t<ranges_t...>::is_sized,
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
                     std::index_sequence<0UL, indices...>) OKAYLIB_NOEXCEPT
    {
        const bool first_inbounds = ok::is_inbounds(
            std::get<0>(range.m_views), std::get<0>(cursor.m_cursors));
        if (!first_inbounds)
            return false;

        // make sure that, if the first is inbounds, all the others are as well.
        // part of the okaylib range specification is that you dont call any
        // getters or setters until you verify that is_inbounds returns true.
        // TODO: option to turn this extra boundschecking off
        bool any_out_of_bounds = false;
        (
            [&] {
                const bool is_inbounds =
                    ok::is_inbounds(std::get<indices>(range.m_views),
                                    std::get<indices>(cursor.m_cursors));
                if (!is_inbounds) [[unlikely]]
                    any_out_of_bounds = true;
            }(),
            ...);

        if (any_out_of_bounds) [[unlikely]]
            __ok_abort(
                "Mismatched sizes of ranges in a zip() view. One of the "
                "secondary ranges when out of bounds before the first range "
                "did (the first range determines how long to iterate for!)");

        return true;
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
    static constexpr void
    offset_impl(const zipped_t& range, cursor_t& cursor, const int64_t& offset,
                std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        (
            [&] {
                ok::iter_offset(std::get<indices>(range.m_views),
                                std::get<indices>(cursor.m_cursors), offset);
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

    __ok_enable_if_static(zipped_t, zipped_t::all_bidirectional, void)
        decrement(const T& range, cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return decrement_impl(range, cursor,
                              std::make_index_sequence<zipped_t::num_ranges>());
    }

    __ok_enable_if_static(zipped_t, zipped_t::all_random_access, void)
        offset(const T& range, cursor_t& cursor,
               const int64_t& offset) OKAYLIB_NOEXCEPT
    {
        return offset_impl(range, cursor, offset,
                           std::make_index_sequence<zipped_t::num_ranges>());
    }

    // comparison just compares the first cursor
    __ok_enable_if_static(zipped_t, zipped_t::all_random_access, ok::ordering)
        compare(const zipped_t& range, const cursor_t& cursor_a,
                const cursor_t& cursor_b) OKAYLIB_NOEXCEPT
    {
        return ok::iter_compare(std::get<0>(range.m_views),
                                std::get<0>(cursor_a.m_cursors),
                                std::get<0>(cursor_b.m_cursors));
    }

    constexpr static auto get(const zipped_t& range,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return get_impl(range, c,
                        std::make_index_sequence<zipped_t::num_ranges>());
    }
};

// in the case that all zipped items are arraylike, just use a single size_t to
// index all of the views
template <typename... ranges_t>
struct range_definition<
    detail::zipped_view_t<ranges_t...>,
    std::enable_if_t<(... + size_t(detail::range_is_arraylike_v<ranges_t>)) ==
                     sizeof...(ranges_t)>>

{
    using zipped_t = detail::zipped_view_t<ranges_t...>;

    static constexpr bool is_view = true;
    static constexpr bool is_arraylike = true;

    static constexpr size_t size(const zipped_t& range) OKAYLIB_NOEXCEPT
    {
        return size_t(range.expected_size);
    }

    static constexpr decltype(auto) get(const zipped_t& range,
                                        const size_t& cursor) OKAYLIB_NOEXCEPT
    {
        return get_impl(range, cursor,
                        std::make_index_sequence<zipped_t::num_ranges>());
    }

  private:
    template <size_t... indices>
    static constexpr decltype(auto)
    get_impl(const zipped_t& range, const size_t& cursor,
             std::index_sequence<indices...>) OKAYLIB_NOEXCEPT
    {
        return std::make_tuple(ok::detail::get_best(
            const_cast<std::conditional_t<
                detail::is_view_v<detail::remove_cvref_t<ranges_t>> ||
                    !std::is_const_v<std::remove_reference_t<ranges_t>>,
                detail::remove_cvref_t<
                    std::tuple_element_t<indices, decltype(range.m_views)>>&,
                const detail::remove_cvref_t<
                    std::tuple_element_t<indices, decltype(range.m_views)>>&>>(
                std::get<indices>(range.m_views)),
            cursor)...);
    }
};

constexpr detail::range_adaptor_t<detail::zip_fn_t> zip;

} // namespace ok

#endif
