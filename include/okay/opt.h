#ifndef __OKAYLIB_OPT_H__
#define __OKAYLIB_OPT_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/construct_at.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/cloneable.h"
#include "okay/detail/traits/is_nonthrowing.h"
#include "okay/detail/traits/mathop_traits.h"
#include "okay/detail/traits/special_member_traits.h"
#include <cstring> // memcpy

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {
struct nullopt_t
{};

inline constexpr nullopt_t nullopt{};

template <typename payload_t, typename = void> class opt;

namespace detail {
enum class opt_impl_type
{
    object,
    reference,
};

template <typename target_t, typename opt_payload_t>
inline constexpr bool converts_from_opt =
    // check if can construct target from optional
    is_std_constructible_v<target_t, const opt<opt_payload_t>&> ||
    is_std_constructible_v<target_t, opt<opt_payload_t>&> ||
    is_std_constructible_v<target_t, const opt<opt_payload_t>&&> ||
    is_std_constructible_v<target_t, opt<opt_payload_t>&&> ||
    // check if can convert optional to target
    std::is_convertible_v<const opt<opt_payload_t>&, target_t> ||
    std::is_convertible_v<opt<opt_payload_t>&, target_t> ||
    std::is_convertible_v<const opt<opt_payload_t>&&, target_t> ||
    std::is_convertible_v<opt<opt_payload_t>&&, target_t>;

template <typename target_t, typename opt_payload_t>
inline constexpr bool assigns_from_opt =
    std::is_assignable_v<target_t&, const opt<opt_payload_t>&> ||
    std::is_assignable_v<target_t&, opt<opt_payload_t>&> ||
    std::is_assignable_v<target_t&, const opt<opt_payload_t>&&> ||
    std::is_assignable_v<target_t&, opt<opt_payload_t>&&>;
} // namespace detail

template <typename payload_t, typename> class opt
{
  public:
    // type constraints
    static_assert(!std::is_const_v<payload_t>,
                  "Wrapped value type is marked const, this has no effect. "
                  "Remove the const.");
    static_assert(detail::is_nonthrowing<payload_t>,
                  OKAYLIB_IS_NONTHROWING_ERRMSG);

    static_assert(!std::is_same_v<std::remove_cv_t<payload_t>, nullopt_t>);
    static_assert(!std::is_same_v<std::remove_cv_t<payload_t>, ok::in_place_t>);
    static_assert(!std::is_array_v<payload_t>,
                  "opt cannot contain C style array");
    static_assert(std::is_object_v<payload_t>);

  private:
    inline static constexpr detail::opt_impl_type impl_type =
        detail::opt_impl_type::object;

    template <typename T>
    inline static constexpr bool not_self =
        !std::is_same_v<opt, detail::remove_cvref_t<T>>;
    template <typename T>
    inline static constexpr bool not_tag =
        !std::is_same_v<ok::in_place_t, detail::remove_cvref_t<T>>;

    detail::uninitialized_storage_t<payload_t> m_payload;
    bool m_has_value;

    // emplace a value into the optional assuming nothing is initialized. will
    // not call the destructor of any live objects
    template <typename... args_t>
    constexpr void emplace_nodestroy(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        ok::construct_at(ok::addressof(this->m_payload), ok::in_place,
                         std::forward<args_t>(args)...);
    }

  public:
    constexpr opt() OKAYLIB_NOEXCEPT : m_has_value(false) {}
    constexpr opt(nullopt_t) OKAYLIB_NOEXCEPT : m_has_value(false) {}

    constexpr opt(opt&& other)
        requires(std::is_trivially_move_constructible_v<payload_t>)
    = default;
    constexpr opt(opt&& other)
        requires(!std::is_trivially_move_constructible_v<payload_t> &&
                 std::is_move_constructible_v<payload_t>)
    OKAYLIB_NOEXCEPT
    {
        if (other) {
            this->emplace_nodestroy(std::move(other.ref_unchecked()));
            m_has_value = true;
        } else {
            m_has_value = false;
        }
    }

    constexpr opt& operator=(opt&& other) &
        requires(std::is_trivially_move_assignable_v<payload_t>)
    = default;
    constexpr opt& operator=(opt&& other) &
        requires(!std::is_trivially_move_assignable_v<payload_t> &&
                 std::is_move_assignable_v<payload_t>) OKAYLIB_NOEXCEPT
    {
        if (other) {
            if (this->has_value()) {
                this->ref_unchecked() = std::move(other.ref_unchecked());
            } else {
                this->emplace_nodestroy(std::move(other.ref_unchecked()));
                m_has_value = true;
            }
        } else {
            this->reset();
        }
    }

    constexpr opt(const opt& other)
        requires(std::is_trivially_copy_constructible_v<payload_t>)
    = default;
    // just in case someone wants to define a non trivial copy constructor for
    // something and then put that in an opt
    constexpr opt(const opt& other)
        requires(!std::is_trivially_copy_constructible_v<payload_t> &&
                 std::is_copy_constructible_v<payload_t>)
    OKAYLIB_NOEXCEPT
    {
        if (other) {
            this->emplace_nodestroy(other.ref_unchecked());
            m_has_value = true;
        } else {
            m_has_value = false;
        }
    }

    constexpr opt& operator=(const opt& other) &
        requires(std::is_trivially_copy_assignable_v<payload_t>)
    = default;
    // just in case someone wants to define a non trivial copy assignment for
    // something and then put that in an opt
    constexpr opt& operator=(const opt& other) &
        requires(!std::is_trivially_copy_assignable_v<payload_t> &&
                 std::is_copy_assignable_v<payload_t>) OKAYLIB_NOEXCEPT
    {
        if (other) {
            if (this->has_value()) {
                this->ref_unchecked() = other.ref_unchecked();
            } else {
                this->emplace_nodestroy(other.ref_unchecked());
                m_has_value = true;
            }
        } else {
            this->reset();
        }
    }

    // constructors which perform conversion of incoming type
    template <typename convert_from_t = payload_t>
    constexpr opt(convert_from_t&& t) OKAYLIB_NOEXCEPT
        requires(!detail::is_instance_v<convert_from_t, opt> &&
                 not_tag<convert_from_t> &&
                 is_std_constructible_v<payload_t, convert_from_t> &&
                 std::is_convertible_v<convert_from_t, payload_t>)
        : m_payload(ok::in_place, std::forward<convert_from_t>(t)),
          m_has_value(true)
    {
    }

    // converting constructor for types which are non convertible. only
    // difference is that it is marked explicit
    template <typename convert_from_t = payload_t>
    explicit constexpr opt(convert_from_t&& t) OKAYLIB_NOEXCEPT
        requires(!detail::is_instance_v<convert_from_t, opt> &&
                 not_tag<convert_from_t> &&
                 is_std_constructible_v<payload_t, convert_from_t> &&
                 !std::is_convertible_v<convert_from_t, payload_t>)
        : m_payload(ok::in_place, std::forward<convert_from_t>(t)),
          m_has_value(true)
    {
    }

    template <typename incoming_t>
        constexpr opt& operator=(incoming_t&& incoming) & OKAYLIB_NOEXCEPT
            requires(!detail::is_instance_v<incoming_t, opt> &&
                     // TODO: cant have payload and decayed incoming_t be the
                     // same, only if scalar though. why? copied this from STL
                     // implementation
                     !(std::is_scalar_v<payload_t> &&
                       std::is_same_v<payload_t, std::decay_t<incoming_t>>) &&
                     is_std_constructible_v<payload_t, incoming_t> &&
                     std::is_assignable_v<payload_t&, incoming_t>)
    {
        if (m_has_value) {
            this->ref_unchecked() = std::forward<incoming_t>(incoming);
        } else {
            this->emplace_nodestroy(std::forward<incoming_t>(incoming));
            m_has_value = true;
        }
        return *this;
    }

    // emplacement constructor
    template <typename... args_t>
    explicit constexpr opt(ok::in_place_t, args_t&&... args) OKAYLIB_NOEXCEPT
        requires(is_infallible_constructible_v<payload_t, args_t...>)
        : m_payload(ok::in_place, std::forward<args_t>(args)...),
          m_has_value(true)
    {
    }

    constexpr opt& operator=(nullopt_t) & OKAYLIB_NOEXCEPT
    {
        this->reset();
        return *this;
    }

    template <typename T>
        requires(std::is_convertible_v<payload_t, T>)
    constexpr operator opt<T>() const&
    {
        if (this->has_value()) {
            return opt<T>(ok::in_place, this->ref_unchecked());
        } else {
            return opt<T>();
        }
    }

    template <typename T>
        requires(std::is_convertible_v<payload_t, T>)
    constexpr operator opt<T>() &&
    {
        if (this->has_value()) {
            return opt<T>(ok::in_place, std::move(this->ref_unchecked()));
        } else {
            return opt<T>();
        }
    }

    template <typename T>
        requires(!std::is_convertible_v<payload_t, T> &&
                 std::is_constructible_v<T, const payload_t&>)
    explicit constexpr operator opt<T>() const&
    {
        if (this->has_value()) {
            return opt<T>(ok::in_place, this->ref_unchecked());
        } else {
            return opt<T>();
        }
    }

    template <typename T>
        requires(!std::is_convertible_v<payload_t, T> &&
                 std::is_constructible_v<T, payload_t &&>)
    explicit constexpr operator opt<T>() &&
    {
        if (this->has_value()) {
            return opt<T>(ok::in_place, std::move(this->ref_unchecked()));
        } else {
            return opt<T>();
        }
    }

    template <typename... args_t>
    constexpr payload_t& emplace(args_t&&... args) OKAYLIB_NOEXCEPT
        requires(is_std_constructible_v<payload_t, args_t...>)
    {
        this->reset();
        this->emplace_nodestroy(std::forward<args_t>(args)...);
        m_has_value = true;
        return this->ref_unchecked();
    }

    [[nodiscard]] constexpr bool has_value() const OKAYLIB_NOEXCEPT
    {
        return m_has_value;
    }

    /// Extract the inner value of the optional, or abort the program. Check
    /// has_value() before calling this.
    [[nodiscard]] constexpr payload_t& ref_or_panic() & OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort("Attempt to get value from a null optional.");
        }
        return this->ref_unchecked();
    }

    [[nodiscard]] constexpr payload_t&& ref_or_panic() && OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort("Attempt to get value from a null optional.");
        }
        return std::move(this->ref_unchecked());
    }

    [[nodiscard]] constexpr const payload_t&
    ref_or_panic() const& OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort("Attempt to get value from a null optional.");
        }
        return this->ref_unchecked();
    }

    [[nodiscard]] constexpr payload_t& ref_unchecked() & OKAYLIB_NOEXCEPT
    {
        return m_payload.value;
    }

    [[nodiscard]] constexpr payload_t&& ref_unchecked() && OKAYLIB_NOEXCEPT
    {
        return std::move(m_payload.value);
    }

    [[nodiscard]] constexpr const payload_t&
    ref_unchecked() const& OKAYLIB_NOEXCEPT
    {
        return m_payload.value;
    }

    [[nodiscard]] constexpr payload_t
    copy_out_or(payload_t alternative) const OKAYLIB_NOEXCEPT
        requires(std::is_copy_constructible_v<payload_t>)
    {
        if (!this->has_value()) [[unlikely]] {
            return alternative;
        }
        return payload_t(this->ref_unchecked());
    }

    template <typename callable_t>
    [[nodiscard]] constexpr payload_t
    copy_out_or_run(callable_t&& callable) const& OKAYLIB_NOEXCEPT
        requires(std::is_copy_constructible_v<payload_t> &&
                 is_std_invocable_r_v<callable_t, payload_t>)
    {
        if (!this->has_value()) [[unlikely]] {
            return callable();
        }
        return this->ref_unchecked();
    }

    /// Performs a move and also destroys the opt which was moved out of,
    /// leaving behind a nullopt.
    [[nodiscard]] constexpr opt take() OKAYLIB_NOEXCEPT
        requires(std::is_move_constructible_v<payload_t>)
    {
        if (this->has_value()) {
            opt out(std::move(this->ref_unchecked()));
            this->reset();
            return out;
        } else {
            return nullopt;
        }
    }

    /// Take but instead of returning an optional it returns either the internal
    /// value or, if null, a wrapped default value.
    [[nodiscard]] constexpr payload_t
    take_or(payload_t&& alternative) OKAYLIB_NOEXCEPT
        requires(std::is_move_constructible_v<payload_t>)
    {
        if (this->has_value()) {
            payload_t out(std::move(this->ref_unchecked()));
            this->reset();
            return out;
        } else {
            return std::move(alternative);
        }
    }

    /// Take but instead of returning an optional it returns either the internal
    /// value or, if null, the return value of a callable.
    template <typename callable_t>
    [[nodiscard]] constexpr payload_t
    take_or_run(callable_t&& callable) OKAYLIB_NOEXCEPT
        requires(is_std_invocable_r_v<callable_t, payload_t>)
    {
        if (this->has_value()) {
            payload_t out(std::move(this->ref_unchecked()));
            this->reset();
            return out;
        } else {
            return callable();
        }
    }

    /// Call destructor of internal type, or just reset it if it doesnt have one
    constexpr void reset() OKAYLIB_NOEXCEPT
    {
        if constexpr (std::is_trivially_destructible_v<payload_t>) {
            m_has_value = false;
        } else {
            if (m_has_value) {
                this->ref_unchecked().~payload_t();
                m_has_value = false;
            }
        }
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return this->has_value();
    }

    [[nodiscard]] constexpr friend bool operator==(const opt& a, const opt& b)
        requires(detail::is_equality_comparable_to_self_v<payload_t>)
    {
        if (a.has_value() != b.has_value())
            return false;

        return !a.has_value() || a.ref_unchecked() == b.ref_unchecked();
    }

    [[nodiscard]] constexpr friend bool operator==(const opt& a,
                                                   const payload_t& b)
        requires(detail::is_equality_comparable_to_self_v<payload_t>)
    {
        return a.has_value() && a.ref_unchecked() == b;
    }

    [[nodiscard]] constexpr friend bool operator==(const payload_t& b,
                                                   const opt& a)
        requires(detail::is_equality_comparable_to_self_v<payload_t>)
    {
        return a.has_value() && a.ref_unchecked() == b;
    }

    constexpr ~opt()
        requires(!std::is_trivially_destructible_v<payload_t>)
    {
        if (m_has_value) {
            this->ref_unchecked().~payload_t();
        }
    }

    constexpr auto try_clone() const& OKAYLIB_NOEXCEPT
        requires try_cloneable<payload_t>
    {
        if (this->has_value()) {
            // res can convert from another res with the same status since
            // the success types (payload_t and opt<payload_t>) are convertible
            return res<opt, try_clone_status_t<payload_t>>(
                ok::in_place, std::move(ok::try_clone(this->ref_unchecked())));
        } else {
            return res<opt, try_clone_status_t<payload_t>>(ok::in_place,
                                                           nullopt);
        }
    }

    constexpr opt clone() const& OKAYLIB_NOEXCEPT
        requires cloneable<payload_t>
    {
        if (this->has_value()) {
            return ok::clone(this->ref_unchecked());
        } else {
            return nullopt;
        }
    }

    constexpr try_clone_status_t<payload_t>
    try_clone_into(opt& dest) const& OKAYLIB_NOEXCEPT
        requires try_cloneable<payload_t>
    {
        if (this->has_value()) {
            if (dest.has_value()) {
                return ok::try_clone_into(this->ref_unchecked(),
                                          dest.ref_unchecked());
            } else {
                if (auto res = ok::try_clone(this->ref_unchecked());
                    res.is_success()) {
                    dest.emplace_nodestroy(std::move(res.unwrap()));
                    return res.status();
                } else {
                    return res.status();
                }
            }
        } else {
            dest.reset();
            return ok::make_success<try_clone_status_t<payload_t>>();
        }
    }

    constexpr void clone_into(opt& dest) const& OKAYLIB_NOEXCEPT
        requires cloneable<payload_t>
    {
        if (this->has_value()) {
            if (dest.has_value()) {
                ok::clone_into(this->ref_unchecked(), dest.ref_unchecked());
            } else {
                dest.emplace_nodestroy(
                    std::move(ok::clone(this->ref_unchecked())));
            }
        } else {
            dest.reset();
        }
    }

    constexpr ~opt()
        requires(std::is_trivially_destructible_v<payload_t>)
    = default;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt>;
