#ifndef __OKAYLIB_DETAIL_OPT_H__
#define __OKAYLIB_DETAIL_OPT_H__

#include "okay/detail/template_util/uninitialized_storage.h"
#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace ok::detail {

template <typename input_contained_t> struct opt_payload_base_t
{
    using contained_t = std::remove_const_t<input_contained_t>;

    opt_payload_base_t() = default;
    ~opt_payload_base_t() = default;
    opt_payload_base_t(const opt_payload_base_t&) = default;
    opt_payload_base_t(opt_payload_base_t&&) = default;
    opt_payload_base_t& operator=(const opt_payload_base_t&) = default;
    opt_payload_base_t& operator=(opt_payload_base_t&&) = default;

    // constructor overload: in_place constructor, forwards to payload
    template <typename... args_t>
    inline constexpr opt_payload_base_t(std::in_place_t tag,
                                        args_t&&... args) OKAYLIB_NOEXCEPT
        : payload(tag, std::forward<args_t>(args)...),
          has_value(true)
    {
    }

    // constructor overload: pass in a bool to first argument to cause actual
    // runtime call to copy and move constructors, in case no trivial
    inline constexpr opt_payload_base_t(bool, const opt_payload_base_t& other)
        OKAYLIB_NOEXCEPT
    {
        if (other.has_value) {
            this->construct(other.get());
        }
    }

    inline constexpr opt_payload_base_t(bool, opt_payload_base_t&& other)
        OKAYLIB_NOEXCEPT
    {
        if (other.has_value) {
            this->construct(std::move(other.get()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
            other.has_value = false;
#endif
        }
    }

    inline constexpr void
    copy_assign(const opt_payload_base_t& other) OKAYLIB_NOEXCEPT
    {
        if (this->has_value && other.has_value) {
            this->get() = other.get();
        } else {
            if (other.has_value) {
                this->construct(other.get());
            } else {
                this->reset();
            }
        }
    }

    inline constexpr void
    move_assign(opt_payload_base_t&& other) OKAYLIB_NOEXCEPT
    {
        if (this->has_value && other->has_value) {
            this->get() = std::move(other.get());
#ifndef OKAYLIB_NO_CHECKED_MOVES
            other.has_value = false;
#endif
        } else {
            if (other.has_value) {
                this->construct(std::move(other.get()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
                other.has_value = false;
#endif
            } else {
                this->reset();
            }
        }
    }

    template <typename... args_t>
    inline constexpr void construct(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        new (std::addressof(this->payload.value))
            contained_t(std::forward<args_t>(args)...);
        has_value = true;
    }

    inline constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        has_value = false;
        payload.value.~contained_t();
    }

    inline constexpr input_contained_t& get() OKAYLIB_NOEXCEPT
    {
        return this->payload.value;
    }

    inline constexpr const input_contained_t& get() const OKAYLIB_NOEXCEPT
    {
        return this->payload.value;
    }

    inline constexpr void reset() OKAYLIB_NOEXCEPT
    {
        if (has_value)
            destroy();
        else
            // improves codegen, according to comment in c++ stl i adapted this
            // from
            has_value = false;
    }

    uninitialized_storage_t<contained_t> payload;
    bool has_value = false;
};

// use template specialization to override payload functionality depending on
// the trivial-ness of the contained type
template <typename contained_t,
          bool = std::is_trivially_destructible_v<contained_t>,
          bool = std::is_trivially_copy_assignable_v<contained_t> &&
                 std::is_trivially_copy_constructible_v<contained_t>,
          bool = std::is_trivially_move_assignable_v<contained_t> &&
                 std::is_trivially_move_constructible_v<contained_t>>
struct opt_payload_t;

// full trivial, maybe constexpr
template <typename contained_t>
struct opt_payload_t<contained_t, true, true, true>
    : opt_payload_base_t<contained_t>
{
    using opt_payload_base_t<contained_t>::opt_payload_base_t;

    opt_payload_t() = default;
};

// nontrivial copy construction and/or assignment
template <typename contained_t>
struct opt_payload_t<contained_t, true, false, true>
    : opt_payload_base_t<contained_t>
{
    using opt_payload_base_t<contained_t>::opt_payload_base_t;

    opt_payload_t() = default;
    ~opt_payload_t() = default;
    opt_payload_t(const opt_payload_t&) = default;
    opt_payload_t& operator=(opt_payload_t&&) = default;
    opt_payload_t(opt_payload_t&&) = default;

    inline constexpr opt_payload_t&
    operator=(const opt_payload_t& other) OKAYLIB_NOEXCEPT
    {
        this->copy_assign(other);
        return *this;
    }
};

// nontrivial move construction and/or assignment
template <typename contained_t>
struct opt_payload_t<contained_t, true, true, false>
    : opt_payload_base_t<contained_t>
{
    using opt_payload_base_t<contained_t>::opt_payload_base_t;

    opt_payload_t() = default;
    ~opt_payload_t() = default;
    opt_payload_t(const opt_payload_t&) = default;
    opt_payload_t& operator=(const opt_payload_t&) = default;
    opt_payload_t(opt_payload_t&&) = default;

    inline constexpr opt_payload_t&
    operator=(opt_payload_t&& other) OKAYLIB_NOEXCEPT
    {
        this->move_assign(std::move(other));
        return *this;
    }
};

// nontrivial move *and* copy construction and/or assignment
template <typename contained_t>
struct opt_payload_t<contained_t, true, false, false>
    : opt_payload_base_t<contained_t>
{
    using opt_payload_base_t<contained_t>::opt_payload_base_t;

    opt_payload_t() = default;
    ~opt_payload_t() = default;
    opt_payload_t(const opt_payload_t&) = default;
    opt_payload_t(opt_payload_t&&) = default;

    inline constexpr opt_payload_t&
    operator=(opt_payload_t&& other) OKAYLIB_NOEXCEPT
    {
        this->move_assign(std::move(other));
        return *this;
    }

    inline constexpr opt_payload_t&
    operator=(const opt_payload_t& other) OKAYLIB_NOEXCEPT
    {
        this->copy_assign(other);
        return *this;
    }
};

// nontrivial destructors (everything else becomes nontrivial also)
template <typename contained_t, bool copy, bool move>
struct opt_payload_t<contained_t, false, copy, move>
    : opt_payload_t<contained_t, true, false, false>
{
    using opt_payload_t<contained_t, true, false, false>::opt_payload_t;

    opt_payload_t() = default;
    opt_payload_t(const opt_payload_t&) = default;
    opt_payload_t& operator=(const opt_payload_t&) = default;
    opt_payload_t(opt_payload_t&&) = default;
    opt_payload_t& operator=(opt_payload_t&&) = default;

    inline ~opt_payload_t() { this->reset(); }
};

// commonly used wrappers in optional implementations (the derived_t)
template <typename input_contained_t, typename derived_t>
class opt_base_common_t
{
  protected:
    using contained_t = std::remove_const_t<input_contained_t>;

    template <typename... args_t>
    inline constexpr void _construct(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        static_cast<derived_t*>(this)->payload.construct(
            std::forward<args_t>(args)...);
    }

    inline constexpr void _destroy() OKAYLIB_NOEXCEPT
    {
        static_cast<derived_t*>(this)->payload.destroy();
    }

    inline constexpr void _reset() OKAYLIB_NOEXCEPT
    {
        static_cast<derived_t*>(this)->payload.reset();
    }

    [[nodiscard]] inline constexpr bool _has_value() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const derived_t*>(this)->payload.has_value;
    }

    inline constexpr input_contained_t& _get() OKAYLIB_NOEXCEPT
    {
        assert(this->_has_value());
        return static_cast<derived_t*>(this)->payload.get();
    }

    inline constexpr const input_contained_t& _get() const OKAYLIB_NOEXCEPT
    {
        assert(this->_has_value());
        return static_cast<const derived_t*>(this)->payload.get();
    }
};

template <typename input_contained_t,
          bool = std::is_trivially_copy_constructible_v<input_contained_t>,
          bool = std::is_trivially_move_constructible_v<input_contained_t>>
struct opt_base_t
    : opt_base_common_t<input_contained_t, opt_base_t<input_contained_t>>
{
    constexpr opt_base_t() = default;

    template <
        typename... args_t,
        // TODO: enable_if better than static_assert here? is sfinae used later
        std::enable_if_t<std::is_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit opt_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    inline constexpr opt_base_t(const opt_base_t& other)
        : payload(other.payload.has_value, other.payload)
    {
    }

    inline constexpr opt_base_t(opt_base_t&& other)
        : payload(other.payload.has_value, std::move(other.payload))
    {
    }

    opt_base_t& operator=(const opt_base_t&) = default;
    opt_base_t& operator=(opt_base_t&&) = default;

    opt_payload_t<input_contained_t> payload;
};

template <typename input_contained_t>
struct opt_base_t<input_contained_t, false, true>
    : opt_base_common_t<input_contained_t, opt_base_t<input_contained_t>>
{
    constexpr opt_base_t() = default;

    template <
        typename... args_t,
        // TODO: enable_if better than static_assert here? is sfinae used later
        std::enable_if_t<std::is_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit opt_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    inline constexpr opt_base_t(const opt_base_t& other)
        : payload(other.payload.has_value, other.payload)
    {
    }

    // trivial move, can default this in this override
    inline constexpr opt_base_t(opt_base_t&& other) = default;

    opt_base_t& operator=(const opt_base_t&) = default;
    opt_base_t& operator=(opt_base_t&&) = default;

    opt_payload_t<input_contained_t> payload;
};

template <typename input_contained_t>
struct opt_base_t<input_contained_t, true, false>
    : opt_base_common_t<input_contained_t, opt_base_t<input_contained_t>>
{
    constexpr opt_base_t() = default;

    template <
        typename... args_t,
        // TODO: enable_if better than static_assert here? is sfinae used later
        std::enable_if_t<std::is_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit opt_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    // trivial copy, can default
    constexpr opt_base_t(const opt_base_t& other) = default;

    inline constexpr opt_base_t(opt_base_t&& other)
        : payload(other.payload.has_value, std::move(other.payload))
    {
    }

    opt_base_t& operator=(const opt_base_t&) = default;
    opt_base_t& operator=(opt_base_t&&) = default;

    opt_payload_t<input_contained_t> payload;
};

template <typename input_contained_t>
struct opt_base_t<input_contained_t, true, true>
    : opt_base_common_t<input_contained_t, opt_base_t<input_contained_t>>
{
    constexpr opt_base_t() = default;

    template <
        typename... args_t,
        // TODO: enable_if better than static_assert here? is sfinae used later
        std::enable_if_t<std::is_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit opt_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    // all trivial
    constexpr opt_base_t(const opt_base_t& other) = default;
    constexpr opt_base_t(opt_base_t&& other) = default;

    opt_base_t& operator=(const opt_base_t&) = default;
    opt_base_t& operator=(opt_base_t&&) = default;

    opt_payload_t<input_contained_t> payload;
};
} // namespace ok::detail

namespace ok {
template <typename payload_t, typename enable_t = void> class opt_t;
}

namespace ok::detail {

// SFINAE to check if payload_t is instance of optional template
template <typename T> inline constexpr bool is_optional = false;
template <typename T> inline constexpr bool is_optional<opt_t<T>> = true;

template <typename target_t, typename opt_payload_t>
inline constexpr bool converts_from_opt =
    // check if can construct target from optional
    std::is_constructible_v<target_t, const opt_t<opt_payload_t>&> ||
    std::is_constructible_v<target_t, opt_t<opt_payload_t>&> ||
    std::is_constructible_v<target_t, const opt_t<opt_payload_t>&&> ||
    std::is_constructible_v<target_t, opt_t<opt_payload_t>&&> ||
    // check if can convert optional to target
    std::is_convertible_v<const opt_t<opt_payload_t>&, target_t> ||
    std::is_convertible_v<opt_t<opt_payload_t>&, target_t> ||
    std::is_convertible_v<const opt_t<opt_payload_t>&&, target_t> ||
    std::is_convertible_v<opt_t<opt_payload_t>&&, target_t>;

template <typename target_t, typename opt_payload_t>
inline constexpr bool assigns_from_opt =
    std::is_assignable_v<target_t&, const opt_t<opt_payload_t>&> ||
    std::is_assignable_v<target_t&, opt_t<opt_payload_t>&> ||
    std::is_assignable_v<target_t&, const opt_t<opt_payload_t>&&> ||
    std::is_assignable_v<target_t&, opt_t<opt_payload_t>&&>;

} // namespace ok::detail

#endif
