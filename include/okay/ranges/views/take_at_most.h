#ifndef __OKAYLIB_RANGES_VIEWS_TAKE_AT_MOST_H__
#define __OKAYLIB_RANGES_VIEWS_TAKE_AT_MOST_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t> struct take_at_most_view_t;

struct take_at_most_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator()(range_t&& range,
                                        size_t amount) const OKAYLIB_NOEXCEPT
    {
        return take_at_most_view_t<decltype(range)>{
            std::forward<range_t>(range), amount};
    }
};

template <typename input_parent_range_t>
struct take_at_most_cursor_t
    : public cursor_wrapper_t<take_at_most_cursor_t<input_parent_range_t>,
                              detail::remove_cvref_t<input_parent_range_t>>
{
  private:
    using parent_range_t = detail::remove_cvref_t<input_parent_range_t>;
    using parent_cursor_t = cursor_type_for<parent_range_t>;
    using wrapper_t =
        cursor_wrapper_t<take_at_most_cursor_t<input_parent_range_t>,
                         parent_range_t>;
    using self_t = take_at_most_cursor_t;

#define __take_at_most_relop(name)                                   \
    constexpr static bool name(const self_t& lhs, const self_t& rhs) \
        OKAYLIB_NOEXCEPT                                             \
    {                                                                \
        return true;                                                 \
    }

    __take_at_most_relop(compare) __take_at_most_relop(less_than)
        __take_at_most_relop(less_than_eql) __take_at_most_relop(greater_than)
            __take_at_most_relop(greater_than_eql)

#undef __take_at_most_relop

                constexpr void increment() OKAYLIB_NOEXCEPT
    {
        ++m_consumed;
    }
    constexpr void decrement() OKAYLIB_NOEXCEPT { --m_consumed; }

    constexpr void plus_eql(size_t delta) OKAYLIB_NOEXCEPT
    {
        m_consumed += delta;
    }
    constexpr void minus_eql(size_t delta) OKAYLIB_NOEXCEPT
    {
        m_consumed -= delta;
    }

  public:
    explicit constexpr take_at_most_cursor_t(parent_cursor_t&& c)
        : OKAYLIB_NOEXCEPT m_consumed(0), wrapper_t(std::move(c))
    {
    }

    constexpr operator parent_cursor_t() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const wrapper_t*>(this)->inner();
    }

    friend wrapper_t;
    friend class range_definition<
        detail::take_at_most_view_t<input_parent_range_t>>;

    constexpr size_t num_consumed() const OKAYLIB_NOEXCEPT
    {
        return m_consumed;
    }

  private:
    size_t m_consumed;
};

template <typename range_t>
struct take_at_most_view_t : public underlying_view_type<range_t>::type
{
  private:
    using cursor_t = take_at_most_cursor_t<detail::remove_cvref_t<range_t>>;
    size_t m_size;

  public:
    constexpr size_t size() const noexcept { return m_size; }

    take_at_most_view_t(const take_at_most_view_t&) = default;
    take_at_most_view_t& operator=(const take_at_most_view_t&) = default;
    take_at_most_view_t(take_at_most_view_t&&) = default;
    take_at_most_view_t& operator=(take_at_most_view_t&&) = default;

    constexpr take_at_most_view_t(range_t&& range, size_t amount)
        : OKAYLIB_NOEXCEPT underlying_view_type<range_t>::type(
              std::forward<range_t>(range))
    {
        if constexpr (detail::range_has_size_v<range_t>) {
            auto& parent_range =
                this->template get_view_reference<take_at_most_view_t,
                                                  range_t>();
            m_size = std::min(ok::size(parent_range), amount);
        } else {
            m_size = amount;
        }
    }
};

template <typename input_range_t> struct sized_take_at_most_range_t
{
  private:
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;

  public:
    static constexpr size_t size(const take_at_most_t& i) { return i.size(); }
};

// take_at_most_cursor_t but it is just the parent's cursor if there's no need
// to track number of items consumed
template <typename input_range_t>
using take_at_most_cursor_optimized_t = std::conditional_t<
    detail::is_random_access_range_v<detail::remove_cvref_t<input_range_t>> &&
        !detail::range_marked_finite_v<detail::remove_cvref_t<input_range_t>>,
    cursor_type_for<detail::remove_cvref_t<input_range_t>>,
    take_at_most_cursor_t<input_range_t>>;

} // namespace detail

