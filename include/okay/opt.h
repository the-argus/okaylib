#ifndef __OKAYLIB_OPT_H__
#define __OKAYLIB_OPT_H__

#include "okay/detail/opt.h"
#include "okay/detail/template_util/enable_copy_move.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/slice.h"

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

struct nullopt_t
{};

inline constexpr nullopt_t nullopt{};

namespace detail {
template <typename T>
using enable_copy_move_opt_for_t = detail::enable_copy_move<
    std::is_copy_constructible_v<T>,
    std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
    std::is_move_constructible_v<T>,
    std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
    // this is the "tag" for enable_copy_move. it must be unique per
    // specialization
    opt_t<T>>;

template <typename payload_t,
          // TODO: make is_instance a bool, not a function
          bool is_slice = detail::is_instance<payload_t, ok::slice_t>(),
          bool is_reference = std::is_lvalue_reference_v<payload_t>>
class opt_inner_t;

// optional which is not
template <typename payload_t>
class opt_inner_t<payload_t, false, false>
    : private detail::opt_base_t<payload_t>,
      private detail::enable_copy_move_opt_for_t<payload_t>
{
  public:
    // type constraints
    static_assert(std::is_nothrow_destructible_v<payload_t>,
                  "opt_t does not support throwing destructors, mark the inner "
                  "type's destructor as noexcept.");
    static_assert(!std::is_rvalue_reference_v<payload_t>,
                  "opt_t cannot store rvalue references");

    static_assert(!std::is_const_v<payload_t>,
                  "Wrapped value type is marked const, this has no effect. "
                  "Remove the const.");

    static_assert(!std::is_same_v<std::remove_cv_t<payload_t>, nullopt_t>);
    static_assert(
        !std::is_same_v<std::remove_cv_t<payload_t>, std::in_place_t>);
    static_assert(!std::is_array_v<payload_t>,
                  "opt cannot contain C style array");
    static_assert(std::is_object_v<payload_t>);

  private:
    inline constexpr static bool is_reference = false;
    inline constexpr static bool is_slice = false;

    using base_t = detail::opt_base_t<payload_t>;

    template <typename T>
    using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
    template <typename T>
    inline static constexpr bool not_self =
        !std::is_same_v<opt_inner_t, remove_cvref_t<T>>;
    template <typename T>
    inline static constexpr bool not_tag =
        !std::is_same_v<std::in_place_t, remove_cvref_t<T>>;

    template <bool condition>
    using requires_t = std::enable_if_t<condition, bool>;

  public:
    constexpr opt_inner_t() OKAYLIB_NOEXCEPT {}
    constexpr opt_inner_t(nullopt_t) OKAYLIB_NOEXCEPT {}

    // constructors which perform conversion of incoming type
    template <
        typename convert_from_t = payload_t,
        requires_t<not_self<convert_from_t> && not_tag<convert_from_t> &&
                   std::is_constructible_v<payload_t, convert_from_t> &&
                   std::is_convertible_v<convert_from_t, payload_t>> = true>
    inline constexpr opt_inner_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
        : base_t(std::in_place, std::forward<convert_from_t>(t))
    {
    }

    // converting constructor for types which are non convertible. only
    // difference is that it is marked explicit
    template <
        typename convert_from_t = payload_t,
        requires_t<not_self<convert_from_t> && not_tag<convert_from_t> &&
                   std::is_constructible_v<payload_t, convert_from_t> &&
                   !std::is_convertible_v<convert_from_t, payload_t>> = false>
    inline explicit constexpr opt_inner_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
        : base_t(std::in_place, std::forward<convert_from_t>(t))
    {
    }

    // converting constructor which takes optional of another convertible type
    template <
        typename incoming_t,
        requires_t<!std::is_same_v<payload_t, incoming_t> &&
                   std::is_constructible_v<payload_t, const incoming_t&> &&
                   std::is_convertible_v<const incoming_t&, payload_t> &&
                   !converts_from_opt<payload_t, incoming_t>> = true>
    inline constexpr opt_inner_t(const opt_inner_t<incoming_t>& t)
        OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

    // same as above, but incoming type is not convertible, so construction
    // should be explicit
    template <
        typename incoming_t,
        requires_t<!std::is_same_v<payload_t, incoming_t> &&
                   std::is_constructible_v<payload_t, const incoming_t&> &&
                   !std::is_convertible_v<const incoming_t&, payload_t> &&
                   !converts_from_opt<payload_t, incoming_t>> = false>
    inline explicit constexpr opt_inner_t(const opt_inner_t<incoming_t>& t)
        OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

    // convert an optional of a convertible type being moved into this optional
    template <typename incoming_t,
              requires_t<!std::is_same_v<payload_t, incoming_t> &&
                         std::is_constructible_v<payload_t, incoming_t> &&
                         std::is_convertible_v<incoming_t, payload_t> &&
                         !converts_from_opt<payload_t, incoming_t>> = true>
    inline constexpr opt_inner_t(opt_inner_t<incoming_t>&& t) OKAYLIB_NOEXCEPT
    {
        if (t) {
            emplace(std::move(t.value()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
            t = nullopt;
#endif
        }
    }

    // also convert moved optional's contents into ours, but this time explicit
    // because the two are not implicitly convertible
    template <typename incoming_t,
              requires_t<!std::is_same_v<payload_t, incoming_t> &&
                         std::is_constructible_v<payload_t, incoming_t> &&
                         !std::is_convertible_v<incoming_t, payload_t> &&
                         !converts_from_opt<payload_t, incoming_t>> = false>
    inline explicit constexpr opt_inner_t(opt_inner_t<incoming_t>&& t)
        OKAYLIB_NOEXCEPT
    {
        if (t) {
            emplace(std::move(t.value()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
            t = nullopt;
#endif
        }
    }

    // emplacement constructor
    template <typename... args_t,
              requires_t<std::is_constructible_v<payload_t, args_t...>> = false>
    explicit constexpr opt_inner_t(std::in_place_t,
                                   args_t&&... args) OKAYLIB_NOEXCEPT
        : base_t(std::in_place, std::forward<args_t>(args)...)
    {
    }

    opt_inner_t& operator=(nullopt_t) OKAYLIB_NOEXCEPT
    {
        this->_reset();
        return *this;
    }

    template <typename incoming_t>
    // TODO: _GLIBCXX20_CONSTEXPR here?
    std::enable_if_t<
        not_self<incoming_t> &&
            // TODO: cant have payload and decayed incoming_t be the same, only
            // if scalar though. why? copied this from STL implementation
            !(std::is_scalar_v<payload_t> &&
              std::is_same_v<payload_t, std::decay_t<incoming_t>>) &&
            std::is_constructible_v<payload_t, incoming_t> &&
            std::is_assignable_v<payload_t&, incoming_t>,
        opt_inner_t&>
    operator=(incoming_t&& incoming) OKAYLIB_NOEXCEPT
    {
        if (this->_has_value()) {
            this->_get() = std::forward<incoming_t>(incoming);
        } else {
            this->_construct(std::forward<incoming_t>(incoming));
        }
        return *this;
    }

    // converting opt constructor: if the inner types of two opts can
    // be converted, allow the opts to be converted
    template <typename incoming_t>
    std::enable_if_t<
        !std::is_same_v<payload_t, incoming_t> &&
            std::is_constructible_v<payload_t, const incoming_t&> &&
            std::is_assignable_v<payload_t&, const incoming_t&> &&
            !detail::converts_from_opt<payload_t, incoming_t> &&
            !detail::assigns_from_opt<payload_t, incoming_t>,
        opt_inner_t&>
    operator=(const opt_inner_t<incoming_t>& incoming) OKAYLIB_NOEXCEPT
    {
        if (incoming) {
            if (this->_has_value()) {
                this->_get() = incoming.value();
            } else {
                this->_construct(incoming.value());
            }
        } else {
            this->_reset();
        }
        return *this;
    }

    // variant of above converting opt constructor which performs move
    template <typename incoming_t>
    std::enable_if_t<!std::is_same_v<payload_t, incoming_t> &&
                         std::is_constructible_v<payload_t, incoming_t> &&
                         std::is_assignable_v<payload_t&, incoming_t> &&
                         !detail::converts_from_opt<payload_t, incoming_t> &&
                         !detail::assigns_from_opt<payload_t, incoming_t>,
                     opt_inner_t&>
    operator=(opt_inner_t<incoming_t>&& incoming) OKAYLIB_NOEXCEPT
    {
        if (incoming) {
            if (this->_has_value()) {
                this->_get() = std::move(incoming.value());
            } else {
                this->_construct(std::move(incoming.value()));
            }
        } else {
            this->_reset();
        }
        return *this;
    }

    template <typename... args_t>
    // TODO: _GLIBCXX20_CONSTEXPR? is enable_if here better than static-assert?
    std::enable_if_t<std::is_constructible_v<payload_t, args_t...>, payload_t&>
    emplace(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        this->_reset();
        this->_construct(std::forward<args_t>(args)...);
        return this->_get();
    }

    // TODO: _GLIBCXX20_CONSTEXPR?
    void swap(opt_inner_t& other) OKAYLIB_NOEXCEPT
    {
        using std::swap;
        if (this->_has_value() && other._has_value())
            swap(this->_get(), other._get());
        else if (this->_has_value()) {
            other._construct(std::move(this->_get()));
            this->_destruct();
#ifndef OKAYLIB_NO_CHECKED_MOVES
            *this = nullopt;
#endif
        } else if (other._has_value()) {
            this->_construct(std::move(other._get()));
            other._destruct();
#ifndef OKAYLIB_NO_CHECKED_MOVES
            other = nullopt;
#endif
        }
    }

    [[nodiscard]] inline bool has_value() const OKAYLIB_NOEXCEPT
    {
        return this->_has_value();
    }

    /// Extract the inner value of the optional, or abort the program. Check
    /// has_value() before calling this.
    [[nodiscard]] inline payload_t& value() & OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return this->_get();
    }

    [[nodiscard]] inline payload_t&& value() && OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return std::move(this->_get());
    }

    inline const payload_t& value() const& OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return this->_get();
    }

    /// Call destructor of internal type, or just reset it if it doesnt have one
    inline void reset() OKAYLIB_NOEXCEPT { this->_reset(); }

    inline constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    inline constexpr friend bool operator==(const opt_inner_t& a,
                                            const opt_inner_t& b)
    {
        if (a.has_value() != b.has_value())
            return false;

        return !a.has_value() || a._get() == b._get();
    }

    // static_assert(std::is_same_v<const opt_inner_t&, int>);

    inline constexpr friend bool operator==(const opt_inner_t& a,
                                            const payload_t& b)
    {
        return a.has_value() && a._get() == b;
    }

    inline constexpr friend bool operator==(const payload_t& b,
                                            const opt_inner_t& a)
    {
        return a.has_value() && a._get() == b;
    }

    // TODO: does c++17 allow autogenerating this or is it just c++20
    inline constexpr friend bool operator!=(const opt_inner_t& a,
                                            const opt_inner_t& b)
    {
        return !(a == b);
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_inner_t>;
#endif
};

// template specialization for lvalue references
template <typename payload_t> class opt_inner_t<payload_t, false, true>
{
    inline static constexpr bool is_reference = true;
    inline static constexpr bool is_slice = false;

    using pointer_t = std::remove_reference_t<payload_t>;

    pointer_t* pointer = nullptr;

    template <typename T>
    using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
    template <typename T>
    inline static constexpr bool not_self =
        !std::is_same_v<opt_inner_t, remove_cvref_t<T>>;

  public:
    constexpr opt_inner_t() = default;
    inline constexpr opt_inner_t(nullopt_t) OKAYLIB_NOEXCEPT {};

    // allow pointer conversion
    inline constexpr opt_inner_t(pointer_t* p) : OKAYLIB_NOEXCEPT pointer(p) {}
    // allow reference construction
    // TODO: does this cover all convertible references, like child class refs?
    inline constexpr opt_inner_t(payload_t p) : OKAYLIB_NOEXCEPT pointer(&p) {}

    inline constexpr payload_t emplace(payload_t other) OKAYLIB_NOEXCEPT
    {
        pointer = &other;
    }

    inline constexpr bool has_value() const OKAYLIB_NOEXCEPT
    {
        return pointer != nullptr;
    }

    inline constexpr operator bool() const OKAYLIB_NOEXCEPT
    {
        return has_value();
    }

    inline constexpr opt_inner_t& operator=(pointer_t& ref) OKAYLIB_NOEXCEPT
    {
        pointer = &ref;
        return *this;
    }

    inline constexpr opt_inner_t& operator=(nullopt_t) OKAYLIB_NOEXCEPT
    {
        pointer = nullptr;
        return *this;
    }

    inline constexpr void reset() OKAYLIB_NOEXCEPT { pointer = nullptr; }

    [[nodiscard]] inline constexpr payload_t value() OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return *pointer;
    }

    [[nodiscard]] inline constexpr payload_t value() const OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return *pointer;
    }

    [[nodiscard]] inline constexpr bool is_alias(const opt_inner_t& other)
    {
        return pointer == other.pointer;
    }

    [[nodiscard]] inline constexpr bool is_alias(const pointer_t& other)
    {
        return pointer == &other;
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_inner_t>;
#endif
};

// template specialization for slices
template <typename wrapped_slice_t>
class opt_inner_t<wrapped_slice_t, true, false>
{
    inline static constexpr bool is_reference = false;
    inline static constexpr bool is_slice = true;

    using viewed_t = typename wrapped_slice_t::type;

    // layout matches slice
    size_t elements;
    viewed_t* data = nullptr;

    [[nodiscard]] inline constexpr wrapped_slice_t&
    unchecked_value() OKAYLIB_NOEXCEPT
    {
        return *reinterpret_cast<wrapped_slice_t*>(this);
    }
    [[nodiscard]] inline constexpr const wrapped_slice_t&
    unchecked_value() const OKAYLIB_NOEXCEPT
    {
        return *reinterpret_cast<const wrapped_slice_t*>(this);
    }

    template <typename T>
    using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
    template <typename T>
    inline static constexpr bool not_self =
        !std::is_same_v<opt_inner_t, remove_cvref_t<T>>;
    template <typename T>
    inline static constexpr bool not_tag =
        !std::is_same_v<std::in_place_t, remove_cvref_t<T>>;

    template <bool condition>
    using requires_t = std::enable_if_t<condition, bool>;

  public:
    static_assert(!std::is_const_v<wrapped_slice_t>,
                  "Wrapped slice type is marked const, this has no effect. "
                  "Remove the const.");

    constexpr opt_inner_t() = default;
    inline constexpr opt_inner_t(nullopt_t) OKAYLIB_NOEXCEPT {};

    inline constexpr wrapped_slice_t&
    emplace(wrapped_slice_t other) OKAYLIB_NOEXCEPT
    {
        std::memcpy(this, &other, sizeof(*this));
        // elements = other.size();
        // data = other.data();
        return *reinterpret_cast<wrapped_slice_t*>(this);
    }

    inline constexpr bool has_value() const OKAYLIB_NOEXCEPT
    {
        return data != nullptr;
    }

    inline constexpr operator bool() const OKAYLIB_NOEXCEPT
    {
        return has_value();
    }

    inline constexpr opt_inner_t&
    operator=(const wrapped_slice_t& ref) OKAYLIB_NOEXCEPT
    {
        emplace(ref);
    }

    inline constexpr opt_inner_t& operator=(nullopt_t) OKAYLIB_NOEXCEPT
    {
        reset();
        return *this;
    }

    inline constexpr void reset() OKAYLIB_NOEXCEPT { data = nullptr; }

    [[nodiscard]] inline constexpr wrapped_slice_t& value() OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return unchecked_value();
    }

    [[nodiscard]] inline constexpr const wrapped_slice_t&
    value() const OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            OK_ABORT();
        }
        return unchecked_value();
    }

    inline constexpr friend bool operator==(const opt_inner_t& a,
                                            const opt_inner_t& b)
    {
        if (a.has_value() != b.has_value())
            return false;

        return !a.has_value() || // both nullopt
                                 // invoke slice == operator
               a.unchecked_value() == b.unchecked_value();
    }

    inline constexpr friend bool operator==(const opt_inner_t& a,
                                            const wrapped_slice_t& b)
    {
        return a.has_value() && a.unchecked_value() == b;
    }

    // TODO: does c++17 allow autogenerating this or is it just c++20
    inline constexpr friend bool operator!=(const opt_inner_t& a,
                                            const opt_inner_t& b)
    {
        return !(a == b);
    }

    template <
        typename convert_from_t = wrapped_slice_t,
        requires_t<not_self<convert_from_t> && not_tag<convert_from_t> &&
                   std::is_constructible_v<wrapped_slice_t, convert_from_t> &&
                   std::is_convertible_v<convert_from_t, wrapped_slice_t>> =
            true>
    inline constexpr opt_inner_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
    {
        new (reinterpret_cast<wrapped_slice_t*>(this))
            wrapped_slice_t(std::forward<convert_from_t>(t));
    }
    // TODO: factor out converting constructors into mixin/helper class

    // converting constructor for types which are non convertible. only
    // difference is that it is marked explicit
    template <
        typename convert_from_t = wrapped_slice_t,
        requires_t<not_self<convert_from_t> && not_tag<convert_from_t> &&
                   std::is_constructible_v<wrapped_slice_t, convert_from_t> &&
                   !std::is_convertible_v<convert_from_t, wrapped_slice_t>> =
            false>
    inline explicit constexpr opt_inner_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
    {
        new (reinterpret_cast<wrapped_slice_t*>(this))
            wrapped_slice_t(std::forward<convert_from_t>(t));
    }

    // converting constructor which takes optional of another convertible type
    template <typename incoming_t,
              requires_t<
                  !std::is_same_v<wrapped_slice_t, incoming_t> &&
                  std::is_constructible_v<wrapped_slice_t, const incoming_t&> &&
                  std::is_convertible_v<const incoming_t&, wrapped_slice_t> &&
                  !converts_from_opt<wrapped_slice_t, incoming_t>> = true>
    inline constexpr opt_inner_t(const opt_inner_t<incoming_t>& t)
        OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

    // same as above, but incoming type is not convertible, so construction
    // should be explicit
    template <typename incoming_t,
              requires_t<
                  !std::is_same_v<wrapped_slice_t, incoming_t> &&
                  std::is_constructible_v<wrapped_slice_t, const incoming_t&> &&
                  !std::is_convertible_v<const incoming_t&, wrapped_slice_t> &&
                  !converts_from_opt<wrapped_slice_t, incoming_t>> = false>
    inline explicit constexpr opt_inner_t(const opt_inner_t<incoming_t>& t)
        OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_inner_t>;
