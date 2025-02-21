#ifndef __OKAYLIB_ALLOCATORS_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_ALLOCATOR_H__

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility> // for std::forward

// pulls in res, then slice and opt, and because of that also ranges and
// ordering
#include "okay/construct.h"
// for reinterpret_as_bytes
#include "okay/stdmem.h"

namespace ok {

class maybe_defined_memory_t
{
  public:
    maybe_defined_memory_t() = delete;
    maybe_defined_memory_t(const maybe_defined_memory_t&) = default;
    maybe_defined_memory_t& operator=(const maybe_defined_memory_t&) = default;
    maybe_defined_memory_t(maybe_defined_memory_t&&) = default;
    maybe_defined_memory_t& operator=(maybe_defined_memory_t&&) = default;

    maybe_defined_memory_t(bytes_t bytes) OKAYLIB_NOEXCEPT : m_is_defined(true)
    {
        m_data.as_bytes = bytes;
    }

    maybe_defined_memory_t(undefined_memory_t<uint8_t> undefined)
        OKAYLIB_NOEXCEPT : m_is_defined(false)
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
            __ok_abort(
                "Attempt to get bytes_t from a maybe_defined_memory_t, but the "
                "memory was undefined. Use as_undefined() instead.")
        }

        return m_data.as_bytes;
    }

    [[nodiscard]] undefined_memory_t<uint8_t>
    as_undefined() const OKAYLIB_NOEXCEPT
    {
        if (is_defined()) [[unlikely]] {
            __ok_abort("Attempt to get an undefined_memory_t from a "
                       "maybe_defined_memory_t, but the memory was already "
                       "defined. Use as_bytes() instead.")
        }

        return m_data.as_undefined;
    }

    /// get data directly without resolving whether or not this is defined
    /// using this is a bad idea, just do as_undefined() or as_bytes()
    [[nodiscard]] uint8_t* data_maybe_defined() const OKAYLIB_NOEXCEPT
    {
        if (m_is_defined) {
            return m_data.as_bytes.data();
        } else {
            return m_data.as_undefined.data();
        }
    }

    [[nodiscard]] size_t size() const OKAYLIB_NOEXCEPT
    {
        if (m_is_defined) {
            return m_data.as_bytes.size();
        } else {
            return m_data.as_undefined.size();
        }
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
    no_value,
    oom,
    unsupported,
    usage,
    couldnt_expand_in_place,
};

inline constexpr size_t default_align = alignof(std::max_align_t);

template <typename T> using result_t = ok::res_t<T, error>;

enum class flags : uint16_t
{
    // clang-format off
    expand_front                    = 0b00000001,
    expand_back                     = 0b00000010,
    shrink_front                    = 0b00000100,
    shrink_back                     = 0b00001000,
    try_defragment                  = 0b00010000,
    leave_nonzeroed                 = 0b00100000,
    // This flag asks the allocator to check if it will be able to reallocate in
    // place before performing it, and fail if not.
    // This is only supported by allocators that
    // can_predictably_realloc_in_place.
    in_place_orelse_fail            = 0b01000000,
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
    is_threadsafe                               = 0x0001,
    // whether this allocator supports clearing. if it does not support
    // clearing, then clear() should just print a warning about potential leaks.
    can_clear                                   = 0x0002,
    // Can this allocator sometimes reallocate in-place, and, crucially, can it
    // know whether the in-place reallocation will succeed before performing it?
    // (C's realloc cannot do this, because you can't know whether the
    // in-placeness succeeded until after calling it)
    // If this flag is off, then passing in_place_orelse_fail will always return
    // error::unsupported.
    can_predictably_realloc_in_place            = 0x0004,
    // If this is true, then the allocator can only allocate. Calling realloc on
    // such an allocator will error with error::unsupported. Calling deallocate
    // will do nothing. If an allocator is only_alloc but not clear, then a
    // warning about potential memory leaks may be printed.
    can_only_alloc                              = 0x0008,
    // Similar to only_alloc (see above) except this allocator supports
    // specifically freeing in LIFO order, and reallocating only the most
    // recent allocation. `reallocate_extended()` is not guaranteed to be
    // supported on any allocation (including the most recent allocation) if
    // this flag is specified. This flag is mutually exclusive with
    // feature_flags::only_alloc and feature_flags::threadsafe.
    is_stacklike                                = 0x001,
    can_expand_back                             = 0x0020,
    can_expand_front                            = 0x0040,
    // Whether shrinking provides any benefits for this allocator
    can_reclaim                                 = 0x0080,
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
    // just for try_defragment or leave_nonzeroed. flags::expand_back and
    // flags::shrink_back do nothing.
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

