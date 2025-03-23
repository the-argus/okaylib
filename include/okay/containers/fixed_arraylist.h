#ifndef __OKAYLIB_CONTAINERS_FIXED_ARRAYLIST_H__
#define __OKAYLIB_CONTAINERS_FIXED_ARRAYLIST_H__

#include "okay/containers/array.h"
#include "okay/defer.h"
#include "okay/detail/template_util/uninitialized_storage.h"

namespace ok {

namespace detail {
template <typename T, size_t max_elems> class fixed_arraylist_members_t
{
    static_assert(max_elems > 0,
                  "Cannot create a fixed_arraylist_t with zero elements.");
    size_t spots_occupied;
    detail::uninitialized_storage_t<T> buffer[max_elems];

  protected:
    constexpr void set_spots_occupied(size_t new_size) noexcept
    {
        __ok_internal_assert(new_size <= max_elems);
        spots_occupied = new_size;
    }

    constexpr void destroy() const OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < this->size(); ++i) {
            this->data()[i].~T();
        }
    }

  public:
    constexpr T* data() noexcept { return reinterpret_cast<T*>(buffer); }
    constexpr const T* data() const noexcept
    {
        return reinterpret_cast<const T*>(buffer);
    }
    constexpr size_t size() const noexcept { return spots_occupied; }
};

template <typename T, size_t max_elems>
class fixed_arraylist_nontrivial_destruction_t
    : protected fixed_arraylist_members_t<T, max_elems>
{
    ~fixed_arraylist_nontrivial_destruction_t() { this->destroy(); }
};
} // namespace detail

