#ifndef __OKAYLIB_ALLOCATORS_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_ALLOCATOR_H__

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

// pulls in slice and opt, and because of that also ranges and ordering
#include "okay/res.h"

namespace ok {

class maybe_defined_memory_t
{
  public:
    maybe_defined_memory_t() = delete;

    maybe_defined_memory_t(bytes_t bytes) OKAYLIB_NOEXCEPT : m_is_defined(true)
    {
        m_data.as_bytes = bytes;
    }

    maybe_defined_memory_t(undefined_memory_t<uint8_t> undefined)
        : m_is_defined(false) OKAYLIB_NOEXCEPT
    {
        m_data.as_undefined = undefined;
    }

    [[nodiscard]] bool is_defined() const OKAYLIB_NOEXCEPT
    {
        return m_is_defined;
    }

    [[nodiscard]] bytes_t as_bytes() const OKAYLIB_NOEXCEPT
    {
        if (!is_defined()) [[unlikely]] {
            __ok_abort()
        }

        return m_data.as_bytes;
    }

    [[nodiscard]] undefined_memory_t<uint8_t>
    as_undefined() const OKAYLIB_NOEXCEPT
    {
        if (is_defined()) [[unlikely]] {
            __ok_abort()
        }

        return m_data.as_undefined;
    }

  private:
    union maybe_undefined_slice_t
    {
        bytes_t as_bytes;
        undefined_memory_t<uint8_t> as_undefined;
        maybe_undefined_slice_t() OKAYLIB_NOEXCEPT {}
    };
    maybe_undefined_slice_t m_data;
    bool m_is_defined;
};

namespace alloc {
enum class error : uint8_t
{
    okay,
    result_released,
    oom,
    unsupported,
    usage,
    couldnt_expand_inplace,
};

inline constexpr size_t default_align = alignof(std::max_align_t);

template <typename T> using result_t = ok::res_t<T, error>;

enum class flags : uint16_t
{
    // clang-format off
    expand_front    = 0b00000001,
    expand_back     = 0b00000010,
    shrink_front    = 0b00000100,
    shrink_back     = 0b00001000,
    keep_old_nocopy = 0b00010000,
    try_defragment  = 0b00100000,
    leave_nonzeroed = 0b01000000,
    inplace_or_fail = 0b10000000,
    // clang-format on
};

enum class feature_flags : uint16_t
{
    // clang-format off
    // whether it is valid to call allocator functions from multiple
    // threads. Additionally requires that allocations do not have any
    // state that can be modified from other threads threough the
    // allocator, like a stacklike allocator where another thread can
    // allocate between operations and change whether your allocation
    // is on top of the stack.
    threadsafe      = 0x0001,
    // whether this allocator supports clearing. if it does not support
    // clearing, then clear() is an empty function call in release mode
    // and an assert() in debug mode.
    clearing        = 0x0002,
    // Can this allocator expand or shrink an allocation in-place?
    // If this flag is off, then it is not worth it to pass the keep_old_nocopy
    // flag. If this flag is off, then passing inplace_or_fail will always
    // return error::unsupported.
    in_place        = 0x0004,
    // whether this allocator supports leaving reallocated memory uninitialized,
    // in the case that inplace reallocation failed (ie. whether you can pass
    // alloc::flags::keep_old_nocopy).
    keep_old_nocopy = 0x0008,
    // If this is true, then the allocator can only allocate. Calling realloc on
    // such an allocator will error with error::unsupported. Calling deallocate
    // will do nothing.
    only_alloc      = 0x0010,
    // Similar to only_alloc (see above) except this allocator supports
    // specifically freeing in LIFO order, and reallocating only the most
    // recent allocation. `reallocate_extended()` is not guaranteed to be
    // supported on any allocation (including the most recent allocation) if
    // this flag is specified. This flag is mutually exclusive with
    // feature_flags::only_alloc and feature_flags::threadsafe.
    stacklike       = 0x0020,
    expand_back     = 0x0040,
    expand_front    = 0x0080,
    // Whether shrinking provides any benefits for this allocator
    can_reclaim     = 0x0100,
    // clang-format on
};

/// Merge two sets of flags.
constexpr flags operator|(flags a, flags b)
{
    using flags = flags;
    return static_cast<flags>(static_cast<std::underlying_type_t<flags>>(a) |
                              static_cast<std::underlying_type_t<flags>>(b));
}

constexpr feature_flags operator|(feature_flags a, feature_flags b)
{
    using flags = feature_flags;
    return static_cast<flags>(static_cast<std::underlying_type_t<flags>>(a) |
                              static_cast<std::underlying_type_t<flags>>(b));
}

/// Check if two sets of flags have anything in common.
constexpr bool operator&(flags a, flags b)
{
    using flags = flags;
    return static_cast<std::underlying_type_t<flags>>(a) &
           static_cast<std::underlying_type_t<flags>>(b);
}

constexpr bool operator&(feature_flags a, feature_flags b)
{
    using flags = feature_flags;
    return static_cast<std::underlying_type_t<flags>>(a) &
           static_cast<std::underlying_type_t<flags>>(b);
}

struct request_t
{
    size_t num_bytes;
    size_t alignment = alloc::default_align;
    void* future_compat = nullptr;
    flags flags;
};

struct reallocate_request_t
{
    bytes_t memory;
    // size of the memory after reallocating
    size_t new_size_bytes;
    // ignored if shrinking or if zero
    size_t preferred_size_bytes = 0;
    // just for keep_old_nocopy, try_defragment, or leave_nonzeroed
    alloc::flags flags;

