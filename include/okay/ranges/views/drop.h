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
        return drop_view_t<decltype(range)>{std::forward<range_t>(range),
                                            amount};
    }
};

template <typename input_range_t>
struct drop_view_t : public underlying_view_type<input_range_t>::type
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;
    using cursor_t = cursor_type_for<range_t>;
    size_t m_amount;

  public:
    constexpr size_t amount() const noexcept { return m_amount; }

    drop_view_t(const drop_view_t&) = default;
    drop_view_t& operator=(const drop_view_t&) = default;
    drop_view_t(drop_view_t&&) = default;
    drop_view_t& operator=(drop_view_t&&) = default;

    constexpr drop_view_t(input_range_t&& range, size_t amount)
        : OKAYLIB_NOEXCEPT underlying_view_type<input_range_t>::type(
              std::forward<input_range_t>(range))
    {
        if constexpr (range_has_size_v<range_t>) {
            const auto& parent_ref =
                this->template get_view_reference<drop_view_t, input_range_t>();

            const size_t actual = parent_ref.size();
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
        const auto& parent_ref =
            i.template get_view_reference<drop_view_t, input_range_t>();

        // should not to overflow thanks to the cap in drop_view_t
        // constructor
        return parent_ref.size() - i.amount();
    }
};

} // namespace detail

template <typename input_range_t>
struct range_definition<detail::drop_view_t<input_range_t>>
    : public detail::propagate_begin_t<detail::drop_view_t<input_range_t>,
                                       detail::remove_cvref_t<input_range_t>,
                                       cursor_type_for<input_range_t>>,
      public detail::propagate_get_set_t<detail::drop_view_t<input_range_t>,
                                         detail::remove_cvref_t<input_range_t>,
                                         cursor_type_for<input_range_t>>,
      public detail::propagate_sizedness_t<
          detail::drop_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>>,
      public detail::propagate_cursor_comparison_optimization_marker_t<
          input_range_t>
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using drop_t = detail::drop_view_t<input_range_t>;
    using cursor_t = cursor_type_for<input_range_t>;

    constexpr static cursor_t begin(const drop_t& i) OKAYLIB_NOEXCEPT
    {
        const auto& parent_ref =
            i.template get_view_reference<drop_t, range_t>();
        if constexpr (detail::is_random_access_range_v<range_t>) {
            return ok::begin(parent_ref) + i.amount();
        } else {
            // TODO:
        }
    }

    __ok_enable_if_static(range_t, detail::range_has_is_inbounds_v<T>, bool)
        is_inbounds(const drop_t& i, const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref = i.template get_view_reference<drop_t, T>();

        if constexpr (detail::is_random_access_range_v<T> &&
                      !detail::range_marked_finite_v<T>) {
            static_assert(std::is_same_v<cursor_t, cursor_type_for<T>>,
                          "Cursor type has extra unneeded stuff in take() even "
                          "though it doesnt need it.");
            auto parent_begin = ok::begin(parent_ref);
            if constexpr (detail::range_cursor_can_go_below_begin<T>::value) {
                if (c < parent_begin) [[unlikely]] {
                    return false;
                }
            }

            auto advanced = parent_begin + i.size();
            // no need to check if within parent bounds- size is constant known,
            // so when instantiating this view we already capped our size
            return c < advanced;
        } else if constexpr (!detail::range_marked_finite_v<T>) {
            return c.num_consumed() < i.size();
        } else {
            return c.num_consumed() < i.size() &&
                   parent_def::is_inbounds(parent_ref, c.inner());
        }
    }

    __ok_enable_if_static(range_t, detail::range_has_is_after_bounds_v<T>, bool)
        is_after_bounds(const drop_t& i, const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref = i.template get_view_reference<drop_t, T>();

        if constexpr (detail::is_random_access_range_v<T> &&
                      !detail::range_marked_finite_v<T>) {
            static_assert(std::is_same_v<cursor_t, cursor_type_for<T>>,
                          "Cursor type has extra unneeded stuff in take() even "
                          "though it doesnt need it.");
            return c >= ok::begin(parent_ref) + i.size();
        } else if constexpr (!detail::range_marked_finite_v<T>) {
            return c.num_consumed() >= i.size();
        } else {
            return c.num_consumed() >= i.size() ||
                   parent_def::is_after_bounds(parent_ref, c.inner());
        }
    }

    __ok_enable_if_static(range_t, detail::range_has_is_before_bounds_v<T>,
                          bool)
        is_before_bounds(const drop_t& i, const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        static_assert(detail::range_cursor_can_go_below_begin<T>::value);
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref = i.template get_view_reference<drop_t, T>();

        if constexpr (detail::is_random_access_range_v<T> &&
                      !detail::range_marked_finite_v<T>) {
            static_assert(std::is_same_v<cursor_t, cursor_type_for<T>>,
                          "Cursor type has extra unneeded stuff in take() even "
                          "though it doesnt need it.");
            return c < ok::begin(parent_ref);
        } else if constexpr (!detail::range_marked_finite_v<T>) {
            return c.num_consumed() >= i.size();
        } else {
            return c.num_consumed() >= i.size() ||
                   parent_def::is_after_bounds(parent_ref, c.inner());
        }
    }
};

constexpr detail::range_adaptor_t<detail::drop_fn_t> drop;

} // namespace ok

#endif
