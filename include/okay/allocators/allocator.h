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
};

inline constexpr size_t default_align = alignof(std::max_align_t);

template <typename T> using result_t = ok::res_t<T, error>;

enum class flags : uint8_t
{
    // clang-format off
    expand_front    = 0b00000001,
    expand_back     = 0b00000010,
    shrink_front    = 0b00000100,
    shrink_back     = 0b00001000,
    keep_old_nocopy = 0b00010000,
    try_defragment  = 0b00100000,
    leave_nonzeroed = 0b01000000,
    // clang-format on
};

enum class feature_flags : uint16_t
{
    // clang-format off
    // Can this allocator expand or shrink an allocation in-place? ie. is
    // it worth it to use the keep_old flag?
    in_place        = 0x0001,
    // arena allocators might not support this. if the allocator only
    // supports freeing the most recent allocation, this will not be
    // specified but "stacklike" will be. If this is not specified, then
    // deallocate and will be an empty function, not an error. reallocate
    // will always error (see nothrow feature flag for how error is
    // propagated).
    free_and_realloc= 0x0002,
    // whether the allocator can make use of reclaimed memory after an
    // allocation is shrunk. The allocator will never fail when
    // shrinking, but this flag tells you if the shrinking is taken
    // advantage of by the allocator.
    shrinking       = 0x0004,
    // whether this allocator supports expanding the allocation off the
    // "back", in-place, as realloc() might. This flag should be 0 if
    // in_place is not specified: if the allocator cannot reallocate in
    // place then it will just always create and entire new allocation to
    // achieve expand_back.
    expand_back     = 0x0008,
    // whether this allocator supports expanding the allocation off the
    // "front", meaning before the data pointer. This is only relevant if
    // in_place is specified, otherwise the allocator will always perform
    // a new allocation to expand at the front, and this flag should be 0.
    expand_front    = 0x0010,
    // Whether *not* memcpy-ing is supported by this allocator. If this is
    // not specified, it is possible to implement a wrapper which uses
    // allocate and deallocate to emulate this functionality.
    nocopy          = 0x0020,
    // If this is specified, then the allocator is capable of keeping an
    // older allocation when reallocate_bytes() is called. If this is not
    // specified, it is possible to implement a wrapper which uses allocate
    // and deallocate to emulate this functionality.
    keep_old        = 0x0040,
    // arena allocators might have this specified, if you can modify
    // only the most recently made allocation. this is mutually exclusie
    // with `threadsafe`.
    stacklike       = 0x0080,
    // whether it is valid to call allocator functions from multiple
    // threads. Additionally requires that allocations do not have any
    // state that can be modified from other threads threough the
    // allocator, like a stacklike allocator where another thread can
    // allocate between operations and change whether your allocation
    // is on top of the stack.
    threadsafe      = 0x0100,
    // whether this allocator supports clearing. if it does not support
    // clearing, then clear() is an empty function call in release mode
    // and an assert() in debug mode.
    clearing        = 0x0200,
    // If this is specified, then the allocator is guaranteed to never
    // throw exceptions.
    nothrow         = 0x0400,
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
    size_t required_bytes;
    // ignored if shrinking or if zero
    size_t preferred_bytes = 0;
    alloc::flags flags;

    [[nodiscard]] constexpr bool is_valid() const OKAYLIB_NOEXCEPT
    {
        constexpr alloc::flags forbidden =
            flags::shrink_front | flags::expand_front;
        // cannot expand or shrink front
        return !(flags & forbidden) &&
               // exactly one of expand_back or shrink_back
               ((flags & flags::expand_back) != (flags & flags::shrink_back)) &&
               // cant shrink more than the size of the existing allocation
               (!(flags & flags::shrink_back) ||
                required_bytes < memory.size()) &&
               // some bytes need to be required; a no-op is assumed to be a
               // mistake
               (required_bytes != 0) &&
               // preferred should be zero or be greater than required
               (preferred_bytes > required_bytes || preferred_bytes == 0);
    }
};

struct reallocate_extended_request_t
{
    bytes_t memory;
    size_t required_bytes_back;
    size_t preferred_bytes_back;
    size_t required_bytes_front;
    size_t preferred_bytes_front;
    void* future_compat = nullptr;
    alloc::flags flags;