    [[nodiscard]] constexpr bool is_valid() const OKAYLIB_NOEXCEPT
    {
        constexpr alloc::flags forbidden =
            flags::shrink_front | flags::expand_front | flags::expand_back |
            flags::shrink_back;

        // cannot expand or shrink front
        return !(flags & forbidden) &&
               // no attempt to... free the memory?
               (new_size_bytes != 0) &&
               // preferred should be zero OR ( (we're growing OR staying the
               // same size) + preferred is greater than required )
               (preferred_size_bytes == 0 ||
                (new_size_bytes >= memory.size() &&
                 preferred_size_bytes > new_size_bytes));
    }
};

struct reallocate_extended_request_t
{
    bytes_t memory;
    size_t required_bytes_back;
    size_t preferred_bytes_back = 0;
    size_t required_bytes_front;
    size_t preferred_bytes_front = 0;
    void* future_compat = nullptr;
    alloc::flags flags;

    [[nodiscard]] constexpr bool is_valid() const OKAYLIB_NOEXCEPT
    {
        const bool changing_back =
            flags & flags::expand_back || flags & flags::shrink_back;
        const bool changing_front =
            flags & flags::expand_front || flags & flags::shrink_front;
        return
            // exactly one of expand_back or shrink_back, and one of shrink_back
            // and expand_back
            ((flags & flags::expand_back) != (flags & flags::shrink_back) ||
             (flags & flags::expand_front) != (flags & flags::shrink_front)) &&
            // preferred bytes should not be specified if shrinking
            ((flags & flags::expand_front) || preferred_bytes_front == 0) &&
            ((flags & flags::expand_back) || preferred_bytes_back == 0) &&
            // cannot shrink more than size of allocation
            (((flags & flags::shrink_back) * required_bytes_back) +
             ((flags & flags::shrink_front) * required_bytes_front)) <
                memory.size() &&
            // some bytes need to be required; a no-op is assumed to be a
            // mistake
            (!changing_back || required_bytes_back != 0) &&
            (!changing_front || required_bytes_front != 0) &&
            // preferred bytes should be either zero or greater than required
            // bytes
            (preferred_bytes_back > required_bytes_back ||
             preferred_bytes_back == 0) &&
            (preferred_bytes_front > required_bytes_front ||
             preferred_bytes_front == 0);
    }
};

namespace detail {
template <typename T> class checked_is_valid_t
{
  private:
    T m_inner;

  public:
    // implicitly convertible from request
    constexpr checked_is_valid_t(T&& req) OKAYLIB_NOEXCEPT
        : m_inner(std::forward<T>(req))
    {
        if (!m_inner.is_valid()) [[unlikely]] {
            __ok_abort();
        }
    }
};
} // namespace detail

using valid_reallocate_extended_request_t =
    detail::checked_is_valid_t<reallocate_extended_request_t>;
using valid_reallocate_request_t =
    detail::checked_is_valid_t<reallocate_request_t>;

struct reallocation_t
{
    // callers job to keep track of whether the additionally allocated memory
    // is initialized (beyond the zeroing allocators do by default)
    bytes_t new_memory;
    bool kept = false;
};

struct reallocation_extended_t
{
    bytes_t new_memory;
    size_t bytes_offset_front = 0;
    bool kept = false;
};

} // namespace alloc

template <> class ok::res_t<alloc::reallocation_t, alloc::error>
{
  private:
    using reallocation_t = alloc::reallocation_t;
    using error_enum_t = alloc::error;

    // mimic layout of bytes_t here so reinterpret_cast works
    size_t size;
    uint8_t* start;
    bool kept;
    error_enum_t error;

  public:
    [[nodiscard]] reallocation_t& release_ref() & OKAYLIB_NOEXCEPT
    {
        error = error_enum_t::result_released;
        return *reinterpret_cast<reallocation_t*>(this);
    }

    [[nodiscard]] reallocation_t release() OKAYLIB_NOEXCEPT
    {
        // can be called as rvalue
        error = error_enum_t::result_released;
        return *reinterpret_cast<reallocation_t*>(this);
    }

    [[nodiscard]] constexpr opt_t<reallocation_t>
    to_opt() const OKAYLIB_NOEXCEPT
    {
        return reallocation_t{
            .new_memory = raw_slice(*start, size),
            .kept = kept,
        };
    }

    [[nodiscard]] constexpr error_enum_t err() const OKAYLIB_NOEXCEPT
    {
        return error;
    }

    [[nodiscard]] constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return err() == error_enum_t::okay;
    }

