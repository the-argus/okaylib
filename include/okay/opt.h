#ifndef __OKAYLIB_OPT_H__
#define __OKAYLIB_OPT_H__

#include <cstdint>
#include <functional>
#include <utility>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
#include "okay/detail/traits/is_instance.h"
#include "okay/slice.h"
#endif

namespace ok {
template <typename payload_t> class opt_t
{
  public:
    // type constraints
    static_assert((!std::is_reference_v<payload_t> &&
                   std::is_nothrow_destructible_v<payload_t>) ||
                      (std::is_reference_v<payload_t> &&
                       std::is_lvalue_reference_v<payload_t>),
                  "Optional type must be either nothrow destructible or an "
                  "lvalue reference type.");

#ifndef OKAYLIB_OPTIONAL_ALLOW_POINTERS
    static_assert(!std::is_pointer_v<payload_t>,
                  "Attempt to create an optional pointer. Pointers are already "
                  "optional. Maybe make an optional reference instead?");
#endif

    static constexpr bool is_reference = std::is_lvalue_reference_v<payload_t>;

    static_assert(is_reference || !std::is_const_v<payload_t>,
                  "Attempt to create optional with const non-reference type. "
                  "This has no effect, remove the const.");

#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
    static constexpr bool is_slice =
        ok::detail::is_instance<payload_t, ok::slice_t>{};
#endif

  private:
    union raw_optional_t
    {
        payload_t some;
        uint8_t none;
        inline ~raw_optional_t() OKAYLIB_NOEXCEPT {}
    };

    struct members_valuetype_t
    {
        bool has_value = false;
        raw_optional_t value{.none = 0};
    };

    struct members_ref_t
    {
        std::remove_reference_t<payload_t>* pointer = nullptr;
    };

#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
    struct members_slice_t
    {
        size_t elements;
        // propagate const-ness of slice
        std::conditional_t<std::is_const_v<typename payload_t::type>,
                           const typename payload_t::type*,
                           typename payload_t::type*>
            data = nullptr;
    };

    using members_t = std::conditional_t<
        is_reference, members_ref_t,
        std::conditional_t<is_slice, members_slice_t, members_valuetype_t>>;
#else
    using members_t = typename std::conditional<is_reference, members_ref_t,
                                                members_valuetype_t>::type;
#endif

    members_t m;

  public:
    [[nodiscard]] inline bool has_value() const OKAYLIB_NOEXCEPT
    {
        if constexpr (is_reference) {
            return m.pointer != nullptr;
        } else
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
            return m.data != nullptr;
        } else
#endif
        {
            return m.has_value;
        }
    }

    /// Extract the inner value of the optional, or abort the program. Check
    /// has_value() before calling this.
    [[nodiscard]] inline payload_t& value() & OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        if constexpr (is_reference) {
            return *m.pointer;
        } else
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
            return *reinterpret_cast<payload_t*>(&m);
        } else
#endif
        {
            return std::ref(m.value.some);
        }
    }

    [[nodiscard]] inline payload_t&& value() && OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        if constexpr (is_reference) {
            return *m.pointer;
        } else
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
            return std::move(*reinterpret_cast<payload_t*>(&m));
        } else
#endif
        {
            return std::move(m.value.some);
        }
    }

    inline const payload_t& value() const& OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        if constexpr (is_reference) {
            return *m.pointer;
        } else
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
            return *reinterpret_cast<const payload_t*>(&m);
        } else
#endif
        {
            return std::ref(m.value.some);
        }
    }

    /// Call destructor of internal type, or just reset it if it doesnt have one
    inline void reset() OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            return;
        }

        if constexpr (is_reference) {
            m.pointer = nullptr;
        } else
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
            m.data = nullptr;
            m.elements = 0;
        } else
#endif
        {
            m.value.some.~T();
            m.has_value = false;
        }
    }

    /// Types can be constructed directly in to the optional
    template <typename... args_t>
    inline void emplace(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        static_assert(
            !is_reference,
            "Reference types cannot be emplaced, assign them instead.");
        static_assert(
            std::is_constructible_v<payload_t, args_t...>,
            "opt_t payload type is not constructible with given arguments");
        if (has_value()) [[unlikely]] {
            reset();
        }
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        if constexpr (is_slice) {
            payload_t* slicelocation = reinterpret_cast<payload_t*>(&m);
            new (slicelocation) payload_t(args...);
        } else {
#endif
            new (&m.value.some)
                payload_t(std::forward<decltype(args)>(args)...);
            m.has_value = true;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        }
#endif
    }

    inline constexpr opt_t() OKAYLIB_NOEXCEPT {}
    inline ~opt_t() OKAYLIB_NOEXCEPT
    {
        if constexpr (is_reference) {
            m.pointer = nullptr;
        } else

#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
            m.data = nullptr;
            m.elements = 0;
        } else