#endif
};

// template specialization for lvalue references
template <typename payload_t>
class opt<payload_t, std::enable_if_t<std::is_reference_v<payload_t>>>
{
  public:
    using pointer_t = std::remove_reference_t<payload_t>;

  private:
    inline static constexpr detail::opt_impl_type impl_type =
        detail::opt_impl_type::reference;

    pointer_t* m_pointer;

  public:
    constexpr opt() : m_pointer(nullptr) {};
    constexpr opt(nullopt_t) OKAYLIB_NOEXCEPT : m_pointer(nullptr) {};

    // allow pointer conversion
    constexpr opt(pointer_t* p) OKAYLIB_NOEXCEPT : m_pointer(p) {}
    // allow non-const reference construction
    constexpr opt(std::remove_const_t<pointer_t>& p) OKAYLIB_NOEXCEPT
        : m_pointer(ok::addressof(p))
    {
    }

    // implicit conversion if references are implicitly convertible
    template <typename other_t>
    constexpr opt(const opt<other_t>& other) OKAYLIB_NOEXCEPT
        requires(std::is_convertible_v<other_t, payload_t> &&
                 !std::is_same_v<detail::remove_cvref_t<other_t>, opt>)
        : m_pointer(other.as_ptr())
    {
    }

    constexpr payload_t emplace(payload_t other) OKAYLIB_NOEXCEPT
    {
        m_pointer = ok::addressof(other);
    }

