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

struct undefined_t
{};

inline constexpr undefined_t undefined = {};

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
        initialized     = 0x0200,
        // whether this allocator supports clearing. if it does not support
        // clearing, then clear() is an empty function call in release mode
        // and an assert() in debug mode.
        clearing        = 0x0400,
        // If this is specified, then the allocator is guaranteed to never
        // throw exceptions.
        nothrow         = 0x0800,
        // If this is specified, then the allocator supports registering
        // destruction callbacks
        callbacks       = 0x1000,
        // clang-format on
    };

    struct reallocate_options_t
    {
        bytes_t memory;
        // no way to change the alignment of an allocation, you can
        // manually realign within the buffer, or reallocate
        size_t alignment = default_align;
        size_t required_bytes_front;
        size_t required_bytes_back;
        size_t preferred_bytes_front;
        size_t preferred_bytes_back;
        flags flags;
    };

    struct reallocation_t
    {
        bytes_t memory;
        // pointer to the part of memory corresponding to what was originally
        // the pointer to the start of your allocation. Unless you shrink_front,
        // in which case this is nullptr. If you don't expand_back, this is
        // equal to data
        void* data_original_offset;
        bool kept;

        constexpr reallocation_t(bytes_t _memory, void* _data_original_offset,
                                 bool _kept) OKAYLIB_NOEXCEPT
            : memory(_memory),
              data_original_offset(_data_original_offset),
              kept(_kept)
        {
        }
    };

    using reallocation_result_t = result_t<reallocation_t>;

    using destruction_callback_t = void (*)(void*);

    struct destruction_callback_entry_t
    {
        void* user_data;
        destruction_callback_t callback;
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
    [[nodiscard]] virtual reallocation_result_t
    reallocate_bytes(const reallocate_options_t& options) = 0;

    [[nodiscard]] virtual feature_flags features() const noexcept = 0;

    [[nodiscard]] virtual result_t<destruction_callback_entry_t&>
    register_destruction_callback(void*, destruction_callback_t) = 0;

    // Free all allocations in this allocator.
    virtual void clear() = 0;

    virtual void deallocate_bytes(bytes_t bytes,
                                  size_t alignment = default_align) = 0;

  protected:
    struct destruction_callback_entry_node_t
    {
        destruction_callback_entry_t entry;
        destruction_callback_entry_node_t* previous = nullptr;
    };

    /// Given an allocator and some pointer to the end of a destruction callback
    /// linked list, append one item to the list. This list can later be
    /// traversed to call all callbacks. That should happen on clear and on
    /// destruction.
    /// Returns false on memory allocation failure.
    template <typename T>
    static inline constexpr result_t<destruction_callback_entry_t&>
    append_destruction_callback(
        T& allocator, destruction_callback_entry_node_t*& current_head,
        void* user_data, destruction_callback_t callback)
    {
        static_assert(std::is_base_of_v<ok::allocator_t, T>,
                      "Cannot append destruction callback to allocator which "
                      "does not inherit from allocator_t");
        auto result =
            allocator.allocate_bytes(sizeof(destruction_callback_t),
                                     alignof(destruction_callback_entry_t));
        if (!result)
            return error::oom;

        if (!current_head) {
            current_head =
                static_cast<destruction_callback_entry_node_t*>(result.data());
            current_head->previous = nullptr;
        } else {
            auto* const temp = current_head;
            current_head =
                static_cast<destruction_callback_entry_node_t*>(result.data());
            current_head->previous = temp;
        }

        current_head->entry.callback = callback;
        current_head->entry.user_data = user_data;
        return current_head->entry;
    }

    // traverse a linked list of destruction callbacks and call each one of
    // them. Does not deallocate the space used by the destruction callback.
    // Intended to be called when an allocator is destroyed.
    static inline constexpr void call_all_destruction_callbacks(
        destruction_callback_entry_node_t* current_head)
    {
        destruction_callback_entry_node_t* iter = current_head;
        while (iter != nullptr) {
            iter->entry.callback(iter->entry.user_data);
            iter = iter->previous;
        }
    }
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
