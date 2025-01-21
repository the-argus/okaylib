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

template <bool size_known, bool can_get_inbounds_from_size>
struct take_at_most_members_t;

enum class take_at_most_size_deduction_t
{
    size_known_bounds_known,
    size_known_bounds_unknown,
    all_unknown,
};

template <> struct take_at_most_members_t<true, true>
{
    constexpr void increment_consumed() OKAYLIB_NOEXCEPT {}
    constexpr size_t size() const OKAYLIB_NOEXCEPT { return m_size; }

    static constexpr take_at_most_size_deduction_t size_deduction =
        take_at_most_size_deduction_t::size_known_bounds_known;

  protected:
    size_t m_size;
};

// never happens- if you can get inbounds from knowing size, that requires you
// know size
template <> struct take_at_most_members_t<false, true>;

template <> struct take_at_most_members_t<true, false>
{
    constexpr size_t size() const OKAYLIB_NOEXCEPT { return m_size; }

    constexpr void increment_consumed() OKAYLIB_NOEXCEPT { ++m_consumed; }

    static constexpr take_at_most_size_deduction_t size_deduction =
        take_at_most_size_deduction_t::size_known_bounds_unknown;

  protected:
    size_t m_size;
    size_t m_consumed = 0;
};

template <> struct take_at_most_members_t<false, false>
{
    constexpr void increment_consumed() OKAYLIB_NOEXCEPT { ++m_consumed; }

    static constexpr take_at_most_size_deduction_t size_deduction =
        take_at_most_size_deduction_t::all_unknown;

  protected:
    size_t m_consumed = 0;
};

template <typename range_t>
struct take_at_most_view_t : public underlying_view_type<range_t>::type,
                             public take_at_most_members_t<
                                 !detail::range_marked_finite_v<range_t>,
                                 !detail::range_marked_finite_v<range_t> &&
                                     detail::is_random_access_range_v<range_t>>
{
  private:
    using parent_t =
        take_at_most_members_t<!detail::range_marked_finite_v<range_t>,
                               !detail::range_marked_finite_v<range_t> &&
                                   detail::is_random_access_range_v<range_t>>;
    constexpr parent_t* parent() noexcept
    {
        return static_cast<parent_t*>(this);
    }
    constexpr const parent_t* parent() const noexcept
    {
        return static_cast<parent_t*>(this);
    }

  public:
    take_at_most_view_t(const take_at_most_view_t&) = default;
    take_at_most_view_t& operator=(const take_at_most_view_t&) = default;
    take_at_most_view_t(take_at_most_view_t&&) = default;
    take_at_most_view_t& operator=(take_at_most_view_t&&) = default;

    constexpr take_at_most_view_t(range_t&& range, size_t amount)
        : OKAYLIB_NOEXCEPT underlying_view_type<range_t>::type(
              std::forward<range_t>(range))
    {
        if constexpr (!detail::range_marked_finite_v<range_t>) {
            if constexpr (detail::range_marked_infinite_v<range_t>)
                parent()->m_size = amount;
            else {
                auto& parent_range =
                    this->template get_view_reference<take_at_most_view_t,
                                                      range_t>();
                parent()->m_size = std::min(ok::size(parent_range), amount);
            }
        }
    }
};

template <typename input_range_t> struct sized_take_at_most_range_t
{
  private:
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;

  public:
    static constexpr size_t size(const take_at_most_t& i)
    {
        return i.size();
    }
};

} // namespace detail

template <typename input_range_t>
struct range_definition<detail::take_at_most_view_t<input_range_t>>
    : public detail::propagate_begin_t<
          detail::take_at_most_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public detail::propagate_get_set_t<
          detail::take_at_most_view_t<input_range_t>,
          detail::remove_cvref_t<input_range_t>,
          cursor_type_for<detail::remove_cvref_t<input_range_t>>>,
      public std::conditional_t<
          detail::range_marked_finite_v<detail::remove_cvref_t<input_range_t>>,
          detail::infinite_static_def_t<false>,
          detail::sized_take_at_most_range_t<input_range_t>>
{
    static constexpr bool is_view = true;

    using range_t = detail::remove_cvref_t<input_range_t>;
    using take_at_most_t = detail::take_at_most_view_t<input_range_t>;
    using cursor_t = cursor_type_for<range_t>;
};

constexpr detail::range_adaptor_t<detail::take_at_most_fn_t> take_at_most;

} // namespace ok

#endif
