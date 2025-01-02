#ifndef __OK_ABORT_MACRO_H__
#define __OK_ABORT_MACRO_H__

#ifdef OKAYLIB_TESTING
#include <exception>
namespace reserve {
class _abort_exception : std::exception
{
  public:
    char* what() { return const_cast<char*>("Program failure."); }
};
} // namespace reserve
#define OK_ABORT()                           \
    {                                        \
        throw ::reserve::_abort_exception(); \
    }
#else
#include <cstdlib>
#define OK_ABORT()    \
    {                 \
        std::abort(); \
    }
#endif

#endif
