#ifndef __OKAYLIB_RANGES_ALGORITHM_H__
#define __OKAYLIB_RANGES_ALGORITHM_H__

#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {

struct ranges_equal_fn_t
{
    template <producing_range_c range_lhs_t, producing_range_c range_rhs_t>
        requires(is_equality_comparable_to_c<value_type_for<range_lhs_t>,
                                             value_type_for<range_rhs_t>> &&
                 (!range_marked_infinite_c<range_lhs_t> ||
                  !range_marked_infinite_c<range_rhs_t>))
    constexpr decltype(auto)
    operator()(range_lhs_t&& lhs, range_rhs_t&& rhs) const OKAYLIB_NOEXCEPT
    {
        constexpr bool both_ranges_have_known_size =
            range_impls_size_c<range_lhs_t> && range_impls_size_c<range_rhs_t>;

        if constexpr (both_ranges_have_known_size) {
            if (ok::size(lhs) != ok::size(rhs)) {
                return false;
            }
        }

        if constexpr (both_ranges_have_known_size ||
                      range_marked_infinite_c<range_rhs_t>) {
            // case where we only check if the lhs is in bounds
            auto rhs_cursor = ok::begin(rhs);
            for (auto lhs_cursor = ok::begin(lhs);
                 ok::is_inbounds(lhs, lhs_cursor);
                 ok::increment(lhs, lhs_cursor)) {
                __ok_internal_assert(ok::is_inbounds(rhs, rhs_cursor));

                auto&& r = ok::range_get_best(rhs, rhs_cursor);
                auto&& l = ok::range_get_best(lhs, lhs_cursor);

                if (r != l) {
                    return false;
                }

                ok::increment(rhs, rhs_cursor);
            }
            return true;
        } else if constexpr (range_marked_infinite_c<range_lhs_t>) {
            // case where we only check if the rhs is in bounds
            auto lhs_cursor = ok::begin(lhs);
            for (auto cursor = ok::begin(rhs); ok::is_inbounds(rhs, cursor);
                 ok::increment(rhs, cursor)) {
                __ok_internal_assert(ok::is_inbounds(lhs, lhs_cursor));

                auto&& r = ok::range_get_best(rhs, cursor);
                auto&& l = ok::range_get_best(lhs, lhs_cursor);

                if (r != l) {
                    return false;
                }

                ok::increment(lhs, lhs_cursor);
            }
            return true;
        } else {
            // case where we keep track of whether either side has gone out of
            // bounds at any point

            // only do this if we have some combination of sized and finite
            // ranges, (either infinite) or (both sized) dont make sense
            static_assert((range_marked_finite_c<range_lhs_t> ||
                           range_marked_finite_c<range_rhs_t>) &&
                              (!range_marked_infinite_c<range_lhs_t> &&
                               !range_marked_infinite_c<range_rhs_t>),
                          "Internal static_assert failed, broken template "
                          "logic in okaylib");
            auto lhs_cursor = ok::begin(lhs);
            auto rhs_cursor = ok::begin(rhs);

            while (true) {
                const bool left_good = ok::is_inbounds(lhs, lhs_cursor);
                const bool right_good = ok::is_inbounds(rhs, rhs_cursor);

                if (!left_good || !right_good) {
                    // only return that theyre the same if they both ended at
                    // the same time
                    return left_good == right_good;
                }

                auto&& r = ok::range_get_best(rhs, rhs_cursor);
                auto&& l = ok::range_get_best(lhs, lhs_cursor);

                if (r != l) {
                    return false;
                }

                ok::increment(lhs, lhs_cursor);
                ok::increment(rhs, rhs_cursor);
            }
        }
    }
};
} // namespace detail

template <typename T> struct dest
{
    constexpr dest(T& ref) noexcept : m_ref(ref) {}

    constexpr T& value() const noexcept { return m_ref; }

  private:
    T& m_ref;
};

template <typename T> struct source
{
    constexpr source(const T& ref) noexcept : m_ref(ref) {}
    constexpr const T& value() const noexcept { return m_ref; }

  private:
    const T& m_ref;
};

