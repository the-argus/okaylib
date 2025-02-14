#ifndef __OKAYLIB_RANGES_VIEWS_REVERSE_H__
#define __OKAYLIB_RANGES_VIEWS_REVERSE_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t> struct reversed_view_t;

struct reverse_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range) const OKAYLIB_NOEXCEPT
    {
        using T = remove_cvref_t<range_t>;
        static_assert(is_range_v<T>,
                      "Cannot reverse given type- it is not a range.");
        static_assert(is_random_access_range_v<T> && range_can_size_v<T>,
                      "Cannot reverse given type- it is either not random "
                      "access, or its size is not finite or cannot be known in "
                      "constant time.");
        return reversed_view_t<decltype(range)>{std::forward<range_t>(range)};
    }
};

template <typename input_parent_range_t> struct reversed_cursor_t
{
  private:
    using parent_range_t = detail::remove_cvref_t<input_parent_range_t>;
    using parent_cursor_t = cursor_type_for<parent_range_t>;
    using self_t = reversed_cursor_t;

  public:
    explicit constexpr reversed_cursor_t(parent_cursor_t&& c) OKAYLIB_NOEXCEPT
        : m_inner(std::move(c))
    {
    }

    friend class ok::range_definition<reversed_view_t<input_parent_range_t>>;

    reversed_cursor_t(const reversed_cursor_t&) = default;
    reversed_cursor_t& operator=(const reversed_cursor_t&) = default;
    reversed_cursor_t(reversed_cursor_t&&) = default;
    reversed_cursor_t& operator=(reversed_cursor_t&&) = default;

    constexpr const parent_cursor_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m_inner;
    }
    constexpr parent_cursor_t& inner() OKAYLIB_NOEXCEPT { return m_inner; }

    constexpr operator parent_cursor_t() const noexcept { return inner(); }

    constexpr self_t& operator++() OKAYLIB_NOEXCEPT
    {
        --m_inner;
        return *this;
    }

    constexpr self_t& operator--() OKAYLIB_NOEXCEPT
    {
        ++m_inner;
        return *this;
    }

    constexpr self_t& operator+=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_inner -= rhs;
        return *this;
    }

    constexpr self_t& operator-=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_inner += rhs;
        return *this;
    }

    constexpr self_t operator+(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        return self_t(m_inner - rhs);
    }

    constexpr self_t operator-(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        return self_t(m_inner + rhs);
    }

    friend class ok::orderable_definition<self_t>;

  private:
    parent_cursor_t m_inner;
};

template <typename range_t>
struct reversed_view_t : public underlying_view_type<range_t>::type
{};
} // namespace detail

template <typename input_parent_range_t>
class ok::orderable_definition<detail::reversed_cursor_t<input_parent_range_t>>
{
    using self_t = detail::reversed_cursor_t<input_parent_range_t>;

  public:
    static constexpr bool is_strong_orderable =
        is_strong_fully_orderable_v<detail::remove_cvref_t<
            decltype(std::declval<const self_t&>().inner())>>;

    static constexpr ordering cmp(const self_t& lhs,
                                  const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        return ok::cmp(lhs.inner(), rhs.inner());
    }
};

template <typename input_range_t>
struct range_definition<detail::reversed_view_t<input_range_t>,
                        std::enable_if_t<!detail::range_is_arraylike_v<
                            detail::remove_cvref_t<input_range_t>>>>
    : public detail::propagate_get_set_t<
          detail::reversed_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          detail::reversed_cursor_t<input_range_t>>,
      public detail::propagate_sizedness_t<
          detail::reversed_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>>
