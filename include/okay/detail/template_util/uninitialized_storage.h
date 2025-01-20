#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_UNINITIALIZED_STORAGE_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_UNINITIALIZED_STORAGE_H__

#include "okay/detail/template_util/empty.h"
#include <type_traits>
#include <utility>

/*
 * uninitialized_storage_t<T> is a template over a union. The type will inherit
 * the trivial-destructibility of the contained type. It is unchecked and does
 * not track whether its contents are initialized. It does not actually destroy
 * its contents- a wrapper must be used to invoke the destructor of the "value"
 * member.
 * It contents can be initialized in place with a std::in_place_t tagged
 * constructor.
 */

namespace ok::detail {

template <typename inner_input_contained_t,
          bool = std::is_trivially_destructible_v<inner_input_contained_t>>
union uninitialized_storage_t
{
    constexpr uninitialized_storage_t() OKAYLIB_NOEXCEPT : empty() {}

    template <typename... args_t>
    constexpr uninitialized_storage_t(std::in_place_t,
                                      args_t&&... args) OKAYLIB_NOEXCEPT
        : value(std::forward<args_t>(args)...)
    {
    }

    empty_t empty;
    inner_input_contained_t value;
};

template <typename inner_input_contained_t>
union uninitialized_storage_t<inner_input_contained_t, false>
{
    constexpr uninitialized_storage_t() OKAYLIB_NOEXCEPT : empty() {}

    template <typename... args_t>
    constexpr uninitialized_storage_t(std::in_place_t,
                                      args_t&&... args) OKAYLIB_NOEXCEPT
        : value(std::forward<args_t>(args)...)
    {
    }

    empty_t empty;
    inner_input_contained_t value;

    // only addition for non trivially destructible types
    inline ~uninitialized_storage_t() {} // TODO: _GLIBCXX20_CONSTEXPR for this?
};
} // namespace ok::detail

#endif
