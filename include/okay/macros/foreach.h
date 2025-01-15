#ifndef __OKAYLIB_MACROS_FOREACH_H__
#define __OKAYLIB_MACROS_FOREACH_H__

// taken from a comment on
// https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
// in turn taken from https://www.chiark.greenend.org.uk/~sgtatham/mp/

#include "okay/iterable/iterable.h"
#include <type_traits>

namespace ok::detail {
// behavior of foreach loop: try mutable reference, then try const reference,
// then try get()
template <typename qualified_iterable_t>
constexpr decltype(auto)
get_best(qualified_iterable_t& i,
         const cursor_type_for<qualified_iterable_t>& c)
{
    static_assert(is_iterable_v<qualified_iterable_t> &&
                      is_input_iterable_v<qualified_iterable_t>,
                  "Cannot run ok_foreach loop on the given type.");
    using iterable_t = std::remove_cv_t<qualified_iterable_t>;

    if constexpr ((detail::iterable_has_get_ref_v<iterable_t> &&
                   !std::is_const_v<qualified_iterable_t>) ||
                  (detail::iterable_has_get_ref_const_v<iterable_t> &&
                   std::is_const_v<qualified_iterable_t>)) {
        return ok::iter_get_ref(i, c);
    } else {
        return ok::iter_copyout(i, c);
    }
}

} // namespace ok::detail

#define __OK_LN(l, x) x##l // creates unique labels
#define __OK_L(x, y) __OK_LN(x, y)
#define ok_foreach(decl, _iterable)                                           \
    auto&& __OK_L(__LINE__, iterable) = _iterable;                            \
    for (bool _run = true; _run; _run = false)                                \
        for (auto cursor = ok::begin(__OK_L(__LINE__, iterable));             \
             ok::is_inbounds(__OK_L(__LINE__, iterable), cursor,              \
                             ok::prefer_after_bounds_check_t{});              \
             ++cursor)                                                        \
            if (1) {                                                          \
                _run = true;                                                  \
                goto __OK_L(__LINE__, body);                                  \
                __OK_L(__LINE__, cont) : _run = true;                         \
                continue;                                                     \
                __OK_L(__LINE__, finish) : break;                             \
            } else                                                            \
                while (1)                                                     \
                    if (1) {                                                  \
                        if (!_run)                                            \
                            goto __OK_L(                                      \
                                __LINE__,                                     \
                                cont); /* we reach here if the block          \
                                          terminated normally/via continue */ \
                        goto __OK_L(__LINE__,                                 \
                                    finish); /* we reach here if the block    \
                                                terminated by break */        \
                    } else                                                    \
                        __OK_L(__LINE__, body)                                \
                            : for (decl = ok::detail::get_best(               \
                                       __OK_L(__LINE__, iterable), cursor);   \
                                   _run; _run = false) /* block following the \
                                                          expanded macro */

// macro specifically to prevent the comma in a pair from being evaluated as
// two separate function arguments when passed to ok_foreach.
#define ok_pair(first, second) auto [first, second]

#define ok_decompose(...) auto [__VA_ARGS__]

#endif
