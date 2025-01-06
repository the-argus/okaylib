#ifndef __OKAYLIB_ITERABLE_VIEW_INTERFACE_H__
#define __OKAYLIB_ITERABLE_VIEW_INTERFACE_H__

#include "okay/detail/ok_assert.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/iterable/view_traits.h"
#include <type_traits>

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok::detail // TODO: make this public?
{

template <typename derived_t, typename> class view_interface
{
    static_assert(false, "Given derived type should be a class and should not "
                         "be const or volatile qualified.");
};

/// NOTE: it is a bug to derive T from view_interface<U> where T and U are
/// different types
template <typename derived_t>
class view_interface<
    derived_t,
    std::enable_if_t<std::is_class_v<derived_t> &&
                     std::is_same_v<derived_t, std::remove_cv_t<derived_t>>>>
{
  private:
    constexpr derived_t& this_as_derived() OKAYLIB_NOEXCEPT
    {
        static_assert(
            is_derived_from_v<derived_t, view_interface<derived_t, void>>);
        static_assert(is_view_v<derived_t>);
        return static_cast<derived_t&>(*this);
    }

    constexpr const derived_t& this_as_derived() const OKAYLIB_NOEXCEPT
    {
        static_assert(
            is_derived_from_v<derived_t, view_interface<derived_t, void>>);
        static_assert(is_view_v<derived_t>);
        return static_cast<const derived_t&>(*this);
    }

  public:
    template <typename T = derived_t>
    inline constexpr auto empty() const OKAYLIB_NOEXCEPT
        -> std::enable_if_t<!detail::iterable_marked_finite_v<T> &&
                                std::is_same_v<derived_t, T>,
                            bool>
    {
        if constexpr (detail::iterable_marked_infinite_v<derived_t>)
            return false;
        else if constexpr (detail::iterable_has_size_v<derived_t>)
            return ok::size(this_as_derived()) == 0;
        else
            static_assert(false, "unknown additional case of size()");
    }

    template <typename T = derived_t>
    constexpr explicit
    operator std::enable_if_t<!detail::iterable_marked_finite_v<T> &&
                                  std::is_same_v<T, derived_t>,
                              bool>() const OKAYLIB_NOEXCEPT
    {
        return !empty();
    }

    template <typename T = derived_t>
    constexpr auto data() const OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, derived_t>,
                            decltype(ok::data(this_as_derived()))>
    {
        return ok::data(this_as_derived());
    }

    template <typename T = derived_t>
    constexpr auto size() const OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, derived_t>,
                            decltype(ok::size(this_as_derived()))>
    {
        return ok::size(this_as_derived());
    }

    template <typename T = derived_t>
    constexpr auto front() OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, derived_t>,
                            decltype(ok::begin(this_as_derived()),
                                     ok::iter_get_ref(this_as_derived()))>
    {
        __ok_assert(!empty());
        return ok::iter_get_ref(this_as_derived(),
                                ok::begin(this_as_derived()));
    }

    template <typename T = derived_t>
    constexpr auto front() const OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, derived_t>,
                            decltype(ok::begin(this_as_derived()),
                                     ok::iter_get_ref(this_as_derived()))>
    {
        __ok_assert(!empty());
        return ok::iter_get_ref(this_as_derived(),
                                ok::begin(this_as_derived()));
    }

    template <typename T = derived_t>
    constexpr auto back() OKAYLIB_NOEXCEPT
        -> std::enable_if_t<
            std::is_same_v<T, derived_t> && detail::iterable_has_size_v<T> &&
                detail::is_random_access_iterable_v<T>,
            decltype(ok::iter_get_ref(this_as_derived(),
                                      ok::begin(this_as_derived()) +
                                          ok::size(this_as_derived())))>
    {
        __ok_assert(!empty());
        return ok::iter_get_ref(this_as_derived(),
                                ok::begin(this_as_derived()) +
                                    ok::size(this_as_derived()));
    }

    template <typename T = derived_t>
    constexpr auto back() const OKAYLIB_NOEXCEPT
        -> std::enable_if_t<
            std::is_same_v<T, derived_t> && detail::iterable_has_size_v<T> &&
                detail::is_random_access_iterable_v<T>,
            decltype(ok::iter_get_ref(this_as_derived(),
                                      ok::begin(this_as_derived()) +
                                          ok::size(this_as_derived())))>
    {
        __ok_assert(!empty());
        return ok::iter_get_ref(this_as_derived(),
                                ok::begin(this_as_derived()) +
                                    ok::size(this_as_derived()));
    }

    template <typename T = derived_t>
    constexpr auto operator[](const cursor_type_unchecked_for<T>& cursor)
        OKAYLIB_NOEXCEPT->decltype(ok::iter_get_ref(this_as_derived(), cursor))
    {
        return ok::iter_get_ref(this_as_derived(), cursor);
    }

    template <typename T = derived_t>
    constexpr auto operator[](const cursor_type_unchecked_for<T>& cursor) const
        OKAYLIB_NOEXCEPT->decltype(ok::iter_get_ref(this_as_derived(), cursor))
    {
        return ok::iter_get_ref(this_as_derived(), cursor);
    }
};
} // namespace ok::detail

#endif
