#pragma once
#include "okay/detail/abort.h"
/// Header to be included in tests and tests only. Must be included first in the
/// file
#ifndef OKAYLIB_TESTING
#error "attempt to compile tests without OKAYLIB_TESTING defined."
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "okay/detail/abort.h"
#include "okay/ranges/algorithm.h"
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

#define REQUIRE_RANGES_EQUAL(range1, range2)              \
    {                                                     \
        auto&& rng1 = range1;                             \
        auto&& rng2 = range2;                             \
        bool ranges_equal = ok::ranges_equal(rng1, rng2); \
        if (!ranges_equal) {                              \
            fmt::println("{} != {}", rng1, rng2);         \
        }                                                 \
        REQUIRE(ranges_equal);                            \
    }
