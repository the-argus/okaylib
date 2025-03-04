#ifndef __OKAYLIB_DEFER_H__
#define __OKAYLIB_DEFER_H__

#include "okay/detail/addressof.h"
#include <type_traits>

namespace ok {
template <typename callable_t> class maydefer
{
    callable_t* statement;
    static_assert(std::is_invocable_r_v<void, callable_t>,
                  "Callable is not invocable with no arguments, and/or it does "
                  "not return void.");

  public:
    explicit maydefer(callable_t&& f) : statement(ok::addressof(f)) {}

    // you cannot move or copy or really mess with a defer at all
    maydefer& operator=(const maydefer&) = delete;
    maydefer(const maydefer&) = delete;

    maydefer& operator=(maydefer&&) = delete;
    maydefer(maydefer&&) = delete;

    inline constexpr void cancel() { statement = nullptr; }

    inline ~maydefer()
    {
        if (statement)
            (*statement)();
    }
};

template <typename callable_t> class defer
{
    callable_t&& statement;
    static_assert(std::is_invocable_r_v<void, callable_t>,
                  "Callable is not invocable with no arguments, and/or it does "
                  "not return void.");

  public:
    explicit defer(callable_t&& f) : statement(std::forward<callable_t>(f)) {}

    // you cannot move or copy or really mess with a defer at all
    defer& operator=(const defer&) = delete;
    defer(const defer&) = delete;

    defer& operator=(defer&&) = delete;
    defer(defer&&) = delete;

    inline ~defer() { statement(); }
};

} // namespace ok
#endif
