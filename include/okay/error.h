#ifndef __OKAYLIB_ERROR_H__
#define __OKAYLIB_ERROR_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/invoke.h"
#include "okay/detail/memory.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/cloneable.h"
#include "okay/detail/traits/error_traits.h"
#include "okay/opt.h"

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
/// The purpose of status is to make enums appear to have the same API as status
/// objects (wrapping a status object in a status doesn't make sense. normally
/// objects provide the functions themselves, its just enums cant have members)
template <status_enum_c enum_t> class status
{
  private:
    enum_t m_status;

#if defined(OKAYLIB_TESTING_BACKTRACE)
  public:
    detail_testing::owned_stack_trace_t stacktrace;

  private:
#endif

  public:
    using enum_type = enum_t;

    [[nodiscard]] constexpr bool is_success() const noexcept
    {
        return m_status == enum_t::success;
    }

    constexpr void or_panic() const OKAYLIB_NOEXCEPT
    {
        if (!is_success()) [[unlikely]]
            ::abort();
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

    constexpr bool operator==(const enum_t& enum_rep) const OKAYLIB_NOEXCEPT
    {
        return m_status == enum_rep;
    }

    // must explicitly initialize status with enum value
    status() = delete;

#if defined(OKAYLIB_USE_FMT)
    friend struct fmt::formatter<status>;
#endif
};

namespace detail {
template <typename T, typename E> struct res_accessor_t
{
    static constexpr res<T, E> construct_uninitialized_res() noexcept;

    template <typename... args_t>
        requires ok::is_infallible_constructible_c<E, args_t...>
    static constexpr void emplace_error_nodestroy(res<T, E>& res,
                                                  args_t&&... byte) noexcept;

    static constexpr T&
    get_result_payload_ref_unchecked(res<T, E>& res) noexcept;
};
} // namespace detail

template <typename success_t, status_type_c status_t>
__OK_RES_REQUIRES_CLAUSE class res<
    success_t, status_t, stdc::enable_if_t<!stdc::is_reference_c<success_t>>>
{
    static_assert(!stdc::is_void_v<success_t> && !stdc::is_void_v<status_t>,
                  "Res does not support void as template arguments.");
    detail::uninitialized_storage_t<success_t> m_success;
    status_t m_status;

#if defined(OKAYLIB_TESTING_BACKTRACE)
  public:
    detail_testing::owned_stack_trace_t stacktrace;

  private:
#endif

    template <typename... args_t>
    constexpr void emplace_nodestroy(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        ok::stdc::construct_at(ok::addressof(m_success.value),
                               stdc::forward<args_t>(args)...);
    }

    // called internally in detail::res_accessor_t  and then in construct.h
    // in order to avoid moves when initializing some objects with failing
    // constructors into a res
    constexpr res() = default;

  public:
    using success_type = success_t;
    using status_type = status_t;

    template <typename T, typename E> friend class detail::res_accessor_t;

    // use compiler generated copy/move if one is present in the
    // uninitialized_storage_t (meaning the success_t is trivialy)
    constexpr res(const res& other) OKAYLIB_NOEXCEPT
        requires(stdc::is_copy_constructible_v<decltype(m_success)>)
    = default;
    constexpr res& operator=(const res& other) OKAYLIB_NOEXCEPT
        requires(stdc::is_copy_assignable_v<decltype(m_success)>)
    = default;
    constexpr res(res&& other) OKAYLIB_NOEXCEPT
        requires stdc::is_move_constructible_v<decltype(m_success)>
    = default;
    constexpr res& operator=(res&& other) OKAYLIB_NOEXCEPT
        requires stdc::is_move_assignable_v<decltype(m_success)>
    = default;

    constexpr res(res&& other) OKAYLIB_NOEXCEPT
        requires(!stdc::is_move_constructible_v<decltype(m_success)> &&
                 stdc::is_move_constructible_v<success_t>)
        : m_status(stdc::move(other.m_status))
    {
        if (other.is_success()) {
            this->emplace_nodestroy(stdc::move(other.unwrap_unchecked()));
        }
    }
    constexpr res& operator=(res&& other) OKAYLIB_NOEXCEPT
        requires(!stdc::is_move_assignable_v<decltype(m_success)> &&
                 stdc::is_move_assignable_v<success_t> &&
                 stdc::is_move_constructible_v<success_t>)
    {
        if (other.is_success()) {
            if (this->is_success()) {
                this->unwrap_unchecked() = stdc::move(other.unwrap_unchecked());
            } else {
                this->emplace_nodestroy(stdc::move(other.unwrap_unchecked()));
            }
        }
        m_status = stdc::move(other.m_status);
        return *this;
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_convertible_to_c<const other_success_t&, success_t> &&
                 is_convertible_to_c<const other_status_t&, status_t>)
    constexpr res(const res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other.is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_convertible_to_c<other_success_t&, success_t> &&
                 is_convertible_to_c<other_status_t&, status_t>)
    constexpr res(res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other.is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_convertible_to_c<other_success_t &&, success_t> &&
                 is_convertible_to_c<other_status_t &&, status_t>)
    constexpr res(res<other_success_t, other_status_t>&& other)
        : m_status(stdc::move(other.status()))
    {
        if (other->is_success()) {
            this->emplace_nodestroy(stdc::move(other.unwrap_unchecked()));
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_std_constructible_c<success_t, const other_success_t&> &&
                 is_std_constructible_c<status_t, const other_status_t&> &&
                 (!is_convertible_to_c<const other_status_t&, status_t> ||
                  !is_convertible_to_c<const other_success_t&, success_t>))
    explicit constexpr res(const res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other->is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_std_constructible_c<success_t, other_success_t&> &&
                 is_std_constructible_c<status_t, other_status_t&> &&
                 (!is_convertible_to_c<other_status_t&, status_t> ||
                  !is_convertible_to_c<other_success_t&, success_t>))
    explicit constexpr res(res<other_success_t, other_status_t>& other)
        : m_status(other.status())
    {
        if (other->is_success()) {
            this->emplace_nodestroy(other.unwrap_unchecked());
        }
    }

    template <typename other_success_t, typename other_status_t>
        requires(is_std_constructible_c<success_t, other_success_t &&> &&
                 is_std_constructible_c<status_t, other_status_t &&> &&
                 (!is_convertible_to_c<other_status_t &&, status_t> ||
                  !is_convertible_to_c<other_success_t &&, success_t>))
    explicit constexpr res(res<other_success_t, other_status_t>&& other)
        : m_status(stdc::move(other.status()))
    {
        if (other->is_success()) {
            this->emplace_nodestroy(stdc::move(other.unwrap_unchecked()));
        }
    }

    constexpr res(status_t&& status) OKAYLIB_NOEXCEPT
        requires status_object_c<status_t>
        : m_status(stdc::move(status))
    {
        if (m_status.is_success()) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(const status_t& status) OKAYLIB_NOEXCEPT
        requires status_object_c<status_t> &&
                 stdc::is_copy_constructible_v<status_t>
        : m_status(status)
    {
        if (m_status.is_success()) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    // NOTE: this is a template to defer instantiating ok::status<status_t>
    // until the `requires` clause, otherwise that will cause a compilation
    // error if the status is not a status enum.
    template <typename T>
    constexpr res(const T& status) OKAYLIB_NOEXCEPT
        requires ok::same_as_c<ok::status<status_t>, T> &&
                     status_enum_c<status_t>
        : m_status(status.as_enum())
#if defined(OKAYLIB_TESTING_BACKTRACE)
          ,
          stacktrace(status.stacktrace)
#endif
    {
        if (m_status == status_t::success) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(const status_t& status) OKAYLIB_NOEXCEPT
        requires status_enum_c<status_t>
        : m_status(status)
    {
        if (m_status == status_t::success) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(const success_t& success) OKAYLIB_NOEXCEPT
        requires stdc::is_copy_constructible_v<success_t>
        : m_status(
              ok::make_success<status_t>("Success value was copied into res")),
          m_success(ok::in_place, success)
    {
    }

    constexpr res(success_t&& success) OKAYLIB_NOEXCEPT
        requires stdc::is_move_constructible_v<success_t>
        : m_status(
              ok::make_success<status_t>("Success value was moved into res")),
          m_success(ok::in_place, stdc::move(success))
    {
    }

    template <typename... args_t>
        requires is_infallible_constructible_c<success_t, args_t...>
    constexpr res(ok::in_place_t, args_t&&... args) OKAYLIB_NOEXCEPT
        : m_status(ok::make_success<status_t>(
              "Success value was emplaced into res")),
          m_success(ok::in_place, stdc::forward<args_t>(args)...)
    {
    }
    // converting constructor
    template <typename incoming_t>
    explicit constexpr res(incoming_t&& incoming) OKAYLIB_NOEXCEPT
        requires(is_std_constructible_c<success_t, decltype(incoming)> &&
                 !is_convertible_to_c<decltype(incoming), success_t>)
        : m_status(ok::make_success<status_t>(
              "Success value was copied (explicitly) into res")),
          m_success(ok::in_place, stdc::forward<incoming_t>(incoming))
    {
    }
    template <typename incoming_t>
    constexpr res(incoming_t&& incoming) OKAYLIB_NOEXCEPT
        requires is_convertible_to_c<decltype(incoming), success_t>
        : m_status(ok::make_success<status_t>(
              "Success value was moved (explicitly) into res")),
          m_success(ok::in_place, stdc::forward<incoming_t>(incoming))
    {
    }

    // NOTE: res only implements try_clone if the status can be cloned without
    // error.
    constexpr auto try_clone() const OKAYLIB_NOEXCEPT
        requires try_cloneable_c<success_t> && cloneable_c<status_t>
    {
        if (this->is_success()) {
            auto cloned = ok::try_clone(this->unwrap_unchecked());
            if (cloned.is_success()) {
                return res<res, try_clone_status_t<success_t>>(
                    ok::in_place, stdc::move(cloned.unwrap_unchecked()));
            } else {
                return res<res, try_clone_status_t<success_t>>(
                    stdc::move(cloned.status()));
            }
        } else {
            return res<res, try_clone_status_t<success_t>>(
                ok::in_place, ok::clone(this->status()));
        }
    }

    constexpr res clone() const OKAYLIB_NOEXCEPT
        requires cloneable_c<success_t> && cloneable_c<status_t>
    {
        if (this->is_success()) {
            return res(ok::clone(this->unwrap_unchecked()));
        } else {
            return res(ok::clone(this->status()));
        }
    }

    // NOTE: res will only implement try_clone_into() if its success type
    // is try_cloneable and its status type is just cloneable.
    constexpr auto try_clone_into(res& dest) const& OKAYLIB_NOEXCEPT
        requires try_cloneable_c<success_t> && cloneable_c<status_t>
    {
        using ret_type = try_clone_status_t<success_t>;
        const auto set_other_status = [this, dest] {
            // clone our status and move it into the other's status
            if constexpr (stdc::is_move_assignable_v<status_t>) {
                dest.m_status = stdc::move(ok::clone(this->status()));
            } else {
                // emulate move assignment with destruction and construction
                dest.m_status.~status_t();
                ok::stdc::construct_at(ok::addressof(dest.m_status),
                                       stdc::move(ok::clone(this->status())));
            }
        };

        if (this->is_success()) {
            const bool other_was_success = dest.is_success();
            set_other_status();
            if (other_was_success) {
                return ret_type(ok::try_clone_into(this->unwrap_unchecked(),
                                                   dest.unwrap_unchecked()));
            } else {
                auto res = ok::try_clone(this->unwrap_unchecked());
                if (res.is_success()) {
                    dest.emplace_nodestroy(stdc::move(res.unwrap_unchecked()));
                }
                return ret_type(res.status());
            }

        } else {
            if (dest.is_success()) {
                dest.m_success.value.~success_t();
            }

            set_other_status();

            return ok::make_success<ret_type>(
                "Cloned while in a res but the res was an error, so no clone "
                "occurred.");
        }
    }

    constexpr void clone_into(res& dest) const& OKAYLIB_NOEXCEPT
        requires cloneable_c<success_t> && cloneable_c<status_t>
    {
        // update state of other success value
        if (this->is_success()) {
            if (dest.is_success()) {
                ok::clone_into(this->unwrap_unchecked(),
                               dest.unwrap_unchecked());
            } else {
                ok::stdc::construct_at(ok::addressof(dest.m_success),
                                       ok::clone(this->unwrap_unchecked()));
            }
        } else {
            if (dest.is_success()) {
                dest.m_success.value.~success_t();
            }
        }

        // clone our status and move it into the other's status
        if constexpr (stdc::is_move_assignable_v<status_t>) {
            dest.m_status = stdc::move(ok::clone(this->status()));
        } else {
            // emulate move assignment with destruction and construction
            dest.m_status.~status_t();
            ok::stdc::construct_at(ok::addressof(dest.m_status),
                                   stdc::move(ok::clone(this->status())));
        }
    }

    [[nodiscard]] constexpr bool is_success() const OKAYLIB_NOEXCEPT
    {
        if constexpr (status_enum_c<status_t>) {
            return m_status == status_t::success;
        } else {
            return m_status.is_success();
        }
    }

    constexpr void or_panic() const OKAYLIB_NOEXCEPT
    {
        if (!is_success()) [[unlikely]]
            ::abort();
    }

    [[nodiscard]] constexpr const status_t& status() const& OKAYLIB_NOEXCEPT
        requires status_object_c<status_t>
    {
        return m_status;
    }

    [[nodiscard]] constexpr status_t&& status() && OKAYLIB_NOEXCEPT
            requires status_object_c<status_t>
    {
        return stdc::move(m_status);
    }

    [[nodiscard]] constexpr auto status() const& OKAYLIB_NOEXCEPT
        requires status_enum_c<status_t>
    {
#if defined(OKAYLIB_TESTING_BACKTRACE)
        ok::status<status_t> out(m_status);
        out.stacktrace = this->stacktrace;
        return out;
#else
        return ok::status<status_t>(m_status);
#endif
    }

    [[nodiscard]] constexpr auto status() const&& OKAYLIB_NOEXCEPT
        requires status_enum_c<status_t>
    {
#if defined(OKAYLIB_TESTING_BACKTRACE)
        ok::status<status_t> out(m_status);
        out.stacktrace = this->stacktrace;
        return out;
#else
        return ok::status<status_t>(m_status);
#endif
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

        return stdc::move(this->unwrap_unchecked());
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

        return stdc::move(this->unwrap_unchecked());
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
        return stdc::move(this->m_success.value);
    }

    /// Move-construct the success value out of the res, or return an
    /// alternative value if this res is an error. Only available on rvalue
    /// res, because doing this leaves the success value in a moved-out state.
    [[nodiscard]] constexpr success_t unwrap_or(success_t alternative) &&
        OKAYLIB_NOEXCEPT
            requires(stdc::is_move_constructible_v<success_t>)
    {
        if (this->is_success()) {
            return stdc::move(this->unwrap_unchecked());
        } else {
            return alternative;
        }
    }

    /// Copy out the success value from the res, or return an alternative if
    /// this res is a failure.
    [[nodiscard]] constexpr success_t
    copy_or(const success_t& alternative) const OKAYLIB_NOEXCEPT
        requires(stdc::is_copy_constructible_v<success_t>)
    {
        if (this->is_success()) {
            return this->unwrap_unchecked();
        } else {
            return alternative;
        }
    }

    /// Keeps the same success value but changes the status, if it is an error.
    template <typename callable_t>
        [[nodiscard]] constexpr auto transform_error(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires stdc::is_move_constructible_v<success_t> && requires {
                requires !stdc::is_void_v<decltype(ok::invoke(
                    stdc::forward<callable_t>(c), stdc::move(this->status())))>;
            }
    {
        using new_status =
            decltype(stdc::forward<callable_t>(c)(stdc::move(this->status())));
        if (this->is_success()) {
            return res<success_t, new_status>(
                stdc::move(this->unwrap_unchecked()));
        } else {
            return res<success_t, new_status>(
                stdc::forward<callable_t>(c)(stdc::move(this->status())));
        }
    }

    /// Keeps the same error but changes the success value, if it exists.
    template <typename callable_t>
        [[nodiscard]] constexpr auto transform_value(callable_t&& c) &&
        OKAYLIB_NOEXCEPT
            requires stdc::is_move_constructible_v<success_t> && (requires {
                         requires !stdc::is_void_v<decltype(ok::invoke(
                             stdc::forward<callable_t>(c),
                             stdc::move(this->unwrap())))>;
                     })
    {
        using new_success = decltype(ok::invoke(stdc::forward<callable_t>(c),
                                                stdc::move(this->unwrap())));
        if (this->is_success()) {
            return res<new_success, status_t>(
                ok::invoke(stdc::forward<callable_t>(c),
                           stdc::move(this->unwrap_unchecked())));
        } else {
            return res<new_success, status_t>(stdc::move(this->status()));
        }
    }

    /// Create a new result which may be a (reference into/getter result from) a
    /// particular member of the success value from this existing one, or an
    /// error if this is an error.
    template <typename callable_t>
    [[nodiscard]] constexpr auto
    transform_value(callable_t&& c) const& OKAYLIB_NOEXCEPT
        requires stdc::is_copy_constructible_v<status_t> && requires {
            requires !stdc::is_void_v<decltype(ok::invoke(
                stdc::forward<callable_t>(c), this->unwrap()))>;
        } || requires {
            // version for member functions which take `this` by pointer
            requires !stdc::is_void_v<decltype(ok::invoke(
                stdc::forward<callable_t>(c), ok::addressof(this->unwrap())))>;
        }
    {
        constexpr bool accepts_reference = requires {
            requires !stdc::is_void_v<decltype(ok::invoke(
                stdc::forward<callable_t>(c), this->unwrap()))>;
        };

        if constexpr (accepts_reference) {
            using new_success = decltype(ok::invoke(
                stdc::forward<callable_t>(c), this->unwrap()));
            if (this->is_success()) {
                return res<new_success, status_t>(ok::invoke(
                    stdc::forward<callable_t>(c), this->unwrap_unchecked()));
            } else {
                return res<new_success, status_t>(this->status());
            }
        } else {
            using new_success = decltype(ok::invoke(
                stdc::forward<callable_t>(c), ok::addressof(this->unwrap())));
            // pointer to member function, need to take address
            if (this->is_success()) {
                return res<new_success, status_t>(
                    ok::invoke(stdc::forward<callable_t>(c),
                               ok::addressof(this->unwrap_unchecked())));
            } else {
                return res<new_success, status_t>(this->status());
            }
        }
    }

    ~res()
        requires(stdc::is_trivially_destructible_v<success_t>)
    = default;

    ~res()
        requires(!stdc::is_trivially_destructible_v<success_t>)
    {
        if (this->is_success()) {
            m_success.value.~success_t();
        }
    }

#if defined(OKAYLIB_USE_FMT)
    friend struct fmt::formatter<res>;
#endif
};

template <typename success_t, status_type_c status_t>
__OK_RES_REQUIRES_CLAUSE class res<
    success_t, status_t, stdc::enable_if_t<stdc::is_reference_c<success_t>>>
{
    static_assert(!stdc::is_void_v<status_t>,
                  "Res does not support void as template arguments.");

    using value_type = stdc::remove_reference_t<success_t>;

    value_type* m_success;
    status_t m_status;

#if defined(OKAYLIB_TESTING_BACKTRACE)
  public:
    detail_testing::owned_stack_trace_t stacktrace;

  private:
#endif

  public:
    constexpr res(const res& other) noexcept = default;
    constexpr res& operator=(const res& other) noexcept = default;
    constexpr res(res&& other) noexcept = default;
    constexpr res& operator=(res&& other) noexcept = default;

    constexpr res(const status_t& status) OKAYLIB_NOEXCEPT
        requires ok::status_enum_c<status_t>
        : m_status(status)
    {
        if (status == status_t::success) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(const status_t& status) OKAYLIB_NOEXCEPT
        requires(!ok::status_enum_c<status_t> &&
                 ok::stdc::is_copy_constructible_v<status_t>)
        : m_status(status)
    {
        if (status.is_success()) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    constexpr res(status_t&& status) OKAYLIB_NOEXCEPT
        requires ok::status_object_c<status_t>
        : m_status(stdc::move(status))
    {
        if (m_status.is_success()) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    // NOTE: this is a template to defer instantiating ok::status<status_t>
    // until the `requires` clause, otherwise that will cause a compilation
    // error if the status is not a status enum.
    template <typename T>
    constexpr res(const T& status) OKAYLIB_NOEXCEPT
        requires ok::same_as_c<ok::status<status_t>, T> &&
                     status_enum_c<status_t>
        : m_status(status.as_enum())
#if defined(OKAYLIB_TESTING_BACKTRACE)
          ,
          stacktrace(status.stacktrace)
#endif
    {
        if (m_status == status_t::success) [[unlikely]]
            __ok_abort("Attempt to construct an ok::res with no success value "
                       "but a status that says there is one.");
    }

    [[nodiscard]] constexpr const status_t& status() const& OKAYLIB_NOEXCEPT
        requires status_object_c<status_t>
    {
        return m_status;
    }

    [[nodiscard]] constexpr status_t&& status() && OKAYLIB_NOEXCEPT
            requires status_object_c<status_t>
    {
        return stdc::move(m_status);
    }

    [[nodiscard]] constexpr auto status() const& OKAYLIB_NOEXCEPT
        requires status_enum_c<status_t>
    {
#if defined(OKAYLIB_TESTING_BACKTRACE)
        ok::status<status_t> out(m_status);
        out.stacktrace = this->stacktrace;
        return out;
#else
        return ok::status<status_t>(m_status);
#endif
    }

    [[nodiscard]] constexpr auto status() const&& OKAYLIB_NOEXCEPT
        requires status_enum_c<status_t>
    {
#if defined(OKAYLIB_TESTING_BACKTRACE)
        ok::status<status_t> out(m_status);
        out.stacktrace = this->stacktrace;
        return out;
#else
        return ok::status<status_t>(m_status);
#endif
    }

    constexpr res(success_t success) OKAYLIB_NOEXCEPT
        : m_status(ok::make_success<status_t>()),
          m_success(ok::addressof(success))
    {
    }

    [[nodiscard]] constexpr bool is_success() const OKAYLIB_NOEXCEPT
    {
        if constexpr (status_enum_c<status_t>) {
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
        return stdc::move(m_status);
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
        __ok_assert(this->is_success(), "Bad access to result.");
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

#if defined(OKAYLIB_USE_FMT)
    friend struct fmt::formatter<res>;
#endif
};

namespace detail {
template <typename T, typename E>
constexpr res<T, E> res_accessor_t<T, E>::construct_uninitialized_res() noexcept
{
    return res<T, E>();
}

template <typename T, typename E>
template <typename... args_t>
    requires ok::is_infallible_constructible_c<E, args_t...>
constexpr void
res_accessor_t<T, E>::emplace_error_nodestroy(res<T, E>& res,
                                              args_t&&... args) noexcept
{
    ok::stdc::construct_at(ok::addressof(res.m_status),
                           stdc::forward<args_t>(args)...);
}

template <typename T, typename E>
constexpr T&
res_accessor_t<T, E>::get_result_payload_ref_unchecked(res<T, E>& res) noexcept
{
    return res.m_success.value;
}
} // namespace detail

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
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
        if constexpr (fmt::is_formattable<enum_t>::value) {
            return fmt::format_to(ctx.out(), "{}", status.as_enum());
        } else {
            if (status.is_success()) {
                return fmt::format_to(ctx.out(), "{:s}::success",
                                      ok::nameof<enum_t>());
            } else {
                return fmt::format_to(
                    ctx.out(), "{:s}::{}", ok::nameof<enum_t>(),
                    stdc::underlying_type_t<enum_t>(status.as_enum()));
            }
        }
    }
};

template <typename success_t, typename status_t>
    requires(stdc::is_reference_c<success_t> ||
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
            if constexpr (stdc::is_reference_c<success_t>) {
                if constexpr (fmt::is_formattable<
                                  stdc::remove_reference_t<success_t>>::value) {
                    return fmt::format_to(ctx.out(), "res<{} &>",
                                          result.unwrap_unchecked());
                } else {
                    return fmt::format_to(ctx.out(), "res<{:s}: {:p}>",
                                          ok::nameof<success_t>(),
                                          static_cast<void*>(result.m_success));
                }
            } else {
                return fmt::format_to(ctx.out(), "res<{}>",
                                      result.unwrap_unchecked());
            }
        } else {
            if constexpr (fmt::is_formattable<status_t>::value) {
                return fmt::format_to(ctx.out(), "{}", result.status());
            } else if constexpr (ok::status_enum<status_t>) {
                using enum_int_t = stdc::underlying_type_t<status_t>;
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
