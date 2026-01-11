#ifndef __OKAYLIB_ALLOCATORS_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_ALLOCATOR_H__

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// pulls in res, then slice and opt
#include "okay/construct.h"
#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/type_traits.h"
#include "okay/slice.h"

namespace ok {

class allocator_t;

namespace alloc {
enum class error : uint8_t
{
    success,
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

enum class realloc_flags : uint16_t
{
    // clang-format off
    // This flag suggests that the caller prefers to be reallocated in order to
    // save memory
    try_defragment                  = 0b0001,
    leave_nonzeroed                 = 0b0010,
    // This flag asks the allocator to check if it will be able to reallocate in
    // place before performing it, and fail if not.
    // This is only supported by allocators that
    // can_predictably_realloc_in_place.
    in_place_orelse_fail            = 0b0100,
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
    // Whether shrinking provides any benefits for this allocator
    can_reclaim                                 = 0b000010,
    keeps_destructor_list                       = 0b000100,
    can_restore_scope                           = 0b001000,
    // If set, this allocator does not keep track of the sizes of allocations,
    // and needs its users to do so. usually only not set for low-level
    // allocators like page allocators which are only intended to be used by
    // other allocators.
    needs_accurate_sizehint                     = 0b010000,
    // clang-format on
};

/// Merge two sets of flags.
constexpr realloc_flags operator|(realloc_flags a, realloc_flags b)
{
    using flags = realloc_flags;
    return static_cast<flags>(static_cast<stdc::underlying_type_t<flags>>(a) |
                              static_cast<stdc::underlying_type_t<flags>>(b));
}

constexpr feature_flags operator|(feature_flags a, feature_flags b)
{
    using flags = feature_flags;
    return static_cast<flags>(static_cast<stdc::underlying_type_t<flags>>(a) |
                              static_cast<stdc::underlying_type_t<flags>>(b));
}

/// Check if two sets of flags have anything in common.
constexpr bool operator&(realloc_flags a, realloc_flags b)
{
    using flags = realloc_flags;
    return static_cast<stdc::underlying_type_t<flags>>(a) &
           static_cast<stdc::underlying_type_t<flags>>(b);
}

constexpr bool operator&(feature_flags a, feature_flags b)
{
    using flags = feature_flags;
    return static_cast<stdc::underlying_type_t<flags>>(a) &
           static_cast<stdc::underlying_type_t<flags>>(b);
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
    // minimum size of the memory after reallocating. arraylist may set this to
    // current size + sizeof(T) when appending. Although it is not the optimal
    // size increase, it is the minimum needed to continue without an error.
    size_t new_size_bytes;
    // the optimal new size after reallocation. for an arraylist this would be
    // current size * growth_factor. ignored if shrinking or if zero
    size_t preferred_size_bytes = 0;
    size_t alignment = ok::alloc::default_align;
    alloc::realloc_flags flags = {};

    [[nodiscard]] constexpr bool is_valid() const OKAYLIB_NOEXCEPT
    {
        return !memory.is_empty() &&
               // no attempt to... free the memory?
               (new_size_bytes != 0) &&
               // preferred should be zero OR ( (we're growing OR staying the
               // same size) + preferred is greater than required )
               (preferred_size_bytes == 0 ||
                (new_size_bytes >= memory.size() &&
                 preferred_size_bytes > new_size_bytes));
    }

    [[nodiscard]] constexpr size_t calculate_preferred_size() const noexcept
    {
        return preferred_size_bytes == 0 ? new_size_bytes
                                         : preferred_size_bytes;
    }
};

template <typename T, typename allocator_impl_t = ok::allocator_t> struct owned
{
    friend class ok::allocator_t;

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
        : m_allocation(stdc::exchange(other.m_allocation, nullptr)),
          m_allocator(other.m_allocator)
    {
    }

    constexpr owned& operator=(owned&& other)
    {
        if (&other == this)
            return *this;
        destroy();
        m_allocation = stdc::exchange(other.m_allocation, nullptr);
        m_allocator = other.m_allocator;
    }

    [[nodiscard]] constexpr T& release()
    {
        __ok_assert(m_allocation, "attempt to release null owned<T>");
        return *static_cast<T*>(stdc::exchange(m_allocation, nullptr));
    }

    constexpr ~owned() { destroy(); }

  private:
    constexpr void destroy() noexcept
    {
        if (m_allocation) [[likely]] {
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

class allocator_t
{
  public:
    [[nodiscard]] constexpr alloc::feature_flags
    features() const OKAYLIB_NOEXCEPT
    {
        return impl_features();
    }

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

    /// Deallocate memory, optionally providing size_hint to tell the allocator
    /// how big you think the allocated memory is (only some allocators require
    /// this, so if you don't want to support those then just leave that
    /// parameter as 0)
    constexpr void deallocate(void* memory,
                              size_t size_hint = 0) OKAYLIB_NOEXCEPT
    {
        if (memory) [[likely]]
            impl_deallocate(memory, size_hint);
    }

    [[nodiscard]] constexpr alloc::result_t<bytes_t>
    reallocate(const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT
    {
        if (!options.is_valid()) [[unlikely]] {
            __ok_assert(false, "invalid reallocate_request_t");
            return alloc::error::usage;
        }
        if ((options.flags & alloc::realloc_flags::in_place_orelse_fail) &&
            !(features() &
              alloc::feature_flags::can_predictably_realloc_in_place))
            [[unlikely]] {
            return alloc::error::couldnt_expand_in_place;
        }
        return impl_reallocate(options);
    }

    template <typename T = detail::deduced_t, typename... args_t>
    [[nodiscard]] constexpr decltype(auto)
    make(args_t&&... args) OKAYLIB_NOEXCEPT
        requires detail::is_deduced_constructible_c<T, args_t...>;

    /// If the allocator has keeps_destructor_list, then this will push the
    /// object's destructor to the arena's list of destructors to call when
    /// clearing the allocator. If it errors, then it is up to the caller to
    /// call the destructor.
    ///
    /// If the allocator this points to does not support this operation, this
    /// panics in debug mode and does nothing in release.
    template <typename T>
        requires(!stdc::is_trivially_destructible_v<T>)
    [[nodiscard]] constexpr ok::status<alloc::error>
    arena_push_destructor(T& allocated)
    {
        const auto features = this->features();
        __ok_assert(features & alloc::feature_flags::keeps_destructor_list,
                    "Attempt to push destructor to a non-arena allocator, "
                    "indicates a possible resource leak.");
        if (features & alloc::feature_flags::keeps_destructor_list) {
            constexpr auto destroy = [](void* v) { static_cast<T*>(v)->~T(); };
            return impl_arena_push_destructor({
                .destructor = destroy,
                .object = ok::addressof(allocated),
            });
        }
        return alloc::error::unsupported;
    }

    struct allocator_restore_point_t
    {
        friend class ok::allocator_t;

        allocator_restore_point_t(const allocator_restore_point_t&) = delete;
        allocator_restore_point_t&
        operator=(const allocator_restore_point_t&) = delete;
        allocator_restore_point_t(allocator_restore_point_t&&) = delete;
        allocator_restore_point_t&
        operator=(allocator_restore_point_t&&) = delete;

        constexpr ~allocator_restore_point_t()
        {
            m_allocator.impl_arena_restore_scope(m_handle);
        }

        allocator_restore_point_t() = delete;

      private:
        constexpr allocator_restore_point_t(ok::allocator_t& allocator)
            : m_allocator(allocator), m_handle(allocator.impl_arena_new_scope())
        {
        }

        void* m_handle;
        ok::allocator_t& m_allocator;
    };

    [[nodiscard]] constexpr allocator_restore_point_t
    begin_scope() OKAYLIB_NOEXCEPT
    {
        __ok_assert(features() & alloc::feature_flags::can_restore_scope,
                    "Attempt to call begin_scope() on an allocator which does "
                    "not support scopes. You may have passed something that is "
                    "not an arena to a task expecting an arena.");
        return allocator_restore_point_t(*this);
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
            stdc::is_same_v<T, detail::deduced_t>;
        static_assert(
            // either analysis found an associated_type, or we were given one
            // explicitly
            !stdc::is_void_v<deduced> || !is_constructed_type_deduced,
            "Type deduction failed for the given allocator.make() call. You "
            "may need to provide the type explicitly, e.g. "
            "`allocator.make<int>(0)`");
        using actual_t =
            stdc::conditional_t<is_constructed_type_deduced, deduced, T>;

        using return_type = alloc::result_t<actual_t&>;

        static_assert(
            !stdc::is_void_v<actual_t> &&
                !stdc::is_same_v<actual_t, detail::deduced_t>,
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

        if constexpr (!stdc::is_trivially_destructible_v<actual_t>) {
            if (features() & alloc::feature_flags::keeps_destructor_list) {
                const auto err = arena_push_destructor(*made);
                if (!ok::is_success(err)) {
                    deallocate(object_start);
                    return return_type(err);
                }
            }
        }

        ok::make_into_uninitialized<actual_t>(*made,
                                              stdc::forward<args_t>(args)...);

        return return_type(*made);
    }

  protected:
    struct destructor_t
    {
        void (*destructor)(void* object);
        void* object;
    };

    [[nodiscard]] constexpr virtual alloc::result_t<bytes_t>
    impl_allocate(const alloc::request_t&) OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] constexpr virtual alloc::feature_flags
    impl_features() const OKAYLIB_NOEXCEPT = 0;

    constexpr virtual ok::status<alloc::error>
    impl_arena_push_destructor(destructor_t) OKAYLIB_NOEXCEPT
    {
#ifndef NDEBUG
        if (!stdc::is_constant_evaluated()) {
            __ok_assert(
                false,
                "called an unimplemented impl_arena_push_destructor. "
                "probably indicates marking a type as being an arena with "
                "features(), but then not providing an override for the "
                "impl_arena_push_destructor function.");
        }
#endif
        return alloc::error::unsupported;
    }

    [[nodiscard]] constexpr virtual void*
    impl_arena_new_scope() OKAYLIB_NOEXCEPT
    {
#ifndef NDEBUG
        if (!stdc::is_constant_evaluated()) {
            __ok_assert(
                false,
                "called unimplemented impl_arena_new_scope, indicates an "
                "allocator which is marked as being an arena but does not "
                "implement new_scope and restore_scope as it needs to.");
        }
#endif
        return nullptr;
    }

    constexpr virtual void
    impl_arena_restore_scope(void* handle) OKAYLIB_NOEXCEPT
    {
#ifndef NDEBUG
        if (!stdc::is_constant_evaluated()) {
            __ok_assert(
                false,
                "called unimplemented impl_arena_new_scope, indicates an "
                "allocator which is marked as being an arena but does not "
                "implement new_scope and restore_scope as it needs to.");
        }
#endif
    }

    /// NOTE: the implementation of this is not required to check for nullptr,
    /// that should be handled by the deallocate() wrapper that users actually
    /// call
    constexpr virtual void
    impl_deallocate(void* memory, size_t size_hint) OKAYLIB_NOEXCEPT = 0;

    [[nodiscard]] constexpr virtual alloc::result_t<bytes_t> impl_reallocate(
        const alloc::reallocate_request_t& options) OKAYLIB_NOEXCEPT = 0;
};

template <typename T>
concept allocator_c = requires(
    const T& const_allocator, T& allocator, const alloc::request_t& request,
    const alloc::reallocate_request_t& reallocate_request, void* voidptr) {
    { allocator.allocate(request) } -> ok::same_as_c<alloc::result_t<bytes_t>>;

    // incomplete test for make_non_owning
    {
        allocator.template make_non_owning<int>(1)
    } -> ok::same_as_c<alloc::result_t<int&>>;

    {
        allocator.template make_non_owning<int>()
    } -> ok::same_as_c<alloc::result_t<int&>>;

    requires !(requires {
        { const_allocator.allocate(request) };
    });

    { const_allocator.features() } -> ok::same_as_c<alloc::feature_flags>;

    { allocator.deallocate(voidptr) } -> ok::is_void_c;

    {
        allocator.reallocate(reallocate_request)
    } -> ok::same_as_c<alloc::result_t<bytes_t>>;

    // make sure nonconst functions do not work on const version
    requires !(requires {
        { const_allocator.deallocate(voidptr) };
        { const_allocator.reallocate(reallocate_request) };
    });

    // approximate make() function
    // the owned value can optionally provide specialization for the
    // allocator, or it can just return an owned pointing to a polymorphic
    // allocator_t
    requires(requires {
                {
                    allocator.template make<int>(1)
                } -> ok::same_as_c<alloc::result_t<alloc::owned<int, T>>>;
                {
                    allocator.template make<int>()
                } -> ok::same_as_c<alloc::result_t<alloc::owned<int, T>>>;
            }) || (requires {
                {
                    allocator.template make<int>(1)
                } -> ok::same_as_c<alloc::result_t<alloc::owned<int>>>;
                {
                    allocator.template make<int>()
                } -> ok::same_as_c<alloc::result_t<alloc::owned<int>>>;
            });
};

namespace alloc {

struct potentially_in_place_reallocation_t
{
    bytes_t memory;
    bool was_in_place;
};

/// Wrapper around reallocate and allocate which tries to do
/// in_place_orelse_fail and then allocates a separate buffer of the correct
/// size if in_place reallocation failed.
template <allocator_c allocator_impl_t = ok::allocator_t>
[[nodiscard]] constexpr result_t<potentially_in_place_reallocation_t>
reallocate_in_place_orelse_keep_old_nocopy(
    allocator_impl_t& allocator, const alloc::reallocate_request_t& options)
{
    __ok_assert(options.flags & realloc_flags::in_place_orelse_fail,
                "Attempt to call reallocate_in_place_orelse_keep_old_nocopy "
                "but the given options do not specify in_place_orelse_fail");

    // try to do it in place if possible
    res reallocation_res = allocator.reallocate(options);
    if (reallocation_res.is_success()) {
        auto& reallocation = reallocation_res.unwrap();
        return potentially_in_place_reallocation_t{
            .memory = reallocation,
            .was_in_place = true,
        };
    }

    res res = allocator.allocate(alloc::request_t{
        .num_bytes = options.calculate_preferred_size(),
        .leave_nonzeroed = options.flags & realloc_flags::leave_nonzeroed,
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
template <typename T, allocator_c allocator_impl_t = ok::allocator_t>
constexpr void destroy_and_free(allocator_impl_t& ally,
                                T& object) OKAYLIB_NOEXCEPT
{
    static_assert(is_std_destructible_c<T>,
                  "The destructor you're trying to call with ok::free is "
                  "not " __ok_msg_nothrow "destructible");
    static_assert(!stdc::is_array_v<T> ||
                      is_std_destructible_c<
                          stdc::remove_reference_t<decltype(object[0])>>,
                  "The destructor of items within the given array are "
                  "not " __ok_msg_nothrow " destructible.");
    static_assert(
        !stdc::is_pointer_v<T>,
        "Reference to pointer passed to ok::free(). This is a potential "
        "indication of array decay. Call allocator.deallocate() directly if "
        "this is actually a pointer.");

    if constexpr (!stdc::is_array_v<stdc::remove_reference_t<T>>) {
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
[[nodiscard]] constexpr decltype(auto)
ok::allocator_t::make(args_t&&... args) OKAYLIB_NOEXCEPT
    requires detail::is_deduced_constructible_c<T, args_t...>
{
    using analysis = detail::analyze_construction_t<args_t...>;
    using deduced = typename analysis::associated_type;
    constexpr bool is_constructed_type_deduced =
        stdc::is_same_v<T, detail::deduced_t>;
    using actual_t =
        stdc::conditional_t<is_constructed_type_deduced, deduced, T>;

    auto allocation_result = allocate(alloc::request_t{
        .num_bytes = sizeof(actual_t),
        .alignment = alignof(actual_t),
        .leave_nonzeroed = true,
    });

    using initialization_return_type =
        decltype(make_into_uninitialized<actual_t>(
            stdc::declval<actual_t&>(), stdc::forward<args_t>(args)...));
    constexpr bool returns_status =
        !stdc::is_void_v<initialization_return_type>;

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
            *made, stdc::forward<args_t>(args)...);
        static_assert(detail::is_instance_c<decltype(result), status>);
        return res<alloc::owned<actual_t>,
                   typename decltype(result)::enum_type>(
            alloc::owned<actual_t>(*made, *this));
    } else {
        make_into_uninitialized<actual_t>(*made,
                                          stdc::forward<args_t>(args)...);
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
