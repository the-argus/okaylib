#ifndef __OKAYLIB_DETAIL_UNIQUE_ID_H__
#define __OKAYLIB_DETAIL_UNIQUE_ID_H__

// __OKAYLIB_UNIQUE_ID will try to expand to a unique identifier per
// instantiation and on unsupported compilers it will just use __LINE__.
// This is *probably* fine, as macros that use it are things that most
// formatters would not place on a single line. To be certain your code will
// compile with all compilers, define OKAYLIB_USE_STANDARDIZED_UNIQUE_ID.

#if !defined(OKAYLIB_USE_STANDARDIZED_UNIQUE_ID) && defined(__COUNTER__) && \
    (__COUNTER__ + 1 == __COUNTER__ + 0)
#define __OKAYLIB_UNIQUE_ID __COUNTER__
#else
#define __OKAYLIB_UNIQUE_ID __LINE__
#endif

#endif
