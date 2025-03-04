#ifndef __OKAYLIB_DETAIL_RES_H__
#define __OKAYLIB_DETAIL_RES_H__

#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/special_member_traits.h"
#include <cassert>
#include <type_traits>
#include <utility>

namespace ok::detail {
template <typename input_contained_t, typename enum_int_t>
struct res_payload_base_t
{
    using contained_t = std::remove_const_t<input_contained_t>;

    // default everything because special member behavior is determined by the
    // selected template specialization for our "storage" member var
    res_payload_base_t() = default;
    ~res_payload_base_t() = default;
    res_payload_base_t(const res_payload_base_t&) = default;
    res_payload_base_t(res_payload_base_t&&) = default;
    res_payload_base_t& operator=(const res_payload_base_t&) = default;
    res_payload_base_t& operator=(res_payload_base_t&&) = default;

    // construct storage value in-place
    template <typename... args_t>
    inline constexpr res_payload_base_t(std::in_place_t tag,
                                        args_t&&... args) OKAYLIB_NOEXCEPT
        : storage(std::in_place, std::forward<args_t>(args)...),
          error(0)
    {
    }

    // unchecked- it is wrapper implementations responsibility to make sure
    // error_param is not enum_t::okay
    inline constexpr res_payload_base_t(const enum_int_t& error_param)
        OKAYLIB_NOEXCEPT : error(error_param)
    {
    }

    inline constexpr input_contained_t& get_value_unchecked() OKAYLIB_NOEXCEPT
    {
        return this->storage.value;
    }
    inline constexpr const input_contained_t&
    get_value_unchecked() const OKAYLIB_NOEXCEPT
    {
        return this->storage.value;
    }

    // result in an invalid state after doing this: error will still be
    // enum_t::okay
    inline constexpr void destroy_value_but_keep_error() OKAYLIB_NOEXCEPT
    {
        storage.value.~contained_t();
    }

    // does not destroy the previous contents of storage
    template <typename... args_t>
    inline constexpr void
    construct_no_destroy(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        new (ok::addressof(this->storage.value))
            contained_t(std::forward<args_t>(args)...);
    }

    // copy construct contents of another payload, conditionally
    inline constexpr res_payload_base_t(enum_int_t,
                                        const res_payload_base_t& other)
        OKAYLIB_NOEXCEPT : error(other.error)
    {
        if (other.error == 0)
            this->construct_no_destroy(other.get());
    }

