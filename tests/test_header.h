#pragma once
#include "okay/detail/abort.h"
/// Header to be included in tests and tests only. Must be included first in the
/// file
#ifndef OKAYLIB_TESTING
#error attempt to compile tests without OKAYLIB_TESTING defined.
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "okay/detail/abort.h"
#include <doctest.h>

#define REQUIREABORTS(operation)                         \
    {                                                    \
        bool status = false;                             \
        try {                                            \
            operation;                                   \
        } catch (::reserve::_abort_exception& e) { \
            status = true;                               \
            e.cancel_stack_trace_print();                \
        }                                                \
        REQUIRE(status);                                 \
    }
