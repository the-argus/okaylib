#ifndef __OKAYLIB_RANGES_VIEWS_ENUMERATE_H__
#define __OKAYLIB_RANGES_VIEWS_ENUMERATE_H__

#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t> struct enumerated_view_t;

struct enumerate_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_range_v<range_t>,
                      "Cannot enumerate given type- it is not a range.");
        return enumerated_view_t<decltype(range)>{std::forward<range_t>(range)};
    }
};

// conditionally either a ref view wrapper or a owned view wrapper or just
// straight up inherits from the range_t if it's a view
template <typename range_t>
struct enumerated_view_t : public underlying_view_type<range_t>::type
{};

template <typename parent_range_t>
struct enumerated_cursor_t
    : public cursor_wrapper_t<enumerated_cursor_t<parent_range_t>,
                              parent_range_t>
{
  private:
    using parent_cursor_t = cursor_type_for<parent_range_t>;
    using wrapper_t =
        cursor_wrapper_t<enumerated_cursor_t<parent_range_t>, parent_range_t>;
    using self_t = enumerated_cursor_t;

    constexpr void increment() OKAYLIB_NOEXCEPT { ++m_index; }
    constexpr void decrement() OKAYLIB_NOEXCEPT { --m_index; }

    constexpr static bool compare(const self_t& lhs,
                                  const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        __ok_assert((lhs.index() == rhs.index()) ==
                    (lhs.inner() == rhs.inner()));
        // we dont care if indices are the same really, except in debug mode.
        // underlying cursor can be compared instead
        return true;
    }

    constexpr static bool less_than(const self_t& lhs,
                                    const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        __ok_assert((lhs.index() < rhs.index()) == (lhs.inner() < rhs.inner()));
        return true;
    }

    constexpr static bool less_than_eql(const self_t& lhs,
                                        const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        __ok_assert((lhs.index() <= rhs.index()) ==
                    (lhs.inner() <= rhs.inner()));
        return true;
    }

    constexpr static bool greater_than_eql(const self_t& lhs,
                                           const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        __ok_assert((lhs.index() >= rhs.index()) ==
                    (lhs.inner() >= rhs.inner()));
        return true;
    }

    constexpr static bool greater_than(const self_t& lhs,
                                       const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        __ok_assert((lhs.index() > rhs.index()) == (lhs.inner() > rhs.inner()));
        return true;
    }

    constexpr void plus_eql(size_t delta) OKAYLIB_NOEXCEPT { m_index += delta; }
    constexpr void minus_eql(size_t delta) OKAYLIB_NOEXCEPT
    {
        m_index -= delta;
    }

  public:
    explicit constexpr enumerated_cursor_t(parent_cursor_t&& c)
        : OKAYLIB_NOEXCEPT m_index(0), wrapper_t(std::move(c))
    {
    }

    friend wrapper_t;
    friend class range_definition<detail::enumerated_view_t<parent_range_t>>;

    constexpr size_t index() const OKAYLIB_NOEXCEPT { return m_index; }

  private:
    size_t m_index;
};

} // namespace detail

// TODO: review const / ref correctness here, range_t input is allowed to be a
// reference or const but thats not really being removed before sending it to
// other templates
template <typename range_t>
struct range_definition<detail::enumerated_view_t<range_t>>
    : public detail::propagate_sizedness_t<detail::enumerated_view_t<range_t>,
                                           detail::remove_cvref_t<range_t>>,
      public detail::propagate_begin_t<detail::enumerated_view_t<range_t>,
                                       detail::remove_cvref_t<range_t>,
                                       detail::enumerated_cursor_t<range_t>>,
      public detail::propagate_boundscheck_t<
          detail::enumerated_view_t<range_t>, detail::remove_cvref_t<range_t>,
          detail::enumerated_cursor_t<range_t>>,
      public detail::propagate_cursor_comparison_optimization_marker_t<range_t>
{
    static constexpr bool is_view = true;

    using enumerated_t = detail::enumerated_view_t<range_t>;
    using cursor_t = detail::enumerated_cursor_t<range_t>;

    __ok_enable_if_static(range_t, detail::range_definition_has_increment_v<T>,
                          void)
        increment(const enumerated_t& range, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        // perform parent's increment function
        ok::increment(range.template get_view_reference<enumerated_t, T>(),
                      c.inner());
        // also do our bit
        c.increment();
    }

    __ok_enable_if_static(range_t, detail::range_definition_has_decrement_v<T>,
                          void)
        decrement(const enumerated_t& range, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        ok::decrement(
            range.template get_view_reference<enumerated_t, range_t>(),
            c.inner());
        c.decrement();
    }

    constexpr static auto get(const enumerated_t& range,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::range_definition_inner<range_t>;
        if constexpr (detail::range_has_get_ref_v<range_t>) {
            // if get_ref nonconst exists, do evil const cast to get to it
            using reftype = decltype(inner_def::get_ref(
                const_cast<enumerated_t&>(range)
                    .template get_view_reference<enumerated_t, range_t>(),
                c.inner()));
            return std::pair<reftype, const size_t>(
                inner_def::get_ref(
                    const_cast<enumerated_t&>(range)
                        .template get_view_reference<enumerated_t, range_t>(),
                    c.inner()),
                c.index());
        } else if constexpr (detail::range_has_get_ref_const_v<range_t>) {
            using reftype = decltype(inner_def::get_ref(
                range.template get_view_reference<enumerated_t, range_t>(),
                c.inner()));
            return std::pair<reftype, const size_t>(
                inner_def::get_ref(
                    range.template get_view_reference<enumerated_t, range_t>(),
                    c.inner()),
                c.index());
        } else {
            using gettype = decltype(inner_def::get(
                range.template get_view_reference<enumerated_t, range_t>(),
                c.inner()));
            return std::pair<gettype, const size_t>(
                inner_def::get(
                    range.template get_view_reference<enumerated_t, range_t>(),
                    c.inner()),
                c.index());
        }
    }

    // satisfy requirement that get() returns value_type
    // TODO: value_type still needed?
    using value_type = decltype(get(std::declval<const enumerated_t&>(),
                                    std::declval<const cursor_t&>()));
};

constexpr detail::range_adaptor_closure_t<detail::enumerate_fn_t> enumerate;

} // namespace ok

#endif