    /// Calculate information about how big the allocation will be after
    /// reallocation if the preferred size is respected exactly.
    ///
    /// Tuple returned has the following elements:
    /// 0: bytes_offset_back. how many bytes will be added/removed from back
    /// 1: bytes_offset_front. how many bytes will be added/removed from front
    /// 2: new_size. the new total size of the allocation
    [[nodiscard]] constexpr std::tuple<size_t, size_t, size_t>
    calculate_new_preferred_size() const OKAYLIB_NOEXCEPT
    {
        std::tuple<size_t, size_t, size_t> out;
        auto& amount_changed_back = std::get<0>(out);
        auto& amount_changed_front = std::get<1>(out);
        auto& new_size = std::get<2>(out);
        amount_changed_back =
            ok::max(required_bytes_back, preferred_bytes_back);
        amount_changed_front =
            ok::max(required_bytes_front, preferred_bytes_front);

        new_size = memory.size();

        if (flags & flags::expand_back)
            new_size += amount_changed_back;
        else if (flags & flags::shrink_back)
            new_size -= amount_changed_back;

        if (flags & flags::expand_front)
            new_size += amount_changed_front;
        else if (flags & flags::shrink_front)
            new_size -= amount_changed_front;

        return out;
    }
};

struct reallocation_extended_t
{
    bytes_t memory;
    size_t bytes_offset_front = 0;
};

} // namespace alloc

/// Abstract/virtual interface for allocators. Not all functions may be
/// implemented, depending on `features()`
class allocator_t
{
  public:
    [[nodiscard]] constexpr alloc::result_t<maybe_defined_memory_t>
    allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT
    {
        return impl_allocate(request);
    }

    constexpr void clear() OKAYLIB_NOEXCEPT { impl_clear(); }

    [[nodiscard]] constexpr alloc::feature_flags
    features() const OKAYLIB_NOEXCEPT
    {
        return impl_features();
    }

    constexpr void deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT
    {
        impl_deallocate(bytes);
    }

    [[nodiscard]] constexpr alloc::result_t<maybe_defined_memory_t>
    reallocate(const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT
    {
        if (!options.is_valid()) [[unlikely]] {
            __ok_assert(false, "invalid reallocate_request_t");
            return alloc::error::usage;
        }
        return impl_reallocate(options);
    }

    [[nodiscard]] constexpr alloc::result_t<alloc::reallocation_extended_t>
    reallocate_extended(const alloc::reallocate_extended_request_t& options)
        OKAYLIB_NOEXCEPT
    {
        if (!options.is_valid()) [[unlikely]] {
            __ok_assert(false, "invalid reallocate_extended_request_t");
            return alloc::error::usage;
        }
        return impl_reallocate_extended(options);
    }

    template <typename T, typename constructor_tag_t, typename... args_t>
    [[nodiscard]] constexpr decltype(auto)
    make(args_t&&... args) OKAYLIB_NOEXCEPT;

    template <typename factory_t, typename... args_t>
    [[nodiscard]] constexpr decltype(auto)
    make_using_factory(const factory_t& factory,
                       args_t&&... args) OKAYLIB_NOEXCEPT;

  protected:
    [[nodiscard]] virtual alloc::result_t<maybe_defined_memory_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT = 0;

    virtual void impl_clear() OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] virtual alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT = 0;

    virtual void impl_deallocate(bytes_t bytes) OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] virtual alloc::result_t<maybe_defined_memory_t>
    impl_reallocate(const alloc::reallocate_request_t& options)
        OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] virtual alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT = 0;
};

