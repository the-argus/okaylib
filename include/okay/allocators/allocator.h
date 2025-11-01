#ifndef __OKAYLIB_ALLOCATORS_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_ALLOCATOR_H__

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility> // for std::forward

// pulls in res, then slice and opt
#include "okay/construct.h"
#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/math/ordering.h"
#include "okay/slice.h"
#include "okay/tuple.h"

namespace ok {

class allocator_t;
class nonthreadsafe_allocator_t;

namespace alloc {
enum class error : uint8_t
{
    success,
    no_value,
    // NOTE: oom has semantic meaning which is "the given request does not work
    // on me, but may work on another allocator with more memory." OOM will
    // never be returned if the request will always be invalid for allocators of
    // the same type. This is important when considering what happens when you
    // request more than a block allocator's maximum allocation size: it returns
    // OOM, not unsupported, because a block allocator with a bigger blocksize
    // may be able to handle the request.
    oom,
    unsupported,
    usage,
    couldnt_expand_in_place,
    platform_failure,
};

inline constexpr size_t default_align = alignof(::max_align_t);

template <typename T> using result_t = ok::res<T, error>;

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
    // Can this allocator sometimes reallocate in-place, and, crucially, can it
    // know whether the in-place reallocation will succeed before performing it?
    // (C's realloc cannot do this, because you can't know whether the
    // in-placeness succeeded until after calling it)
    // If this flag is off, then passing in_place_orelse_fail will always return
    // error::unsupported.
    can_predictably_realloc_in_place            = 0b000001,
    can_expand_back                             = 0b000010,
    can_expand_front                            = 0b000100,
    // Whether shrinking provides any benefits for this allocator
    can_reclaim                                 = 0b001000,
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
    bool leave_nonzeroed = false;
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
            flags::shrink_front | flags::expand_front;

        // cannot expand or shrink front
        return !(flags & forbidden) && !memory.is_empty() &&
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
    [[nodiscard]] constexpr ok::tuple<size_t, size_t, size_t>
    calculate_new_preferred_size() const OKAYLIB_NOEXCEPT
    {
        ok::tuple<size_t, size_t, size_t> out;
        auto& amount_changed_back = ok::get<0>(out);
        auto& amount_changed_front = ok::get<1>(out);
        auto& new_size = ok::get<2>(out);
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

template <typename T, typename allocator_impl_t = ok::nonthreadsafe_allocator_t>
struct owned
{
    friend class ok::nonthreadsafe_allocator_t;

    [[nodiscard]] constexpr T& operator*() const
    {
        __ok_assert(m_allocation, "nullptr dereference on owned<T>");
        return *reinterpret_cast<T*>(m_allocation);
    }
    [[nodiscard]] constexpr T* operator->() const
    {
        __ok_assert(m_allocation, "nullptr dereference on owned<T>");
        return reinterpret_cast<T*>(m_allocation);
    }
    [[nodiscard]] constexpr T& value() const
    {
        __ok_assert(m_allocation, "nullptr dereference on owned<T>");
        return *reinterpret_cast<T*>(m_allocation);
    }

    owned() = delete;
    owned(const owned&) = delete;
    owned& operator=(const owned&) = delete;

    constexpr owned(owned&& other)
        : m_allocation(std::exchange(other.m_allocation, nullptr)),
          m_allocator(other.m_allocator)
    {
    }

    constexpr owned& operator=(owned&& other)
    {
        destroy();
        m_allocation = std::exchange(other.m_allocation, nullptr);
        m_allocator = other.m_allocator;
    }

    [[nodiscard]] constexpr T& release()
    {
        __ok_assert(m_allocation, "attempt to release null owned<T>");
        return *static_cast<T*>(std::exchange(m_allocation, nullptr));
    }

    constexpr ~owned() { destroy(); }

  private:
    constexpr void destroy() noexcept
    {
        if (m_allocation) {
            m_allocator->deallocate(m_allocation);
        }
    }

    constexpr owned(T& allocation, allocator_impl_t& allocator) noexcept
        : m_allocation(ok::addressof(allocation)),
          m_allocator(ok::addressof(allocator))
    {
    }

    void* m_allocation;
    allocator_impl_t* m_allocator;
};

} // namespace alloc

class nonthreadsafe_allocate_interface_t
{
  public:
    [[nodiscard]] constexpr alloc::result_t<bytes_t>
    allocate(const alloc::request_t& request) OKAYLIB_NOEXCEPT
    {
        // one way for request to be invalid
        if (request.num_bytes == 0) [[unlikely]] {
            __ok_usage_error(false,
                             "Attempt to allocate 0 bytes from allocator.");
            return alloc::error::unsupported;
        }
        return impl_allocate(request);
    }

