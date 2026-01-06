#ifndef __OKAYLIB_ITERABLES_ALGORITHM_H__
#define __OKAYLIB_ITERABLES_ALGORITHM_H__

#include "okay/iterables/iterables.h"

namespace ok {
namespace detail {

struct identity_fn_t
{
    template <typename T>
    constexpr decltype(auto) operator()(T&& item) const noexcept
    {
        return stdc::forward<T>(item);
    }
};

struct iterators_equal_fn_t
{
    template <typename iterator_lhs_t, typename iterator_rhs_t>
    constexpr decltype(auto)
    operator()(iterator_lhs_t&& lhs,
               iterator_rhs_t&& rhs) const OKAYLIB_NOEXCEPT
        requires requires {
            requires(!is_iterable_infinite<decltype(lhs)> ||
                     !is_iterable_infinite<decltype(rhs)>);
            requires is_equality_comparable_to_c<
                value_type_for<iterator_lhs_t>, value_type_for<iterator_rhs_t>>;
        }
    {
        constexpr bool both_ranges_have_known_size =
            sized_iterator_c<iterator_lhs_t> &&
            sized_iterator_c<iterator_rhs_t>;

        if constexpr (both_ranges_have_known_size) {
            if (lhs.size() != rhs.size()) {
                return false;
            }
        }

        auto&& lhs_iterator = ok::iter(stdc::forward<iterator_lhs_t>(lhs));
        auto&& rhs_iterator = ok::iter(stdc::forward<iterator_rhs_t>(rhs));

        while (true) {
            ok::opt lhs_value = lhs_iterator.next();
            ok::opt rhs_value = rhs_iterator.next();

            if (!lhs_value.deep_compare_with(rhs_value))
                return false;
            if (!lhs_value && !rhs_value)
                return true;
        }
        // NOTE: unreachable
    }
};
} // namespace detail

namespace detail {

template <bool allow_small_destination = false>
struct iterators_copy_assign_fn_t
{
    template <typename dest_iterator_t, typename source_iterator_t>
    constexpr void operator()(dest_iterator_t&& dest,
                              source_iterator_t&& source) const OKAYLIB_NOEXCEPT
        requires iterable_c<decltype(dest)> && iterable_c<decltype(source)>
    {
        static_assert(
            ok::stdc::is_lvalue_reference_v<value_type_for<dest_iterator_t>>,
            "Attempt to copy assign into an iterator which does not "
            "produce references.");
        static_assert(
            requires(value_type_for<dest_iterator_t> destitem,
                     value_type_for<source_iterator_t> sourceitem) {
                {
                    destitem = sourceitem
                } -> ok::same_as_c<stdc::add_lvalue_reference_t<
                      value_type_for<dest_iterator_t>>>;
            },
            "Attempt to copy assign from an iterator which cannot assign into "
            "the destination.");

        static_assert(
            !is_iterable_infinite<decltype(dest)> ||
                !is_iterable_infinite<decltype(source)>,
            "Attempt to copy an infinite range into an infinite range, "
            "this will just loop forever.");

        auto&& dest_iter = iter(dest);
        auto&& source_iter = iter(source);

        while (true) {
            ok::opt dest_item = dest_iter.next();
            ok::opt source_item = source_iter.next();

            if constexpr (!allow_small_destination) {
                if (!dest_item && source_item) {
                    __ok_abort(
                        "Attempt to iterators_copy_assign() from a source "
                        "which is larger than the destination.");
                }
            }

            if (!source_item || !dest_item) [[unlikely]]
                break;

            dest_item.ref_unchecked() = source_item.ref_unchecked();
        }
    }
};

} // namespace detail

inline constexpr detail::iterators_equal_fn_t iterators_equal;
inline constexpr detail::iterators_copy_assign_fn_t<true> iterators_copy_assign;
inline constexpr detail::iterators_copy_assign_fn_t<false>
    iterators_copy_assign_strict;
inline constexpr detail::identity_fn_t identity;
} // namespace ok

#endif