    constexpr res_t(const reallocation_t& reallocation) OKAYLIB_NOEXCEPT
        : size(reallocation.new_memory.size()),
          start(reallocation.new_memory.data()),
          kept(reallocation.kept),
          error(error_enum_t::okay)
    {
    }

    constexpr res_t(size_t _size, uint8_t& _start, bool _kept) OKAYLIB_NOEXCEPT
        : size(_size),
          start(ok::addressof(_start)),
          kept(_kept),
          error(error_enum_t::okay)
    {
    }

    constexpr res_t(error_enum_t failure) OKAYLIB_NOEXCEPT : size(0),
                                                             start(0),
                                                             kept(false),
                                                             error(failure)
    {
        if (failure == error_enum_t::okay) [[unlikely]] {
            __ok_abort();
        }
    }
};

/// Abstract/virtual interface for allocators. Not all functions may be
/// implemented, depending on `features()`
class allocator_t
{
  public:
    [[nodiscard]] virtual alloc::result_t<maybe_defined_memory_t>
    allocate(const alloc::request_t&) noexcept = 0;

    virtual void clear() noexcept = 0;

    [[nodiscard]] virtual alloc::feature_flags features() const noexcept = 0;

    virtual void deallocate(bytes_t bytes) noexcept = 0;

    [[nodiscard]] virtual alloc::result_t<alloc::reallocation_t>
    reallocate(const alloc::valid_reallocate_request_t& options) noexcept = 0;

    [[nodiscard]] virtual alloc::result_t<alloc::reallocation_extended_t>
    reallocate_extended(
        const alloc::valid_reallocate_extended_request_t& options) noexcept = 0;
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <> struct fmt::formatter<ok::alloc::error>
{
    using error_t = ok::alloc::error;

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const error_t& err,
                                    format_context& ctx) const
    {
        switch (err) {
        case error_t::okay:
            return fmt::format_to(ctx.out(), "alloc::error::okay");
        case error_t::unsupported:
            return fmt::format_to(ctx.out(), "alloc::error::unsupported");
        case error_t::oom:
            return fmt::format_to(ctx.out(), "alloc::error::oom");
        case error_t::result_released:
            return fmt::format_to(ctx.out(), "alloc::error::result_released");
        case error_t::usage:
            return fmt::format_to(ctx.out(), "alloc::error::usage");
        case error_t::couldnt_expand_inplace:
            return fmt::format_to(ctx.out(),
                                  "alloc::error::couldnt_expand_inplace");
        }
    }
};
#endif

#endif
