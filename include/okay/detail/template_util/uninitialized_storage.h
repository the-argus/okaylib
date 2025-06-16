#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_UNINITIALIZED_STORAGE_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_UNINITIALIZED_STORAGE_H__

#include "okay/detail/in_place.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/empty.h"
#include "okay/detail/traits/constructor_traits.h"
#include <type_traits>
#include <utility>

/*
 * uninitialized_storage_t<T> is a template over a union. The type will inherit
 * the trivial-destructibility of the contained type. It is unchecked and does
 * not track whether its contents are initialized. It does not actually destroy
 * its contents- a wrapper must be used to invoke the destructor of the "value"
 * member.
 * It contents can be initialized in place with a ok::in_place_t tagged
 * constructor.
 */

namespace ok::detail {

template <typename inner_input_contained_t> union uninitialized_storage_t
{
  public:
    using type = inner_input_contained_t;

    empty_t empty;
    inner_input_contained_t value;

    constexpr uninitialized_storage_t() OKAYLIB_NOEXCEPT : empty() {}

    template <typename... args_t>
        requires is_std_constructible_c<inner_input_contained_t, args_t...>
    constexpr uninitialized_storage_t(ok::in_place_t,
                                      args_t&&... args) OKAYLIB_NOEXCEPT
        : value(std::forward<args_t>(args)...)
    {
    }

    template <typename... args_t>
        requires(
            is_infallible_constructible_c<inner_input_contained_t, args_t...> &&
            !is_std_constructible_c<inner_input_contained_t, args_t...> &&
            is_move_constructible_c<inner_input_contained_t>)
    constexpr uninitialized_storage_t(ok::in_place_t,
                                      args_t&&... args) OKAYLIB_NOEXCEPT
        : value(call_first_type_with_others(args...))
    {
    }

    constexpr ~uninitialized_storage_t()
        requires(!std::is_trivially_destructible_v<inner_input_contained_t>)
    {
    }

    constexpr ~uninitialized_storage_t()
        requires(std::is_trivially_destructible_v<inner_input_contained_t>)
    = default;
};

static_assert(std::is_trivially_destructible_v<uninitialized_storage_t<int>>);
} // namespace ok::detail

#endif
