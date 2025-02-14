#ifndef __OKAYLIB_OPT_H__
#define __OKAYLIB_OPT_H__

#include "okay/detail/noexcept.h"
#include "okay/detail/opt.h"
#include "okay/detail/template_util/enable_copy_move.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/is_nonthrowing.h"
#include "okay/ranges/ranges.h"
#include "okay/slice.h"
#include <cstring> // memcpy

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
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
}

template <typename payload_t, typename>
class opt_t : private detail::opt_base_t<payload_t>,
              private detail::enable_copy_move_opt_for_t<payload_t>
{
  public:
    // type constraints
    static_assert(!std::is_rvalue_reference_v<payload_t>,
                  "opt_t cannot store rvalue references");
    static_assert(!std::is_const_v<payload_t>,
                  "Wrapped value type is marked const, this has no effect. "
                  "Remove the const.");
    static_assert(detail::is_nonthrowing<payload_t>,
                  OKAYLIB_IS_NONTHROWING_ERRMSG);

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
    inline static constexpr bool not_self =
        !std::is_same_v<opt_t, detail::remove_cvref_t<T>>;
    template <typename T>
    inline static constexpr bool not_tag =
        !std::is_same_v<std::in_place_t, detail::remove_cvref_t<T>>;

    template <bool condition>
    using requires_t = std::enable_if_t<condition, bool>;

  public:
    constexpr opt_t() OKAYLIB_NOEXCEPT {}
    constexpr opt_t(nullopt_t) OKAYLIB_NOEXCEPT {}

    // constructors which perform conversion of incoming type
    template <
        typename convert_from_t = payload_t,
        requires_t<not_self<convert_from_t> && not_tag<convert_from_t> &&
                   std::is_constructible_v<payload_t, convert_from_t> &&
                   std::is_convertible_v<convert_from_t, payload_t>> = true>
    inline constexpr opt_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
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
    inline explicit constexpr opt_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
        : base_t(std::in_place, std::forward<convert_from_t>(t))
    {
    }

    // converting constructor which takes optional of another convertible type
    template <
        typename incoming_t,
        requires_t<!std::is_same_v<payload_t, incoming_t> &&
                   std::is_constructible_v<payload_t, const incoming_t&> &&
                   std::is_convertible_v<const incoming_t&, payload_t> &&
                   !detail::converts_from_opt<payload_t, incoming_t>> = true>
    inline constexpr opt_t(const opt_t<incoming_t>& t) OKAYLIB_NOEXCEPT
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
                   !detail::converts_from_opt<payload_t, incoming_t>> = false>
    inline explicit constexpr opt_t(const opt_t<incoming_t>& t) OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

    // convert an optional of a convertible type being moved into this optional
    template <
        typename incoming_t,
        requires_t<!std::is_same_v<payload_t, incoming_t> &&
                   std::is_constructible_v<payload_t, incoming_t> &&
                   std::is_convertible_v<incoming_t, payload_t> &&
                   !detail::converts_from_opt<payload_t, incoming_t>> = true>
    inline constexpr opt_t(opt_t<incoming_t>&& t) OKAYLIB_NOEXCEPT
    {
        if (t) {
            emplace(std::move(t.value()));
        }
    }

    // also convert moved optional's contents into ours, but this time explicit
    // because the two are not implicitly convertible
    template <
        typename incoming_t,
        requires_t<!std::is_same_v<payload_t, incoming_t> &&
                   std::is_constructible_v<payload_t, incoming_t> &&
                   !std::is_convertible_v<incoming_t, payload_t> &&
                   !detail::converts_from_opt<payload_t, incoming_t>> = false>
    inline explicit constexpr opt_t(opt_t<incoming_t>&& t) OKAYLIB_NOEXCEPT
    {
        if (t) {
            emplace(std::move(t.value()));
        }
    }

    // emplacement constructor
    template <typename... args_t,
              requires_t<std::is_constructible_v<payload_t, args_t...>> = false>
    explicit constexpr opt_t(std::in_place_t, args_t&&... args) OKAYLIB_NOEXCEPT
        : base_t(std::in_place, std::forward<args_t>(args)...)
    {
    }

    opt_t& operator=(nullopt_t) OKAYLIB_NOEXCEPT
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
        opt_t&>
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
        opt_t&>
    operator=(const opt_t<incoming_t>& incoming) OKAYLIB_NOEXCEPT
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
                     opt_t&>
    operator=(opt_t<incoming_t>&& incoming) OKAYLIB_NOEXCEPT
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
    void swap(opt_t& other) OKAYLIB_NOEXCEPT
    {
        using std::swap;
        if (this->_has_value() && other._has_value())
            swap(this->_get(), other._get());
        else if (this->_has_value()) {
            other._construct(std::move(this->_get()));
            this->_destruct();
        } else if (other._has_value()) {
            this->_construct(std::move(other._get()));
            other._destruct();
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
            __ok_abort();
        }
        return this->_get();
    }

    [[nodiscard]] inline payload_t&& value() && OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort();
        }
        return std::move(this->_get());
    }

    inline const payload_t& value() const& OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort();
        }
        return this->_get();
    }

    /// Call destructor of internal type, or just reset it if it doesnt have one
    inline void reset() OKAYLIB_NOEXCEPT { this->_reset(); }

    inline constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    inline constexpr friend bool operator==(const opt_t& a, const opt_t& b)
    {
        if (a.has_value() != b.has_value())
            return false;

        return !a.has_value() || a._get() == b._get();
    }

    // static_assert(std::is_same_v<const opt_inner_t&, int>);

    inline constexpr friend bool operator==(const opt_t& a, const payload_t& b)
    {
        return a.has_value() && a._get() == b;
    }

    inline constexpr friend bool operator==(const payload_t& b, const opt_t& a)
    {
        return a.has_value() && a._get() == b;
    }

    // TODO: does c++17 allow autogenerating this or is it just c++20
    inline constexpr friend bool operator!=(const opt_t& a, const opt_t& b)
    {
        return !(a == b);
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_t>;
#endif
};

