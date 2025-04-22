#ifndef __OKAYLIB_RANGES_VIEWS_DROP_H__
#define __OKAYLIB_RANGES_VIEWS_DROP_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t> struct drop_view_t;

struct drop_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range,
                                        size_t amount) const OKAYLIB_NOEXCEPT
    {
        // TODO: support these. it should be a simple extension of the
        // drop_cursor_bidir_t
        static_assert(
            !(detail::is_random_access_range_v<range_t> &&
              detail::range_marked_finite_v<range_t>),
            "Drop view does not support random access ranges of unknown size.");
        return drop_view_t<decltype(range)>{std::forward<range_t>(range),
                                            amount};
    }
};

template <typename input_parent_range_t>
struct drop_cursor_bidir_t
    : public cursor_wrapper_t<drop_cursor_bidir_t<input_parent_range_t>,
                              detail::remove_cvref_t<input_parent_range_t>>
{
  private:
    using range_t = std::remove_reference_t<input_parent_range_t>;
    using parent_cursor_t = cursor_type_for<range_t>;
    using wrapper_t =
        cursor_wrapper_t<drop_cursor_bidir_t<input_parent_range_t>, range_t>;
    using self_t = drop_cursor_bidir_t;

    constexpr static bool compare(const self_t& lhs,
                                  const self_t& rhs) OKAYLIB_NOEXCEPT
    {
        return true;
    }

    constexpr void increment() OKAYLIB_NOEXCEPT { ++m_consumed; }
    constexpr void decrement() OKAYLIB_NOEXCEPT { --m_consumed; }

  public:
    explicit constexpr drop_cursor_bidir_t(parent_cursor_t&& c,
                                           size_t consumed) OKAYLIB_NOEXCEPT
        : m_consumed(consumed),
          wrapper_t(std::move(c))
    {
    }

    constexpr operator parent_cursor_t() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const wrapper_t*>(this)->inner();
    }

    friend wrapper_t;
    friend class range_definition<detail::drop_view_t<input_parent_range_t>>;

    constexpr int64_t num_consumed() const OKAYLIB_NOEXCEPT
    {
        return m_consumed;
    }

  private:
    int64_t m_consumed;
};

template <typename input_range_t>
using drop_cursor_t = std::conditional_t<
    detail::is_bidirectional_range_v<detail::remove_cvref_t<input_range_t>> &&
        !detail::is_random_access_range_v<
            detail::remove_cvref_t<input_range_t>>,
    drop_cursor_bidir_t<input_range_t>,
    cursor_type_for<detail::remove_cvref_t<input_range_t>>>;

template <typename input_range_t>
struct drop_view_t : public underlying_view_type<input_range_t>::type
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;
    size_t m_amount;

  public:
    constexpr size_t amount() const noexcept { return m_amount; }

    drop_view_t(const drop_view_t&) = default;
    drop_view_t& operator=(const drop_view_t&) = default;
    drop_view_t(drop_view_t&&) = default;
    drop_view_t& operator=(drop_view_t&&) = default;

    constexpr drop_view_t(input_range_t&& range, size_t amount) OKAYLIB_NOEXCEPT
        : underlying_view_type<input_range_t>::type(
              std::forward<input_range_t>(range))
    {
        if constexpr (range_can_size_v<range_t>) {
            const auto& parent_ref =
                this->template get_view_reference<drop_view_t, input_range_t>();

            const size_t actual = ok::size(parent_ref);
            if (amount > actual) [[unlikely]] {
                m_amount = actual;
            } else {
                m_amount = amount;
            }
        } else {
            m_amount = amount;
        }
    }
};

template <typename input_range_t> struct sized_drop_range_t
{
  private:
    using drop_t = detail::drop_view_t<input_range_t>;

  public:
    static constexpr size_t size(const drop_t& i)
    {
        const auto& parent_ref = i.template get_view_reference<
            drop_t, detail::remove_cvref_t<input_range_t>>();

        // should not to overflow thanks to the cap in drop_view_t
        // constructor
        return ok::size(parent_ref) - i.amount();
    }
};

} // namespace detail

template <typename input_range_t>
struct range_definition<detail::drop_view_t<input_range_t>>
    : public detail::propagate_begin_t<detail::drop_view_t<input_range_t>,
                                       detail::remove_cvref_t<input_range_t>,
                                       detail::drop_cursor_t<input_range_t>>,
      public detail::propagate_get_set_t<detail::drop_view_t<input_range_t>,
                                         detail::remove_cvref_t<input_range_t>,
                                         detail::drop_cursor_t<input_range_t>>,
      // propagate infinite / finite status, but if sized then use unique
      // sized_drop_range_t type
      public std::conditional_t<
          detail::range_can_size_v<detail::remove_cvref_t<input_range_t>>,
          detail::sized_drop_range_t<input_range_t>,
          detail::infinite_static_def_t<detail::range_marked_infinite_v<
              detail::remove_cvref_t<input_range_t>>>>
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using drop_t = detail::drop_view_t<input_range_t>;
    using cursor_t = detail::drop_cursor_t<input_range_t>;

    static_assert(
        !(detail::is_random_access_range_v<range_t> &&
          detail::range_marked_finite_v<range_t>),
        "Drop view does not support random access ranges of unknown size.");

    constexpr static cursor_t begin(const drop_t& i) OKAYLIB_NOEXCEPT
    {
        const auto& parent_ref =
            i.template get_view_reference<drop_t, range_t>();
        if constexpr (detail::is_random_access_range_v<range_t>) {
            return ok::begin(parent_ref) + i.amount();
        } else {
            auto c = ok::begin(parent_ref);
            const auto amount = i.amount();
            auto counter = amount;
            // increment begin a number of times since we don't have random
            // access and can't jump ahead
            while (ok::is_inbounds(parent_ref, c) && counter != 0) {
                ok::increment(parent_ref, c);
                --counter;
            }
            if constexpr (detail::is_bidirectional_range_v<range_t>) {
                return cursor_t(std::move(c), amount);
            } else {
                return c;
            }
        }
    }

    __ok_enable_if_static(range_t, detail::range_can_is_inbounds_v<T>, bool)
        is_inbounds(const drop_t& i, const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref = i.template get_view_reference<drop_t, T>();

        if constexpr (detail::is_random_access_range_v<T>) {
            static_assert(!detail::range_marked_finite_v<T>);
            const cursor_t parent_begin = ok::begin(parent_ref);
            const cursor_t our_begin = parent_begin + i.amount();
            const bool begin_check = c >= our_begin;

            if constexpr (detail::range_marked_infinite_v<T>) {
                return begin_check;
            } else {
                static_assert(detail::range_can_size_v<T>);
                const cursor_t parent_end = parent_begin + ok::size(parent_ref);
                return c < parent_end && begin_check;
            }
        } else if constexpr (detail::is_bidirectional_range_v<T>) {
            return c.num_consumed() >= i.amount() &&
                   ok::is_inbounds(parent_ref, c.inner());
        } else {
            // in this case, we are not bidirectional or random access, meaning
            // that its only possible to go out of bounds by moving off the end,
            // and drop() cannot change what the end is for the parent, so its
            // check should remain valid.
            static_assert(!detail::range_impls_decrement_v<range_t>);
            return parent_def::is_inbounds(parent_ref, c);
        }
    }
};

constexpr detail::range_adaptor_t<detail::drop_fn_t> drop;

} // namespace ok

#endif
