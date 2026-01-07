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

struct copy_assign_or_set_fn_t
{
    template <typename lhs_t, typename rhs_t>
    constexpr auto operator()(lhs_t& lhs, rhs_t&& rhs) const noexcept
        requires(!settable_value_type_c<lhs_t, decltype(rhs)> &&
                 detail::assignable_reference_c<decltype(lhs), decltype(rhs)>)
    {
        return lhs = std::forward<rhs_t>(rhs);
    }

    template <typename lhs_t, typename rhs_t>
    constexpr auto operator()(lhs_t lhs, rhs_t&& rhs) const noexcept
        requires(!stdc::is_reference_c<lhs_t> &&
                 settable_value_type_c<lhs_t, decltype(rhs)>)
    {
        return std::forward<lhs_t>(lhs).value_type_set(
            std::forward<rhs_t>(rhs));
    }
};

constexpr copy_assign_or_set_fn_t copy_assign_or_set{};

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
    template <typename dest_iterable_t, typename source_iterable_t>
    constexpr void operator()(dest_iterable_t&& dest,
                              source_iterable_t&& source) const OKAYLIB_NOEXCEPT
        requires iterable_c<decltype(dest)> && iterable_c<decltype(source)> &&
                 settable_iterator_c<iterator_for<decltype(dest)>,
                                     value_type_for<decltype(source)>>
    {
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

            detail::copy_assign_or_set(dest_item.ref_unchecked(),
                                       source_item.ref_unchecked());
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
