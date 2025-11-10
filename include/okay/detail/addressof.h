#ifndef __OKAYLIB_DETAIL_OK_ADDRESSOF_H__
#define __OKAYLIB_DETAIL_OK_ADDRESSOF_H__

#include "okay/detail/type_traits.h"
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
#include <utility>
#endif

namespace ok {
template <class T> constexpr T* addressof(T& arg) noexcept
{
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
    return ::std::addressof(arg);
#elif defined(OKAYLIB_COMPAT_STRATEGY_NO_STD)
    return __builtin_addressof(arg);
#elif defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
    // NOTE: cannot be constexpr
    return reinterpret_cast<T*>(
        &const_cast<char&>(reinterpret_cast<char const volatile&>(arg)));
#endif
}
} // namespace ok

#endif
