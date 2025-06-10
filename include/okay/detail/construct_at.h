#ifndef __OKAYLIB_DETAIL_OK_CONSTRUCT_AT_H__
#define __OKAYLIB_DETAIL_OK_CONSTRUCT_AT_H__

#include <memory>

namespace ok {
template <class T, typename... args_t>
constexpr T* construct_at(T* location, args_t&&... args)
{
    // c++20 :)
    return std::construct_at(location, args...);
}
} // namespace ok

#endif
