#ifndef __OKAYLIB_ANYSTATUS_H__
#define __OKAYLIB_ANYSTATUS_H__

#include "okay/allocators/allocator.h"
#include "okay/ctti/ctti.h"
#include "okay/detail/noexcept.h"
#include <cstdint>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {

class abstract_status_t
{
  public:
    virtual uint64_t typehash() const noexcept = 0;
    virtual bool is_success() const noexcept = 0;
    // it is assumed that an abstract status is dynamically allocated in some
    // way but if not, this function may return null to indicate that owning
    // pointers don't need to free the pointed-to memory. Normally it is not
    // the job of an object to know its own allocator, but otherwise the pointer
    // would be carried by the owning wrapper type, so this is done in an effort
    // to reduce the size of the res<..., ...> on the stack.
    virtual opt<ok::allocator_t&> allocator() noexcept = 0;
    virtual ~abstract_status_t() = 0;

    /// Given a derived class, construct an instance of it within memory
    /// allocated by the given allocator, using provided make arguments.
    /// If the constructed type returns something other than the allocator
    /// used here to create it, that may result in a memory leak as the
    /// anystatus_t owning it will not free it properly.
    template <typename derived_t, typename... args_t>
        requires(std::derived_from<derived_t, abstract_status_t> &&
                 is_infallible_constructible_c<derived_t, args_t...>)
    static res<abstract_status_t&, alloc::error>
    make_success_derived(ok::allocator_t& allocator, args_t&&... args)
    {
        res<abstract_status_t&, alloc::error> out =
            allocator.make_non_owning(std::forward<args_t>(args)...);
#ifndef NDEBUG
        if (out.is_success()) {
            opt maybe_allocator = out.unwrap_unchecked().allocator();
            __ok_assert(
                !maybe_allocator.has_value() ||
                    ok::addressof(maybe_allocator.ref_unchecked()) ==
                        ok::addressof(allocator),
                "Status constructed by make_success_derived returns a "
                "different allocator than the one it was allocated with.");
        }
#endif
        return out;
    }
};

/// Anystatus can be one of:
///   - A reference to an abstract_status_t
///   - A typehash (8 bytes) and an arbitrary unint32_t (usually an enum value)
///   - A boolean which has no information besides whether it indicates success
///     or failure.
class anystatus_t
{
  public:
    enum class variant
    {
        abstract,
        status_enum,
        boolean,
    };

    enum class tag
    {
        success,
        failure,
    };

  private:
    uint64_t typehash;
    // this is guaranteed to be at least 8 byte aligned so the pointer's last
    // three bits are free. The first bit refers to whether or not this is
    // an error. The second bit refers to whether or not this is a boolean.
    // The third bit refers to whether or not this is an enum value (ie. the
    // top 32 bits are active). If this is a pointer to an abstract_status_t,
    // all the bottom bits will be zero (ie. the ptr will be evenly divisible
    // by 8).
    abstract_status_t* abstract_ptr;

    // TODO: support 32 bit platforms
    static_assert(sizeof(abstract_status_t*) == 8,
                  "Only platforms where pointers are 8 bytes are supported by "
                  "okaylib's anystatus_t.");

    static constexpr uint64_t is_enum_mask = 0b0100;
    static constexpr uint64_t is_bool_mask = 0b0010;
    static constexpr uint64_t is_failure_mask = 0b0001;
    static constexpr uint64_t is_any_mask = 0b0111;

    [[nodiscard]] constexpr variant
    variant_inner(uint64_t& bottom_three_bits) const OKAYLIB_NOEXCEPT
    {
        bottom_three_bits = std::bit_cast<uint64_t>(abstract_ptr) & is_any_mask;
        if (bottom_three_bits) {
            // either an enum or boolean
            if (bottom_three_bits & is_enum_mask) {
                __ok_assert(!(bottom_three_bits & is_bool_mask),
                            "Invalid/corrupted anystatus_t");
                return variant::status_enum;
            } else {
                __ok_assert(bottom_three_bits & is_bool_mask,
                            "Invalid/corrupted anystatus_t");
                return variant::boolean;
            }
        } else {
            return variant::abstract;
        }
    }

    constexpr void destroy() noexcept
    {
        // we are only owning if our contained type is a pointer to something
        if (this->type() != variant::abstract)
            return;

        auto allocator = abstract_ptr->allocator();

        // call virtual destructor
        abstract_ptr->~abstract_status_t();

        // deallocate actual memory
        if (allocator) {
            // NOTE: telling the allocator that this object is just the vptr
            // but that is probably not true. However allocators are
            // designed to remember size information if it is relevant so
            // there's no need to report the correct size here.
            allocator.ref_unchecked().deallocate(
                ok::raw_slice(*std::bit_cast<uint8_t*>(abstract_ptr), 8));
        }
    }

