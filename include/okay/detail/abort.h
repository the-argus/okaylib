#ifndef __OK_ABORT_MACRO_H__
#define __OK_ABORT_MACRO_H__

#ifdef OKAYLIB_TESTING
#include <cstdio>
#include <exception>
namespace reserve {
class _abort_exception : std::exception
{
  public:
    char* what() { return const_cast<char*>("Program failure."); }
};
} // namespace reserve

namespace detail_testing {
void print_stack_trace();
}

#define __ok_abort(msg)                                                \
    {                                                                  \
        ::fprintf(stderr, "Okaylib abort called at %s:%d in %s: %s\n", \
                  __FILE__, __LINE__, __FUNCTION__, msg);              \
        detail_testing::print_stack_trace();                           \
        throw ::reserve::_abort_exception();                           \
    }
#else
#include <cstdlib>
#define __ok_abort(msg) \
    {                   \
        std::abort();   \
    }
#endif

#endif
