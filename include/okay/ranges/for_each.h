#ifndef __OKAYLIB_RANGES_FOR_EACH_H__
#define __OKAYLIB_RANGES_FOR_EACH_H__

#include "okay/detail/traits/special_member_traits.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {

struct for_each_fn_t
{
    template <typename range_t, typename callable_t>
    constexpr decltype(auto)
    operator()(range_t&& range, callable_t&& callable) const OKAYLIB_NOEXCEPT
    {
        using T = remove_cvref_t<range_t>;
        static_assert(is_range_v<T>,
                      "Cannot for_each given type- it is not a range.");
        static_assert(is_producing_range_v<T>,
                      "Cannot for_each given type- it is not an input range.");

        // default to using get_ref, only don't do this if the type isnt
        // copyable or something and only iter_copyout can handle it
        constexpr bool use_get_ref =
            (range_can_get_ref_const_v<range_t> ||
             range_can_get_ref_v<range_t>) &&
            is_std_invocable_r_v<callable_t, void, value_type_for<T>&>;
        constexpr bool use_copyout =
            !use_get_ref &&
            is_std_invocable_r_v<callable_t, void, value_type_for<T>>;

        static_assert(use_get_ref || use_copyout,
                      "Given for_each function and given range do not match "
                      "up: there is no way to call the given function with the "
                      "result of `ok::iter_get_ref(const range)` or "
                      "`ok::iter_copyout(const range)`. This may also be "
                      "caused by a lambda being marked \"mutable\".");

        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            if constexpr (use_get_ref) {
                callable(ok::iter_get_ref(range, cursor));
            } else if (use_copyout) {
                callable(ok::iter_copyout(range, cursor));
            }
        }
    }
};
} // namespace detail

constexpr detail::range_adaptor_t<detail::for_each_fn_t> for_each;

} // namespace ok

#endif
