#ifndef __OKAYLIB_DETAIL_GET_BEST_H__
#define __OKAYLIB_DETAIL_GET_BEST_H__

#include "okay/ranges/ranges.h"

namespace ok::detail {
// behavior of foreach loop: try mutable reference, then try const reference,
// then try get()
template <typename qualified_range_t>
constexpr decltype(auto) get_best(qualified_range_t& i,
                                  const cursor_type_for<qualified_range_t>& c)
{
    constexpr range_def_for<qualified_range_t> asserts;
    static_assert(
        is_producing_range_v<qualified_range_t>,
        "Cannot get anything from given type- it is not an input range.");
    using range_t = std::remove_cv_t<qualified_range_t>;

    if constexpr ((detail::range_can_get_ref_v<range_t> &&
                   !std::is_const_v<qualified_range_t>) ||
                  detail::range_can_get_ref_const_v<range_t>) {
        return ok::iter_get_ref(i, c);
    } else {
        return ok::iter_copyout(i, c);
    }
}

} // namespace ok::detail

#endif
