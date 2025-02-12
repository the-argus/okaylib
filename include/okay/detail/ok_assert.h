#ifndef __OKAYLIB_DETAIL_OK_ASSERT_H__
#define __OKAYLIB_DETAIL_OK_ASSERT_H__

#include "okay/detail/abort.h"
#include "okay/version.h"
#include <cassert>
#include <cstdio>

#ifndef __ok_assert

#define __ok_assert(expr, msg)                                               \
    {                                                                        \
        if (!(expr)) {                                                       \
            ::fprintf(stderr, "Assert " #expr " triggered at %s:%d in %s\n", \
                      __FILE__, __LINE__, __FUNCTION__);                     \
            __ok_abort();                                                    \
        }                                                                    \
    }

// internal assert has a different error message and does not go through
// __ok_abort, meaning that it will still abort the program in testing mode
#define __ok_internal_assert(expr)                                            \
    {                                                                         \
        if (!(expr)) {                                                        \
            ::fprintf(stderr,                                                 \
                      "okaylib v" __ok_version_str                            \
                      " implementor assert " #expr                            \
                      " triggered at %s:%d in function %s, file an issue at " \
                      "github.com/the-argus/okaylib\n",                       \
                      __FILE__, __LINE__, __FUNCTION__);                      \
            std::abort();                                                     \
        }                                                                     \
    }

#endif

#endif
