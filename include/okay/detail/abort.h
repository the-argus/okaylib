#ifndef __OK_ABORT_MACRO_H__
#define __OK_ABORT_MACRO_H__

#ifdef OKAYLIB_TESTING
#include <cstdio>
#include <exception>
namespace detail_testing {
// defined in backward.cpp
void print_stack_trace(void* st);
void* get_stack_trace();
void delete_stack_trace(void* st);
} // namespace detail_testing

namespace reserve {
class _abort_exception : std::exception
{
  public:
    char* what() { return const_cast<char*>("Program failure."); }

    _abort_exception(void* st) : m_st(st), std::exception() {}

    constexpr void cancel_stack_trace_print()
    {
        detail_testing::delete_stack_trace(m_st);
        m_st = nullptr;
    }

    inline ~_abort_exception() override
    {
        if (m_st) {
            detail_testing::print_stack_trace(m_st);
            detail_testing::delete_stack_trace(m_st);
        }
    }

  private:
    void* m_st;
};
} // namespace reserve

#define __ok_abort(msg)                                                       \
    {                                                                         \
        ::fprintf(stderr, "Okaylib abort called at %s:%d in %s: %s\n",        \
                  __FILE__, __LINE__, __FUNCTION__, msg);                     \
        throw ::reserve::_abort_exception(detail_testing::get_stack_trace()); \
    }
#else
#include <cstdlib>
#define __ok_abort(msg) \
    {                   \
        std::abort();   \
    }
#endif

#endif