// template specialization for lvalue references
template <typename payload_t>
class opt_t<payload_t, std::enable_if_t<std::is_lvalue_reference_v<payload_t>>>
{
  public:
    using pointer_t = std::remove_reference_t<payload_t>;

  private:
    inline static constexpr bool is_reference = true;
    inline static constexpr bool is_slice = false;

    pointer_t* pointer = nullptr;

    template <typename T>
    inline static constexpr bool not_self =
        !std::is_same_v<opt_t, detail::remove_cvref_t<T>>;

  public:
    constexpr opt_t() = default;
    inline constexpr opt_t(nullopt_t) OKAYLIB_NOEXCEPT {};

    // allow pointer conversion
    inline constexpr opt_t(pointer_t* p) OKAYLIB_NOEXCEPT : pointer(p) {}
    // allow non-const reference construction
    inline constexpr opt_t(std::remove_const_t<payload_t> p) OKAYLIB_NOEXCEPT
        : pointer(ok::addressof(p))
    {
    }

    // implicit conversion if references are implicitly convertible
    template <typename other_t,
              std::enable_if_t<
                  std::is_convertible_v<other_t, payload_t> &&
                      !std::is_same_v<detail::remove_cvref_t<other_t>, opt_t>,
                  bool> = false>
    inline constexpr opt_t(const opt_t<other_t>& other) OKAYLIB_NOEXCEPT
        : pointer(other.as_ptr())
    {
    }

    inline constexpr payload_t emplace(payload_t other) OKAYLIB_NOEXCEPT
    {
        // this function just to keep APIs of optionals as similar as possible
        pointer = ok::addressof(other);
    }

    inline constexpr bool has_value() const OKAYLIB_NOEXCEPT
    {
        return pointer != nullptr;
    }

    inline constexpr explicit operator bool() const OKAYLIB_NOEXCEPT
    {
        return has_value();
    }

    inline constexpr opt_t& operator=(payload_t ref) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_lvalue_reference_v<decltype(ref)>);
        pointer = ok::addressof(ref);
        return *this;
    }

    inline constexpr opt_t& operator=(nullopt_t) OKAYLIB_NOEXCEPT
    {
        pointer = nullptr;
        return *this;
    }

    inline constexpr void reset() OKAYLIB_NOEXCEPT { pointer = nullptr; }

    [[nodiscard]] inline constexpr payload_t value() const OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort();
        }
        return *pointer;
    }

    [[nodiscard]] inline constexpr bool
    is_alias_for(const pointer_t& other) OKAYLIB_NOEXCEPT
    {
        return pointer == ok::addressof(other);
    }

    [[nodiscard]] inline constexpr bool
    is_alias_for(const opt_t& other) OKAYLIB_NOEXCEPT
    {
        return pointer && pointer == other.pointer;
    }

    [[nodiscard]] inline constexpr pointer_t* as_ptr() const OKAYLIB_NOEXCEPT
    {
        return pointer;
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_t>;
#endif
};

