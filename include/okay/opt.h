#ifndef __OKAYLIB_OPT_H__
#define __OKAYLIB_OPT_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/invoke.h"
#include "okay/detail/memory.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/cloneable.h"
#include "okay/detail/traits/mathop_traits.h"
#include "okay/detail/traits/special_member_traits.h"
#include <cstring> // memcpy

#if defined(OKAYLIB_USE_FMT)
#include <fmt/core.h>
#endif

namespace ok {
struct nullopt_t
{};

inline constexpr nullopt_t nullopt{};

template <typename payload_t> class opt;

namespace detail {
enum class opt_impl_type
{
    object,
    reference,
};

template <typename target_t, typename opt_payload_t>
inline constexpr bool converts_from_opt =
    // check if can construct target from optional
    is_std_constructible_c<target_t, const opt<opt_payload_t>&> ||
    is_std_constructible_c<target_t, opt<opt_payload_t>&> ||
    is_std_constructible_c<target_t, const opt<opt_payload_t>&&> ||
    is_std_constructible_c<target_t, opt<opt_payload_t>&&> ||
    // check if can convert optional to target
    is_convertible_to_c<const opt<opt_payload_t>&, target_t> ||
    is_convertible_to_c<opt<opt_payload_t>&, target_t> ||
    is_convertible_to_c<const opt<opt_payload_t>&&, target_t> ||
    is_convertible_to_c<opt<opt_payload_t>&&, target_t>;

template <typename target_t, typename opt_payload_t>
inline constexpr bool assigns_from_opt =
    stdc::is_assignable_v<target_t&, const opt<opt_payload_t>&> ||
    stdc::is_assignable_v<target_t&, opt<opt_payload_t>&> ||
    stdc::is_assignable_v<target_t&, const opt<opt_payload_t>&&> ||
    stdc::is_assignable_v<target_t&, opt<opt_payload_t>&&>;
} // namespace detail

template <typename payload_t>
    requires(!stdc::is_reference_c<payload_t>)
class opt<payload_t>
{
  public:
    // type constraints
    static_assert(!is_const_c<payload_t>,
                  "Wrapped value type is marked const, this has no effect. "
                  "Remove the const.");
    static_assert(detail::is_nonthrowing_c<payload_t>,
                  OKAYLIB_IS_NONTHROWING_ERRMSG);

    static_assert(!stdc::is_same_v<stdc::remove_cv_t<payload_t>, nullopt_t>);
    static_assert(
        !stdc::is_same_v<stdc::remove_cv_t<payload_t>, ok::in_place_t>);
    static_assert(!stdc::is_array_v<payload_t>,
                  "opt cannot contain C style array");
    static_assert(detail::is_object_c<payload_t>);

  private:
    inline static constexpr detail::opt_impl_type impl_type =
        detail::opt_impl_type::object;

    // friends with all other types of opts
    template <typename T> friend class opt;

    template <typename T>
    inline static constexpr bool not_self =
        !stdc::is_same_v<opt, remove_cvref_t<T>>;
    template <typename T>
    inline static constexpr bool not_tag =
        !stdc::is_same_v<ok::in_place_t, remove_cvref_t<T>>;

    detail::uninitialized_storage_t<payload_t> m_payload;
    bool m_has_value;

    // emplace a value into the optional assuming nothing is initialized. will
    // not call the destructor of any live objects
    template <typename... args_t>
    __ok_constructing_constexpr void
    emplace_nodestroy(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        ok::stdc::construct_at(ok::addressof(this->m_payload), ok::in_place,
                               stdc::forward<args_t>(args)...);
    }

  public:
    using value_type = payload_t;

    constexpr opt() OKAYLIB_NOEXCEPT : m_has_value(false) {}
    constexpr opt(nullopt_t) OKAYLIB_NOEXCEPT : m_has_value(false) {}

    constexpr opt(opt&& other)
        requires(stdc::is_trivially_move_constructible_v<payload_t>)
    = default;
    constexpr opt(opt&& other)
        requires(!stdc::is_trivially_move_constructible_v<payload_t> &&
                 stdc::is_move_constructible_v<payload_t>)
    OKAYLIB_NOEXCEPT
    {
        if (other) {
            this->emplace_nodestroy(stdc::move(other.ref_unchecked()));
            m_has_value = true;
        } else {
            m_has_value = false;
        }
    }

