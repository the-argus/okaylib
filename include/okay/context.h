#ifndef __OKAYLIB_CONTEXT_H__
#define __OKAYLIB_CONTEXT_H__

#include "okay/allocators/allocator.h"
#include "okay/allocators/c_allocator.h"

namespace ok {

class context_switch_t;
const char* context_error_message();
void set_context_error_message(const char*);

class context_t
{
  public:
    context_t() = delete;
    inline constexpr context_t(allocator_t& allocator)
        : m_allocator(allocator)
    {
    }

    inline allocator_t* allocator() const noexcept
    {
        return &m_allocator;
    }

    friend const char* ok::context_error_message();
    friend void ok::set_context_error_message(const char*);
    friend context_switch_t;

  private:
    allocator_t& m_allocator;
    const char* error_message = nullptr;
};

inline c_allocator_t __default_global_allocator;
inline context_t __default_global_context{__default_global_allocator};
static_assert(decltype(__default_global_allocator)::type_features &
                  allocator_t::feature_flags::threadsafe,
              "every part of default context must be threadsafe because all "
              "threads are initialized to use it.");

// name of this variable is implementation defined
inline thread_local context_t* __context = &__default_global_context;

inline context_t& context() noexcept { return *__context; }

class context_switch_t
{
    inline context_switch_t(context_t ctx) noexcept
        : m_context(ctx), m_previous_context(context())
    {
        __context = &m_context;
    }

    inline ~context_switch_t() noexcept
    {
        // check if somebody set and forgot to restore the context
        assert(&m_context == &context());
        const char* msg = __context->error_message;
        // only propagate error message upwards if one exists, dont overwrite
        // parent with null
        if (msg) [[unlikely]]
            m_previous_context.error_message = msg;
        __context = &m_previous_context;
    }

  private:
    context_t m_context;
    context_t& m_previous_context;
};

inline const char* context_error_message() { return context().error_message; }
inline void set_context_error_message(const char* error)
{
    context().error_message = error;
}

} // namespace ok

#endif
