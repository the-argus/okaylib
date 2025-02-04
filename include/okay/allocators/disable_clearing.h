#ifndef __OKAYLIB_ALLOCATOR_DISABLE_CLEARING_H__
#define __OKAYLIB_ALLOCATOR_DISABLE_CLEARING_H__

#include "okay/allocators/allocator_interface.h"

namespace ok {
template <typename T> class disable_clearing_t : public allocator_interface_t
{
  public:
    static_assert(std::is_base_of_v<allocator_interface_t, T>,
                  "disable_clearing_t given a type which does not inherit from "
                  "allocator_interface_t");

    disable_clearing_t() = delete;

    inline constexpr disable_clearing_t(T& allocator)
        : m_backing_allocator(allocator)
    {
    }

    inline void clear() noexcept {}

    [[nodiscard]] inline result_t<bytes_t>
    allocate_bytes(size_t nbytes, size_t alignment = default_align) final
    {
        return m_backing_allocator.allocate_bytes(nbytes, alignment);
    }

    [[nodiscard]] inline reallocation_result_t reallocate_bytes(
        const allocator_interface_t::reallocate_options_t& options) final
    {
        return m_backing_allocator.reallocate_bytes(options);
    }

    [[nodiscard]] inline feature_flags features() const noexcept final
    {
        return m_backing_allocator.features();
    }

    inline void deallocate_bytes(void* p, size_t nbytes,
                                 size_t alignment = default_align) final
    {
        m_backing_allocator.deallocate_bytes(p, nbytes, alignment);
    }

    [[nodiscard]] inline status_t<error>
    register_destruction_callback(void* user_data,
                                  destruction_callback_t callback) final
    {
        return m_backing_allocator.register_destruction_callback(user_data,
                                                                 callback);
    }

  private:
    T& m_backing_allocator;
};
} // namespace ok

#endif
