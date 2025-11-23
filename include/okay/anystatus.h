#ifndef __OKAYLIB_ANYSTATUS_H__
#define __OKAYLIB_ANYSTATUS_H__

#include "okay/allocators/allocator.h"
#include "okay/detail/noexcept.h"
#include "okay/error.h"
#include "okay/reflection/typehash.h"
#include <cstdint>

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {

class abstract_status_t
{
  public:
    [[nodiscard]] virtual bool is_success() const noexcept = 0;

    /// Ask the object to cast itself to another type, returning it as a void*.
    /// If the object cannot cast to the given type, then this function should
    /// return nullptr.
    [[nodiscard]] virtual void* try_cast_to(uint64_t typehash) noexcept = 0;
    // it is assumed that an abstract status is dynamically allocated in some
    // way but if not, this function may return null to indicate that owning
    // pointers don't need to free the pointed-to memory. Normally it is not
    // the job of an object to know its own allocator, but otherwise the pointer
    // would be carried by the owning wrapper type, so this is done in an effort
    // to reduce the size of the res<..., ...> on the stack.
    //
    // NOTE: when freeing the object, the pointer to the abstract_status_t will
    // be passed.
    [[nodiscard]] virtual opt<ok::allocator_t&> allocator() noexcept = 0;

    virtual ~abstract_status_t() = default;
};

/// An anyerr is a (somewhat poorly named) type which is just a u64
/// internally. It can store any arbitrary enum values *as long as their numeric
/// representation is not larger than 4 bytes. After type erasure, the enum can
/// still be casted back to the original type, with runtime checking. However,
/// hash collisions between types cannot be checked at compile time, and though
/// very rare, could potentially cause UB down the line due to enum values
/// incorrectly initialized with values that don't correspond to a valid enum
/// variant.
///
/// It is bad form to use this in any public API. Those should
/// provide the most possible information and allow the user to erase it if they
/// want. Really this class should only be used for quick and dirty tasks and/or
/// entirely private code. Public APIs (especially ones that are generic) should
/// not expose this.
///
/// To ward off UB, you can put static_asserts in a header with all
/// the enum types and this header included. But this example solution only
/// works provided that `hashes` contains all of the types ever converted to an
/// anyerr_t as well as all the types passed to .try_cast<...>()
///
/// #include <okay/error.h>
/// #include <okay/containers/array.h>
/// #include "myerrors.h"
///
/// constexpr ok::array hashes = {
///     uint32_t(ok::ctti::typehash_32<MyCustomError>()),
///     uint32_t(ok::ctti::typehash_32<FileIOError>()),
///     uint32_t(ok::ctti::typehash_32<ok::alloc::error>()),
/// };
///
/// template <typename T>
/// consteval bool noduplicates(const T& hashlist)
/// {
///     for (uint32_t hash : hashlist) {
///         size_t times_occurred = 0;
///         for (uint32_t otherhash : hashlist) {
///             times_occurred += (otherhash == hash);
///             if (times_occurred > 1)
///                 return false;
///         }
///     }
///     return true;
/// }
///
/// static_assert(noduplicates(hashes));
///
class anyerr_t
{
    constexpr static uint64_t enum_value_mask = uint64_t(uint32_t(-1));
    constexpr static uint64_t enum_typehash_mask = enum_value_mask << 32;

  public:
    template <status_enum_c enum_t>
    constexpr anyerr_t(enum_t error) : m_value(uint32_t(error))
    {
        static_assert(sizeof(void*) == sizeof(uint64_t),
                      "unsupported platform");
        static_assert(
            sizeof(enum_t) <= sizeof(uint32_t),
            "enum type representation too large to fit in an anyerr_t");
    }

    template <typename status_t>
        requires detail::is_instance_c<status_t, ok::status>
    constexpr anyerr_t(status_t status) : anyerr_t(status.as_enum())
    {
    }

    template <status_enum_c enum_t>
    [[nodiscard]] constexpr opt<enum_t> try_cast() const noexcept
    {
        constexpr auto typehash = ok::typehash_32<enum_t>();
        const uint32_t stored_typehash = (m_value & enum_typehash_mask) >> 32;

        if (typehash != stored_typehash)
            return {};
        return enum_t(m_value & enum_value_mask);
    }

    constexpr static anyerr_t make_success() noexcept { return anyerr_t(); }

    [[nodiscard]] constexpr bool is_success() const noexcept
    {
        return (m_value & enum_value_mask) == 0;
    }

  private:
    constexpr anyerr_t() = default;
    uint64_t m_value = 0;
};

/// anystatus_t is an owning pointer to an abstract_status_t. It is usually
/// nonnull unless it is initialized with anystatus_t::make_success() or after
/// being moved out of. In these cases, the status appears as a success, but can
/// never be acquired with try_cast_to<T>().
class anystatus_t
{
  public:
    constexpr static anystatus_t make_success() noexcept
    {
        return anystatus_t();
    }

    [[nodiscard]] constexpr bool is_success() const noexcept
    {
        return !m_status || m_status->is_success();
    }

    constexpr void or_panic() const OKAYLIB_NOEXCEPT
    {
        if (!is_success()) [[unlikely]]
            ::abort();
    }

    template <typename T>
        requires detail::is_derived_from_c<T, abstract_status_t>
    explicit constexpr anystatus_t(T* error) : m_status(error)
    {
    }

    template <typename T>
        requires detail::is_derived_from_c<T, abstract_status_t>
    explicit constexpr anystatus_t(T& error) : m_status(ok::addressof(error))
    {
    }

    template <typename T>
        requires detail::is_derived_from_c<T, abstract_status_t>
    constexpr opt<T&> try_cast() noexcept
    {
        if (!m_status)
            return {};

        if (void* casted = m_status->try_cast_to(ok::typehash<T>()))
            return *static_cast<T*>(casted);
        return {};
    }

    template <typename T>
        requires detail::is_derived_from_c<T, abstract_status_t>
    constexpr opt<const T&> try_cast() const noexcept
    {
        if (!m_status)
            return {};

        if (const void* casted = m_status->try_cast_to(ok::typehash<T>()))
            return *static_cast<const T*>(casted);
        return {};
    }

    anystatus_t(const anystatus_t&) = delete;
    anystatus_t& operator=(const anystatus_t&) = delete;

    constexpr anystatus_t(anystatus_t&& other) noexcept
        : m_status(other.m_status)
    {
        other.m_status = nullptr;
    }

    constexpr anystatus_t& operator=(anystatus_t&& other) noexcept
    {
        if (ok::addressof(other) == this)
            return *this;

        if (m_status)
            destroy_assume_nonnull();

        m_status = other.m_status;
        other.m_status = nullptr;

        return *this;
    }

    constexpr ~anystatus_t()
    {
        if (m_status)
            destroy_assume_nonnull();
    }

  private:
    anystatus_t() noexcept = default;

    void destroy_assume_nonnull() const noexcept
    {
        auto allocator = m_status->allocator();

        m_status->~abstract_status_t();

        if (allocator) {
            allocator.ref_unchecked().deallocate(m_status);
        }
    }

    abstract_status_t* m_status = nullptr;
};

} // namespace ok

#endif
