#ifndef __OKAYLIB_DETAIL_GET_BEST_H__
#define __OKAYLIB_DETAIL_GET_BEST_H__

#include "okay/ranges/ranges.h"

namespace ok::detail {
// behavior of foreach loop: try mutable reference, then try const reference,
// then try get()
template <producing_range_c qualified_range_t>
constexpr decltype(auto) get_best(qualified_range_t& i,
                                  const cursor_type_for<qualified_range_t>& c)
{
    using range_t = std::remove_cv_t<qualified_range_t>;

    if constexpr ((detail::range_can_get_ref_c<range_t> &&
                   !is_const_c<qualified_range_t>) ||
                  detail::range_can_get_ref_const_c<range_t>) {
        return ok::iter_get_ref(i, c);
    } else {
        return ok::iter_copyout(i, c);
    }
}

} // namespace ok::detail

#endif
