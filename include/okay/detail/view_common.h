#ifndef __OKAYLIB_DETAIL_VIEW_COMMON_H__
#define __OKAYLIB_DETAIL_VIEW_COMMON_H__

#include "okay/detail/addressof.h"
#include "okay/detail/template_util/empty.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/iterable/iterable.h"

namespace ok {
template <typename T, typename = void>
struct enable_view : public std::false_type
{};

// specialize enable_view or add is_view to iterable_definition
template <typename T>
struct enable_view<
    T, std::enable_if_t<detail::iterable_definition_inner<T>::is_view>>
    : public std::true_type
{};

template <typename T> constexpr bool enable_view_v = enable_view<T>::value;

namespace detail {

template <typename range_t, typename = void> class owning_view;
template <typename range_t, typename = void> class ref_view;

/// Conditionally inherit iterable_definition from this to mark as infinite
struct infinite_iterable_t
{
    static constexpr bool infinite = true;
};

/// Conditionally inherit iterable_definition from this to mark as finite
struct finite_iterable_t
{
    static constexpr bool infinite = false;
};

/// Conditionally inherit iterable_definition from this to mark as sized.
/// iterable_t: the iterable which is being defined. must inherit from
/// owning_view or ref_view, and therefore have a base() function.
template <typename iterable_t> struct sized_iterable_t
{
    static constexpr size_t size(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return i.base().size();
    }
};

template <typename range_t>
class owning_view<
    range_t, std::enable_if_t<is_moveable_v<range_t> && is_iterable_v<range_t>>>
{
  private:
    // this gets accessed by evil const_casts in views
    // TODO: i dont think there are any problems with the way this is used
    // I should spend some more time thinking about that, though
    range_t m_range;

  public:
    constexpr owning_view(range_t&& range)
        : OKAYLIB_NOEXCEPT m_range(std::move(range))
    {
    }

    owning_view(owning_view&&) = default;
    owning_view& operator=(owning_view&&) = default;

    template <typename derived_t, typename desired_reference_t>
    constexpr remove_cvref_t<desired_reference_t>&
    get_view_reference() & noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_v<derived_t, owning_view>);
        static_assert(is_convertible_to_v<derived_t&, owning_view&>);
        if constexpr (std::is_convertible_v<derived_t&, desired_t&>) {
            return *static_cast<derived_t*>(this);
        } else {
            static_assert(std::is_convertible_v<range_t&, desired_t&>);
            return m_range;
        }
    }
    template <typename derived_t, typename desired_reference_t>
    constexpr const remove_cvref_t<desired_reference_t>&
    get_view_reference() const& noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_v<derived_t, owning_view>);
        static_assert(
            is_convertible_to_v<const derived_t&, const owning_view&>);
        if constexpr (std::is_convertible_v<const derived_t&,
                                            const desired_t&>) {
            return *static_cast<const derived_t*>(this);
        } else {
            static_assert(
                std::is_convertible_v<const range_t&, const desired_t&>);
            return m_range;
        }
    }
};

template <typename range_t>
class ref_view<range_t, std::enable_if_t<std::is_object_v<range_t> &&
                                         is_iterable_v<range_t>>>
{
  private:
    range_t* m_range;

    // not defined
    static void is_referenceable_func(range_t&);
    static void is_referenceable_func(range_t&&) = delete;

    template <typename T, typename = void>
    class is_referenceable : public std::false_type
    {};

    /// Checks if a type will select lvalue reference overload to range_t
    template <typename T>
    class is_referenceable<
        T, std::void_t<decltype(is_referenceable_func(std::declval<T>()))>>
        : public std::true_type
    {};

  public:
    template <typename T>
    constexpr ref_view(
        T&& t,
        std::enable_if_t<!std::is_same_v<ref_view, T> &&
                             is_convertible_to_v<decltype(t), range_t&> &&
                             is_referenceable<decltype(t)>::value,
                         empty_t> = {})
        : OKAYLIB_NOEXCEPT m_range(
              ok::addressof(static_cast<range_t&>(std::forward<T>(t))))
    {
    }

    template <typename derived_t, typename desired_reference_t>
    constexpr remove_cvref_t<desired_reference_t>&
    get_view_reference() & noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_v<derived_t, ref_view>);
        static_assert(is_convertible_to_v<derived_t&, ref_view&>);
        if constexpr (std::is_convertible_v<derived_t&, desired_t&>) {
            return *static_cast<derived_t*>(this);
        } else {
            static_assert(std::is_convertible_v<range_t&, desired_t&>);
            return *m_range;
        }
    }
    template <typename derived_t, typename desired_reference_t>
    constexpr const remove_cvref_t<desired_reference_t>&
    get_view_reference() const& noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_v<derived_t, ref_view>);
        static_assert(is_convertible_to_v<const derived_t&, const ref_view&>);
        if constexpr (std::is_convertible_v<const derived_t&,
                                            const desired_t&>) {
            return *static_cast<const derived_t*>(this);
        } else {
            static_assert(
                std::is_convertible_v<const range_t&, const desired_t&>);
            return *m_range;
        }
    }
};

template <typename range_t> ref_view(range_t&) -> ref_view<range_t>;
template <typename range_t> owning_view(range_t&&) -> owning_view<range_t>;

template <typename T>
constexpr bool is_view_v =
    is_iterable_v<std::decay_t<T>> && enable_view_v<T> && is_moveable_v<T>;

template <typename T> struct underlying_view_type
{
    template <typename range_t>
    [[nodiscard]] static constexpr auto
    wrap_range_with_view(range_t&& view) OKAYLIB_NOEXCEPT
    {
        static_assert(
            is_view_v<std::decay_t<range_t>> ||
                std::is_lvalue_reference_v<decltype(view)> ||
                std::is_rvalue_reference_v<decltype(view)>,
            "Attempt to wrap something like a value type which is not a view.");
        if constexpr (is_view_v<std::decay_t<range_t>>)
            return std::forward<range_t>(view);
        else if constexpr (std::is_lvalue_reference_v<decltype(view)>)
            return ref_view{std::forward<range_t>(view)};
        else if constexpr (std::is_rvalue_reference_v<decltype(view)>)
            return owning_view{std::forward<range_t>(view)};
    }

    using type = decltype(wrap_range_with_view(std::declval<T>()));
};

} // namespace detail
} // namespace ok
#endif
