#ifndef __OKAYLIB_ASYNC_GENERATOR_H__
#define __OKAYLIB_ASYNC_GENERATOR_H__

#include "okay/coroutine.h"
#include "okay/opt.h"

namespace ok {
template <typename T> class generator_t
{
  public:
    struct promise_type;
    using handle_type = ok::coroutine_handle<promise_type>;

    struct promise_type
    {
        ok::opt<T> value;

        generator_t get_return_object() noexcept
        {
            return generator_t(handle_type::from_promise(*this));
        }

        ok::suspend_always initial_suspend() const noexcept { return {}; }
        ok::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() { value.reset(); }
        void unhandled_exception()
        {
            __ok_abort("unhandled exception in coroutine");
        }

        template <typename other_t>
            requires stdc::is_convertible_v<other_t, T>
        ok::suspend_always yield_value(other_t&& v)
        {
            value.emplace(stdc::forward<other_t>(v));
            return {};
        }
    };

    constexpr ok::opt<T> next()
    {
        if (m_handle.done())
            return {};

        m_handle();

        return m_handle.promise().value.take();
    }

    generator_t(handle_type handle) : m_handle(handle) {}

    ~generator_t() { m_handle.destroy(); }

    generator_t(const generator_t&) = delete;
    generator_t& operator=(const generator_t&) = delete;
    generator_t(generator_t&&) = delete;
    generator_t& operator=(generator_t&&) = delete;

  private:
    handle_type m_handle;
};

} // namespace ok

#endif
