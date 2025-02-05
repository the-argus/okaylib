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

    explicit maybe_defined_memory_t(bytes_t bytes) OKAYLIB_NOEXCEPT
        : m_is_defined(true)
    {
        m_data.as_bytes = bytes;
    }

    explicit maybe_defined_memory_t(undefined_memory_t<uint8_t> undefined)
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

/// Abstract virtual interface for allocators which can realloc
class allocator_t
{
  public:
    inline static constexpr size_t default_align = alignof(std::max_align_t);

    enum class error : uint8_t
    {
        okay,
        result_released,
        oom,
        unsupported,
        usage,
    };

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
        // whether expanded / newly allocated memory will be a defined value
        // This value may be different in debug and release modes.
        // TODO: maybe this isnt super useful if its not the same in all build
        // modes? maybe change that spec
        initialized     = 0x0200,
        // whether this allocator supports clearing. if it does not support
        // clearing, then clear() is an empty function call in release mode
        // and an assert() in debug mode.
        clearing        = 0x0400,
        // If this is specified, then the allocator is guaranteed to never
        // throw exceptions.
        nothrow         = 0x0800,
        // clang-format on
    };

    struct reallocate_options_t
    {
        bytes_t memory;
        size_t required_additional_bytes;
        size_t preferred_additional_bytes = 0;
        flags flags;
    };

    struct reallocation_options_extended_t
    {
        bytes_t memory;
        size_t required_bytes_back;
        size_t required_bytes_front;
        size_t preferred_bytes_back;
        size_t preferred_bytes_front;
        flags flags;
    };

    struct reallocation_extended_t
    {
        bytes_t memory;
        // pointer to the part of memory corresponding to what was originally
        // the pointer to the start of your allocation. Unless you shrink_front,
        // in which case this is nullptr. If you don't expand_back, this is
        // equal to data
        void* data_original_offset;
        bool kept;

        constexpr reallocation_extended_t(bytes_t _memory,
                                          void* _data_original_offset,
                                          bool _kept) OKAYLIB_NOEXCEPT
            : memory(_memory),
              data_original_offset(_data_original_offset),
              kept(_kept)
        {
        }
    };

    /// Allocate some amount of memory of size at least nbytes.
    /// Return pointer to memory and the size of the block.
    /// The allocator will return at least nbytes but may make a decision to
    /// arbitrarily return more, especially if there is extra space it cannot
    /// reuse off the back of the allocation.
    [[nodiscard]] virtual result_t<undefined_memory_t<uint8_t>>
    allocate_bytes(size_t nbytes, size_t alignment = default_align) = 0;

    // Given a live allocation specified by the "data" and "size" fields, alter
    // it by expanding or shrinking it from the front or back, or both in some
    // combination.
    // expand_front and shrink_front are mutually exclusive, along with
    // expand_back and shrink_back.
    // The amount expanded in either direction will be between required_bytes_*
    // and preferred_bytes_*, and if it cannot be done then a nullptr will be
    // returned.
    // If keep_old is specified, then the original allocation will still be
    // live after the reallocation, *if* a new allocation had to be created. In
    // this case, the "kept" boolean will be true in the returned
    // allocation_result.
    // If nocopy is specified, then a copy will not be performed. nocopy +
    // keep_old together can be used to handle copying yourself, when it must
    // happen. nocopy without keep_old is always invalid as it would result in
    // data loss.
    // If try_defragment is specified, then the allocator will prioritize
    // moving the allocation to a better fit spot, being more likely to
    // cause copying or new allocations.
    [[nodiscard]] virtual result_t<reallocation_extended_t>
    reallocate_bytes_extended(const reallocate_options_t& options) = 0;

    [[nodiscard]] virtual maybe_defined_memory_t
    reallocate_bytes(const reallocate_options_t& options) = 0;

    [[nodiscard]] virtual feature_flags features() const noexcept = 0;

    // Free all allocations in this allocator.
    virtual void clear() = 0;

    virtual void deallocate_bytes(bytes_t bytes,
                                  size_t alignment = default_align) = 0;
};

/// Merge two sets of flags.
inline constexpr allocator_t::flags operator|(allocator_t::flags a,
                                              allocator_t::flags b)
{
    using flags = allocator_t::flags;
    return static_cast<flags>(static_cast<std::underlying_type_t<flags>>(a) |
                              static_cast<std::underlying_type_t<flags>>(b));
}

inline constexpr allocator_t::feature_flags
operator|(allocator_t::feature_flags a, allocator_t::feature_flags b)
{
    using flags = allocator_t::feature_flags;
    return static_cast<flags>(static_cast<std::underlying_type_t<flags>>(a) |
                              static_cast<std::underlying_type_t<flags>>(b));
}

/// Check if two sets of flags have anything in common.
inline constexpr bool operator&(allocator_t::flags a, allocator_t::flags b)
{
    using flags = allocator_t::flags;
    return static_cast<std::underlying_type_t<flags>>(a) &
           static_cast<std::underlying_type_t<flags>>(b);
}

inline constexpr bool operator&(allocator_t::feature_flags a,
                                allocator_t::feature_flags b)
{
    using flags = allocator_t::feature_flags;
    return static_cast<std::underlying_type_t<flags>>(a) &
           static_cast<std::underlying_type_t<flags>>(b);
}

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <> struct fmt::formatter<ok::allocator_t::error>
{
    using error_t = ok::allocator_t::error;

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
            return fmt::format_to(ctx.out(), "allocator_t::error::okay");
        case error_t::unsupported:
            return fmt::format_to(ctx.out(), "allocator_t::error::unsupported");
        case error_t::oom:
            return fmt::format_to(ctx.out(), "allocator_t::error::oom");
        case error_t::result_released:
            return fmt::format_to(ctx.out(),
                                  "allocator_t::error::result_released");
        case error_t::usage:
            return fmt::format_to(ctx.out(), "allocator_t::error::usage");
        }
    }
};
#endif

#endif
