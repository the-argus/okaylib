#ifndef __OKAYLIB_RANGES_FOR_EACH_H__
#define __OKAYLIB_RANGES_FOR_EACH_H__

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
        static_assert(range_c<T>,
                      "Cannot for_each given type- it is not a range.");
        static_assert(producing_range_c<T>,
                      "Cannot for_each given type- it is not an input range.");

        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
                callable(ok::range_get_best(range, cursor));
        }
    }
};
} // namespace detail

constexpr detail::range_adaptor_t<detail::for_each_fn_t> for_each;

} // namespace ok

#endif