    // move construct contents of another payload, conditionally
    inline constexpr res_payload_base_t(enum_int_t, res_payload_base_t&& other)
        OKAYLIB_NOEXCEPT : error(other.error)
    {
        if (other.error == 0) {
            this->construct_no_destroy(std::move(other.get_value_unchecked()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
            other.error = 1;
#endif
        }
    }

    inline constexpr void
    copy_assign(const res_payload_base_t& other) OKAYLIB_NOEXCEPT
    {
        const bool this_okay = this->error == 0;
        const bool other_okay = other.error == 0;
        if (this_okay && other_okay) {
            this->get_value_unchecked() = other.get_value_unchecked();
        } else {
            if (other_okay) {
                this->construct_no_destroy(other.get_value_unchecked());
            } else {
                this->destroy_value_but_keep_error();
            }
            this->error = other.error;
        }
    }

    inline constexpr void
    move_assign(res_payload_base_t&& other) OKAYLIB_NOEXCEPT
    {
        const bool this_okay = this->error == 0;
        const bool other_okay = other.error == 0;
        if (this_okay && other_okay) {
            this->get_value_unchecked() =
                std::move(other.get_value_unchecked());
#ifndef OKAYLIB_NO_CHECKED_MOVES
            other.error = 1;
#endif
        } else {
            if (other_okay) {
                this->construct_no_destroy(
                    std::move(other.get_value_unchecked()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
                other.error = 1;
#endif
            } else {
                this->destroy_value_but_keep_error();
#ifndef OKAYLIB_NO_CHECKED_MOVES
                this->error = 1;
#endif
            }
            this->error = other.error;
        }
    }

    uninitialized_storage_t<input_contained_t> storage;
    enum_int_t error;
};

template <typename contained_t, typename enum_int_t,
          bool = std::is_trivially_destructible_v<contained_t>,
          bool = std::is_trivially_copy_assignable_v<contained_t> &&
                 std::is_trivially_copy_constructible_v<contained_t>,
          bool = std::is_trivially_move_assignable_v<contained_t> &&
                 std::is_trivially_move_constructible_v<contained_t>>
struct res_payload_t;

// full trivial, maybe constexpr
template <typename contained_t, typename enum_int_t>
struct res_payload_t<contained_t, enum_int_t, true, true, true>
    : res_payload_base_t<contained_t, enum_int_t>
{
    using res_payload_base_t<contained_t, enum_int_t>::res_payload_base_t;
    res_payload_t() = default;
};

// nontrivial copy construction and/or assignment
template <typename contained_t, typename enum_int_t>
struct res_payload_t<contained_t, enum_int_t, true, false, true>
    : res_payload_base_t<contained_t, enum_int_t>
{
    using res_payload_base_t<contained_t, enum_int_t>::res_payload_base_t;

    res_payload_t() = default;
    ~res_payload_t() = default;
    res_payload_t(const res_payload_t&) = default;
    res_payload_t& operator=(res_payload_t&&) = default;
    res_payload_t(res_payload_t&&) = default;

    inline constexpr res_payload_t&
    operator=(const res_payload_t& other) OKAYLIB_NOEXCEPT
    {
        this->copy_assign(other);
        return *this;
    }
};

// nontrivial move construction and/or assignment
template <typename contained_t, typename enum_int_t>
struct res_payload_t<contained_t, enum_int_t, true, true, false>
    : res_payload_base_t<contained_t, enum_int_t>
{
    using res_payload_base_t<contained_t, enum_int_t>::res_payload_base_t;

    res_payload_t() = default;
    ~res_payload_t() = default;
    res_payload_t(const res_payload_t&) = default;
    res_payload_t& operator=(const res_payload_t&) = default;
    res_payload_t(res_payload_t&&) = default;

    inline constexpr res_payload_t&
    operator=(res_payload_t&& other) OKAYLIB_NOEXCEPT
    {
        this->move_assign(std::move(other));
        return *this;
    }
};

// nontrivial move *and* copy construction and/or assignment
template <typename contained_t, typename enum_int_t>
struct res_payload_t<contained_t, enum_int_t, true, false, false>
    : res_payload_base_t<contained_t, enum_int_t>
{
    using res_payload_base_t<contained_t, enum_int_t>::res_payload_base_t;

    res_payload_t() = default;
    ~res_payload_t() = default;
    res_payload_t(const res_payload_t&) = default;
    res_payload_t(res_payload_t&&) = default;

    inline constexpr res_payload_t&
    operator=(res_payload_t&& other) OKAYLIB_NOEXCEPT
    {
        this->move_assign(std::move(other));
        return *this;
    }

    inline constexpr res_payload_t&
    operator=(const res_payload_t& other) OKAYLIB_NOEXCEPT
    {
        this->copy_assign(other);
        return *this;
    }
};

// nontrivial destructors (everything else becomes nontrivial also)
template <typename contained_t, typename enum_int_t, bool copy, bool move>
struct res_payload_t<contained_t, enum_int_t, false, copy, move>
    : res_payload_t<contained_t, enum_int_t, true, false, false>
{
    using res_payload_t<contained_t, enum_int_t, true, false,
                        false>::res_payload_t;

    res_payload_t() = default;
    res_payload_t(const res_payload_t&) = default;
    res_payload_t& operator=(const res_payload_t&) = default;
    res_payload_t(res_payload_t&&) = default;
    res_payload_t& operator=(res_payload_t&&) = default;

    inline ~res_payload_t() { this->destroy_value_but_keep_error(); }
};

template <typename input_contained_t, typename enum_int_t, typename derived_t>
class res_base_common_t
{
  protected:
    using contained_t = std::remove_const_t<input_contained_t>;

    template <typename... args_t>
    inline constexpr void
    construct_no_destroy_payload(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        static_cast<derived_t*>(this)->payload.construct_no_destroy(
            std::forward<args_t>(args)...);
    }

    inline constexpr void
    destroy_value_but_keep_error_payload() OKAYLIB_NOEXCEPT
    {
        static_cast<derived_t*>(this)->payload.destroy_value_but_keep_error();
    }

    [[nodiscard]] inline constexpr bool okay_payload() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const derived_t*>(this)->payload.error == 0;
    }

    inline constexpr input_contained_t&
    get_value_unchecked_payload() OKAYLIB_NOEXCEPT
    {
        return static_cast<derived_t*>(this)->payload.get_value_unchecked();
    }

    inline constexpr const input_contained_t&
    get_value_unchecked_payload() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const derived_t*>(this)
            ->payload.get_value_unchecked();
    }

    inline constexpr const enum_int_t&
    get_error_payload() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const derived_t*>(this)->payload.error;
    }
    inline constexpr enum_int_t& get_error_payload() OKAYLIB_NOEXCEPT
    {
        return static_cast<derived_t*>(this)->payload.error;
    }
};

template <typename input_contained_t, typename enum_int_t,
          bool = std::is_trivially_copy_constructible_v<input_contained_t>,
          bool = std::is_trivially_move_constructible_v<input_contained_t>,
          bool = std::is_lvalue_reference_v<input_contained_t>>
struct res_base_t : res_base_common_t<input_contained_t, enum_int_t,
                                      res_base_t<input_contained_t, enum_int_t>>
{
    constexpr res_base_t() = default;

    template <
        typename... args_t,
        std::enable_if_t<is_std_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit res_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    inline constexpr res_base_t(const res_base_t& other)
        : payload(other.payload.error, other.payload)
    {
    }

    inline constexpr res_base_t(res_base_t&& other)
        : payload(other.payload.error, std::move(other.payload))
    {
    }

    res_base_t& operator=(const res_base_t&) = default;
    res_base_t& operator=(res_base_t&&) = default;

    res_payload_t<input_contained_t, enum_int_t> payload;
};

template <typename input_contained_t, typename enum_int_t>
struct res_base_t<input_contained_t, enum_int_t, false, true, false>
    : res_base_common_t<input_contained_t, enum_int_t,
                        res_base_t<input_contained_t, enum_int_t>>
{
    constexpr res_base_t() = default;

    template <
        typename... args_t,
        std::enable_if_t<is_std_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit res_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    inline constexpr res_base_t(const res_base_t& other)
        : payload(other.payload.has_value, other.payload)
    {
    }

    // trivial move, can default this in this override
    inline constexpr res_base_t(res_base_t&& other) = default;

    res_base_t& operator=(const res_base_t&) = default;
    res_base_t& operator=(res_base_t&&) = default;

    res_payload_t<input_contained_t, enum_int_t> payload;
};

template <typename input_contained_t, typename enum_int_t>
struct res_base_t<input_contained_t, enum_int_t, true, false, false>
    : res_base_common_t<input_contained_t, enum_int_t,
                        res_base_t<input_contained_t, enum_int_t>>
{
    constexpr res_base_t() = default;

    template <
        typename... args_t,
        std::enable_if_t<is_std_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit res_base_t(std::in_place_t, args_t&&... args)
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    // trivial copy, can default
    constexpr res_base_t(const res_base_t& other) = default;

    inline constexpr res_base_t(res_base_t&& other)
        : payload(other.payload.has_value, std::move(other.payload))
    {
    }

    res_base_t& operator=(const res_base_t&) = default;
    res_base_t& operator=(res_base_t&&) = default;

    res_payload_t<input_contained_t, enum_int_t> payload;
};

template <typename input_contained_t, typename enum_int_t>
struct res_base_t<input_contained_t, enum_int_t, true, true, false>
    : res_base_common_t<input_contained_t, enum_int_t,
                        res_base_t<input_contained_t, enum_int_t>>
{
    constexpr res_base_t() = default;

    template <
        typename... args_t,
        std::enable_if_t<is_std_constructible_v<input_contained_t, args_t...>,
                         bool> = false>
    inline constexpr explicit res_base_t(std::in_place_t,
                                         args_t&&... args) OKAYLIB_NOEXCEPT
        : payload(std::in_place, std::forward<args_t>(args)...)
    {
    }

    // all trivial
    constexpr res_base_t(const res_base_t& other) = default;
    constexpr res_base_t(res_base_t&& other) = default;

    res_base_t& operator=(const res_base_t&) = default;
    res_base_t& operator=(res_base_t&&) = default;

    res_payload_t<input_contained_t, enum_int_t> payload;
};

// overload for when contained type is a reference type
// does not mark as no_value when moved, because guaranteed trivially
// movable, same as opt
template <typename input_contained_t, typename enum_int_t, bool move, bool copy>
struct res_base_t<input_contained_t, enum_int_t, copy, move, true>
{
    using pointer_t = std::remove_reference_t<input_contained_t>;

