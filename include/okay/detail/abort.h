#ifndef __OK_ABORT_MACRO_H__
#define __OK_ABORT_MACRO_H__

// NOTE: testing doesnt really work without COMPAT_STRATEGY_STD, because theres
// no <exception>
#if defined(OKAYLIB_TESTING)
// #if !defined(OKAYLIB_COMPAT_STRATEGY_STD)
// #error "Attempt to compile in testing mode, but the compat strategy does not
// allow inclusion of <exception>" #endif
#include <cstdio>
#include <exception>
#if defined(OKAYLIB_TESTING_BACKTRACE)
#include "okay/platform/atomic.h"
#include <cstdint>
namespace detail_testing {
// defined in backward.cpp
void print_stack_trace(void* st);
void* get_stack_trace();
void delete_stack_trace(void* st);

struct owned_stack_trace_t
{
    struct payload_t
    {
        ok::atomic_t<uint64_t> refcount;
        void* st;
    };

    payload_t* payload;

    constexpr owned_stack_trace_t() noexcept : payload(new payload_t{})
    {
        payload->refcount.store(1);

#ifndef OKAYLIB_TESTING_BACKTRACE_DISABLE_FOR_RES_AND_STATUS
        if (!ok::stdc::is_constant_evaluated()) {
            payload->st = get_stack_trace();
        }
#endif
    }

  private:
    constexpr void destroy() noexcept
    {
        if (payload)
            if (payload->refcount.fetch_sub(1) == 1) {
                if constexpr (!ok::stdc::is_constant_evaluated()) {
                    delete_stack_trace(payload->st);
                }
                delete payload;
            }
    }

  public:
    void print() const noexcept
    {
        if (payload && payload->st)
            print_stack_trace(payload->st);
    }

    constexpr owned_stack_trace_t(owned_stack_trace_t&& other) noexcept
        : payload(other.payload)
    {
        other.payload = nullptr;
    }

    constexpr owned_stack_trace_t&
    operator=(owned_stack_trace_t&& other) noexcept
    {
        if (&other == this)
            return *this;
        destroy();
        payload = other.payload;
        other.payload = nullptr;
        return *this;
    }

    constexpr owned_stack_trace_t(const owned_stack_trace_t& other) noexcept
        : payload(other.payload)
    {
        payload->refcount.fetch_add(1);
    }

    constexpr owned_stack_trace_t&
    operator=(const owned_stack_trace_t& other) noexcept
    {
        if (&other == this)
            return *this;
        payload = other.payload;
        payload->refcount.fetch_add(1);
        return *this;
    }

    constexpr ~owned_stack_trace_t() { destroy(); }
};

} // namespace detail_testing
#endif

namespace reserve {
class _abort_exception : std::exception
{
  public:
    char* what() { return const_cast<char*>("Program failure."); }

    _abort_exception(void* st) : m_st(st), std::exception() {}

    constexpr void cancel_stack_trace_print()
    {
#if defined(OKAYLIB_TESTING_BACKTRACE)
        detail_testing::delete_stack_trace(m_st);
#endif
        m_st = nullptr;
    }

    inline ~_abort_exception() override
    {
#if defined(OKAYLIB_TESTING_BACKTRACE)
        if (m_st) {
            detail_testing::print_stack_trace(m_st);
            detail_testing::delete_stack_trace(m_st);
        }
#endif
    }

  private:
    void* m_st;
};
} // namespace reserve

#if defined(OKAYLIB_TESTING_BACKTRACE)
#define OKAYLIB_REQUIRE_RES_WITH_BACKTRACE(arg)   \
    ([&]() -> decltype(auto) {                    \
        if (!ok::is_success(arg)) {               \
            arg.stacktrace.print();               \
            REQUIRE(ok::is_success(arg));         \
        }                                         \
        if constexpr (requires { arg.unwrap(); }) \
            return arg.unwrap();                  \
    }())
#else
#define OKAYLIB_REQUIRE_RES_WITH_BACKTRACE(arg)   \
    ([&]() -> decltype(auto) {                    \
        REQUIRE(ok::is_success(arg));             \
        if constexpr (requires { arg.unwrap(); }) \
            return arg.unwrap();                  \
    }())
#endif

#if defined(OKAYLIB_TESTING_BACKTRACE)
#define __ok_abort(msg)                                                       \
    {                                                                         \
        ::fprintf(stderr, "Okaylib abort called at %s:%d in %s: %s\n",        \
                  __FILE__, __LINE__, __FUNCTION__, msg);                     \
        throw ::reserve::_abort_exception(detail_testing::get_stack_trace()); \
    }
#else
#define __ok_abort(msg)                                                \
    {                                                                  \
        ::fprintf(stderr, "Okaylib abort called at %s:%d in %s: %s\n", \
                  __FILE__, __LINE__, __FUNCTION__, msg);              \
        throw ::reserve::_abort_exception(nullptr);                    \
    }
#endif
#else
#include <cstdlib>
#define __ok_abort(msg) \
    {                   \
        ::abort();      \
    }
#endif

#endif
