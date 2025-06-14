#ifndef __OKAYLIB_ERROR_H__
#define __OKAYLIB_ERROR_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/construct_at.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/cloneable.h"
#include "okay/detail/traits/error_traits.h"
#include "okay/opt.h"

#include <type_traits>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {
template <status_enum enum_t> class status
{
  private:
    enum_t m_status;

  public:
    using enum_type = enum_t;

    [[nodiscard]] constexpr bool is_success() const noexcept
    {
        return m_status == enum_t::success;
    }

    [[nodiscard]] constexpr enum_t as_enum() const OKAYLIB_NOEXCEPT
    {
        return m_status;
    }

    [[nodiscard]] static constexpr status make_success() noexcept
    {
        return status(enum_t::success);
    }

    constexpr status(enum_t failure) OKAYLIB_NOEXCEPT { m_status = failure; }

    constexpr operator enum_t() noexcept { return m_status; }

    // must explicitly initialize status with enum value
    status() = delete;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<status>;
#endif
};

namespace detail {
template <typename callable_t, typename success_t, typename status_t>
concept and_then_callable = requires(callable_t c) {
    detail::is_instance_v<decltype(c(std::declval<success_t>())), res>;
    std::is_same_v<typename decltype(c(std::declval<success_t>()))::status_type,
                   status_t>;
};
template <typename callable_t, typename status_t>
concept and_then_callable_noargs = requires(callable_t c) {
    detail::is_instance_v<decltype(c()), res>;
    std::is_same_v<typename decltype(c())::status_type, status_t>;
};
template <typename callable_t, typename status_t>
concept convert_error_callable = requires(callable_t c) {
    status_object<decltype(c(std::declval<status_t>()))> ||
        status_enum<decltype(c(std::declval<status_t>()))>;
};
template <typename callable_t>
concept convert_error_callable_noargs = requires(callable_t c) {
    status_object<decltype(c())> || status_enum<decltype(c())>;
};
template <typename callable_t, typename success_t, typename status_t>
concept transform_callable = requires(callable_t c) {
    // it is valid to form res with the returned type and the same error type
    !std::is_void_v<res<decltype(c(std::declval<success_t>())), status_t>>;
};
template <typename callable_t, typename status_t>
concept transform_callable_noargs = requires(callable_t c) {
    // it is valid to form res with the returned type and the same error type
    !std::is_void_v<res<decltype(c()), status_t>>;
};
} // namespace detail