template <typename input_range_t>
struct range_definition<detail::take_at_most_view_t<input_range_t>>
    : public detail::propagate_begin_t<
          detail::take_at_most_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          detail::take_at_most_cursor_optimized_t<input_range_t>>,
      public detail::propagate_get_set_t<
          detail::take_at_most_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          detail::take_at_most_cursor_optimized_t<input_range_t>>,
      public std::conditional_t<
          detail::range_marked_finite_v<detail::remove_cvref_t<input_range_t>>,
          detail::infinite_static_def_t<false>,
          detail::sized_take_at_most_range_t<input_range_t>>,
      public detail::propagate_cursor_comparison_optimization_marker_t<
          input_range_t>
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;
    using cursor_t = detail::take_at_most_cursor_optimized_t<input_range_t>;

    template <typename T = range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<range_t, T> && detail::range_has_is_inbounds_v<T>, bool>
    is_inbounds(const take_at_most_t& i, const cursor_t& c)
    {
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref =
            i.template get_view_reference<take_at_most_t, T>();

        if constexpr (detail::is_random_access_range_v<T> &&
                      detail::range_has_size_v<T>) {
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
        } else if constexpr (detail::range_has_size_v<T>) {
            return c.num_consumed() < i.size();
        } else {
            return c.num_consumed() < i.size() &&
                   parent_def::is_inbounds(parent_ref, c.inner());
        }
    }

    template <typename T = range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<range_t, T> && detail::range_has_is_after_bounds_v<T>,
        bool>
    is_after_bounds(const take_at_most_t& i, const cursor_t& c)
    {
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref =
            i.template get_view_reference<take_at_most_t, T>();

        if constexpr (detail::is_random_access_range_v<T> &&
                      detail::range_has_size_v<T>) {
            static_assert(std::is_same_v<cursor_t, cursor_type_for<T>>,
                          "Cursor type has extra unneeded stuff in take() even "
                          "though it doesnt need it.");
            return c >= ok::begin(parent_ref) + i.size();
        } else if constexpr (detail::range_has_size_v<T>) {
            return c.num_consumed() >= i.size();
        } else {
            return c.num_consumed() >= i.size() ||
                   parent_def::is_after_bounds(parent_ref, c.inner());
        }
    }

    template <typename T = range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<range_t, T> && detail::range_has_is_before_bounds_v<T>,
        bool>
    is_before_bounds(const take_at_most_t& i, const cursor_t& c)
    {
        static_assert(detail::range_cursor_can_go_below_begin<T>::value);
        using parent_def = detail::range_definition_inner<T>;
        const range_t& parent_ref =
            i.template get_view_reference<take_at_most_t, T>();

        if constexpr (detail::is_random_access_range_v<T> &&
                      detail::range_has_size_v<T>) {
            static_assert(std::is_same_v<cursor_t, cursor_type_for<T>>,
                          "Cursor type has extra unneeded stuff in take() even "
                          "though it doesnt need it.");
            return c < ok::begin(parent_ref);
        } else if constexpr (detail::range_has_size_v<T>) {
            return c.num_consumed() >= i.size();
        } else {
            return c.num_consumed() >= i.size() ||
                   parent_def::is_after_bounds(parent_ref, c.inner());
        }
    }

    template <typename T = range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<range_t, T> &&
        detail::range_definition_has_increment_v<T>>
    increment(const take_at_most_t& i, cursor_t& c)
    {
        // if its random access, the cursor type might just be the parent cursor
        // type
        static_assert(!detail::is_random_access_range_v<T>,
                      "this code relies on all range definitions which have "
                      "increment() defined also being not random access");
        const auto& parent_ref =
            i.template get_view_reference<take_at_most_t, T>();
        ok::increment(parent_ref, c.inner());
        c.increment();
    }

    template <typename T = range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<range_t, T> &&
        detail::range_definition_has_decrement_v<T>>
    decrement(const take_at_most_t& i, cursor_t& c)
    {
        static_assert(!detail::is_random_access_range_v<T>,
                      "this code relies on all range definitions which have "
                      "decrement() defined also being not random access");
        const auto& parent_ref =
            i.template get_view_reference<take_at_most_t, T>();
        ok::decrement(parent_ref, c.inner());
        c.decrement();
    }
};

constexpr detail::range_adaptor_t<detail::take_at_most_fn_t> take_at_most;

} // namespace ok

#endif