    constexpr res_base_t() = default;

    inline constexpr explicit res_base_t(input_contained_t _ref)
        OKAYLIB_NOEXCEPT : pointer(ok::addressof(_ref)),
                           error(0)
    {
    }

    inline constexpr explicit res_base_t(enum_int_t _error) OKAYLIB_NOEXCEPT
        : error(_error)
    {
        assert(error != 0);
    }

    inline constexpr void
    construct_no_destroy_payload(input_contained_t _ref) OKAYLIB_NOEXCEPT
    {
        pointer = ok::addressof(_ref);
    }

    inline constexpr void
    destroy_value_but_keep_error_payload() OKAYLIB_NOEXCEPT
    {
        pointer = nullptr;
    }

    [[nodiscard]] inline constexpr bool okay_payload() const OKAYLIB_NOEXCEPT
    {
        return error == 0;
    }

    // NOTE: missing get_value_unchecked_payload here, any calls to that must
    // be surrounded by some if constexpr (!is_lvalue_reference) { ... }

    inline constexpr const enum_int_t&
    get_error_payload() const OKAYLIB_NOEXCEPT
    {
        return error;
    }

    inline constexpr enum_int_t& get_error_payload() OKAYLIB_NOEXCEPT
    {
        return error;
    }

    constexpr res_base_t(const res_base_t& other) = default;
    constexpr res_base_t(res_base_t&& other) = default;
    res_base_t& operator=(const res_base_t&) = default;
    res_base_t& operator=(res_base_t&&) = default;

