#ifndef __OKAYLIB_ITERABLE_STD_FOR_H__
#define __OKAYLIB_ITERABLE_STD_FOR_H__

#include "okay/iterable/iterable.h"
#include <iterator>

namespace ok {
template <typename T> class std_for
{
  public:
    static_assert(is_iterable_v<T>, "Cannot wrap given type for a standard for "
                                    "loop- it is not a valid iterable.");

    static_assert(
        detail::iterable_has_get_ref_v<T> ||
            detail::iterable_has_get_ref_const_v<T>,
        "Cannot wrap type which does not provide any get_ref() functions for "
        "standard for loop, which requires dereferencing.");

    using cursor_t = detail::cursor_type_unchecked_for<T>;
    using sentinel_t = detail::sentinel_type_unchecked_for<T>;

    // TODO: support this? not sure if its possible, but as long as this type
    // is only for use in foreach loops it may be possible through hacks
    static_assert(
        std::is_same_v<cursor_t, sentinel_t>,
        "Attempt to create std_for wrapper for a type whose sentinel is "
        "different from cursor. This is not currently supported.");

    inline constexpr std_for(T& _inner) : inner(_inner) {}

  private:
    T& inner;

  public:
    struct iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = detail::value_type_unchecked_for<T>;
        using pointer = value_type*;
        using reference = value_type&;

        inline constexpr iterator(T& _parent, cursor_t _cursor)
            : parent(_parent), cursor(_cursor)
        {
        }

        inline constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            return ok::iter_get_ref(parent, cursor);
        }

        inline constexpr pointer operator->() OKAYLIB_NOEXCEPT
        {
            return std::addressof(ok::iter_get_ref(parent, cursor));
        }

        // Prefix increment
        inline constexpr iterator& operator++() OKAYLIB_NOEXCEPT
        {
            ++cursor;
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        inline constexpr iterator operator++(int) OKAYLIB_NOEXCEPT
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        inline constexpr friend bool
        operator==(const iterator& a, const iterator& b) OKAYLIB_NOEXCEPT
        {
            return a.cursor == b.cursor;
        };
        inline constexpr friend bool
        operator!=(const iterator& a, const iterator& b) OKAYLIB_NOEXCEPT
        {
            return !(a.cursor == b.cursor);
        };

      private:
        T& parent;
        cursor_t cursor;
    };

    struct const_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const detail::value_type_unchecked_for<T>;
        using pointer = const value_type*;
        using reference = const value_type&;

        inline constexpr const_iterator(const T& _parent, cursor_t _cursor)
            : parent(_parent), cursor(_cursor)
        {
        }

        inline constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            return ok::iter_get_ref(parent, cursor);
        }

        inline constexpr pointer operator->() OKAYLIB_NOEXCEPT
        {
            return std::addressof(ok::iter_get_ref(parent, cursor));
        }

        // Prefix increment
        inline constexpr const_iterator& operator++() OKAYLIB_NOEXCEPT
        {
            ++cursor;
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        inline constexpr const_iterator operator++(int) OKAYLIB_NOEXCEPT
        {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        inline constexpr friend bool
        operator==(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            return a.cursor == b.cursor;
        };
        inline constexpr friend bool
        operator!=(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            return !(a.cursor == b.cursor);
        };

      private:
        const T& parent;
        cursor_t cursor;
    };

    using correct_iterator_t =
        std::conditional_t<std::is_const_v<T>, const_iterator, iterator>;

    inline constexpr correct_iterator_t begin() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t(inner, ok::begin(inner));
    }

    inline constexpr correct_iterator_t end() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t(inner, ok::end(inner));
    }

    inline constexpr const_iterator begin() const OKAYLIB_NOEXCEPT
    {
        return const_iterator(inner, ok::begin(inner));
    }

    inline constexpr const_iterator end() const OKAYLIB_NOEXCEPT
    {
        return const_iterator(inner, ok::end(inner));
    }
};
}; // namespace ok

#endif