    [[nodiscard]] constexpr bool is_valid() const OKAYLIB_NOEXCEPT
    {
        const bool changing_back =
            flags & flags::expand_back || flags & flags::shrink_back;
        const bool changing_front =
            flags & flags::expand_front || flags & flags::shrink_front;
        // exactly one of expand_back or shrink_back, and one of shrink_back and
        // expand_back
        return ((flags & flags::expand_back) != (flags & flags::shrink_back) ||
                (flags & flags::expand_front) !=
                    (flags & flags::shrink_front)) &&
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

struct reallocation_t
{
    struct
    {
        // number of bytes expanded/shrunk in back
        size_t back_bytes;
        // number of bytes expanded/shrunk off front
        size_t front_bytes;
        // start of the new allocation, only relevant if moving the front or if
        // inplace reallocation failed.
        uint8_t* new_memory;

        bool kept = false;

        void* future_compat = nullptr;
    } inner;

    /// Whether the original allocation is still live.
    /// Will always be false if flags::keep_old_nocopy was not specified
    constexpr bool original_was_kept() const OKAYLIB_NOEXCEPT
    {
        return inner.kept;
    }

    constexpr bool is_new_memory_zeroed() const OKAYLIB_NOEXCEPT {}

    constexpr void new_memory(const request_t&)
};

} // namespace alloc

/// Abstract/virtual interface for allocators. Not all functions may be
/// implemented, depending on `features()`
class allocator_t
{
  public:
    /// Allocate some amount of memory of size at least nbytes.
    /// Return pointer to memory and the size of the block.
    /// This will be undefined memory if the allocator is explicitly asked to
    /// leave_nonzeroed. The allocator will return at least nbytes but may make
    /// a decision to arbitrarily return more, especially if there is extra
    /// space it cannot reuse off the back of the allocation.
    [[nodiscard]] virtual alloc::result_t<maybe_defined_memory_t>
    allocate(const alloc::request_t&) = 0;

    // Free all allocations in this allocator.
    virtual void clear() noexcept = 0;

    [[nodiscard]] virtual alloc::feature_flags features() const noexcept = 0;

    virtual void deallocate(bytes_t bytes) noexcept = 0;

    /// Reallocate bytes- but any usage of expand_back or shrink_back will fail.
    /// The returned memory will be uninitialized if any of the memory is
    /// uninitialized: if expand_back and leave_nonzeroed are passed it will be
    /// uninitialized, or if keep_old_nocopy was passed, and inplace expansion
    /// failed.
    [[nodiscard]] virtual alloc::result_t<reallocation_t>
    reallocate(const reallocate_options_t& options) = 0;

    // Given a live allocation specified by the "data" and "size" fields, alter
    // it by expanding or shrinking it from the front or back, or both in some
    // combination.
    // expand_front and shrink_front are mutually exclusive, along with
    // expand_back and shrink_back.
    // The amount expanded off the back/front will be greater than or equal to
    // required_bytes_[back|front].
    // If preferred_bytes_[back|front] are zero, then it is assumed that
    // required == preferred. If preferred_bytes_[back|front] is nonzero but
    // less than required_bytes_[back|front], then an assert fires and a usage
    // error is returned.
    // If keep_old_nocopy is specified, *and* a new allocation had to be created
    // (ie. in-place reallocation failed) then the original allocation will
    // still be live after the reallocation. In this case, `expanded_memory`
    // will be undefined memory pointing to the new buffer, and
    // `original_subslice` will be an alias for the memory passed in.
    // allocation_result. If nocopy is specified, then a copy will not be
    // performed. nocopy + keep_old together can be used to handle copying
    // yourself, when it must happen. nocopy without keep_old is always invalid
    // as it would result in data loss. If try_defragment is specified, then the
    // allocator will prioritize moving the allocation to a better fit spot,
    // being more likely to cause copying or new allocations.
    [[nodiscard]] virtual alloc::result_t<reallocation_t>
    reallocate_extended(const reallocation_options_extended_t& options) = 0;
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
        }
    }
};
#endif

#endif
