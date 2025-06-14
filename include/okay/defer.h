#ifndef __OKAYLIB_DEFER_H__
#define __OKAYLIB_DEFER_H__

#include "okay/detail/noexcept.h"
#include <type_traits>
#include <utility>

namespace ok {
/// A type which stores a lambda and calls it when it is destroyed. It can store
/// the callable by reference- but make sure you know what you're doing before
/// enabling that. It's intended for use by the defer macros
/// (see "okay/macros/defer.h")
template <typename callable_t, bool store_by_reference = false> class defer
{
    std::conditional_t<store_by_reference, const callable_t&, callable_t>
        statement;
    bool should_call = true;

    static_assert(std::is_invocable_v<const callable_t&>,
                  "Cannot call the given lambda with no arguments.");
    static_assert(std::is_void_v<decltype(std::declval<const callable_t&>()())>,
                  "Cannot return a value from a deferred block.");

  public:
    constexpr defer(callable_t&& f) OKAYLIB_NOEXCEPT
        : statement(std::forward<callable_t>(f))
    {
    }

    // you cannot move or copy or really mess with a defer at all
    defer& operator=(const defer&) = delete;
    defer(const defer&) = delete;
    defer& operator=(defer&&) = delete;
    defer(defer&&) = delete;

    constexpr void cancel() { should_call = false; }

    constexpr ~defer()
    {
        if (should_call)
            statement();
    }
};
} // namespace ok
#endif