// lose boundscheck optimization marker
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using reverse_t = detail::reversed_view_t<input_range_t>;
    using cursor_t = detail::reversed_cursor_t<input_range_t>;

    static_assert(
        detail::is_random_access_range_v<range_t> &&
            detail::range_can_size_v<range_t>,
        "Cannot reverse a range which is not both random access and sized.");

    constexpr static cursor_t begin(const reverse_t& i) OKAYLIB_NOEXCEPT
    {
        const auto& parent =
            i.template get_view_reference<reverse_t, range_t>();
        auto parent_cursor = ok::begin(parent);
        auto size = ok::size(parent);
        return cursor_t(parent_cursor + (size == 0 ? 0 : size - 1));
    }

    __ok_enable_if_static(range_t, detail::range_can_is_inbounds_v<T>, bool)
        is_inbounds(const reverse_t& i, const cursor_t& c)
    {
        return ok::is_inbounds(i.template get_view_reference<reverse_t, T>(),
                               cursor_type_for<T>(c));
    }

    // switch after bounds to do the parents before bounds check
    __ok_enable_if_static(range_t, detail::range_can_is_before_bounds_v<T>,
                          bool)
        is_after_bounds(const reverse_t& i, const cursor_t& c)
    {
        return detail::range_definition_inner<T>::is_before_bounds(
            i.template get_view_reference<reverse_t, T>(),
            cursor_type_for<T>(c));
    }

    // switch before bounds to do the parent's after bounds check
    __ok_enable_if_static(range_t, detail::range_can_is_after_bounds_v<T>, bool)
        is_before_bounds(const reverse_t& i, const cursor_t& c)
    {
        return detail::range_definition_inner<T>::is_after_bounds(
            i.template get_view_reference<reverse_t, T>(),
            cursor_type_for<T>(c));
    }
};

// in the case that something is arraylike, just subtract size when doing gets
// and sets
template <typename input_range_t>
struct range_definition<detail::reversed_view_t<input_range_t>,
                        std::enable_if_t<detail::range_is_arraylike_v<
                            detail::remove_cvref_t<input_range_t>>>>
    : public detail::propagate_sizedness_t<
          detail::reversed_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>>
{
    using range_t = detail::remove_cvref_t<input_range_t>;
    using reversed_t = detail::reversed_view_t<input_range_t>;

    static constexpr bool is_view = true;
    static constexpr bool is_arraylike = true;

    __ok_enable_if_static(range_t, detail::range_has_get_v<T>,
                          value_type_for<range_t>)
        get(const reversed_t& range, size_t cursor) OKAYLIB_NOEXCEPT
    {
        const auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(
            cursor < size,
            "Bad cursor passed to reverse_view_t::get(), overflow will occur");
        return ok::iter_copyout(parent_ref, size - (cursor + 1));
    }

    __ok_enable_if_static(range_t, detail::range_has_get_ref_v<T>,
                          value_type_for<range_t>&)
        get_ref(reversed_t& range, size_t cursor) OKAYLIB_NOEXCEPT
    {
        auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(cursor < size,
                    "Bad cursor passed to reverse_view_t::get_ref() const, "
                    "overflow will occur");
        return ok::iter_get_ref(parent_ref, size - (cursor + 1));
    }

    __ok_enable_if_static(range_t, detail::range_has_get_ref_const_v<T>,
                          const value_type_for<range_t>&)
        get_ref(const reversed_t& range, size_t cursor) OKAYLIB_NOEXCEPT
    {
        const auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(cursor < size,
                    "Bad cursor passed to reverse_view_t::get_ref(), overflow "
                    "will occur");
        return ok::iter_get_ref(parent_ref, size - (cursor + 1));
    }

    __ok_enable_if_static(range_t, detail::range_has_get_v<T>, void)
        set(const reversed_t& range, size_t cursor,
            value_type_for<range_t>&& value) OKAYLIB_NOEXCEPT
    {
        const auto& parent_ref =
            range.template get_view_reference<reversed_t, range_t>();
        const size_t size = ok::size(parent_ref);
        __ok_assert(
            cursor < size,
            "Bad cursor passed to reverse_view_t::set(), overflow will occur");
        ok::iter_set(parent_ref, size - (cursor + 1),
                     std::forward<value_type_for<range_t>>(value));
    }
};

constexpr detail::range_adaptor_closure_t<detail::reverse_fn_t> reverse;

} // namespace ok

#endif