// template specialization for slices
template <typename wrapped_slice_t>
class opt_t<
    wrapped_slice_t,
    std::enable_if_t<(detail::is_instance<wrapped_slice_t, ok::slice_t>())>>
{
    inline static constexpr bool is_reference = false;
    inline static constexpr bool is_slice = true;

    using viewed_t = typename wrapped_slice_t::value_type;

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
    inline static constexpr bool not_self =
        !std::is_same_v<opt_t, detail::remove_cvref_t<T>>;
    template <typename T>
    inline static constexpr bool not_tag =
        !std::is_same_v<std::in_place_t, detail::remove_cvref_t<T>>;

    template <bool condition>
    using requires_t = std::enable_if_t<condition, bool>;

  public:
    static_assert(!std::is_const_v<wrapped_slice_t>,
                  "Wrapped slice type is marked const, this has no effect. "
                  "Remove the const.");

    constexpr opt_t() = default;
    inline constexpr opt_t(nullopt_t) OKAYLIB_NOEXCEPT {};

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

    inline constexpr explicit operator bool() const OKAYLIB_NOEXCEPT
    {
        return has_value();
    }

    inline constexpr opt_t&
    operator=(const wrapped_slice_t& ref) OKAYLIB_NOEXCEPT
    {
        emplace(ref);
    }

    inline constexpr opt_t& operator=(nullopt_t) OKAYLIB_NOEXCEPT
    {
        reset();
        return *this;
    }

    inline constexpr void reset() OKAYLIB_NOEXCEPT { data = nullptr; }

    [[nodiscard]] inline constexpr wrapped_slice_t& value() OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort();
        }
        return unchecked_value();
    }

    [[nodiscard]] inline constexpr const wrapped_slice_t&
    value() const OKAYLIB_NOEXCEPT
    {
        if (!has_value()) [[unlikely]] {
            __ok_abort();
        }
        return unchecked_value();
    }

    template <
        typename convert_from_t = wrapped_slice_t,
        requires_t<not_self<convert_from_t> && not_tag<convert_from_t> &&
                   std::is_constructible_v<wrapped_slice_t, convert_from_t> &&
                   std::is_convertible_v<convert_from_t, wrapped_slice_t>> =
            true>
    inline constexpr opt_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
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
    inline explicit constexpr opt_t(convert_from_t&& t) OKAYLIB_NOEXCEPT
    {
        new (reinterpret_cast<wrapped_slice_t*>(this))
            wrapped_slice_t(std::forward<convert_from_t>(t));
    }

    // converting constructor which takes optional of another convertible type
    template <
        typename incoming_t,
        requires_t<
            !std::is_same_v<wrapped_slice_t, incoming_t> &&
            std::is_constructible_v<wrapped_slice_t, const incoming_t&> &&
            std::is_convertible_v<const incoming_t&, wrapped_slice_t> &&
            !detail::converts_from_opt<wrapped_slice_t, incoming_t>> = true>
    inline constexpr opt_t(const opt_t<incoming_t>& t) OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

    // same as above, but incoming type is not convertible, so construction
    // should be explicit
    template <
        typename incoming_t,
        requires_t<
            !std::is_same_v<wrapped_slice_t, incoming_t> &&
            std::is_constructible_v<wrapped_slice_t, const incoming_t&> &&
            !std::is_convertible_v<const incoming_t&, wrapped_slice_t> &&
            !detail::converts_from_opt<wrapped_slice_t, incoming_t>> = false>
    inline explicit constexpr opt_t(const opt_t<incoming_t>& t) OKAYLIB_NOEXCEPT
    {
        if (t)
            emplace(t.value());
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<opt_t>;
#endif
};

template <typename payload_t> struct ok::range_definition<opt_t<payload_t>>
{
    struct cursor_t
    {
        friend class range_definition;

      private:
        bool is_out_of_bounds = false;
    };

    using opt_range_t = opt_t<payload_t>;

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
        return range.has_value() && !cursor.is_out_of_bounds;
    }

    static constexpr size_t size(const opt_range_t& range) OKAYLIB_NOEXCEPT
    {
        return size_t(range.has_value());
    }

    static constexpr auto& get_ref(opt_range_t& range, const cursor_t& cursor)
    {
        return range.value();
    }

    static constexpr const auto& get_ref(const opt_range_t& range,
                                         const cursor_t& cursor)
    {
        return range.value();
    }
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename payload_t> struct fmt::formatter<ok::opt_t<payload_t>>
{
    using formatted_type_t = ok::opt_t<payload_t>;
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

    format_context::iterator format(const ok::opt_t<payload_t>& optional,
                                    format_context& ctx) const
    {
        if (optional.has_value()) {
            if constexpr (ok::opt_t<payload_t>::is_reference) {
                if constexpr (fmt::is_formattable<typename formatted_type_t::
                                                      pointer_t>::value) {
                    return fmt::format_to(ctx.out(), "{}", optional.value());
                } else {
                    // just format reference as pointer, if the contents itself
                    // can't be formatted
                    return fmt::format_to(ctx.out(), "{:p}", optional.pointer);
                }
            } else if constexpr (ok::opt_t<payload_t>::is_slice) {
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
