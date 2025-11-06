#ifndef __OKAYLIB_DETAIL_COMPILER_CONFIG_H__
#define __OKAYLIB_DETAIL_COMPILER_CONFIG_H__


// copied straight from gcc libcpp impl
#if defined(__apple_build_version__)
// Given AppleClang XX.Y.Z, _LIBCPP_APPLE_CLANG_VER is XXYZ (e.g. AppleClang 14.0.3 => 1403)
#  define OKAYLIB_COMPILER_CLANG_BASED
#  define OKAYLIB_APPLE_CLANG_VER (__apple_build_version__ / 10000)
#elif defined(__clang__)
#  define OKAYLIB_COMPILER_CLANG_BASED
#  define OKAYLIB_CLANG_VER (__clang_major__ * 100 + __clang_minor__)
#elif defined(__GNUC__)
#  define OKAYLIB_COMPILER_GCC
#  define OKAYLIB_GCC_VER (__GNUC__ * 100 + __GNUC_MINOR__)
#endif

// '__is_identifier' returns '0' if '__x' is a reserved identifier provided by
// the compiler and '1' otherwise.
#ifndef __is_identifier
#define __ok_is_identifier(x) 1
#else
#define __ok_is_identifier(x) __is_identifier(x)
#endif

#define __ok_has_keyword(x) !(__ok_is_identifier(x))

#endif