    constexpr opt& operator=(opt&& other) &
        requires(stdc::is_trivially_move_assignable_v<payload_t>)
    = default;
    constexpr opt& operator=(opt&& other) &
        requires(!stdc::is_trivially_move_assignable_v<payload_t> &&
                 stdc::is_move_assignable_v<payload_t>) OKAYLIB_NOEXCEPT
    {
        if (other) {
            if (this->has_value()) {
                this->ref_unchecked() = stdc::move(other.ref_unchecked());
            } else {
                this->emplace_nodestroy(stdc::move(other.ref_unchecked()));
                m_has_value = true;
            }
        } else {
            this->reset();
        }
        return *this;
    }

    constexpr opt(const opt& other)
        requires(stdc::is_trivially_copy_constructible_v<payload_t>)
    = default;
    // just in case someone wants to define a non trivial copy constructor for
    // something and then put that in an opt
    constexpr opt(const opt& other)
        requires(!stdc::is_trivially_copy_constructible_v<payload_t> &&
                 stdc::is_copy_constructible_v<payload_t>)
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
        requires(stdc::is_trivially_copy_assignable_v<payload_t>)
    = default;
    // just in case someone wants to define a non trivial copy assignment for
    // something and then put that in an opt
    constexpr opt& operator=(const opt& other) &
        requires(!stdc::is_trivially_copy_assignable_v<payload_t> &&
                 stdc::is_copy_assignable_v<payload_t>) OKAYLIB_NOEXCEPT
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
        return *this;
    }

    // constructors which perform conversion of incoming type
    template <typename convert_from_t = payload_t>
    constexpr opt(convert_from_t&& t) OKAYLIB_NOEXCEPT
        requires(!detail::is_instance_c<convert_from_t, opt> &&
                 not_tag<convert_from_t> &&
                 is_std_constructible_c<payload_t, convert_from_t> &&
                 is_convertible_to_c<convert_from_t, payload_t>)
        : m_payload(ok::in_place, stdc::forward<convert_from_t>(t)),
          m_has_value(true)
    {
    }

    // converting constructor for types which are non convertible. only
    // difference is that it is marked explicit
    template <typename convert_from_t = payload_t>
    explicit constexpr opt(convert_from_t&& t) OKAYLIB_NOEXCEPT
        requires(!detail::is_instance_c<convert_from_t, opt> &&
                 not_tag<convert_from_t> &&
                 is_std_constructible_c<payload_t, convert_from_t> &&
                 !is_convertible_to_c<convert_from_t, payload_t>)
        : m_payload(ok::in_place, stdc::forward<convert_from_t>(t)),
          m_has_value(true)
    {
    }

    template <typename incoming_t>
        constexpr opt& operator=(incoming_t&& incoming) & OKAYLIB_NOEXCEPT
            requires(!detail::is_instance_c<incoming_t, opt> &&
                     // TODO: cant have payload and decayed incoming_t be the
                     // same, only if scalar though. why? copied this from STL
                     // implementation
                     !(stdc::is_scalar_v<payload_t> &&
                       stdc::is_same_v<payload_t, stdc::decay_t<incoming_t>>) &&
                     is_std_constructible_c<payload_t, incoming_t> &&
                     stdc::is_assignable_v<payload_t&, incoming_t>)
    {
        if (m_has_value) {
            this->ref_unchecked() = stdc::forward<incoming_t>(incoming);
        } else {
            this->emplace_nodestroy(stdc::forward<incoming_t>(incoming));
            m_has_value = true;
        }
        return *this;
    }

    // emplacement constructor
    template <typename... args_t>
    explicit constexpr opt(ok::in_place_t, args_t&&... args) OKAYLIB_NOEXCEPT
        requires(is_infallible_constructible_c<payload_t, args_t...>)
        : m_payload(ok::in_place, stdc::forward<args_t>(args)...),
          m_has_value(true)
    {
    }

    constexpr opt& operator=(nullopt_t) & OKAYLIB_NOEXCEPT
    {
        this->reset();
        return *this;
    }

    template <typename T>
        requires(is_convertible_to_c<const T&, payload_t>)
    constexpr opt(const opt<T>& other) : m_has_value(other.has_value())
    {
        if (other.has_value())
            this->emplace_nodestroy(other.ref_unchecked());
    }