namespace alloc {

struct potentially_in_place_reallocation_t
{
    bytes_t memory;
    size_t bytes_offset_front;
    bool was_in_place;
};

/// Wrapper around reallocate_extended which tries to do
/// in_place_orelse_fail and then allocates a separate buffer of the correct
/// size if in_place reallocation failed.
template <typename allocator_impl_t>
[[nodiscard]] constexpr result_t<potentially_in_place_reallocation_t>
reallocate_in_place_orelse_keep_old_nocopy(
    const allocator_impl_t& allocator,
    const alloc::reallocate_extended_request_t& options)
{
    static_assert(detail::is_derived_from_v<allocator_impl_t, allocator_t>,
                  "Cannot call reallocate on the given type- it is not derived "
                  "from ok::allocator_t.");

    __ok_assert(options.flags & flags::in_place_orelse_fail,
                "Attempt to call reallocate_in_place_orelse_keep_old_nocopy "
                "but the given options do not specify in_place_orelse_fail");

    // try to do it in place
    result_t<reallocation_extended_t> reallocation_res =
        allocator.impl_reallocate_extended(options);
    if (reallocation_res.okay()) {
        auto& reallocation = reallocation_res.release_ref();
        return potentially_in_place_reallocation_t{
            .memory = reallocation.memory,
            .bytes_offset_front = reallocation.bytes_offset_front,
            .was_in_place = true,
        };
    }

    using flags_underlying_t = std::underlying_type_t<flags>;
    constexpr auto mask =
        static_cast<flags_underlying_t>(flags::leave_nonzeroed);
    const auto leave_nonzeroed_bit =
        static_cast<flags_underlying_t>(options.flags) & mask;

    auto [bytes_offset_back, bytes_offset_front, new_size] =
        options.calculate_new_preferred_size();

    // propagate leave_nonzeroed request to allocate call
    result_t<maybe_defined_memory_t> res =
        allocator.impl_allocate(alloc::request_t{
            .num_bytes = new_size,
            .flags = flags(leave_nonzeroed_bit),
        });

    if (!res.okay()) [[unlikely]]
        return res.err();

    if (leave_nonzeroed_bit) {
        return potentially_in_place_reallocation_t{
            .memory = res.release_ref().as_undefined().leave_undefined(),
            .was_in_place = false,
        };
    } else {
        return potentially_in_place_reallocation_t{
            .memory = res.release_ref().as_bytes(),
            .was_in_place = false,
        };
    }
}
} // namespace alloc

template <typename T, typename allocator_impl_t>
constexpr void free(allocator_impl_t& ally, T& object) OKAYLIB_NOEXCEPT
{
    static_assert(
        detail::is_derived_from_v<allocator_impl_t, allocator_t>,
        "Type given in first argument does not inherit from ok::allocator_t.");
    static_assert(is_std_destructible_v<T>,
                  "The destructor you're trying to call with ok::free is "
                  "not " __ok_msg_nothrow "destructible");
    static_assert(
        !std::is_array_v<T> ||
            is_std_destructible_v<std::remove_reference_t<decltype(object[0])>>,
        "The destructor of items within the given array are "
        "not " __ok_msg_nothrow " destructible.");
    static_assert(
        !std::is_pointer_v<T>,
        "Reference to pointer passed to ok::free(). This is a potential "
        "indication of array decay. Call allocator.deallocate() directly if "
        "this is actually a pointer.");

    if constexpr (!std::is_array_v<std::remove_reference_t<T>>) {
        object.~T();
    } else {
        using VT = detail::c_array_value_type<T>;
        for (size_t i = 0; i < detail::c_array_length(object); ++i) {
            object[i].~VT();
        }
    }
    ally.deallocate(ok::reinterpret_as_bytes(ok::slice_from_one(object)));
}

namespace detail {
template <typename T, typename = void> struct memberfunc_error_enum_type_or_void
{
    using type = void;
};

template <typename T>
struct memberfunc_error_enum_type_or_void<
    T, std::void_t<typename detail::memberfunc_error_type_t<T>::enum_type>>
{
    using type = typename detail::memberfunc_error_type_t<T>::enum_type;
};

template <typename... args_t> struct make_return_type
{
    template <typename T, typename constructor_tag_t, typename = void>
    struct inner
    {
        using type = alloc::result_t<T&>;
    };
    template <typename T, typename constructor_tag_t>
    struct inner<
        T, constructor_tag_t,
        std::enable_if_t<
            !std::is_void_v<detail::memberfunc_error_type_t<T>> &&
            is_fallible_constructible_v<T, constructor_tag_t, args_t...>>>
    {
        using type =
            res_t<T&, typename detail::memberfunc_error_type_t<T>::enum_type>;
    };
};
} // namespace detail

