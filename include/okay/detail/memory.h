#ifndef __OKAYLIB_DETAIL_MEMORY_H__
#define __OKAYLIB_DETAIL_MEMORY_H__

#include "okay/detail/config.h"

#if defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
#define __ok_constructing_constexpr
#else
#define __ok_constructing_constexpr constexpr
#endif

// if we're not using PURE_CPP, then we need to bring std::construct_at into
// scope, either by including <memory> or implementing it ourselves
#if defined(OKAYLIB_COMPAT_STRATEGY_NO_STD)

#include "okay/detail/utility.h"

// implement std::construct_at ourselves
namespace std {
template <typename _Tp, typename... _Args>
constexpr void construct_at(_Tp* __location, _Args&&... __args) noexcept(
    noexcept(::new((void*)0) _Tp(stdc::declval<_Args>()...)))
{
    ::new ((void*)__location) _Tp(stdc::forward<_Args>(__args)...);
}
} // namespace std

#elif defined(OKAYLIB_COMPAT_STRATEGY_STD)
// full stdlib compatibility, just include it to avoid duplicate symbols
#include <memory>
#endif

// actually implement ok::stdc::construct_at
#if !defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
// implement stdc::construct_at by just calling std::construct_at
namespace ok::stdc {
template <class T, typename... args_t>
constexpr T* construct_at(T* location, args_t&&... args)
{
    // c++20 :)
    return std::construct_at(location, std::forward<args_t>(args)...);
}
}
#else

// implement stdc::construct_at using (not constexpr) placement new
#include "okay/detail/utility.h"
namespace ok::stdc {
template <class T, typename... args_t>
constexpr T* construct_at(T* location, args_t&&... args)
{
    return new (location) T(stdc::forward<args_t>(args)...);
}
} // namespace ok::stdc

#endif
#endif