    /// Version of make which simply returns an allocation error or a reference
    /// to the newly allocated thing. Does not accept failing constructors.
    /// Meant for simple code that uses arenas and doesn't need to manually free
    /// every allocation.
    template <typename T = detail::deduced_t, typename... args_t>
    [[nodiscard]] constexpr auto
    make_non_owning(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        using analysis = decltype(detail::analyze_construction<args_t...>());
        using deduced = typename analysis::associated_type;
        constexpr bool is_constructed_type_deduced =
            std::is_same_v<T, detail::deduced_t>;
        static_assert(
            // either analysis found an associated_type, or we were given one
            // explicitly
            !std::is_void_v<deduced> || !is_constructed_type_deduced,
            "Type deduction failed for the given allocator.make() call. You "
            "may "
            "need to provide the type explicitly, e.g. "
            "`allocator.make<int>(0)`");
        using actual_t =
            std::conditional_t<is_constructed_type_deduced, deduced, T>;

        using return_type = alloc::result_t<actual_t&>;

        static_assert(
            !std::is_void_v<actual_t> &&
                !std::is_same_v<actual_t, detail::deduced_t>,
            "Unable to deduce the type you're trying to make with this "
            "allocator. The arguments to the constructor may be invalid, "
            "or you may just need to specify the returned type when "
            "calling: `allocator.make_non_owning<MyType>(...)`.");

        static_assert(is_infallible_constructible_c<actual_t, args_t...>,
                      "Cannot call make_non_owning with the given arguments, "
                      "there is no matching infallible constructor.");

        auto allocation_result = allocate(alloc::request_t{
            .num_bytes = sizeof(actual_t),
            .alignment = alignof(actual_t),
            .leave_nonzeroed = true,
        });
        if (!allocation_result.is_success()) [[unlikely]] {
            return return_type(allocation_result.status());
        }
        uint8_t* object_start =
            allocation_result.unwrap().unchecked_address_of_first_item();

        __ok_assert(uintptr_t(object_start) % alignof(actual_t) == 0,
                    "Misaligned memory produced by allocator");

        actual_t* made = reinterpret_cast<actual_t*>(object_start);
        ok::make_into_uninitialized<actual_t>(*made,
                                              std::forward<args_t>(args)...);
        return return_type(*made);
    }

  protected:
    [[nodiscard]] virtual alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT = 0;
};

class arena_allocator_restore_scope_interface_t
{
    struct allocator_restore_point_t
    {
        friend class ok::arena_allocator_restore_scope_interface_t;

        allocator_restore_point_t(const allocator_restore_point_t&) = delete;
        allocator_restore_point_t&
        operator=(const allocator_restore_point_t&) = delete;
        allocator_restore_point_t(allocator_restore_point_t&&) = delete;
        allocator_restore_point_t&
        operator=(allocator_restore_point_t&&) = delete;

        constexpr ~allocator_restore_point_t()
        {
            m_allocator.restore_scope(m_handle);
        }

        allocator_restore_point_t() = delete;

      private:
        constexpr allocator_restore_point_t(
            ok::arena_allocator_restore_scope_interface_t& allocator)
            : m_allocator(allocator), m_handle(allocator.new_scope())
        {
        }

        void* m_handle;
        ok::arena_allocator_restore_scope_interface_t& m_allocator;
    };

    [[nodiscard]] constexpr allocator_restore_point_t
    begin_scope() OKAYLIB_NOEXCEPT
    {
        return allocator_restore_point_t(*this);
    }