    pointer_t* pointer;
    enum_int_t error;
};

} // namespace ok::detail

namespace ok {
template <typename contained_t, typename enum_t, typename = void> class res;
}

namespace ok::detail {

template <typename T> inline constexpr bool is_result = is_instance<T, res>();

template <typename target_t, typename res_contained_t, typename res_enum_t>
inline constexpr bool converts_from_res =
    is_std_constructible_v<target_t,
                           const res<res_contained_t, res_enum_t>&> ||
    is_std_constructible_v<target_t, res<res_contained_t, res_enum_t>&> ||
    is_std_constructible_v<target_t,
                           const res<res_contained_t, res_enum_t>&&> ||
    is_std_constructible_v<target_t, res<res_contained_t, res_enum_t>&&> ||
    std::is_convertible_v<const res<res_contained_t, res_enum_t>&,
                          target_t> ||
    std::is_convertible_v<res<res_contained_t, res_enum_t>&, target_t> ||
    std::is_convertible_v<const res<res_contained_t, res_enum_t>&&,
                          target_t> ||
    std::is_convertible_v<res<res_contained_t, res_enum_t>&&, target_t>;

template <typename target_t, typename res_contained_t, typename res_enum_t>
inline constexpr bool assigns_from_res =
    std::is_assignable_v<target_t&,
                         const res<res_contained_t, res_enum_t>&> ||
    std::is_assignable_v<target_t&, res<res_contained_t, res_enum_t>&> ||
    std::is_assignable_v<target_t&,
                         const res<res_contained_t, res_enum_t>&&> ||
    std::is_assignable_v<target_t&, res<res_contained_t, res_enum_t>&&>;
} // namespace ok::detail

#endif
