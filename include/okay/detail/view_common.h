#ifndef __OKAYLIB_DETAIL_VIEW_COMMON_H__
#define __OKAYLIB_DETAIL_VIEW_COMMON_H__

// TODO: try to replace usages of uninstantiable_t in this file with template
// deduction guides

#include "okay/detail/addressof.h"
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
    range_t m_range;

  public:
    constexpr owning_view(range_t&& range) : m_range(std::move(range)) {}

    owning_view(owning_view&&) = default;
    owning_view& operator=(owning_view&&) = default;

    constexpr range_t& base() & noexcept { return m_range; }
    constexpr const range_t& base() const& noexcept { return m_range; }
    constexpr range_t&& base() && noexcept { return std::move(m_range); }
    constexpr const range_t&& base() const&& noexcept
    {
        return std::move(m_range);
    }
    constexpr range_t& base_nonconst() const& noexcept { return m_range; }
    constexpr range_t&& base_nonconst() const&& noexcept
    {
        return std::move(m_range);
    }
};

template <typename range_t>
class ref_view<range_t, std::enable_if_t<std::is_object_v<range_t> &&
                                         is_iterable_v<range_t>>>
{
  private:
    range_t* m_range;

    // not defined
    static std::true_type can_take_ref(range_t&);
    static std::false_type can_take_ref(range_t&&);

    struct uninstantiable_t
    {
        friend class ref_view;

      private:
        uninstantiable_t() = default;
    };

  public:
    template <typename T>
    constexpr ref_view(
        T&& t, std::enable_if_t<
                   is_convertible_to_v<T, range_t>&& decltype(can_take_ref(
                       std::declval<T>()))::value,
                   uninstantiable_t> = {})
        : m_range(ok::addressof(static_cast<range_t&>(std::forward<T>(t))))
    {
    }

    constexpr const range_t& base() const { return *m_range; }
    constexpr range_t& base() { return *m_range; }
    constexpr range_t& base_nonconst() const { return *m_range; }
};

template <typename T, typename = void>
struct can_owning_view : public std::false_type
{};
template <typename T>
struct can_owning_view<T, std::void_t<owning_view<T>>> : public std::true_type
{};
template <typename T, typename = void>
struct can_ref_view : public std::false_type
{};
template <typename T>
struct can_ref_view<T, std::void_t<ref_view<T>>> : public std::true_type
{};

template <typename T>
constexpr bool is_view_v =
    is_iterable_v<std::decay_t<T>> && enable_view_v<T> && is_moveable_v<T>;

template <typename T, typename = void> struct underlying_view_type
{};
template <typename T>
struct underlying_view_type<T, std::enable_if_t<is_view_v<T>>>
{
    using type = T;
};
template <typename T>
struct underlying_view_type<
    T, std::enable_if_t<!is_view_v<T> && can_ref_view<T>::value>>
{
    using type = ref_view<T>;
};
template <typename T>
struct underlying_view_type<
    T, std::enable_if_t<!is_view_v<T> && !can_ref_view<T>::value &&
                        can_owning_view<T>::value>>
{
    using type = owning_view<T>;
};

} // namespace detail
} // namespace ok
#endif