    constexpr bool has_value() const OKAYLIB_NOEXCEPT
    {
        return m_pointer != nullptr;
    }

    constexpr explicit operator bool() const OKAYLIB_NOEXCEPT
    {
        return this->has_value();
    }

    constexpr opt& operator=(payload_t ref) OKAYLIB_NOEXCEPT
    {
        m_pointer = ok::addressof(ref);
        return *this;
    }

    constexpr opt& operator=(nullopt_t) OKAYLIB_NOEXCEPT
    {
        m_pointer = nullptr;
        return *this;
    }

    constexpr void reset() OKAYLIB_NOEXCEPT { m_pointer = nullptr; }

    [[nodiscard]] constexpr payload_t ref_unchecked() const OKAYLIB_NOEXCEPT
    {
        return static_cast<payload_t>(*m_pointer);
    }

    [[nodiscard]] constexpr payload_t ref_or_panic() const OKAYLIB_NOEXCEPT
    {
        if (!this->has_value()) [[unlikely]] {
            __ok_abort("Attempt to get value from a null optional.");
        }
        return this->ref_unchecked();
    }

    [[nodiscard]] constexpr bool
    is_alias_for(const pointer_t& other) OKAYLIB_NOEXCEPT
    {
        return m_pointer == ok::addressof(other);
    }