#endif
        {
            reset();
            m.has_value = false;
        }
    }

    /// Able to assign a moved type if the type is moveable
    template <typename maybe_t = payload_t>
    inline constexpr opt_t&
    operator=(typename std::enable_if_t<
              !is_reference &&
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
                  !is_slice &&
#endif
                  std::is_constructible_v<maybe_t, maybe_t&&>,
              maybe_t>&& something) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_constructible_v<maybe_t, maybe_t&&>,
                      "Attempt to move type into an opt, but the move "
                      "constructor of the type can throw an exception.");
        if (m.has_value) {
            m.value.some.~maybe_t();
        }
        new (&m.value.some) maybe_t(std::move(something));
        m.has_value = true;
        return *this;
    }

    template <typename maybe_t = payload_t>
    inline constexpr opt_t(typename std::enable_if_t<
                           !is_reference &&
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
                               !is_slice &&
#endif
                               std::is_constructible_v<maybe_t, maybe_t&&>,
                           maybe_t>&& something) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_constructible_v<maybe_t, maybe_t&&>,
                      "Attempt to move type into an opt, but the move "
                      "constructor of the type can throw an exception.");
        new (&m.value.some) maybe_t(std::move(something));
        m.has_value = true;
    }

    /// Copyable types can also be assigned into their optionals
    template <typename maybe_t = payload_t>
    inline constexpr opt_t&
    operator=(const typename std::enable_if_t<
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
              is_slice ||
#endif
                  (!is_reference &&
                   std::is_constructible_v<maybe_t, const maybe_t&>),
              maybe_t>& something) OKAYLIB_NOEXCEPT
    {
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        if constexpr (is_slice) {
            m.data = something.data();
            m.elements = something.size();
            return *this;
        } else {
#endif
            if (m.has_value) {
                m.value.some.~maybe_t();
            }
            new (&m.value.some) maybe_t(something);
            m.has_value = true;
            return *this;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        }
#endif
    }

    // copy constructor
    template <typename maybe_t = payload_t>
    inline constexpr opt_t(
        const typename std::enable_if_t<
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            is_slice ||
#endif
                (!is_reference &&
                 std::is_constructible_v<maybe_t, const maybe_t&>),
            maybe_t>& something) OKAYLIB_NOEXCEPT
    {
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        if constexpr (is_slice) {
            m.data = something.data();
            m.elements = something.size();
        } else {
#endif
            new (std::addressof(m.value.some)) payload_t(something);
            m.has_value = true;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        }
#endif
    }

    template <typename... args_t>
    inline constexpr opt_t(
        std::enable_if_t<(
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
                             !is_slice &&
#endif
                             !is_reference &&
                             std::is_constructible_v<payload_t, args_t...>),
                         std::in_place_t>,
        args_t&&... args) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_constructible_v<payload_t, args_t...>,
                      "Attempt to construct item in-place in optional but the "
                      "constructor invoked can throw exceptions.");
        new (std::addressof(m.value.some))
            payload_t(std::forward<args_t>(args)...);
        m.has_value = true;
    }

    /// Optional containing a reference type can be directly constructed from
    /// the reference type
    template <typename maybe_t = payload_t>
    inline constexpr opt_t(typename std::enable_if_t<is_reference, maybe_t>
                               something) OKAYLIB_NOEXCEPT
    {
        m.pointer = std::addressof(something);
    }

    /// Reference types can be assigned to an optional to overwrite it.
    template <typename maybe_t = payload_t>
    inline constexpr opt_t&
    operator=(typename std::enable_if_t<is_reference, maybe_t> something)
        OKAYLIB_NOEXCEPT
    {
        m.pointer = &something;
        return *this;
    }

    inline constexpr explicit operator bool() noexcept { return has_value(); }

    /// NOTE: References are not able to use the == overload because
    /// it would not be clear whether it was a strict comparison or not. (ie is
    /// it comparing the address or the contents of the thing at the address?)

    /// EQUALS: Compare an optional to another optional of the same type
    template <typename this_t>
    inline constexpr friend bool
    operator==(const typename std::enable_if_t<
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
                   is_slice ||
#endif
                       (!is_reference && std::is_same_v<this_t, opt_t>),
                   this_t>& self,
               const this_t& other) OKAYLIB_NOEXCEPT
    {
        if (!self.has_value()) {
            return !other.has_value();
        } else {
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
                return !other.has_value()
                           ? false
                           : *reinterpret_cast<const payload_t*>(&self.m) ==
                                 *reinterpret_cast<const payload_t*>(&other.m);
            } else {
#endif
                return !other.has_value()
                           ? false
                           : self.m.value.some == other.m.value.some;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            }
#endif
        }
    }

    /// EQUALS: Compare an optional to something of its contained type
    template <typename maybe_t = payload_t>
    inline constexpr bool
    operator==(const std::enable_if_t<
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
               is_slice ||
#endif
                   (!is_reference && std::is_same_v<maybe_t, payload_t>),
               maybe_t>& other) const OKAYLIB_NOEXCEPT
    {
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        if constexpr (is_slice) {
            return !has_value() ? false
                                : *reinterpret_cast<payload_t*>(&m) == other;
        } else {
#endif
            return !has_value() ? false : m.value.some == other;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        }
#endif
    }

    /// NOT EQUALS: Compare an optional to another optional of the same type
    template <typename this_t>
    inline constexpr friend bool
    operator!=(const typename std::enable_if_t<
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
                   is_slice ||
#endif
                       (!is_reference && std::is_same_v<this_t, opt_t>),
                   this_t>& self,
               const this_t& other) OKAYLIB_NOEXCEPT
    {
        if (!self.has_value()) {
            return other.has_value();
        } else {
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            if constexpr (is_slice) {
                return !other.has_value()
                           ? true
                           : *reinterpret_cast<payload_t*>(&self.m) !=
                                 *reinterpret_cast<payload_t*>(&other.m);
            } else {
#endif
                return !other.has_value()
                           ? true
                           : self.m.value.some != other.m.value.some;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
            }
#endif
        }
    }

    /// NOT EQUALS: Compare an optional to something of its contained type
    template <typename maybe_t = payload_t>
    inline constexpr bool
    operator!=(const std::enable_if_t<
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
               is_slice ||
#endif
                   (!is_reference && std::is_same_v<maybe_t, payload_t>),
               maybe_t>& other) const OKAYLIB_NOEXCEPT
    {
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        if constexpr (is_slice) {
            return !has_value() ? true
                                : *reinterpret_cast<payload_t*>(&m) != other;
        } else {
#endif
            return !has_value() ? true : m.value.some != other;
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
        }
#endif
    }

    // Strict comparison for an optional reference: return true if the optional
    // reference is pointing at the item passed in.
    /// ONLY VALID FOR REFERENCE-TYPE OPTIONALS
    template <typename maybe_t = payload_t>
    inline constexpr bool strict_compare(
        const std::enable_if_t<
            is_reference && std::is_same_v<maybe_t, payload_t>, maybe_t>& other)
        const OKAYLIB_NOEXCEPT
    {
        if (!has_value())
            return false;
        return std::addressof(other) == m.pointer;
    }

    /// Loose comparison: compare the thing the optional reference is pointing
    /// to to the item passed in. They do not have to literally be the same
    /// object.
    /// ONLY VALID FOR REFERENCE-TYPE OPTIONALS
    template <typename maybe_t = payload_t>
    inline constexpr bool loose_compare(
        const std::enable_if_t<is_reference &&
                                   (std::is_same_v<maybe_t, payload_t>),
                               maybe_t>& other) const OKAYLIB_NOEXCEPT
    {
        if (!has_value())
            return false;
        return other == *m.pointer;
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_t>;
#endif
};
} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename payload_t> struct fmt::formatter<ok::opt_t<payload_t>>
{
    static_assert(
        fmt::is_formattable<payload_t>::value,
        "Attempt to format an optional whose content is not formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();

        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");

        // just immediately return the iterator to the ending valid character
        return it;
    }

    format_context::iterator format(const ok::opt_t<payload_t>& optional,
                                    format_context& ctx) const
    {
        if (optional.has_value()) {
            if constexpr (ok::opt_t<payload_t>::is_reference) {
                return fmt::format_to(ctx.out(), "{}", *optional.m.pointer);
            } else
#ifndef OKAYLIB_NO_SMALL_OPTIONAL_SLICE
                if constexpr (ok::opt_t<payload_t>::is_slice) {
                return fmt::format_to(
                    ctx.out(), "{}",
                    *reinterpret_cast<const payload_t*>(&optional.m));
            } else
#endif
            {
                return fmt::format_to(ctx.out(), "{}", optional.m.value.some);
            }
        }
        return fmt::format_to(ctx.out(), "null");
    }
};
#endif

#endif
