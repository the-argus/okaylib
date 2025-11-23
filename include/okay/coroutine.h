#ifndef __OKAYLIB_COROUTINE_H__
#define __OKAYLIB_COROUTINE_H__

#if defined(OKAYLIB_COMPAT_STRATEGY_PURE_CPP)
// TODO: probably fall back to STRATEGY_STD in this case
#warning \
    "Attempt to include coroutine with OKAYLIB_COMPAT_STRATEGY_PURE_CPP, this is not supported"
#endif

#include "okay/detail/type_traits.h"
#include "okay/math/ordering.h"

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
#include <coroutine>
#endif

namespace ok {
// always the same regardless of compat strategy
template <typename result_t>
concept coroutine_c = requires { typename result_t::promise_type; };

namespace detail {
template <typename T> struct coroutine_promise_type_impl
{
    using type = void;
};

template <coroutine_c T> struct coroutine_promise_type_impl<T>
{
    using type = typename T::promise_type;
};
} // namespace detail

template <typename result_t, typename... args_t>
struct coroutine_promise_type
    : public detail::coroutine_promise_type_impl<result_t>
{};

template <typename result_t, typename... args_t>
using coroutine_promise_type_t =
    typename coroutine_promise_type<result_t, args_t...>::type;

template <typename promise_t = void> struct coroutine_handle;

template <> struct coroutine_handle<void>
{
  public:
    constexpr coroutine_handle<void>() noexcept : m_handle({}) {}

    constexpr coroutine_handle<void>(ok::nullptr_t null) noexcept
        : m_handle(null)
    {
    }

    coroutine_handle<void>& operator=(ok::nullptr_t) noexcept
    {
        m_handle = nullptr;
        return *this;
    }

  public:
    constexpr void* address() const noexcept
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return m_handle.address();
#else
        return m_handle;
#endif
    }

    constexpr static coroutine_handle<void> from_address(void* address) noexcept
    {
        coroutine_handle<void> self;
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        self.m_handle = std::coroutine_handle<void>::from_address(address);
#else
        self.m_handle = address;
#endif
        return self;
    }

  public:
    constexpr explicit operator bool() const noexcept { return bool(m_handle); }

    bool done() const noexcept
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return m_handle.done();
#else
        return __builtin_coro_done(m_handle);
#endif
    }

    void operator()() const { resume(); }

    void resume() const
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        m_handle.resume();
#else
        __builtin_coro_resume(m_handle);
#endif
    }

    void destroy() const
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        m_handle.destroy();
#else
        __builtin_coro_destroy(m_handle);
#endif
    }

  protected:
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
    std::coroutine_handle<void> m_handle;
#else
    void* m_handle;
#endif
};

constexpr bool operator==(ok::coroutine_handle<> lhs,
                          ok::coroutine_handle<> rhs) noexcept
{
    return lhs.address() == rhs.address();
}

constexpr ordering operator<=>(ok::coroutine_handle<> lhs,
                               ok::coroutine_handle<> rhs) noexcept
{
    return ok::cmp(lhs.address(), rhs.address());
}

template <typename promise_t> struct coroutine_handle
{
    constexpr coroutine_handle<promise_t>() noexcept {}

    constexpr coroutine_handle<promise_t>(nullptr_t) noexcept {}

    static coroutine_handle<promise_t> from_promise(promise_t& promise)
    {
        coroutine_handle<promise_t> self;
        self.m_handle =
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
            std::coroutine_handle<promise_t>::from_promise(promise);
#else
            __builtin_coro_promise((char*)&promise, __alignof(promise_t), true);
#endif
        return self;
    }

    coroutine_handle<promise_t>& operator=(nullptr_t) noexcept
    {
        m_handle = nullptr;
        return *this;
    }

    constexpr void* address() const noexcept
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return m_handle.address();
#else
        return m_handle;
#endif
    }

    constexpr static coroutine_handle<promise_t>
    from_address(void* address) noexcept
    {
        coroutine_handle<promise_t> self;
        self.m_handle =
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
            std::coroutine_handle<promise_t>::from_address(address);
#else
            address;
#endif
        return self;
    }

    constexpr operator ok::coroutine_handle<>() const noexcept
    {
        return ok::coroutine_handle<>::from_address(address());
    }

    constexpr explicit operator bool() const noexcept { return bool(m_handle); }

    bool done() const noexcept
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return m_handle.done();
#else
        return __builtin_coro_done(m_handle);
#endif
    }

    void operator()() const { resume(); }

    void resume() const
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        m_handle.resume();
#else
        __builtin_coro_resume(m_handle);
#endif
    }

    void destroy() const
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        m_handle.destroy();
#else
        __builtin_coro_destroy(m_handle);
#endif
    }

    promise_t& promise() const
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return m_handle.promise();
#else
        void* t = __builtin_coro_promise(m_handle, __alignof(promise_t), false);
        return *static_cast<promise_t*>(t);
#endif
    }

  private:
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
    std::coroutine_handle<promise_t> m_handle;
#else
    void* m_handle = nullptr;
#endif
};

#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
using noop_coroutine_promise = std::noop_coroutine_promise;
#else
struct noop_coroutine_promise
{};
#endif

template <> struct coroutine_handle<noop_coroutine_promise>
{
    constexpr operator coroutine_handle<>() const noexcept
    {
        return coroutine_handle<>::from_address(address());
    }

    constexpr explicit operator bool() const noexcept { return true; }

    constexpr bool done() const noexcept { return false; }

    void operator()() const noexcept {}

    void resume() const noexcept {}

    void destroy() const noexcept {}

    noop_coroutine_promise& promise() const noexcept
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return std::noop_coroutine().promise();
#else
        return static_frame_instance.noop_promise;
#endif
    }

    // NOTE: subtle difference between us and std is that this function is not
    // constexpr. shouldn't really ever matter since it doesn't make sense to
    // call it from a constexpr context anyways, it's getting the address of a
    // static variable.
    void* address() const noexcept
    {
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        return std::noop_coroutine().address();
#else
        return m_handle;
#endif
    }

  private:
    friend ok::coroutine_handle<noop_coroutine_promise>
    noop_coroutine() noexcept;

#if !defined(OKAYLIB_COMPAT_STRATEGY_STD)

    struct frame_t
    {
        static void dummy_resume_destroy() {}

        void (*resume_ptr)() = dummy_resume_destroy;
        void (*destroy_ptr)() = dummy_resume_destroy;
        noop_coroutine_promise noop_promise;
    };

    static frame_t static_frame_instance;
#endif

    explicit coroutine_handle() noexcept = default;

    void* m_handle =
#if defined(OKAYLIB_COMPAT_STRATEGY_STD)
        // unused in this case, we just create a noop_coroutine every time a
        // member function is called
        nullptr;
#else
        &static_frame_instance;
#endif
};

using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

#if !defined(OKAYLIB_COMPAT_STRATEGY_STD)
inline noop_coroutine_handle::frame_t
    noop_coroutine_handle::static_frame_instance{};
#endif

inline noop_coroutine_handle noop_coroutine() noexcept
{
    return noop_coroutine_handle();
}

struct suspend_always
{
    constexpr bool await_ready() const noexcept { return false; }

    constexpr void await_suspend(coroutine_handle<>) const noexcept {}

    constexpr void await_resume() const noexcept {}
};

struct suspend_never
{
    constexpr bool await_ready() const noexcept { return true; }

    constexpr void await_suspend(coroutine_handle<>) const noexcept {}

    constexpr void await_resume() const noexcept {}
};
} // namespace ok

#endif