#endif
};

} // namespace detail

template <typename payload_t> using opt_t = detail::opt_inner_t<payload_t>;

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename payload_t>
struct fmt::formatter<ok::detail::opt_inner_t<payload_t>>
{
    using formatted_type_t = ok::detail::opt_inner_t<payload_t>;
    static_assert(
        formatted_type_t::is_reference || fmt::is_formattable<payload_t>::value,
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

    format_context::iterator
    format(const ok::detail::opt_inner_t<payload_t>& optional,
           format_context& ctx) const
    {
        if (optional.has_value()) {
            if constexpr (ok::detail::opt_inner_t<payload_t>::is_reference) {
                if constexpr (fmt::is_formattable<typename formatted_type_t::
                                                      pointer_t>::value) {
                    return fmt::format_to(ctx.out(), "{}", optional.value());
                } else {
                    // just format reference as pointer, if the contents itself
                    // can't be formatted
                    return fmt::format_to(ctx.out(), "{:p}", optional.pointer);
                }
            } else if constexpr (ok::detail::opt_inner_t<payload_t>::is_slice) {
                return fmt::format_to(
                    ctx.out(), "{}",
                    *reinterpret_cast<const payload_t*>(&optional));
            } else {
                return fmt::format_to(ctx.out(), "{}", optional._get());
            }
        }
        return fmt::format_to(ctx.out(), "null");
    }
};
#endif

#endif