    constexpr static inline const char* default_hint =
        "anystatus_t::tag construction";

  public:
    constexpr anystatus_t(tag success, const char* hint = default_hint) noexcept
        : abstract_ptr(std::bit_cast<abstract_status_t*>(
              is_bool_mask & uint64_t(success == tag::failure))),
          typehash(std::bit_cast<uint64_t>(hint))
    {
    }

    template <status_object status_t>
        requires(std::derived_from<status_t, abstract_status_t>)
    constexpr anystatus_t(status_t& status) noexcept
        : abstract_ptr(ok::addressof(status)), typehash(status.typehash())
    {
    }

    template <status_enum status_t>
    constexpr anystatus_t(status_t status) noexcept
        : abstract_ptr(std::bit_cast<abstract_status_t*>(
              uint64_t(std::underlying_type_t<status_t>(status) << 32) |
              is_enum_mask)),
          typehash(ok::ctti::typehash<status_t>())
    {
        static_assert(sizeof(status_t) <= 4,
                      "anystatus implementation relies on status enums being "
                      "less than four bytes.");
    }

    // cannot copy an anystatus because it may be a pointer to something which
    // cannot be copied
    anystatus_t(const anystatus_t& other) = delete;
    anystatus_t& operator=(const anystatus_t& other) = delete;
    constexpr anystatus_t(anystatus_t&& other) noexcept
        : abstract_ptr(std::exchange(other.abstract_ptr, nullptr)),
          typehash(other.typehash)
    {
    }

    constexpr anystatus_t& operator=(anystatus_t&& other) noexcept
    {
        this->destroy();
        abstract_ptr = std::exchange(other.abstract_ptr, nullptr);
        typehash = other.typehash;
        return *this;
    }

    [[nodiscard]] constexpr static anystatus_t
    make_success(const char* hint) noexcept
    {
        return anystatus_t(tag::success, hint);
    }

    [[nodiscard]] constexpr variant type() const OKAYLIB_NOEXCEPT
    {
        uint64_t dummy;
        return this->variant_inner(dummy);
    }

    /// Check if this status contains the given type (enum, abstract_status_t
    /// reference, or boolean)
    template <typename T> [[nodiscard]] constexpr bool is() const noexcept
    {
        constexpr auto hash = ok::ctti::typehash<T>();
        // if this is a bool, then instead of a typehash there is a char*
        // present
        if (std::bit_cast<uint64_t>(abstract_ptr) & is_bool_mask) {
            return hash == ok::ctti::typehash<bool>();
        }
        return hash == typehash;
    }

    [[nodiscard]] constexpr bool is_success() const OKAYLIB_NOEXCEPT
    {
        uint64_t bottom_three_bits;
        switch (this->variant_inner(bottom_three_bits)) {
        case variant::abstract:
            return this->abstract_ptr->is_success();
            break;
        case variant::status_enum:
            return (std::bit_cast<uint64_t>(abstract_ptr) ^
                    bottom_three_bits) == 0;
            break;
        case variant::boolean:
            return !(std::bit_cast<uint64_t>(abstract_ptr) & is_failure_mask);
            break;
        }
    }

    constexpr ~anystatus_t() noexcept { this->destroy(); }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<anystatus_t>;
#endif
};

static_assert(status_object<anystatus_t>);

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <> struct fmt::formatter<ok::anystatus_t::variant>
{
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    format_context::iterator format(const ok::anystatus_t::variant& tag,
                                    format_context& ctx) const
    {
        switch (tag) {
        case ok::anystatus_t::variant::abstract:
            return fmt::format_to(ctx.out(), "anystatus_t::variant::abstract");
            break;
        case ok::anystatus_t::variant::status_enum:
            return fmt::format_to(ctx.out(),
                                  "anystatus_t::variant::status_enum");
            break;
        case ok::anystatus_t::variant::boolean:
            return fmt::format_to(ctx.out(), "anystatus_t::variant::boolean");
            break;
        }
    }
};

template <> struct fmt::formatter<ok::anystatus_t>
{
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    format_context::iterator format(const ok::anystatus_t& anystatus,
                                    format_context& ctx) const
    {
        if (anystatus.is_success()) {
            return fmt::format_to(ctx.out(), "anystatus_t<success, {}>",
                                  anystatus.type());
        } else {
            return fmt::format_to(ctx.out(), "anystatus_t<failure, {}>",
                                  anystatus.type());
        }
    }
};
#endif

#endif