template <typename success_t, status_type status_t>
__OK_RES_REQUIRES_CLAUSE class res<
    success_t, status_t, std::enable_if_t<!std::is_reference_v<success_t>>>
{
    static_assert(!std::is_void_v<success_t> && !std::is_void_v<status_t>,
                  "Res does not support void as template arguments.");
    detail::uninitialized_storage_t<success_t> m_success;
    status_t m_status;

    template <typename... args_t>
    constexpr void emplace_nodestroy(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        ok::construct_at(ok::addressof(m_success.value),
                         std::forward<args_t>(args)...);
    }

  public:
    using success_type = success_t;
    using status_type = status_t;

    // use compiler generated copy/move if one is present in the
    // uninitialized_storage_t (meaning the success_t is trivialy)
    constexpr res(const res& other) OKAYLIB_NOEXCEPT
        requires(std::is_copy_constructible_v<decltype(m_success)>)
    = default;
    constexpr res& operator=(const res& other) OKAYLIB_NOEXCEPT
        requires(std::is_copy_assignable_v<decltype(m_success)>)
    = default;
    constexpr res(res&& other) OKAYLIB_NOEXCEPT
        requires std::is_move_constructible_v<decltype(m_success)>
    = default;
    constexpr res& operator=(res&& other) OKAYLIB_NOEXCEPT
        requires std::is_move_assignable_v<decltype(m_success)>
    = default;

    constexpr res(res&& other) OKAYLIB_NOEXCEPT
        requires(!std::is_move_constructible_v<decltype(m_success)> &&
                 std::is_move_constructible_v<success_t>)
        : m_status(std::move(other.m_status))
    {
        if (other.is_success()) {
            this->emplace_nodestroy(std::move(other.unwrap_unchecked()));
        }
    }
    constexpr res& operator=(res&& other) OKAYLIB_NOEXCEPT
        requires(!std::is_move_assignable_v<decltype(m_success)> &&
                 std::is_move_assignable_v<success_t> &&
                 std::is_move_constructible_v<success_t>)
    {
        if (other.is_success()) {
            if (this->is_success()) {
                this->unwrap_unchecked() = std::move(other.unwrap_unchecked());
            } else {
                this->emplace_nodestroy(std::move(other.unwrap_unchecked()));
            }
        }
        m_status = std::move(other.m_status);
        return *this;
    }

    template <typename other_success_t, typename other_status_t>
        requires(std::is_convertible_v<const other_success_t&, success_t> &&
                 std::is_convertible_v<const other_status_t&, status_t>)
    constexpr res(const res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other.is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(std::is_convertible_v<other_success_t&, success_t> &&
                 std::is_convertible_v<other_status_t&, status_t>)
    constexpr res(res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other.is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(std::is_convertible_v<other_success_t &&, success_t> &&
                 std::is_convertible_v<other_status_t &&, status_t>)
    constexpr res(res<other_success_t, other_status_t>&& other)
        : m_status(std::move(other.status()))
    {
        if (other->is_success()) {
            this->emplace_nodestroy(std::move(other.unwrap_unchecked()));
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_std_constructible_v<success_t, const other_success_t&> &&
                 is_std_constructible_v<status_t, const other_status_t&> &&
                 (!std::is_convertible_v<const other_status_t&, status_t> ||
                  !std::is_convertible_v<const other_success_t&, success_t>))
    explicit constexpr res(const res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other->is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_std_constructible_v<success_t, other_success_t&> &&
                 is_std_constructible_v<status_t, other_status_t&> &&
                 (!std::is_convertible_v<other_status_t&, status_t> ||
                  !std::is_convertible_v<other_success_t&, success_t>))
    explicit constexpr res(res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other->is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_std_constructible_v<success_t, other_success_t &&> &&
                 is_std_constructible_v<status_t, other_status_t &&> &&
                 (!std::is_convertible_v<other_status_t &&, status_t> ||
                  !std::is_convertible_v<other_success_t &&, success_t>))
    explicit constexpr res(res<other_success_t, other_status_t>&& other)
        : m_status(std::move(other.status()))
    {
        if (other->is_success()) {
            this->emplace_nodestroy(std::move(other.unwrap_unchecked()));
        }
    }

    constexpr res(status_t status) OKAYLIB_NOEXCEPT : m_status(status)
    {
        bool other_is_success;
        if constexpr (status_object<status_t>)
            other_is_success = status.is_success();
        else
            other_is_success = status == status_t::success;
        if (other_is_success) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(const success_t& success) OKAYLIB_NOEXCEPT
        requires std::is_copy_constructible_v<success_t>
        : m_status(ok::make_success<status_t>()),
          m_success(ok::in_place, success)
    {
    }

    constexpr res(success_t&& success) OKAYLIB_NOEXCEPT
        requires std::is_move_constructible_v<success_t>
        : m_status(ok::make_success<status_t>()),
          m_success(ok::in_place, std::move(success))
    {
    }

    template <typename... args_t>
        requires is_infallible_constructible_v<success_t, args_t...>
    constexpr res(ok::in_place_t, args_t&&... args) OKAYLIB_NOEXCEPT
        : m_status(ok::make_success<status_t>()),
          m_success(ok::in_place, std::forward<args_t>(args)...)
    {
    }
    // converting constructor
    // TODO: make this respect implicitness/explicitness of the constructor of
    // success_t. ATM explicitly convertible contents can become implicitly
    // convertible if wrapped in a res.
    template <typename incoming_t>
    constexpr res(incoming_t&& incoming) OKAYLIB_NOEXCEPT
        requires is_std_constructible_v<success_t, decltype(incoming)>
        : m_status(ok::make_success<status_t>()),
          m_success(ok::in_place, std::forward<incoming_t>(incoming))
    {
    }

    // NOTE: res only implements try_clone if the status can be cloned without
    // error.
    constexpr auto try_clone() const OKAYLIB_NOEXCEPT
        requires try_cloneable<success_t> && cloneable<status_t>
    {
        if (this->is_success()) {
            auto cloned = ok::try_clone(this->unwrap_unchecked());
            if (cloned.is_success()) {
                return res<res, try_clone_status_t<success_t>>(
                    ok::in_place, std::move(cloned.unwrap_unchecked()));
            } else {
                return res<res, try_clone_status_t<success_t>>(
                    std::move(cloned.status()));
            }
        } else {
            return res<res, try_clone_status_t<success_t>>(
                ok::in_place, ok::clone(this->status()));
        }
    }

    constexpr res clone() const OKAYLIB_NOEXCEPT
        requires cloneable<success_t> && cloneable<status_t>
    {
        if (this->is_success()) {
            return res(ok::clone(this->unwrap_unchecked()));
        } else {
            return res(ok::clone(this->status()));
        }
    }

    // NOTE: res will only implement try_clone_into() if its success type
    // is try_cloneable and its status type is just cloneable.
    constexpr try_clone_status_t<success_t>
    try_clone_into(res& dest) const& OKAYLIB_NOEXCEPT
        requires try_cloneable<success_t> && cloneable<status_t>
    {
        const auto set_other_status = [this, dest] {
            // clone our status and move it into the other's status
            if constexpr (std::is_move_assignable_v<status_t>) {
                dest.m_status = std::move(ok::clone(this->status()));
            } else {
                // emulate move assignment with destruction and construction
                dest.m_status.~status_t();
                ok::construct_at(ok::addressof(dest.m_status),
                                 std::move(ok::clone(this->status())));
            }
        };

        if (this->is_success()) {
            const bool other_was_success = dest.is_success();
            set_other_status();
            if (other_was_success) {
                return ok::try_clone_into(this->unwrap_unchecked(),
                                          dest.unwrap_unchecked());
            } else {
                auto res = ok::try_clone(this->unwrap_unchecked());
                if (res.is_success()) {
                    dest.emplace_nodestroy(std::move(res.unwrap_unchecked()));
                }
                return res.status();
            }

        } else {
            if (dest.is_success()) {
                dest.m_success.value.~success_t();
            }

            set_other_status();

            return ok::make_success<try_clone_status_t<success_t>>();
        }
    }

    constexpr void clone_into(res& dest) const& OKAYLIB_NOEXCEPT
        requires cloneable<success_t> && cloneable<status_t>
    {
        // update state of other success value
        if (this->is_success()) {
            if (dest.is_success()) {
                ok::clone_into(this->unwrap_unchecked(),
                               dest.unwrap_unchecked());
            } else {
                ok::construct_at(ok::addressof(dest.m_success),
                                 ok::clone(this->unwrap_unchecked()));
            }
        } else {
            if (dest.is_success()) {
                dest.m_success.value.~success_t();
            }
        }

        // clone our status and move it into the other's status
        if constexpr (std::is_move_assignable_v<status_t>) {
            dest.m_status = std::move(ok::clone(this->status()));
        } else {
            // emulate move assignment with destruction and construction
            dest.m_status.~status_t();
            ok::construct_at(ok::addressof(dest.m_status),
                             std::move(ok::clone(this->status())));
        }
    }

    [[nodiscard]] constexpr bool is_success() const OKAYLIB_NOEXCEPT
    {
        if constexpr (status_enum<status_t>) {
            return m_status == status_t::success;
        } else {
            return m_status.is_success();
        }
    }

    [[nodiscard]] constexpr const status_t& status() const& OKAYLIB_NOEXCEPT
    {
        return m_status;
    }

    [[nodiscard]] constexpr status_t&& status() && OKAYLIB_NOEXCEPT
    {
        return std::move(m_status);
    }

    [[nodiscard]] constexpr opt<success_t&> to_opt() & OKAYLIB_NOEXCEPT
    {
        if (!this->is_success())
            return nullopt;

        return this->unwrap_unchecked();
    }

    [[nodiscard]] constexpr opt<const success_t&>
    to_opt() const& OKAYLIB_NOEXCEPT
    {
        if (!this->is_success())
            return nullopt;

        return this->unwrap_unchecked();
    }

    [[nodiscard]] constexpr opt<success_t&&> to_opt() && OKAYLIB_NOEXCEPT
    {
        if (!this->is_success())
            return nullopt;

        return std::move(this->unwrap_unchecked());
    }

    [[nodiscard]] constexpr success_t& unwrap() & OKAYLIB_NOEXCEPT
    {
        if (!this->is_success()) [[unlikely]]
            __ok_abort(
                "Attempt to unwrap success value of a res which is error.");

        return this->unwrap_unchecked();
    }

    [[nodiscard]] constexpr const success_t& unwrap() const& OKAYLIB_NOEXCEPT
    {
        if (!this->is_success()) [[unlikely]]
            __ok_abort(
                "Attempt to unwrap success value of a res which is error.");

        return this->unwrap_unchecked();
    }

    [[nodiscard]] constexpr success_t&& unwrap() && OKAYLIB_NOEXCEPT
    {
        if (!this->is_success()) [[unlikely]]
            __ok_abort(
                "Attempt to unwrap success value of a res which is error.");

        return std::move(this->unwrap_unchecked());
    }

    /// Unsafe access into res, potentially accessing uninitialized memory
    /// if the contents is an error. Check is_success() before calling this.
    [[nodiscard]] constexpr success_t& unwrap_unchecked() & OKAYLIB_NOEXCEPT
    {
        __ok_assert(this->is_success(), "Bad access to result.");
        return this->m_success.value;
    }

    /// Unsafe access into res, potentially accessing uninitialized memory
    /// if the contents is an error. Check is_success() before calling this.
    [[nodiscard]] constexpr const success_t&
    unwrap_unchecked() const& OKAYLIB_NOEXCEPT
    {
        __ok_assert(this->is_success(), "Bad access to result.");
        return this->m_success.value;
    }

    /// Unsafe access into res, potentially accessing uninitialized memory
    /// if the contents is an error. Check is_success() before calling this.
    [[nodiscard]] constexpr success_t&& unwrap_unchecked() && OKAYLIB_NOEXCEPT
    {
        __ok_assert(this->is_success(), "Bad access to result.");
        return std::move(this->m_success.value);
    }

    /// Move-construct the success value out of the res, or return an
    /// alternative value if this res is an error. Only available on rvalue
    /// res, because doing this leaves the success value in a moved-out state.
    [[nodiscard]] constexpr success_t unwrap_or(success_t alternative) &&
        OKAYLIB_NOEXCEPT
            requires(std::is_move_constructible_v<success_t>)
    {
        if (this->is_success()) {
            return std::move(this->unwrap_unchecked());
        } else {
            return alternative;
        }
    }

    /// Copy out the success value from the res, or return an alternative if
    /// this res is a failure.
    [[nodiscard]] constexpr success_t
    copy_or(const success_t& alternative) const OKAYLIB_NOEXCEPT
        requires(std::is_copy_constructible_v<success_t>)
    {
        if (this->is_success()) {
            return this->unwrap_unchecked();
        } else {
            return alternative;
        }
    }

    template <detail::and_then_callable<success_t&&, status_t> callable_t>
        [[nodiscard]] constexpr auto and_then(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires std::is_move_constructible_v<status_t>
    {
        using rettype = decltype(c(this->unwrap_unchecked()));
        if (this->is_success()) {
            return c(std::move(this->unwrap_unchecked()));
        } else {
            return rettype(std::move(this->status()));
        }
    }

    template <detail::and_then_callable_noargs<status_t&&> callable_t>
        [[nodiscard]] constexpr auto and_then(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires std::is_move_constructible_v<status_t>
    {
        using rettype = decltype(c());
        if (this->is_success()) {
            return c();
        } else {
            return rettype(std::move(this->status()));
        }
    }

    template <detail::convert_error_callable<status_t&&> callable_t>
        [[nodiscard]] constexpr auto convert_error(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires std::is_move_constructible_v<success_t>
    {
        using new_status = decltype(c(std::move(this->status())));
        if (this->is_success()) {
            return res<success_t, new_status>(
                std::move(this->unwrap_unchecked()));
        } else {
            return res<success_t, new_status>(c(std::move(this->status())));
        }
    }

    template <detail::convert_error_callable_noargs callable_t>
        [[nodiscard]] constexpr auto convert_error(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires std::is_move_constructible_v<success_t>
    {
        using new_status = decltype(c());
        if (this->is_success()) {
            return res<success_t, new_status>(
                std::move(this->unwrap_unchecked()));
        } else {
            return res<success_t, new_status>(c());
        }
    }

    template <detail::transform_callable<success_t&&, status_t> callable_t>
        [[nodiscard]] constexpr auto transform(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires std::is_move_constructible_v<success_t>
    {
        using new_success = decltype(c(std::move(this->unwrap_unchecked())));
        if (this->is_success()) {
            return res<new_success, status_t>(
                c(std::move(this->unwrap_unchecked())));
        } else {
            return res<new_success, status_t>(std::move(this->status()));
        }
    }

    template <detail::transform_callable_noargs<status_t> callable_t>
        [[nodiscard]] constexpr auto transform(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires std::is_move_constructible_v<success_t>
    {
        using new_success = decltype(c());
        if (this->is_success()) {
            return res<new_success, status_t>(c());
        } else {
            return res<new_success, status_t>(std::move(this->status()));
        }
    }

    ~res()
        requires(std::is_trivially_destructible_v<success_t>)
    = default;

    ~res()
        requires(!std::is_trivially_destructible_v<success_t>)
    {
        if (this->is_success()) {
            m_success.value.~success_t();
        }
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<res>;
#endif
};

template <typename success_t, status_type status_t>
__OK_RES_REQUIRES_CLAUSE class res<
    success_t, status_t, std::enable_if_t<std::is_reference_v<success_t>>>
{
    static_assert(!std::is_void_v<status_t>,
                  "Res does not support void as template arguments.");

    using value_type = std::remove_reference_t<success_t>;

    value_type* m_success;
    status_t m_status;

  public:
    constexpr res(const res& other) noexcept = default;
    constexpr res& operator=(const res& other) noexcept = default;
    constexpr res(res&& other) noexcept = default;
    constexpr res& operator=(res&& other) noexcept = default;

    constexpr res(status_t status) OKAYLIB_NOEXCEPT : m_status(status)
    {
        bool other_is_success;
        if constexpr (status_object<status_t>)
            other_is_success = status.is_success();
        else
            other_is_success = status == status_t::success;
        if (other_is_success) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(success_t success) OKAYLIB_NOEXCEPT
        : m_status(ok::make_success<status_t>()),
          m_success(ok::addressof(success))
    {
    }

    [[nodiscard]] constexpr bool is_success() const OKAYLIB_NOEXCEPT
    {
        if constexpr (status_enum<status_t>) {
            return m_status == status_t::success;
        } else {
            return m_status.is_success();
        }
    }

    [[nodiscard]] constexpr const status_t& status() const& OKAYLIB_NOEXCEPT
    {
        return m_status;
    }

    [[nodiscard]] constexpr status_t&& status() && OKAYLIB_NOEXCEPT
    {
        return std::move(m_status);
    }

    [[nodiscard]] constexpr opt<success_t> to_opt() const OKAYLIB_NOEXCEPT
    {
        if (!this->is_success())
            return nullopt;
        return this->unwrap_unchecked();
    }

    [[nodiscard]] constexpr success_t unwrap() const OKAYLIB_NOEXCEPT
    {
        if (!this->is_success())
            __ok_abort(
                "Attempt to unwrap success value of a res which is error.");
        return this->unwrap_unchecked();
    }

    [[nodiscard]] constexpr success_t unwrap_unchecked() const OKAYLIB_NOEXCEPT
    {
        return static_cast<success_t>(*this->m_success);
    }

    [[nodiscard]] constexpr success_t
    unwrap_or(success_t alternative) OKAYLIB_NOEXCEPT
    {
        if (this->is_success()) {
            return this->unwrap_unchecked();
        } else {
            return alternative;
        }
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<res>;
#endif
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <ok::status_enum enum_t> struct fmt::formatter<ok::status<enum_t>>
{
    using status_template_t = ok::status<enum_t>;

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const status_template_t& status,
                                    format_context& ctx) const
    {
        // TODO: use ctti to get nice typename for enum_t here
        if constexpr (fmt::is_formattable<enum_t>::value) {
            return fmt::format_to(ctx.out(), "[status::{}]", status.as_enum());
        } else {
            if (status.is_success()) {
                return fmt::format_to(ctx.out(), "[status::success]");
            } else {
                return fmt::format_to(
                    ctx.out(), "[status::{}]",
                    std::underlying_type_t<enum_t>(status.as_enum()));
            }
        }
    }
};

template <typename success_t, typename status_t>
    requires(std::is_reference_v<success_t> ||
             fmt::is_formattable<success_t>::value)
struct fmt::formatter<ok::res<success_t, status_t>>
{
    using res_t = ok::res<success_t, status_t>;

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const res_t& result,
                                    format_context& ctx) const
    {
        if (result.is_success()) {
            if constexpr (std::is_reference_v<success_t>) {
                if constexpr (fmt::is_formattable<
                                  std::remove_reference_t<success_t>>::value) {
                    return fmt::format_to(ctx.out(), "{}",
                                          result.unwrap_unchecked());
                } else {
                    // TODO: use ctti to get nice typeof/typename here?
                    return fmt::format_to(ctx.out(), "{:p}",
                                          static_cast<void*>(result.m_success));
                }
            } else {
                return fmt::format_to(ctx.out(), "{}",
                                      result.unwrap_unchecked());
            }
        } else {
            if constexpr (fmt::is_formattable<status_t>::value) {
                return fmt::format_to(ctx.out(), "{}", result.status());
            } else if constexpr (ok::status_enum<status_t>) {
                using enum_int_t = std::underlying_type_t<status_t>;
                return fmt::format_to(ctx.out(), "[FAILURE, CODE {}]",
                                      enum_int_t(result.status()));
            } else {
                return fmt::format_to(ctx.out(), "[FAILURE]");
            }
        }
    }
};

#endif

#endif