    [[nodiscard]] constexpr bool is_alias_for(const opt& other) OKAYLIB_NOEXCEPT
    {
        return m_pointer && (m_pointer == other.m_pointer);
    }

    [[nodiscard]] constexpr pointer_t* as_ptr() const OKAYLIB_NOEXCEPT
    {
        return m_pointer;
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt>;
#endif
};

template <typename range_t, typename enable> struct range_definition;

template <typename payload_t>
struct ok::range_definition<ok::opt<payload_t>, void>
{
    struct cursor_t
    {
        friend class range_definition;

      private:
        bool is_out_of_bounds = false;
    };

    using opt_range_t = opt<payload_t>;

    inline static constexpr bool is_ref_wrapper =
        std::is_reference_v<payload_t>;

    using value_type = std::remove_reference_t<payload_t>;

    static constexpr cursor_t begin(const opt_range_t& range) OKAYLIB_NOEXCEPT
    {
        return cursor_t{};
    }

    static constexpr void increment(const opt_range_t& range,
                                    cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        cursor.is_out_of_bounds = true;
    }

    static constexpr bool is_inbounds(const opt_range_t& range,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        // NOTE: this is defensive, I'm assuming you might use a cursor on
        // an opt<T> which has had its item added or removed, so check on access
        return range.has_value() && !cursor.is_out_of_bounds;
    }