    template <typename T>
        requires(is_convertible_to_c<T&, payload_t>)
    constexpr opt(opt<T>& other) : m_has_value(other.has_value())
    {
        if (other.has_value())
            this->emplace_nodestroy(other.ref_unchecked());
    }

    template <typename T>
        requires(is_convertible_to_c<T &&, payload_t>)
    constexpr opt(opt<T>&& other) : m_has_value(other.has_value())
    {
        if (other.has_value())
            this->emplace_nodestroy(stdc::move(other.ref_unchecked()));
    }

    template <typename T>
        requires(is_std_constructible_c<payload_t, const T&> &&
                 !is_convertible_to_c<const T&, payload_t>)
    explicit constexpr opt(const opt<T>& other) : m_has_value(other.has_value())
    {
        if (other.has_value())
            this->emplace_nodestroy(other.ref_unchecked());
    }

    template <typename T>
        requires(is_std_constructible_c<payload_t, T&> &&
                 !is_convertible_to_c<T&, payload_t>)
    explicit constexpr opt(opt<T>& other) : m_has_value(other.has_value())
    {
        if (other.has_value())
            this->emplace_nodestroy(other.ref_unchecked());
    }

    template <typename T>
        requires(is_std_constructible_c<payload_t, T &&> &&
                 !is_convertible_to_c<T &&, payload_t>)
    explicit constexpr opt(opt<T>&& other) : m_has_value(other.has_value())
    {
        if (other.has_value())
            this->emplace_nodestroy(stdc::move(other.ref_unchecked()));
    }

    template <typename... args_t>
    constexpr payload_t& emplace(args_t&&... args) OKAYLIB_NOEXCEPT
        requires(is_std_constructible_c<payload_t, args_t...>)
    {
        this->reset();
        this->emplace_nodestroy(stdc::forward<args_t>(args)...);
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
        return stdc::move(this->ref_unchecked());
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
        __ok_assert(this->has_value(), "Bad access to opt payload.");
        return m_payload.value;
    }

    [[nodiscard]] constexpr payload_t&& ref_unchecked() && OKAYLIB_NOEXCEPT
    {
        __ok_assert(this->has_value(), "Bad access to opt payload.");
        return stdc::move(m_payload.value);
    }

    [[nodiscard]] constexpr const payload_t&
    ref_unchecked() const& OKAYLIB_NOEXCEPT
    {
        __ok_assert(this->has_value(), "Bad access to opt payload.");
        return m_payload.value;
    }

    [[nodiscard]] constexpr payload_t
    copy_out_or(payload_t alternative) const OKAYLIB_NOEXCEPT
        requires(stdc::is_copy_constructible_v<payload_t>)
    {
        if (!this->has_value()) [[unlikely]] {
            return alternative;
        }
        return payload_t(this->ref_unchecked());
    }

    template <typename callable_t>
    [[nodiscard]] constexpr payload_t
    copy_out_or_run(callable_t&& callable) const& OKAYLIB_NOEXCEPT
        requires(stdc::is_copy_constructible_v<payload_t> &&
                 is_std_invocable_r_c<callable_t, payload_t>)
    {
        if (!this->has_value()) [[unlikely]] {
            return callable();
        }
        return this->ref_unchecked();
    }

