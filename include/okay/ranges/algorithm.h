#ifndef __OKAYLIB_RANGES_ALGORITHM_H__
#define __OKAYLIB_RANGES_ALGORITHM_H__

#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {

template <typename lhs, typename rhs, typename = void>
struct is_comparable : public std::false_type
{};

template <typename lhs, typename rhs>
struct is_comparable<lhs, rhs,
                     std::enable_if_t<std::is_same_v<
                         bool, decltype(std::declval<const lhs&>() ==
                                        std::declval<const rhs&>())>>>
    : public std::true_type
{};

struct ranges_equal_fn_t
{
    template <typename range_lhs_t, typename range_rhs_t>
    constexpr decltype(auto)
    operator()(range_lhs_t&& lhs, range_rhs_t&& rhs) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_range_v<range_lhs_t>,
                      "Cannot compare given type on left-hand side because it "
                      "is not a range.");
        static_assert(is_input_range_v<range_lhs_t>,
                      "Cannot compare given type on left-hand side because it "
                      "is not an input range.");
        static_assert(is_range_v<range_rhs_t>,
                      "Cannot compare given type on right-hand side because it "
                      "is not a range.");
        static_assert(is_input_range_v<range_rhs_t>,
                      "Cannot compare given type on right-hand side because it "
                      "is not an input range.");
        static_assert(
            is_comparable<value_type_for<range_lhs_t>,
                          value_type_for<range_rhs_t>>::value,
            "Cannot call ranges_equal on the given types, there is no "
            "operator== between their value types which returns a boolean.");

        static_assert(!range_marked_infinite_v<range_lhs_t> ||
                          !range_marked_infinite_v<range_rhs_t>,
                      "At least one range passed to ok::range_equal must be "
                      "non-infinite.");

        constexpr bool both_ranges_have_known_size =
            range_definition_has_size_v<range_lhs_t> &&
            range_definition_has_size_v<range_rhs_t>;

        if constexpr (both_ranges_have_known_size) {
            if (ok::size(lhs) != ok::size(rhs)) {
                return false;
            }
        }

        if constexpr (both_ranges_have_known_size ||
                      range_marked_infinite_v<range_rhs_t>) {
            // case where we only check if the lhs is in bounds
            auto rhs_cursor = ok::begin(rhs);
            for (auto lhs_cursor = ok::begin(lhs);
                 ok::is_inbounds(lhs, lhs_cursor);
                 ok::increment(lhs, lhs_cursor)) {
                __ok_internal_assert(ok::is_inbounds(rhs, rhs_cursor));

                auto&& r = ok::iter_get_temporary_ref(rhs, rhs_cursor);
                auto&& l = ok::iter_get_temporary_ref(lhs, lhs_cursor);

                if (r != l) {
                    return false;
                }

                ok::increment(rhs, rhs_cursor);
            }
            return true;
        } else if constexpr (range_marked_infinite_v<range_lhs_t>) {
            // case where we only check if the rhs is in bounds
            auto lhs_cursor = ok::begin(lhs);
            for (auto cursor = ok::begin(rhs); ok::is_inbounds(rhs, cursor);
                 ok::increment(rhs, cursor)) {
                __ok_internal_assert(ok::is_inbounds(lhs, lhs_cursor));

                auto&& r = ok::iter_get_temporary_ref(rhs, cursor);
                auto&& l = ok::iter_get_temporary_ref(lhs, lhs_cursor);

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
            static_assert((range_marked_finite_v<range_lhs_t> ||
                           range_marked_finite_v<range_rhs_t>) &&
                              (!range_marked_infinite_v<range_lhs_t> &&
                               !range_marked_infinite_v<range_rhs_t>),
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

                auto&& r = ok::iter_get_temporary_ref(rhs, rhs_cursor);
                auto&& l = ok::iter_get_temporary_ref(lhs, lhs_cursor);

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

constexpr detail::range_adaptor_t<detail::ranges_equal_fn_t> ranges_equal;

} // namespace ok

#endif
