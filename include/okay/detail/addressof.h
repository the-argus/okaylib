#ifndef __OKAYLIB_DETAIL_OK_ADDRESSOF_H__
#define __OKAYLIB_DETAIL_OK_ADDRESSOF_H__

#include <memory>

namespace ok {
template <typename... args_t>
constexpr decltype(auto) addressof(args_t&&... args)
{
    return std::addressof(std::forward<args_t>(args)...);
}
} // namespace ok

#endif
