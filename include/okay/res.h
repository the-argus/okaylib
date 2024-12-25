#ifndef __OKAYLIB_RES_H__
#define __OKAYLIB_RES_H__

#include "okay/detail/abort.h"
#include "okay/detail/traits/is_status_enum.h"
#include <cstdint>
#include <type_traits>
#include <utility> // std::in_place_t

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {
template <typename payload_t, typename enum_t> class res_t
{
  public:
    static_assert(
        std::is_lvalue_reference_v<payload_t> ||
            (std::is_nothrow_destructible_v<payload_t> &&
             // type must be either moveable or trivially copyable, otherwise it
             // cant be returned from/moved out of a function
             (std::is_move_constructible_v<payload_t> ||
              std::is_trivially_copy_constructible_v<payload_t>)),
        "Invalid type passed to res's first template argument. The type "
        "must either be a lvalue reference, trivially copy constructible, or "
        "nothrow move constructible.");

    static_assert(
        detail::is_status_enum<enum_t>(),
        "Bad enum errorcode type provided to res. Make sure it is only a "
        "byte in size, and that the okay entry is = 0.");

  private:
    /// wrapper struct which just exits so that we can put reference types
    /// inside of the union
    struct wrapper_t
    {
        payload_t item;
        wrapper_t() = delete;
        inline constexpr wrapper_t(payload_t item) OKAYLIB_NOEXCEPT
            : item(item) {};
    };

    static constexpr bool is_reference = std::is_lvalue_reference_v<payload_t>;

    union raw_optional_t
    {
        std::conditional_t<is_reference, wrapper_t, payload_t> some;
        uint8_t none;
        ~raw_optional_t() OKAYLIB_NOEXCEPT {}
    };

    struct members_t
    {
        enum_t status;
        raw_optional_t value{.none = 0};
    };

    members_t m;

  public:
    [[nodiscard]] inline constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return m.status == enum_t::okay;
    }

    [[nodiscard]] inline constexpr enum_t err() const OKAYLIB_NOEXCEPT
    {
        return m.status;
    }

    [[nodiscard]] inline std::conditional_t<is_reference, payload_t,
                                            payload_t&&>
    release() OKAYLIB_NOEXCEPT
    {
        if (!okay()) [[unlikely]] {
            OK_ABORT();
        }

        m.status = enum_t::result_released;
        if constexpr (is_reference) {
            return m.value.some.item;
        } else {
            return std::move(m.value.some);
        }
    }

    /// Return a reference to the data inside the result. This reference
    /// becomes invalid when the result is destroyed or moved. If the result is
    /// an error, this aborts the program. Check okay() before calling this
    /// function. Do not try to call release() or release_ref() again, after
    /// calling release() or release_ref() once, the result is invalidated.
    template <typename maybe_t = payload_t>
        [[nodiscard]] inline typename std::enable_if_t<!is_reference, maybe_t>&
        release_ref() &
        OKAYLIB_NOEXCEPT
    {
        if (!okay()) [[unlikely]] {
            OK_ABORT();
        }
        m.status = enum_t::result_released;
        return m.value.some;
    }

    template <typename maybe_t = payload_t, typename... args_t>
    inline constexpr res_t(
        std::enable_if_t<!is_reference &&
                             std::is_constructible_v<payload_t, args_t...>,
                         std::in_place_t>,
        args_t&&... args) noexcept
    {
        static_assert(std::is_nothrow_constructible_v<payload_t, args_t...>,
                      "Attempt to construct in place but constructor invoked "
                      "can throw exceptions.");
        m.status = enum_t::okay;
        new (&m.value.some) payload_t(std::forward<args_t>(args)...);
    }

    /// if T is a reference type, then you can construct a result from it
    template <typename maybe_t = payload_t>
    inline constexpr res_t(typename std::enable_if_t<is_reference, maybe_t>
                               success) OKAYLIB_NOEXCEPT
    {
        m.status = enum_t::okay;
        new (&m.value.some) wrapper_t(success);
    }

    /// Wrapped type can moved into a result
    template <typename maybe_t = payload_t>
    inline constexpr res_t(
        typename std::enable_if_t<!is_reference &&
                                      std::is_move_constructible_v<payload_t>,
                                  maybe_t>&& success) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_move_constructible_v<payload_t>,
                      "Attempt to use move constructor, but it throws and "
                      "function is marked noexcept.");
        m.status = enum_t::okay;
        new (&m.value.some) payload_t(std::move(success));
    }

    /// A statuscode can also be implicitly converted to a result
    inline constexpr res_t(enum_t failure) OKAYLIB_NOEXCEPT
    {
        if (failure == enum_t::okay) [[unlikely]] {
            OK_ABORT();
        }
        m.status = failure;
    }

    /// Copy constructor only available if the wrapped type is trivially
    /// copy constructible.
    template <typename this_t = res_t>
    inline constexpr res_t(
        const typename std::enable_if_t<
            (is_reference ||
             std::is_trivially_copy_constructible_v<payload_t>) &&
                std::is_same_v<this_t, res_t>,
            this_t>& other) OKAYLIB_NOEXCEPT
    {
        if (other.okay()) {
            if constexpr (is_reference) {
                m.value.some = other.m.value.some.item;
            } else {
                m.value.some = other.m.value.some;
            }
        }
        m.status = other.m.status;
    }

    // Result cannot be assigned to, only constructed and then released.
    res_t& operator=(const res_t& other) = delete;
    res_t& operator=(res_t&& other) = delete;

    /// Move construction of result, requires that T is a reference or that it's
    /// move constructible.
    template <typename this_t = res_t>
    inline constexpr res_t(
        typename std::enable_if_t<(is_reference ||
                                   std::is_move_constructible_v<payload_t>) &&
                                      std::is_same_v<this_t, res_t>,
                                  this_t>&& other) OKAYLIB_NOEXCEPT
    {
        if (other.okay()) {
            if constexpr (is_reference) {
                m.value.some.item = other.m.value.some.item;
            } else {
                m.value.some = std::move(other.m.value.some);
            }
        }
        m.status = other.m.status;
        // make it an error to access a result after it has been moved into
        // another
        other.m.status = enum_t::result_released;
    }

    inline ~res_t() OKAYLIB_NOEXCEPT
    {
        if constexpr (!is_reference) {
            if (okay()) {
                m.value.some.~T();
            }
        }
        m.status = enum_t::result_released;
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<res_t>;
#endif

  private:
    inline constexpr explicit res_t() OKAYLIB_NOEXCEPT
    {
        m.status = enum_t::okay;
    }
};
} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename payload_t, typename enum_t>
struct fmt::formatter<ok::res_t<payload_t, enum_t>>
{
    using res_template_t = ok::res_t<payload_t, enum_t>;

    static_assert(
        fmt::is_formattable<payload_t>::value,
        "Attempt to format an ok::res_t whose contents are not formattable.");
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const res_template_t& result,
                                    format_context& ctx) const
    {
        if (result.okay()) {
            if constexpr (res_template_t::is_reference) {
                return fmt::format_to(ctx.out(), "{}",
                                      result.m.value.some.item);
            } else {
                return fmt::format_to(ctx.out(), "{}", result.m.value.some);
            }
        } else {
            if constexpr (fmt::is_formattable<
                              decltype(result.m.status)>::value) {
                return fmt::format_to(ctx.out(), "err {}", result.m.status);
            } else {
                return fmt::format_to(
                    ctx.out(), "err {}",
                    std::underlying_type_t<decltype(result.m.status)>(
                        result.m.status));
            }
        }
    }
};
#endif

#endif