// like ok::array_t but it can grow and shrink within its maximum amount
template <typename T, size_t max_elems>
class fixed_arraylist_t
    : protected std::conditional_t<
          std::is_trivially_destructible_v<T>,
          detail::fixed_arraylist_members_t<T, max_elems>,
          detail::fixed_arraylist_nontrivial_destruction_t<T, max_elems>>
{

  public:
    // initialize fixed_arraylist_t with an array_t for nice syntax
    template <
        size_t num_elems_in_initializer,
        std::enable_if_t<num_elems_in_initializer <= max_elems, bool> = true>
    constexpr fixed_arraylist_t(
        const ok::array_t<T, num_elems_in_initializer>& array) OKAYLIB_NOEXCEPT
    {
        this->set_spots_occupied(num_elems_in_initializer);
        for (size_t i = 0; i < num_elems_in_initializer; ++i) {
            new (this->data() + i) T(array.data()[i]);
        }
    }

    constexpr fixed_arraylist_t() noexcept { this->set_spots_occupied(0); }

    // operation is only guaranteed to be nonfailing if we are larger or equal
    // to the size of the thing being moved into us
    template <size_t other_max_elems,
              std::enable_if_t<other_max_elems <= max_elems, bool> = true>
    constexpr fixed_arraylist_t(fixed_arraylist_t<T, other_max_elems>&& other)
        OKAYLIB_NOEXCEPT
    {
        this->set_spots_occupied(other.size());
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(this->data(), other.data(), other.size() * sizeof(T));
        } else {
            for (size_t i = 0; i < other.size(); ++i) {
                new (this->data() + i) T(std::move(other.data()[i]));
            }
        }
        other.set_spots_occupied(0);
    }

    template <size_t other_max_elems,
              std::enable_if_t<other_max_elems <= max_elems, bool> = true>
    constexpr fixed_arraylist_t&
    operator=(fixed_arraylist_t<T, other_max_elems>&& other) OKAYLIB_NOEXCEPT
    {
        this->destroy();
        new (this) fixed_arraylist_t(std::move(other));
        return *this;
    }

    template <typename... args_t>
    [[nodiscard]] constexpr auto append(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        return insert_at(this->size(), std::forward<args_t>(args)...);
    }

    template <typename... args_t>
    [[nodiscard]] constexpr auto insert_at(const size_t idx,
                                           args_t&&... args) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<
            // only enabled if the given arguments construct a T
            is_constructible_v<T, args_t...>,
            // return type is bool normally, unless you pass in a failing
            // constructor, in which case this function returns that same thing.
            // if out of space, it becomes enum_t::no_value.
            std::conditional_t<
                std::is_void_v<decltype(ok::make_into_uninitialized<T>(
                    std::declval<T&>(), std::forward<args_t>(args)...))>,
                bool,
                decltype(ok::make_into_uninitialized<T>(
                    std::declval<T&>(), std::forward<args_t>(args)...))>>
    {
        using make_result_type = decltype(ok::make_into_uninitialized<T>(
            std::declval<T&>(), std::forward<args_t>(args)...));
        using enum_t = typename make_result_type::enum_type;

        constexpr bool make_returns_status = !std::is_void_v<make_result_type>;

        if (idx > this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to fixed_arraylist in insert_at.");
        }

        if (this->size() == max_elems) [[unlikely]] {
            if constexpr (make_returns_status) {
                static_assert(detail::is_instance_v<make_result_type, status>);
                return enum_t::no_value;
            } else {
                return false;
            }
        }
        __ok_internal_assert(this->size() < max_elems);

        if (idx < this->size()) {
            // move all other items towards the back of the arraylist
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(this->data() + idx + 1, this->data() + idx,
                             (this->size() - idx) * sizeof(T));
            } else {
                for (size_t i = this->size(); i > idx; --i) {
                    auto* target = this->data() + i;
                    auto* source = this->data() + (i - 1);
                    new (target) T(std::move(*source));
                }
            }
        }

        // populate item
        auto& uninit = this->data()[idx];

        if constexpr (make_returns_status) {
            auto result = ok::make_into_uninitialized<T>(
                uninit, std::forward<args_t>(args)...);

            // only add to occupied spots if the construction was successful
            if (result.okay()) [[likely]] {
                this->set_spots_occupied(this->size() + 1);
            } else {
                // move all other items BACK to where they were before (this is
                // supposed to be the cold path and it only invokes nonfailing
                // operations so it should be fine to do this)
                if constexpr (std::is_trivially_copyable_v<T>) {
                    std::memmove(this->data() + idx, this->data() + idx + 1,
                                 (this->size() - idx) * sizeof(T));
                } else {
                    for (size_t i = idx; i < this->size(); ++i) {
                        auto* target = this->data() + i;
                        auto* source = this->data() + (i + 1);
                        new (target) T(std::move(*source));
                    }
                }
            }
            return result;
        } else {
            ok::make_into_uninitialized<T>(uninit,
                                           std::forward<args_t>(args)...);
            this->set_spots_occupied(this->size() + 1);
            return true;
        }
    }

    constexpr T remove(size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access in fixed_arraylist_t::remove()");
        }

        // moved out at index
        T out(std::move(this->data()[idx]));

        defer decrement([this] { this->set_spots_occupied(this->size() - 1); });

        // if no need to move anything (popping last)
        if (idx == this->size() - 1) {
            return out;
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            const size_t idxplusone = idx + 1;
            std::memmove(this->data() + idx, this->data() + idxplusone,
                         (this->size() - idxplusone) * sizeof(T));
        } else {
            for (size_t i = this->size() - 1; i > idx; --i) {
                auto* target = this->data() + (i - 1);
                auto* source = this->data() + (i);
                new (ok::addressof(target)) T(std::move(*source));
            }
        }
        return out;
    }

    constexpr opt<T> remove_and_swap_last(size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]]
            return {};

        T& target = this->data()[idx];

        // moved out at index
        opt<T> out(std::move(target));

        defer decrement([this] { this->set_spots_occupied(this->size() - 1); });

        if (idx == this->size() - 1) {
            return out;
        }

        new (ok::addressof(target))
            T(std::move(this->data()[this->size() - 1]));

        return out;
    }

    constexpr opt<T> pop_last() OKAYLIB_NOEXCEPT
    {
        if (is_empty())
            return {};
        return remove(this->size() - 1);
    }

    constexpr void clear() OKAYLIB_NOEXCEPT
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < this->size(); ++i) {
                this->data()[i].~T();
            }
        }

        this->set_spots_occupied(0);
    }

    template <typename... args_t>
    constexpr auto resize(size_t new_size, args_t&&... args) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<is_infallible_constructible_v<T, args_t...>, void>
    {
        if (new_size > max_elems) [[unlikely]] {
            __ok_abort("Attempt to resize fixed_arraylist_t beyond its "
                       "internal buffer size.");
        }
        if (this->size() == new_size) [[unlikely]] {
            return;
        } else if (new_size == 0) [[unlikely]] {
            clear();
            return;
        }

        const bool shrinking = this->size() > new_size;

        if (shrinking) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = new_size; i < this->size(); ++i) {
                    this->data()[i].~T();
                }
            }
        } else {
            // initialize new elements
            if constexpr (std::is_trivially_default_constructible_v<T> &&
                          sizeof...(args_t) == 0) {
                // manually zero memory
                // TODO: some ifdef here maybe to avoid zeroing based on build
                // option? or a template param?
                std::memset(this->data() + this->size(), 0,
                            (new_size - this->size()) * sizeof(T));
            } else {
                for (size_t i = this->size(); i < new_size; ++i) {
                    ok::make_into_uninitialized<T>(
                        this->data()[i], std::forward<args_t>(args)...);
                }
            }
        }
        this->set_spots_occupied(new_size);
    }

    [[nodiscard]] constexpr size_t capacity() const OKAYLIB_NOEXCEPT
    {
        return max_elems;
    }

    [[nodiscard]] constexpr bool is_empty() const OKAYLIB_NOEXCEPT
    {
        return this->size() == 0;
    }

    [[nodiscard]] slice<T> items() & OKAYLIB_NOEXCEPT
    {
        return raw_slice(*this->data(), this->size());
    }

    [[nodiscard]] slice<const T> items() const& OKAYLIB_NOEXCEPT
    {
        return raw_slice(*this->data(), this->size());
    }
};
} // namespace ok

#endif