    /// Performs a move and also destroys the opt which was moved out of,
    /// leaving behind a nullopt.
    [[nodiscard]] constexpr opt take() OKAYLIB_NOEXCEPT
        requires(stdc::is_move_constructible_v<payload_t>)
    {
        if (this->has_value()) {
            opt out(stdc::move(this->ref_unchecked()));
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
        requires(stdc::is_move_constructible_v<payload_t>)
    {
        if (this->has_value()) {
            payload_t out(stdc::move(this->ref_unchecked()));
            this->reset();
            return out;
        } else {
            return stdc::move(alternative);
        }
    }

    /// Take but instead of returning an optional it returns either the internal
    /// value or, if null, the return value of a callable.
    template <typename callable_t>
    [[nodiscard]] constexpr payload_t
    take_or_run(callable_t&& callable) OKAYLIB_NOEXCEPT
        requires(is_std_invocable_c<callable_t>)
    {
        if (this->has_value()) {
            payload_t out(stdc::move(this->ref_unchecked()));
            this->reset();
            return out;
        } else {
            return ok::invoke(callable);
        }
    }

    /// Take the value, and call a transformation function on it before moving
    /// it into the returned optional. If this is already null, then we also
    /// return null
    template <typename callable_t>
    [[nodiscard]] constexpr auto
    take_and_run(callable_t&& callable) OKAYLIB_NOEXCEPT
        requires requires {
            {
                invoke(stdc::forward<callable_t>(callable),
                       stdc::move(this->ref_unchecked()))
            } -> is_nonvoid_c;
        }
    {
        using ret_t = decltype(invoke(stdc::forward<callable_t>(callable),
                                      stdc::move(this->ref_unchecked())));
        if (this->has_value()) {
            opt<ret_t> out(invoke(stdc::forward<callable_t>(callable),
                                  stdc::move(this->ref_unchecked())));
            this->reset();
            return out;
        } else {
            return opt<ret_t>{};
        }
    }

    /// Call destructor of internal type, or just reset it if it doesnt have one
    constexpr void reset() OKAYLIB_NOEXCEPT
    {
        if constexpr (stdc::is_trivially_destructible_v<payload_t>) {
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

    // comparison from this opt to any other type that isn't an optional
    template <typename other_t>
        requires(!detail::is_instance_c<other_t, opt>)
    constexpr friend bool operator==(const opt a,
                                     const other_t& b) OKAYLIB_NOEXCEPT
        requires detail::is_equality_comparable_to_c<payload_t, other_t>
    {
        if (!a.has_value())
            return false;
        return a.ref_unchecked() == b;
    }

    // comparison from this opt to any other optional
    template <typename other_payload_t>
    constexpr friend bool
    operator==(const opt a, const opt<other_payload_t>& b) OKAYLIB_NOEXCEPT
        requires detail::is_equality_comparable_to_c<payload_t, other_payload_t>
    {
        if (!a.has_value() && !b.has_value())
            return true;
        if (a.has_value() != b.has_value())
            return false;

        return a.ref_unchecked() == b.ref_unchecked();
    }

    constexpr friend bool operator==(const opt a, nullopt_t) OKAYLIB_NOEXCEPT
    {
        return !a.has_value();
    }

    // compatibility with optional reference deep_compare_with()
    template <typename other_t>
    [[nodiscard]] constexpr bool
    deep_compare_with(const other_t& b) const OKAYLIB_NOEXCEPT
        requires detail::is_equality_comparable_to_c<opt, other_t>
    {
        return *this == b;
    }

    constexpr ~opt()
        requires(!stdc::is_trivially_destructible_v<payload_t>)
    {
        if (m_has_value) {
            this->ref_unchecked().~payload_t();
        }
    }

    constexpr auto try_clone() const& OKAYLIB_NOEXCEPT
        requires try_cloneable_c<payload_t>
    {
        if (this->has_value()) {
            // res can convert from another res with the same status since
            // the success types (payload_t and opt<payload_t>) are convertible
            return res<opt, try_clone_status_t<payload_t>>(
                ok::in_place, stdc::move(ok::try_clone(this->ref_unchecked())));
        } else {
            return res<opt, try_clone_status_t<payload_t>>(ok::in_place,
                                                           nullopt);
        }
    }

    constexpr opt clone() const& OKAYLIB_NOEXCEPT
        requires cloneable_c<payload_t>
    {
        if (this->has_value()) {
            return ok::clone(this->ref_unchecked());
        } else {
            return nullopt;
        }
    }

    constexpr auto try_clone_into(opt& dest) const& OKAYLIB_NOEXCEPT
        requires try_cloneable_c<payload_t>
    {
        using ret = try_clone_status_t<payload_t>;
        if (this->has_value()) {
            if (dest.has_value()) {
                return ret(ok::try_clone_into(this->ref_unchecked(),
                                              dest.ref_unchecked()));
            } else {
                if (auto res = ok::try_clone(this->ref_unchecked());
                    res.is_success()) {
                    dest.emplace_nodestroy(stdc::move(res.unwrap()));
                    return ret(res.status());
                } else {
                    return ret(res.status());
                }
            }
        } else {
            dest.reset();
            return ok::make_success<ret>();
        }
    }

    constexpr void clone_into(opt& dest) const& OKAYLIB_NOEXCEPT
        requires cloneable_c<payload_t>
    {
        if (this->has_value()) {
            if (dest.has_value()) {
                ok::clone_into(this->ref_unchecked(), dest.ref_unchecked());
            } else {
                dest.emplace_nodestroy(
                    stdc::move(ok::clone(this->ref_unchecked())));
            }
        } else {
            dest.reset();
        }
    }

    constexpr ~opt()
        requires(stdc::is_trivially_destructible_v<payload_t>)
    = default;

    // standard library style iteration, for compatibility with foreach loops
    struct sentinel_t
    {};

  private:
    template <bool is_const> struct iterator_impl;
    template <bool is_const> friend struct iterator_impl;

    template <bool is_const> struct iterator_impl
    {
      private:
        stdc::conditional_t<is_const, const opt&, opt&> m_parent;
        bool m_iterated = false;

      public:
        using value_type =
            stdc::conditional_t<is_const, stdc::add_const_t<payload_t>,
                                payload_t>;
        using reference = ok::stdc::add_lvalue_reference_t<value_type>;
        using pointer = ok::stdc::add_pointer_t<value_type>;

        constexpr iterator_impl() = delete;

        constexpr iterator_impl(decltype(m_parent) parent) : m_parent(parent) {}

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            if (m_iterated || !m_parent) [[unlikely]] {
                __ok_abort("out of bounds dereference of opt iterator");
            }
            return m_parent.ref_unchecked();
        }

        constexpr iterator_impl& operator++() OKAYLIB_NOEXCEPT
        {
            m_iterated = true;
            return *this;
        }

        constexpr iterator_impl operator++(int) const OKAYLIB_NOEXCEPT
        {
            iterator_impl tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr friend bool operator==(const iterator_impl& a,
                                         const sentinel_t&) OKAYLIB_NOEXCEPT
        {
            return a.m_iterated || !a.m_parent;
        }

        constexpr friend bool operator!=(const iterator_impl& a,
                                         const sentinel_t& b) OKAYLIB_NOEXCEPT
        {
            return !(a == b);
        }
    };

  public:
    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    [[nodiscard]] constexpr iterator begin() { return iterator(*this); }
    [[nodiscard]] constexpr const_iterator begin() const
    {
        return const_iterator(*this);
    }

    [[nodiscard]] constexpr sentinel_t end() const { return {}; }

#if defined(OKAYLIB_USE_FMT)
    friend struct fmt::formatter<opt>;
#endif
};

// template specialization for lvalue references
template <stdc::is_lvalue_reference_c payload_t> class opt<payload_t>
{
  public:
    using pointer_t = stdc::remove_reference_t<payload_t>;

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
    constexpr opt(stdc::remove_const_t<pointer_t>& p) OKAYLIB_NOEXCEPT
        : m_pointer(ok::addressof(p))
    {
    }

    // NOTE: if this constructor is deleted, explicitly use ok::addressof() or
    // the & operator to convert from a pointer instead. This is in place to
    // make it slightly harder to accidentally return const references to things
    // from the stack.
    constexpr opt(pointer_t& p)
        requires ok::is_const_c<pointer_t>
    = delete;

    // implicit conversion if references are implicitly convertible
    template <typename other_t>
    constexpr opt(const opt<other_t>& other) OKAYLIB_NOEXCEPT
        requires(is_convertible_to_c<other_t, payload_t> &&
                 !stdc::is_same_v<remove_cvref_t<other_t>, opt>)
        : m_pointer(other.as_ptr())
    {
    }

    constexpr payload_t emplace(payload_t other) OKAYLIB_NOEXCEPT
    {
        m_pointer = ok::addressof(other);
        return *m_pointer;
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

    template <typename other_t>
        requires ok::detail::is_equality_comparable_to_c<pointer_t, other_t>
    [[nodiscard]] constexpr bool
    deep_compare_with(const other_t& other) OKAYLIB_NOEXCEPT
    {
        return m_pointer && *m_pointer == other;
    }

    template <typename other_payload_t>
        requires ok::detail::is_equality_comparable_to_c<pointer_t,
                                                         other_payload_t>
    [[nodiscard]] constexpr bool
    deep_compare_with(const opt<other_payload_t>& other) OKAYLIB_NOEXCEPT
    {
        if (!m_pointer != !other)
            return false;
        if (!m_pointer && !other)
            return true;
        __ok_internal_assert(m_pointer && other);
        return *m_pointer == other.ref_unchecked();
    }

    [[nodiscard]] constexpr pointer_t* as_ptr() const OKAYLIB_NOEXCEPT
    {
        return m_pointer;
    }

    /// Take the value, and call a transformation function on it before moving
    /// it into the returned optional. If this is already null, then we also
    /// return null
    template <typename callable_t>
    [[nodiscard]] constexpr auto
    take_and_run(callable_t&& callable) OKAYLIB_NOEXCEPT
        requires requires {
            {
                invoke(stdc::forward<callable_t>(callable),
                       this->ref_unchecked())
            } -> is_nonvoid_c;
        }
    {
        using ret_t = decltype(invoke(stdc::forward<callable_t>(callable),
                                      this->ref_unchecked()));
        if (this->has_value()) {
            opt<ret_t> out(invoke(stdc::forward<callable_t>(callable),
                                  this->ref_unchecked()));
            this->reset();
            return out;
        } else {
            return opt<ret_t>{};
        }
    }

  private:
    struct sentinel_t
    {};

    template <bool is_const> struct iterator_impl;
    template <bool is_const> friend struct iterator_impl;

    template <bool is_const> struct iterator_impl
    {
      private:
        stdc::conditional_t<is_const, const opt&, opt&> m_parent;
        bool m_iterated = false;

      public:
        using value_type =
            stdc::conditional_t<is_const, stdc::add_const_t<payload_t>,
                                payload_t>;
        using reference = ok::stdc::add_lvalue_reference_t<value_type>;
        using pointer = ok::stdc::add_pointer_t<value_type>;

        constexpr iterator_impl() = delete;

        constexpr iterator_impl(decltype(m_parent) parent) : m_parent(parent) {}

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            if (m_iterated || !m_parent) [[unlikely]] {
                __ok_abort("out of bounds dereference of opt iterator");
            }
            return m_parent.ref_unchecked();
        }

        constexpr iterator_impl& operator++() OKAYLIB_NOEXCEPT
        {
            m_iterated = true;
            return *this;
        }

        constexpr iterator_impl operator++(int) const OKAYLIB_NOEXCEPT
        {
            iterator_impl tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr friend bool operator==(const iterator_impl& a,
                                         const sentinel_t&) OKAYLIB_NOEXCEPT
        {
            return a.m_iterated || !a.m_parent;
        }

        constexpr friend bool operator!=(const iterator_impl& a,
                                         const sentinel_t& b) OKAYLIB_NOEXCEPT
        {
            return !(a == b);
        }
    };

  public:
    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    [[nodiscard]] constexpr iterator begin() { return iterator(*this); }
    [[nodiscard]] constexpr const_iterator begin() const
    {
        return const_iterator(*this);
    }

    [[nodiscard]] constexpr sentinel_t end() const { return {}; }

#if defined(OKAYLIB_USE_FMT)
    friend struct fmt::formatter<opt>;
#endif
};

} // namespace ok

#if defined(OKAYLIB_USE_FMT)
template <typename payload_t> struct fmt::formatter<ok::opt<payload_t>>
{
    using formatted_type_t = ok::opt<payload_t>;
    static_assert(
        formatted_type_t::impl_type == ok::detail::opt_impl_type::reference ||
            fmt::is_formattable<payload_t>::value,
        "Attempt to format an optional whose content is not formattable.");

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        return ctx.begin(); // no warnings to stay constexpr
    }

    format_context::iterator format(const ok::opt<payload_t>& optional,
                                    format_context& ctx) const
    {
        if (optional) {
            if constexpr (ok::opt<payload_t>::impl_type ==
                          ok::detail::opt_impl_type::reference) {
                if constexpr (fmt::is_formattable<typename formatted_type_t::
                                                      pointer_t>::value) {
                    return fmt::format_to(ctx.out(), "opt<{} &>",
                                          optional.ref_unchecked());
                } else {
                    // just format reference as pointer, if the contents itself
                    // can't be formatted
                    return fmt::format_to(
                        ctx.out(), "opt<{:s}: {:p}>", ok::nameof<payload_t>(),
                        static_cast<const void*>(optional.m_pointer));
                }
            } else if constexpr (ok::opt<payload_t>::impl_type ==
                                 ok::detail::opt_impl_type::object) {
                return fmt::format_to(ctx.out(), "opt<{}>",
                                      optional.ref_unchecked());
            }
        }
        return fmt::format_to(ctx.out(), "opt<{:s} : null>",
                              ok::nameof<payload_t>());
    }
};
#endif

#endif