    static constexpr size_t size(const opt_range_t& range) OKAYLIB_NOEXCEPT
    {
        return size_t(range.has_value());
    }

    /// nonconst get_ref is disabled for reference wrapper optionals which are
    /// storing a const reference. no need to check if payload_t is a reference
    /// as it can't be const otherwise.
    template <typename T = payload_t>
    static constexpr auto& get_ref(opt_range_t& range, const cursor_t& cursor)
        requires(!std::is_const_v<std::remove_reference_t<value_type>>)
    {
        return range.ref_or_panic();
    }

    static constexpr auto& get_ref(const opt_range_t& range,
                                   const cursor_t& cursor)
    {
        return range.ref_or_panic();
    }
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename payload_t> struct fmt::formatter<ok::opt<payload_t>>
{
    using formatted_type_t = ok::opt<payload_t>;
    static_assert(
        formatted_type_t::impl_type == ok::detail::opt_impl_type::reference ||
            fmt::is_formattable<payload_t>::value,
        "Attempt to format an optional whose content is not formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const ok::opt<payload_t>& optional,
                                    format_context& ctx) const
    {
        if (optional) {
            if constexpr (ok::opt<payload_t>::impl_type ==
                          ok::detail::opt_impl_type::reference) {
                if constexpr (fmt::is_formattable<typename formatted_type_t::
                                                      pointer_t>::value) {
                    return fmt::format_to(ctx.out(), "{}",
                                          optional.ref_unchecked());
                } else {
                    // just format reference as pointer, if the contents itself
                    // can't be formatted
                    return fmt::format_to(ctx.out(), "{:p}", optional.pointer);
                }
            } else if constexpr (ok::opt<payload_t>::impl_type ==
                                 ok::detail::opt_impl_type::object) {
                return fmt::format_to(ctx.out(), "{}",
                                      optional.ref_unchecked());
            }
        }
        return fmt::format_to(ctx.out(), "null");
    }
};
#endif

#endif
