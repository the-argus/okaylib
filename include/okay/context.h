#ifndef __OKAYLIB_CONTEXT_H__
#define __OKAYLIB_CONTEXT_H__

#include "okay/allocators/allocator.h"
#include "okay/allocators/arena.h"
#include "okay/allocators/c_allocator.h"

namespace ok {

class context_switch_t;

struct context_switch_options_t
{
    // if any of these fields are null, their values will be taken from the
    // current context, before the switch
    ok::opt<allocator_t&> new_allocator;
    ok::opt<allocator_t&> new_task_allocator;
    ok::opt<const char*> new_error_message;
};

class context_t
{
  public:
    context_t() = delete;
    constexpr context_t(allocator_t& allocator, allocator_t& task_allocator,
                        const char* error_message)
        : m_allocator(ok::addressof(allocator)),
          m_task_allocator(ok::addressof(task_allocator)),
          m_error_message(error_message)
    {
    }

    allocator_t& allocator() const noexcept { return *m_allocator; }

    allocator_t& task_allocator() const noexcept { return *m_task_allocator; }

    friend context_switch_t;

  protected:
    /// This function is called by a context_switch_t when an inner context
    /// gets destroyed, and we want to return to us
    virtual void restore_from(context_t& inner_context) OKAYLIB_NOEXCEPT;

  private:
    allocator_t* m_allocator;
    allocator_t* m_task_allocator;
    const char* m_error_message = nullptr;
};

namespace detail {
inline c_allocator_t __default_global_allocator;
inline arena_t __default_task_allocator{__default_global_allocator};
inline context_t __default_global_context{
    __default_global_allocator,
    __default_task_allocator,
    nullptr,
};

// name of this variable is implementation defined
inline thread_local context_t* __context =
    ok::addressof(__default_global_context);
} // namespace detail

inline context_t& context() noexcept { return *detail::__context; }

class context_switch_t
{
    context_switch_t(context_t ctx) noexcept
        : m_context(ctx), m_previous_context(context())
    {
        detail::__context = ok::addressof(m_context);
    }

    ~context_switch_t() OKAYLIB_NOEXCEPT_FORCE
    {
        __ok_assert(
            ok::addressof(m_context) == ok::addressof(context()),
            "unexpected context found when destroying a context_switch_t");
        m_previous_context.restore_from(m_context);
        detail::__context = ok::addressof(m_previous_context);
    }

  private:
    context_t m_context;
    context_t& m_previous_context;
};

inline void context_t::restore_from(context_t& inner_context) OKAYLIB_NOEXCEPT
{
    // propagate const char* error message upwards- if an inner scope
    // experienced an error, we want to know about it
    const char* inner_msg = detail::__context->m_error_message;
    if (inner_msg) [[unlikely]]
        this->m_error_message = inner_msg;
}
} // namespace ok

#endif
