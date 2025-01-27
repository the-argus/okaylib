#ifndef __OKAYLIB_DETAIL_OK_ASSERT_H__
#define __OKAYLIB_DETAIL_OK_ASSERT_H__

#include <cassert>

#ifdef OKAYLIB_TESTING
#include "okay/detail/abort.h"
#endif

#ifndef __ok_assert

#ifdef OKAYLIB_TESTING
#define __ok_assert(expr) \
    {                     \
        if (!(expr)) {    \
            __ok_abort()  \
        }                 \
    }
#else
#define __ok_assert(expr) assert(expr)
#endif

#endif

#endif
