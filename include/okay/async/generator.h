#ifndef __OKAYLIB_ASYNC_GENERATOR_H__
#define __OKAYLIB_ASYNC_GENERATOR_H__

#include "okay/detail/coroutine.h"
#include "okay/iterables/iterables.h"
#include "okay/opt.h"

namespace ok {
template <typename T> class generator_t
{
  public:
    // ok::opt does not support rvalue references, also doing this is probably a
    // bad idea
    static_assert(!stdc::is_rvalue_reference_v<T>);

    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

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

    constexpr ok::opt<T> next() OKAYLIB_NOEXCEPT
    {
        if (m_handle.done())
            return {};

        m_handle();

        return m_handle.promise().value.take();
    }

    generator_t(handle_type handle) : m_handle(handle) {}

    ~generator_t()
    {
        if (m_handle) {
            m_handle.destroy();
            m_handle = nullptr;
        }
    }

    generator_t(const generator_t&) = delete;
    generator_t& operator=(const generator_t&) = delete;

    constexpr generator_t(generator_t&& other) noexcept
        : m_handle(other.m_handle)
    {
        other.m_handle = nullptr;
    }

    constexpr generator_t& operator=(generator_t&& other)
    {
        this->~generator_t();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

    // make generator iterable
    struct cursor_t
    {
        using value_type = T;
        constexpr auto next(generator_t& generator) const OKAYLIB_NOEXCEPT
        {
            return generator.next();
        }
    };

    /// Iterating over a generator is a fundamentally modifying operation so we
    /// don't do by-reference iteration, the iterator always has to take
    /// ownership of the generator. This function is syntactic sugar to avoid
    /// having to write stdc::move(generator).iter()
    [[nodiscard]] constexpr auto move_and_iter() &
    {
        return stdc::move(*this).iter();
    }

    [[nodiscard]] constexpr auto iter() &&
    {
        return owning_iterator_t<generator_t, cursor_t>{
            stdc::move(*this),
            cursor_t{},
        };
    }

  private:
    handle_type m_handle;
};

} // namespace ok

#endif
