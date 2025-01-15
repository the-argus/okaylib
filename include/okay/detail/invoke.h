#ifndef __OKAYLIB_DETAIL_INVOKE_H__
#define __OKAYLIB_DETAIL_INVOKE_H__

// TODO: remove this dependency, gcc STL implementation seems to pull in <array>
// and <vector> along with it...
// use possible implementation from
// https://en.cppreference.com/w/cpp/utility/functional/invoke ?
#include <functional>

namespace ok {
template <typename... args_t> constexpr decltype(auto) invoke(args_t&&... args)
{
    return std::invoke(std::forward<args_t&&>(args)...);
}
} // namespace ok

#endif