  protected:
    [[nodiscard]] virtual void* new_scope() OKAYLIB_NOEXCEPT = 0;
    virtual void restore_scope(void* handle) OKAYLIB_NOEXCEPT = 0;
};

class nonthreadsafe_allocator_t
    : virtual public nonthreadsafe_allocate_interface_t
{
  public:
    [[nodiscard]] alloc::feature_flags features() const OKAYLIB_NOEXCEPT
    {
        return impl_features();
    }

    void deallocate(void* memory) OKAYLIB_NOEXCEPT
    {
        if (memory) [[likely]]
            impl_deallocate(memory);
    }

    [[nodiscard]] alloc::result_t<bytes_t>
    reallocate(const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT
    {
        if (!options.is_valid()) [[unlikely]] {
            __ok_assert(false, "invalid reallocate_request_t");
            return alloc::error::usage;
        }
        return impl_reallocate(options);
    }

    [[nodiscard]] alloc::result_t<alloc::reallocation_extended_t>
    reallocate_extended(const alloc::reallocate_extended_request_t& options)
        OKAYLIB_NOEXCEPT
    {
        if (!options.is_valid()) [[unlikely]] {
            __ok_assert(false, "invalid reallocate_extended_request_t");
            return alloc::error::usage;
        }
        return impl_reallocate_extended(options);
    }

    template <typename T = detail::deduced_t, typename... args_t>
    [[nodiscard]] decltype(auto) make(args_t&&... args) OKAYLIB_NOEXCEPT;

  protected:
    [[nodiscard]] virtual alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT = 0;

    /// NOTE: the implementation of this is not required to check for nullptr,
    /// that should be handled by the deallocate() wrapper that users actually
    /// call
    virtual void impl_deallocate(void* memory) OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] virtual alloc::result_t<bytes_t> impl_reallocate(
        const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] virtual alloc::result_t<alloc::reallocation_extended_t>
    impl_reallocate_extended(const alloc::reallocate_extended_request_t&
                                 options) OKAYLIB_NOEXCEPT = 0;
};

/// Threadsafe allocate interface
class allocate_interface_t : virtual public nonthreadsafe_allocate_interface_t
{};

/// Threadsafe allocator
class allocator_t : public allocate_interface_t,
                    virtual public nonthreadsafe_allocator_t
{};

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
    allocator_impl_t& allocator,
    const alloc::reallocate_extended_request_t& options)
{
    static_assert(detail::is_derived_from_c<allocator_impl_t, allocator_t>,
                  "Cannot call reallocate on the given type- it is not derived "
                  "from ok::allocator_t.");

    __ok_assert(options.flags & flags::in_place_orelse_fail,
                "Attempt to call reallocate_in_place_orelse_keep_old_nocopy "
                "but the given options do not specify in_place_orelse_fail");

    // try to do it in place if possible
    result_t<reallocation_extended_t> reallocation_res =
        allocator.reallocate_extended(options);
    if (reallocation_res.is_success()) {
        auto& reallocation = reallocation_res.unwrap();
        return potentially_in_place_reallocation_t{
            .memory = reallocation.memory,
            .bytes_offset_front = reallocation.bytes_offset_front,
            .was_in_place = true,
        };
    }

    using flags_underlying_t = std::underlying_type_t<flags>;
    constexpr auto mask =
        static_cast<flags_underlying_t>(flags::leave_nonzeroed);
    const bool leave_nonzeroed =
        static_cast<flags_underlying_t>(options.flags) & mask;

    auto [bytes_offset_back, bytes_offset_front, new_size] =
        options.calculate_new_preferred_size();

    // propagate leave_nonzeroed request to allocate call
    result_t<bytes_t> res = allocator.allocate(alloc::request_t{
        .num_bytes = new_size,
        .leave_nonzeroed = leave_nonzeroed,
    });

    if (!res.is_success()) [[unlikely]]
        return res.status();

    return potentially_in_place_reallocation_t{
        .memory = res.unwrap(),
        .was_in_place = false,
    };
}
} // namespace alloc

/// Calls the destructor of an object and then frees its memory
template <typename T, typename allocator_impl_t>
    requires ok::detail::is_derived_from_c<allocator_impl_t,
                                           ok::nonthreadsafe_allocator_t>
