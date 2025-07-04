#ifndef __OKAYLIB_DETAIL_CONFIG_H__
#define __OKAYLIB_DETAIL_CONFIG_H__

// by default, use okaylib compat strategy std, where you can seamlessly use
// okaylib alongside the standard library. OKAYLIB_COMPAT_STRATEGY 1 would
// make it borderline impossible to use the standard library because it
// reimplements some of the compiler hardcoded std namespace symbols like
// std::construct_at. OKAYLIB_COMPAT_STRATEGY 0 will not result in as many
// compile errors, but will cause okaylib formatting functions such as
// ok::println to not work on STL containers such as std::string. Additionally,
// it may be slower and resulting in larger binaries due to some types/function
// no longer being constexpr (with this strategy selected, okaylib will not use
// std::construct_at).
#ifndef OKAYLIB_COMPAT_STRATEGY
#define OKAYLIB_COMPAT_STRATEGY 2
#endif

#if OKAYLIB_COMPAT_STRATEGY == 0
#define OKAYLIB_COMPAT_STRATEGY_PURE_CPP
#undef OKAYLIB_COMPAT_STRATEGY_STD
#undef OKAYLIB_COMPAT_STRATEGY_NO_STD
#elif OKAYLIB_COMPAT_STRATEGY == 1
#define OKAYLIB_COMPAT_STRATEGY_NO_STD
#undef OKAYLIB_COMPAT_STRATEGY_STD
#undef OKAYLIB_COMPAT_STRATEGY_PURE_CPP
#elif OKAYLIB_COMPAT_STRATEGY == 2
#define OKAYLIB_COMPAT_STRATEGY_STD
#undef OKAYLIB_COMPAT_STRATEGY_NO_STD
#undef OKAYLIB_COMPAT_STRATEGY_PURE_CPP
#else
#error "unknown value for macro OKAYLIB_COMPAT_STRATEGY, provide integers 0-2"
#endif

#endif