template <typename T, typename constructor_tag_t = ok::default_constructor_tag,
          typename... args_t>
[[nodiscard]] constexpr decltype(auto)
ok::allocator_t::make(args_t&&... args) OKAYLIB_NOEXCEPT
{
    using out_t = typename detail::make_return_type<args_t...>::template inner<
        T, constructor_tag_t>::type;
    constexpr bool is_fallible =
        is_fallible_constructible_v<T, constructor_tag_t, args_t...>;
    constexpr bool is_infallible =
        is_std_constructible_v<T, constructor_tag_t, args_t...>;
    constexpr bool is_std = is_std_constructible_v<T, args_t...>;
    static_assert(is_fallible || is_infallible || is_std,
                  "Cannot construct given type with the given arguments.");
    static_assert(
        !is_fallible ||
            std::is_convertible_v<
                alloc::error,
                typename detail::memberfunc_error_enum_type_or_void<T>::type>,
        "When allocating and calling a fallible constructor with "
        "ok::make, the enum type needs to define a conversion from "
        "an ok::alloc::error.");
    static_assert(
        int(is_fallible) + int(is_infallible) + int(is_std) == 1,
        "Ambiguous constructor selection. the given args could address "
        "multiple constructors with different tags or out errors.");

    auto res = allocate(alloc::request_t{
        .num_bytes = sizeof(T),
        .alignment = alignof(T),
        .flags = alloc::flags::leave_nonzeroed,
    });

    if (!res.okay()) [[unlikely]] {
        return out_t(res.err());
    }

    uint8_t* object_start = res.release_ref().data_maybe_defined();

    __ok_assert(uintptr_t(object_start) % alignof(T) == 0,
                "Misaligned memory produced by allocator");

    T* made = reinterpret_cast<T*>(object_start);

    if constexpr (is_fallible) {
        return make_into_uninitialized<constructor_tag_t>(
            made, std::forward<args_t>(args)...);
    } else {
        return out_t(make_into_uninitialized<constructor_tag_t>(
            made, std::forward<args_t>(args)...));
    }
}

template <typename factory_t, typename... args_t>
[[nodiscard]] constexpr decltype(auto)
ok::allocator_t::make_using_factory(const factory_t& factory,
                                    args_t&&... args) OKAYLIB_NOEXCEPT
{
    static_assert(is_std_invocable_v<factory_t, args_t...>,
                  "Type given as first argument to make_using_factory is not "
                  "invokable with the given args.");
    using T = decltype(factory(std::forward<args_t>(args)...));
    static_assert(
        !std::is_reference_v<T>,
        "Given factory function is invalid- it returns a reference type.");
    static_assert(std::is_class_v<T>,
                  "Given factory function is invalid- it does not return a "
                  "class or struct type.");

    auto res = allocate(alloc::request_t{
        .num_bytes = sizeof(T),
        .alignment = alignof(T),
        .flags = alloc::flags::leave_nonzeroed,
    });

    if (!res.okay()) [[unlikely]] {
        return alloc::result_t<T&>(res.err());
    }

    uint8_t* object_start = res.release_ref().data_maybe_defined();

    __ok_assert(uintptr_t(object_start) % alignof(T) == 0,
                "Misaligned memory produced by allocator");

    T* made = reinterpret_cast<T*>(object_start);

    *made = factory(std::forward<args_t>(args)...);

    return alloc::result_t<T&>(*made);
}

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
        case error_t::no_value:
            return fmt::format_to(ctx.out(), "alloc::error::no_value");
        case error_t::usage:
            return fmt::format_to(ctx.out(), "alloc::error::usage");
        case error_t::couldnt_expand_in_place:
            return fmt::format_to(ctx.out(),
                                  "alloc::error::couldnt_expand_in_place");
        }
    }
};
#endif

#endif
