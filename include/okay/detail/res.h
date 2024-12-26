#ifndef __OKAYLIB_DETAIL_RES_H__
#define __OKAYLIB_DETAIL_RES_H__

#include "okay/detail/template_util/uninitialized_storage.h"
#include <type_traits>
#include <utility>

namespace ok::detail {
template <typename input_contained_t, typename enum_t> class res_payload_base_t
{
    using contained_t = std::remove_const_t<input_contained_t>;

    // default everything because special member behavior is determined by the
    // selected template specialization for our "storage" member var
    ~res_payload_base_t() = default;
    res_payload_base_t(const res_payload_base_t&) = default;
    res_payload_base_t(res_payload_base_t&&) = default;
    res_payload_base_t& operator=(const res_payload_base_t&) = default;
    res_payload_base_t& operator=(res_payload_base_t&&) = default;

    // result *must* me initialized with something
    res_payload_base_t() = delete;

    // construct storage value in-place
    template <typename... args_t>
    inline constexpr res_payload_base_t(std::in_place_t tag,
                                        args_t&&... args) OKAYLIB_NOEXCEPT
        : storage(std::in_place, std::forward<args_t>(args)...),
          error(enum_t::okay)
    {
    }

    // unchecked- it is wrapper implementations responsibility to make sure
    // error_param is not enum_t::okay
    inline constexpr res_payload_base_t(const enum_t& error_param)
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
        new (std::addressof(this->storage.value))
            contained_t(std::forward<args_t>(args)...);
    }

    // copy construct contents of another payload, conditionally
    inline constexpr res_payload_base_t(bool, const res_payload_base_t& other)
        OKAYLIB_NOEXCEPT : error(other.error)
    {
        if (other.error == enum_t::okay)
            this->construct_no_destroy(other.get());
    }

    // move construct contents of another payload, conditionally
    inline constexpr res_payload_base_t(bool, res_payload_base_t&& other)
        OKAYLIB_NOEXCEPT : error(other.error)
    {
        if (other.errorr == enum_t::okay)
            this->construct_no_destroy(std::move(other.get()));
    }

    inline constexpr void
    copy_assign(const res_payload_base_t& other) OKAYLIB_NOEXCEPT
    {
        const bool this_okay = this->error == enum_t::okay;
        const bool other_okay = other.error == enum_t::okay;
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
        const bool this_okay = this->error == enum_t::okay;
        const bool other_okay = other.error == enum_t::okay;
        if (this_okay && other_okay) {
            this->get_value_unchecked() =
                std::move(other.get_value_unchecked());
        } else {
            if (other_okay) {
                this->construct_no_destroy(
                    std::move(other.get_value_unchecked()));
#ifndef OKAYLIB_NO_CHECKED_MOVES
                other.error = enum_t::result_released;
#endif
            } else {
                this->destroy_value_but_keep_error();
#ifndef OKAYLIB_NO_CHECKED_MOVES
                this->error = enum_t::result_released;
#endif
            }
            this->error = other.error;
        }
    }

    uninitialized_storage_t<input_contained_t> storage;
    enum_t error;
};

template <typename contained_t, typename enum_t,
          bool = std::is_trivially_destructible_v<contained_t>,
          bool = std::is_trivially_copy_assignable_v<contained_t> &&
                 std::is_trivially_copy_constructible_v<contained_t>,
          bool = std::is_trivially_move_assignable_v<contained_t> &&
                 std::is_trivially_move_constructible_v<contained_t>>
struct res_payload_t;

// full trivial, maybe constexpr
template <typename contained_t, typename enum_t>
struct res_payload_t<contained_t, enum_t, true, true, true>
    : res_payload_base_t<contained_t, enum_t>
{
    using res_payload_base_t<contained_t, enum_t>::res_payload_base_t;
    res_payload_t() = default;
};

// nontrivial copy construction and/or assignment
template <typename contained_t, typename enum_t>
struct res_payload_t<contained_t, enum_t, true, false, true>
    : res_payload_base_t<contained_t, enum_t>
{
    using res_payload_base_t<contained_t, enum_t>::res_payload_base_t;

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
template <typename contained_t, typename enum_t>
struct res_payload_t<contained_t, enum_t, true, true, false>
    : res_payload_base_t<contained_t, enum_t>
{
    using res_payload_base_t<contained_t, enum_t>::res_payload_base_t;

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
template <typename contained_t, typename enum_t>
struct res_payload_t<contained_t, enum_t, true, false, false>
    : res_payload_base_t<contained_t, enum_t>
{
    using res_payload_base_t<contained_t, enum_t>::res_payload_base_t;

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
template <typename contained_t, typename enum_t, bool copy, bool move>
struct res_payload_t<contained_t, enum_t, false, copy, move>
    : res_payload_t<contained_t, enum_t, true, false, false>
{
    using res_payload_t<contained_t, enum_t, true, false, false>::res_payload_t;

    res_payload_t() = default;
    res_payload_t(const res_payload_t&) = default;
    res_payload_t& operator=(const res_payload_t&) = default;
    res_payload_t(res_payload_t&&) = default;
    res_payload_t& operator=(res_payload_t&&) = default;

    inline ~res_payload_t() { this->reset(); }
};

} // namespace ok::detail

#endif
