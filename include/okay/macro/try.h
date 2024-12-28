#ifndef __OKAYLIB_MACRO_TRY_H__
#define __OKAYLIB_MACRO_TRY_H__

#include "okay/detail/is_lvalue.h"
#include "okay/res.h"

#define TRY(capture, result)                                       \
    static_assert(!decltype(ok::detail::is_lvalue(result))::value, \
                  "Attempting to try an lvalue.");                 \
    decltype(result) _private_result_##capture(result);            \
    if (!_private_result_##capture.okay()) {                       \
        return _private_result_##capture.err();                    \
    }                                                              \
    decltype(result)::type(capture)(                               \
        std::move(_private_result_##capture.release()));

#define TRY_REF(capture, result)                                   \
    static_assert(!decltype(ok::detail::is_lvalue(result))::value, \
                  "Attempting to try an lvalue.");                 \
    decltype(result) _private_result_##capture(result);            \
    if (!_private_result_##capture.okay()) {                       \
        return _private_result_##capture.err();                    \
    }                                                              \
    decltype(result)::type(capture)(                               \
        std::move(_private_result_##capture.release_ref()));

#define TRY_BLOCK(capture, result, code) \
    {                                    \
        TRY(capture, result) { code }    \
    }

#define TRY_REF_BLOCK(capture, result, code) \
    {                                        \
        TRY_REF(capture, result) { code }    \
    }

#endif
