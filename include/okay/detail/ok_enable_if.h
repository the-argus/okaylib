#ifndef __OKAYLIB_DETAIL_OK_ENABLE_IF_H__
#define __OKAYLIB_DETAIL_OK_ENABLE_IF_H__

#include <type_traits>

// always constexpr
#define __ok_enable_if(type_T, cond, type_to_return)                \
    template <typename T = type_T>                                  \
    constexpr std::enable_if_t<std::is_same_v<type_T, T> && (cond), \
                               type_to_return>

#define __ok_enable_if_static(type_T, cond, type_to_return)                \
    template <typename T = type_T>                                         \
    constexpr static std::enable_if_t<std::is_same_v<type_T, T> && (cond), \
                                      type_to_return>

#define __ok_enable_if_friend(type_T, cond, type_to_return)                \
    template <typename T = type_T>                                         \
    constexpr friend std::enable_if_t<std::is_same_v<type_T, T> && (cond), \
                                      type_to_return>

#endif
