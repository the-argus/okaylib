#ifndef __OKAYLIB_RANGES_VIEWS_ANY_H__
#define __OKAYLIB_RANGES_VIEWS_ANY_H__

#include "okay/detail/view_common.h"
#include "okay/ranges/adaptors.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace detail {
template <typename range_t, typename predicate_t> struct any_closure_t;

// function which creates an any_closure_t
struct any_fn_t
{
    template <typename range_t, typename predicate_t>
    constexpr decltype(auto)
    operator()(range_t&& range, predicate_t&& pred) const OKAYLIB_NOEXCEPT
    {
        static_assert(detail::is_producing_range_v<range_t>,
                      "Input to ok::any is not a valid input range (a range "
                      "which can be iterated over to get values.)");
        static_assert(
            is_std_invocable_r_v<predicate_t,
                                 // returns bool
                                 bool,
                                 // has one argument: const ref to value type of
                                 // the range
                                 const value_type_for<range_t>&>,
            "Function given to ok::any() must accept a const reference to the "
            "value type of the range, and return a bool.");
        return any_closure_t<decltype(range), decltype(pred)>(
            std::forward<range_t>(range), std::forward<predicate_t>(pred));
    }
};

// type which stores a range and a predicate, and then can be rvalue converted
// into a boolean, causing it to run the predicate on all elements and return
// true if a single element returns true from the predicate
template <typename input_range_t, typename input_predicate_t>
struct any_closure_t : public underlying_view_type<input_range_t>::type
{
  private:
    using range_t = std::remove_reference_t<input_range_t>;
    using predicate_t = detail::remove_cvref_t<input_predicate_t>;
    assignment_op_wrapper_t<std::remove_reference_t<input_predicate_t>>
        m_predicate;

  public:
    any_closure_t(const any_closure_t&) = default;
    any_closure_t& operator=(const any_closure_t&) = default;
    any_closure_t(any_closure_t&&) = default;
    any_closure_t& operator=(any_closure_t&&) = default;

    constexpr any_closure_t(input_range_t&& range,
                            input_predicate_t&& pred) OKAYLIB_NOEXCEPT
        : underlying_view_type<input_range_t>::type(
              std::forward<input_range_t>(range)),
          m_predicate(std::forward<input_predicate_t>(pred))
    {
    }

    constexpr operator bool() &&
    {
        auto&& range =
            this->template get_view_reference<any_closure_t, range_t>();

        for (auto cursor = ok::begin(range); ok::is_inbounds(range, cursor);
             ok::increment(range, cursor)) {
            if (m_predicate.value()(
                    ok::iter_get_temporary_ref(range, cursor))) {
                return true;
            }
        }
        return false;
    }
};

} // namespace detail

constexpr detail::range_adaptor_t<detail::any_fn_t> any;

} // namespace ok

#endif