namespace detail {

template <bool allow_small_destination = false> struct ranges_copy_fn_t
{
    template <typename dest_range_t, typename source_range_t>
    constexpr void
    operator()(dest<dest_range_t> dest_wrapper,
               source<source_range_t> source_wrapper) const OKAYLIB_NOEXCEPT
    {
        dest_range_t& dest = dest_wrapper.value();
        const source_range_t& source = source_wrapper.value();

        static_assert(detail::consuming_range_c<dest_range_t>,
                      "Range given as `dest` is not an output range.");
        static_assert(detail::producing_range_c<source_range_t>,
                      "Range given as `source` is not an input range.");
        static_assert(is_convertible_to_c<value_type_for<source_range_t>,
                                          value_type_for<dest_range_t>>,
                      "The values inside the given ranges are not convertible, "
                      "cannot copy from source to dest.");
        static_assert(
            !detail::range_marked_infinite_c<dest_range_t> ||
                !detail::range_marked_infinite_c<source_range_t>,
            "Attempt to copy an infinite range into an infinite range, "
            "this will just loop forever.");
        constexpr bool both_ranges_have_known_size =
            range_impls_size_c<dest_range_t> &&
            range_impls_size_c<source_range_t>;

        if constexpr (both_ranges_have_known_size) {

            if (ok::size(dest) < ok::size(source)) {
                if constexpr (!allow_small_destination) {
                    __ok_abort(
                        "Attempt to ranges_copy() from a source which is "
                        "larger than the destination.");
                } else {
                    // we know destination is smaller, track that only
                    auto dest_cursor = ok::begin(dest);
                    auto source_cursor = ok::begin(source);
                    while (ok::is_inbounds(dest, dest_cursor)) {
                        ok::range_set(
                            dest, dest_cursor,
                            ok::range_get_best(source, source_cursor));

                        ok::increment(dest, dest_cursor);
                        ok::increment(source, source_cursor);
                    }
                }
            } else {
                // identical code to above but we only check if the source is
                // exhausted, since we know that dest is at least as big
                auto dest_cursor = ok::begin(dest);
                auto source_cursor = ok::begin(source);
                while (ok::is_inbounds(source, source_cursor)) {
                    ok::range_set(dest, dest_cursor,
                                  ok::range_get_best(source, source_cursor));

                    ok::increment(dest, dest_cursor);
                    ok::increment(source, source_cursor);
                }
            }

        } else {
            auto dest_cursor = ok::begin(dest);
            auto source_cursor = ok::begin(source);
            while (true) {

                // optimizations: we only need to check the bounds for
                // non-infinite ranges
                if constexpr (range_marked_infinite_c<source_range_t>) {
                    if (!ok::is_inbounds(dest, dest_cursor)) {
                        return;
                    }
                } else if constexpr (range_marked_infinite_c<dest_range_t>) {
                    if (!ok::is_inbounds(source, source_cursor)) {
                        return;
                    }
                } else {
                    // if both ranges are either finite or sized, we have to
                    // check both ranges
                    const bool dest_inbounds =
                        ok::is_inbounds(dest, dest_cursor);
                    const bool source_inbounds =
                        ok::is_inbounds(source, source_cursor);

                    if (!source_inbounds) {
                        return;
                    } else if (!dest_inbounds) {
                        if constexpr (!allow_small_destination) {
                            // still more things to copy, but dest ran out
                            __ok_abort(
                                "Attempt to ranges_copy() from a source which "
                                "is larger than the destination can hold.");
                        } else {
                            return;
                        }
                    }
                }

                ok::range_set(dest, dest_cursor,
                              ok::range_get_best(source, source_cursor));

                ok::increment(dest, dest_cursor);
                ok::increment(source, source_cursor);
            }
        }
    }
};

} // namespace detail

inline constexpr detail::ranges_equal_fn_t ranges_equal;
inline constexpr detail::ranges_copy_fn_t<false> ranges_copy;
inline constexpr detail::ranges_copy_fn_t<true> ranges_copy_as_much_as_will_fit;

} // namespace ok

#endif
