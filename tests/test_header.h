#pragma once
#include "okay/detail/abort.h"
/// Header to be included in tests and tests only. Must be included first in the
/// file
#ifndef OKAYLIB_TESTING
#error "attempt to compile tests without OKAYLIB_TESTING defined."
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "okay/detail/abort.h"
#include "okay/iterables/algorithm.h"
#include <doctest.h>

#define REQUIREABORTS(operation)                    \
    {                                               \
        bool status = false;                        \
        try {                                       \
            operation;                              \
        } catch (::reserve::_abort_exception & e) { \
            status = true;                          \
            e.cancel_stack_trace_print();           \
        }                                           \
        REQUIRE(status);                            \
    }

#define REQUIRE_RANGES_EQUAL(iter1, iter2)                     \
    {                                                          \
        bool ranges_equal = ok::iterators_equal(iter1, iter2); \
        REQUIRE(ranges_equal);                                 \
    }