constexpr void destroy_and_free(allocator_impl_t& ally,
                                T& object) OKAYLIB_NOEXCEPT
{
    static_assert(
        detail::is_derived_from_c<allocator_impl_t, allocator_t>,
        "Type given in first argument does not inherit from ok::allocator_t.");
    static_assert(is_std_destructible_c<T>,
                  "The destructor you're trying to call with ok::free is "
                  "not " __ok_msg_nothrow "destructible");
    static_assert(
        !std::is_array_v<T> ||
            is_std_destructible_c<std::remove_reference_t<decltype(object[0])>>,
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
    ally.deallocate(ok::addressof(object));
}

template <typename T, typename... args_t>
[[nodiscard]] decltype(auto)
ok::nonthreadsafe_allocator_t::make(args_t&&... args) OKAYLIB_NOEXCEPT
{
    using analysis = decltype(detail::analyze_construction<args_t...>());
    using deduced = typename analysis::associated_type;
    constexpr bool is_constructed_type_deduced =
        std::is_same_v<T, detail::deduced_t>;
    static_assert(
        // either analysis found an associated_type, or we were given one
        // explicitly
        !std::is_void_v<deduced> || !is_constructed_type_deduced,
        "Type deduction failed for the given allocator.make() call. You may "
        "need to provide the type explicitly, e.g. `allocator.make<int>(0)`");
    using actual_t =
        std::conditional_t<is_constructed_type_deduced, deduced, T>;

    static_assert(!std::is_void_v<actual_t> &&
                      !std::is_same_v<actual_t, detail::deduced_t>,
                  "Unable to deduce the type you're trying to make with this "
                  "allocator. The arguments to the constructor may be invalid, "
                  "or you may just need to specify the returned type when "
                  "calling: `allocator.make<MyType>(...)`.");

    auto allocation_result = allocate(alloc::request_t{
        .num_bytes = sizeof(actual_t),
        .alignment = alignof(actual_t),
        .leave_nonzeroed = true,
    });

    using initialization_return_type =
        decltype(make_into_uninitialized<actual_t>(
            std::declval<actual_t&>(), std::forward<args_t>(args)...));
    constexpr bool returns_status = !std::is_void_v<initialization_return_type>;

    if (!allocation_result.is_success()) [[unlikely]] {
        if constexpr (returns_status) {
            static_assert(
                is_convertible_to_c<
                    alloc::error,
                    typename initialization_return_type::enum_type>,
                "Cannot call potentially failing constructor from "
                "allocator_t::make() if the error type returned from the "
                "constructor does not define a conversion from alloc::error.");
            return res<alloc::owned<actual_t>,
                       typename initialization_return_type::enum_type>(
                allocation_result.status());
        } else {
            return alloc::result_t<alloc::owned<actual_t>>(
                allocation_result.status());
        }
    }

    uint8_t* object_start =
        allocation_result.unwrap_unchecked().unchecked_address_of_first_item();

    __ok_assert(uintptr_t(object_start) % alignof(actual_t) == 0,
                "Misaligned memory produced by allocator");

    actual_t* made = reinterpret_cast<actual_t*>(object_start);

    if constexpr (returns_status) {
        auto result = make_into_uninitialized<actual_t>(
            *made, std::forward<args_t>(args)...);
        static_assert(detail::is_instance_c<decltype(result), status>);
        return res<alloc::owned<actual_t>,
                   typename decltype(result)::enum_type>(
            alloc::owned<actual_t>(*made, *this));
    } else {
        make_into_uninitialized<actual_t>(*made, std::forward<args_t>(args)...);
        return alloc::result_t<alloc::owned<actual_t>>(
            alloc::owned<actual_t>(*made, *this));
    }
}
} // namespace ok

#if defined(OKAYLIB_USE_FMT)
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
        case error_t::success:
            return fmt::format_to(ctx.out(), "alloc::error::success");
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
        case error_t::platform_failure:
            return fmt::format_to(ctx.out(), "alloc::error::platform_failure");
        }
    }
};
#endif

#endif
