#ifndef __OKAYLIB_ITERABLE_STD_FOR_H__
#define __OKAYLIB_ITERABLE_STD_FOR_H__

#include "okay/iterable/iterable.h"
#include "okay/opt.h"
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

    constexpr std_for(T& _inner) : inner(_inner) {}

  private:
    T& inner;

  public:
    struct iterator;
    struct const_iterator;
    friend struct iterator;
    friend struct const_iterator;

    struct iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = detail::value_type_unchecked_for<T>;
        using pointer = value_type*;
        using reference = value_type&;

        constexpr iterator() = default;

        constexpr iterator(T& _parent, cursor_t _cursor) OKAYLIB_NOEXCEPT
            : m(members_t{_parent, _cursor})
        {
        }

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            auto& members = m.value();
            return ok::iter_get_ref(members.parent, members.cursor);
        }

        // TODO: should this be const?
        constexpr pointer operator->() OKAYLIB_NOEXCEPT
        {
            auto& members = m.value();
            return ok::addressof(
                ok::iter_get_ref(members.parent, members.cursor));
        }

        // Prefix increment
        constexpr iterator& operator++() OKAYLIB_NOEXCEPT
        {
            ++(m.value().cursor);
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        constexpr iterator operator++(int) OKAYLIB_NOEXCEPT
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr friend bool operator==(const iterator& a,
                                         const iterator& b) OKAYLIB_NOEXCEPT
        {
            if (a.m.has_value() != b.m.has_value()) [[likely]] {
                if (a.m.has_value())
                    return !ok::is_inbounds(a.m.value().parent,
                                            a.m.value().cursor);
                return !ok::is_inbounds(b.m.value().parent, b.m.value().cursor);
            }
            return a.m.value().cursor == b.m.value().cursor;
        };
        constexpr friend bool operator!=(const iterator& a,
                                         const iterator& b) OKAYLIB_NOEXCEPT
        {
            if (a.m.has_value() != b.m.has_value()) [[likely]] {
                // only one is nonnull. treat null as "end" cursor (signals need
                // to check for out-of-bounds)
                if (a.m.has_value())
                    return ok::is_inbounds(a.m.value().parent,
                                           a.m.value().cursor);
                return ok::is_inbounds(b.m.value().parent, b.m.value().cursor);
            }
            return a.m.value().cursor != b.m.value().cursor;
        };

      private:
        struct members_t
        {
            T& parent;
            cursor_t cursor;
        };
        opt_t<members_t> m;
    };

    struct const_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const detail::value_type_unchecked_for<T>;
        using pointer = const value_type*;
        using reference = const value_type&;

        constexpr const_iterator() = default;

        constexpr const_iterator(const T& _parent,
                                 cursor_t _cursor) OKAYLIB_NOEXCEPT
            : m(members_t{_parent, _cursor})
        {
        }

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            const auto& members = m.value();
            return ok::iter_get_ref(members.parent, members.cursor);
        }

        constexpr pointer operator->() OKAYLIB_NOEXCEPT
        {
            const auto& members = m.value();
            return ok::addressof(
                ok::iter_get_ref(members.parent, members.cursor));
        }

        // Prefix increment
        constexpr const_iterator& operator++() OKAYLIB_NOEXCEPT
        {
            ++(m.value().cursor);
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        constexpr const_iterator operator++(int) OKAYLIB_NOEXCEPT
        {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr friend bool
        operator==(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            if (a.m.has_value() != b.m.has_value()) [[likely]] {
                if (a.m.has_value())
                    return !ok::is_inbounds(a.m.value().parent,
                                            a.m.value().cursor);
                return !ok::is_inbounds(b.m.value().parent, b.m.value().cursor);
            }
            return a.m.value().cursor == b.m.value().cursor;
        };
        constexpr friend bool
        operator!=(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            if (a.m.has_value() != b.m.has_value()) [[likely]] {
                if (a.m.has_value())
                    return ok::is_inbounds(a.m.value().parent,
                                           a.m.value().cursor);
                return ok::is_inbounds(b.m.value().parent, b.m.value().cursor);
            }
            return a.m.value().cursor != b.m.value().cursor;
        };

      private:
        struct members_t
        {
            const T& parent;
            cursor_t cursor;
        };
        opt_t<members_t> m;
    };

    using correct_iterator_t =
        std::conditional_t<std::is_const_v<T>, const_iterator, iterator>;

    constexpr correct_iterator_t begin() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t(inner, ok::begin(inner));
    }

    constexpr correct_iterator_t end() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t();
    }

    constexpr const_iterator begin() const OKAYLIB_NOEXCEPT
    {
        return const_iterator(inner, ok::begin(inner));
    }

    constexpr const_iterator end() const OKAYLIB_NOEXCEPT
    {
        return const_iterator();
    }
};
}; // namespace ok

#endif
