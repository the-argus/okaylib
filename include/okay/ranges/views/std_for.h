#ifndef __OKAYLIB_RANGES_VIEWS_STD_FOR_H__
#define __OKAYLIB_RANGES_VIEWS_STD_FOR_H__

#include "okay/detail/get_best.h"
#include "okay/detail/view_common.h"
#include "okay/opt.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"
#include <iterator>

/*
 * Given any okaylib range, create a view which can be used as a c++17 input
 * iterator, MINUS operator->. This makes the iterator not actually conformant
 * and only sometimes suitable as a hack to get compat w/ range-based for loops.
 */

namespace ok {
template <typename T>
class std_for_view : public detail::underlying_view_type<T>::type
{
  public:
    static_assert(is_range_v<T>, "Cannot wrap given type for a standard for "
                                 "loop- it is not a valid range.");
    using cursor_t = detail::cursor_type_unchecked_for_t<T>;
    using parent_t = typename detail::underlying_view_type<T>::type;

  public:
    struct iterator;
    struct const_iterator;
    friend struct iterator;
    friend struct const_iterator;

    struct iterator
    {
      private:
        // TODO: can use uninitialized_storage_t here to avoid construction of
        // cursor_t here instead and then embed the optional in the T&, to save
        // some bytes
        struct members_t
        {
            T& parent;
            cursor_t cursor;
        };
        opt<members_t> m;

      public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = value_type_for<T>;
        using reference = decltype(ok::detail::get_best(
            m.ref_or_panic().parent, m.ref_or_panic().cursor));
        using pointer = value_type*;

        static_assert(
            std::is_convertible_v<reference, value_type>,
            "Unable to convert the value that would be gotten for an "
            "ok_foreach loop into the value type. This conversion is needed to "
            "make an STL compatible input iterator. You may need to define a "
            "copy constructor for your value type.");

        constexpr iterator() = default;

        constexpr iterator(T& _parent, cursor_t _cursor) OKAYLIB_NOEXCEPT
            : m(members_t{_parent, _cursor})
        {
        }

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            auto& members = m.ref_or_panic();
            return ok::detail::get_best(members.parent, members.cursor);
        }

        // Prefix increment
        constexpr iterator& operator++() OKAYLIB_NOEXCEPT
        {
            auto& members = m.ref_or_panic();
            ok::increment(members.parent, members.cursor);
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        constexpr iterator operator++(int) const OKAYLIB_NOEXCEPT
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr friend bool operator==(const iterator& a,
                                         const iterator& b) OKAYLIB_NOEXCEPT
        {
            if (!(a.m.has_value() == b.m.has_value())) [[likely]] {
                if (a.m.has_value())
                    return !ok::is_inbounds(a.m.ref_or_panic().parent,
                                            a.m.ref_or_panic().cursor);
                return !ok::is_inbounds(b.m.ref_or_panic().parent,
                                        b.m.ref_or_panic().cursor);
            }
            return a.m.ref_or_panic().cursor == b.m.ref_or_panic().cursor;
        };
        constexpr friend bool operator!=(const iterator& a,
                                         const iterator& b) OKAYLIB_NOEXCEPT
        {
            if (!(a.m.has_value() == b.m.has_value())) [[likely]] {
                // only one is nonnull. treat null as "end" cursor (signals need
                // to check for out-of-bounds)
                if (a.m.has_value())
                    return ok::is_inbounds(a.m.ref_or_panic().parent,
                                           a.m.ref_or_panic().cursor);
                return ok::is_inbounds(b.m.ref_or_panic().parent,
                                       b.m.ref_or_panic().cursor);
            }
            return !(a.m.ref_or_panic().cursor == b.m.ref_or_panic().cursor);
        };
    };

    struct const_iterator
    {
      private:
        struct members_t
        {
            const T& parent;
            cursor_t cursor;
        };
        opt<members_t> m;

      public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const value_type_for<T>;
        using reference = decltype(ok::detail::get_best(
            m.ref_or_panic().parent, m.ref_or_panic().cursor));
        using pointer = const value_type*;

        constexpr const_iterator() = default;

        constexpr const_iterator(const T& _parent,
                                 cursor_t _cursor) OKAYLIB_NOEXCEPT
            : m(members_t{_parent, _cursor})
        {
        }

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            const auto& members = m.ref_or_panic();
            return ok::detail::get_best(members.parent, members.cursor);
        }

        // constexpr pointer operator->() OKAYLIB_NOEXCEPT
        // {
        //     const auto& members = m.ref_or_panic();
        //     return ok::addressof(
        //         ok::detail::get_best(members.parent, members.cursor));
        // }

        // Prefix increment
        constexpr const_iterator& operator++() OKAYLIB_NOEXCEPT
        {
            auto& members = m.ref_or_panic();
            ok::increment(members.parent, members.cursor);
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
            if (!(a.m.has_value() == b.m.has_value())) [[likely]] {
                if (a.m.has_value())
                    return !ok::is_inbounds(a.m.ref_or_panic().parent,
                                            a.m.ref_or_panic().cursor);
                return !ok::is_inbounds(b.m.ref_or_panic().parent,
                                        b.m.ref_or_panic().cursor);
            }
            return a.m.ref_or_panic().cursor == b.m.ref_or_panic().cursor;
        };
        constexpr friend bool
        operator!=(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            if (!(a.m.has_value() == b.m.has_value())) [[likely]] {
                if (a.m.has_value())
                    return ok::is_inbounds(a.m.ref_or_panic().parent,
                                           a.m.ref_or_panic().cursor);
                return ok::is_inbounds(b.m.ref_or_panic().parent,
                                       b.m.ref_or_panic().cursor);
            }
            return !(a.m.ref_or_panic().cursor == b.m.ref_or_panic().cursor);
        };
    };

    using correct_iterator_t =
        std::conditional_t<std::is_const_v<T>, const_iterator, iterator>;

    constexpr correct_iterator_t begin() OKAYLIB_NOEXCEPT
    {
        auto&& ref = parent_t::template get_view_reference<std_for_view, T>();
        return correct_iterator_t(ref, ok::begin(ref));
    }

    constexpr correct_iterator_t end() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t();
    }

    constexpr const_iterator begin() const OKAYLIB_NOEXCEPT
    {
        auto&& ref = parent_t::template get_view_reference<std_for_view, T>();
        return const_iterator(ref, ok::begin(ref));
    }

    constexpr const_iterator end() const OKAYLIB_NOEXCEPT
    {
        return const_iterator();
    }
};

namespace detail {
struct std_for_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range) const
    {
        return std_for_view<decltype(range)>{std::forward<range_t>(range)};
    }
};
}; // namespace detail

constexpr detail::range_adaptor_closure_t<detail::std_for_fn_t> std_for;

}; // namespace ok

#endif
