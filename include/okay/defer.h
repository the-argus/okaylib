#ifndef __OKAYLIB_DEFER_H__
#define __OKAYLIB_DEFER_H__

#include <memory> // std::addressof
#include <type_traits>

namespace ok {
template <typename callable_t> class defer_t
{
    callable_t* statement;
    static_assert(std::is_invocable_r_v<void, callable_t>,
                  "Callable is not invocable with no arguments, and/or it does "
                  "not return void.");

  public:
    explicit defer_t(callable_t&& f) : statement(std::addressof(f)) {}

    // you cannot move or copy or really mess with a defer at all
    defer_t& operator=(const defer_t&) = delete;
    defer_t(const defer_t&) = delete;

    defer_t& operator=(defer_t&&) = delete;
    defer_t(defer_t&&) = delete;

    inline constexpr void cancel() { statement = nullptr; }

    ~defer_t()
    {
        if (statement)
            (*statement)();
    }
};
} // namespace ok
#endif
