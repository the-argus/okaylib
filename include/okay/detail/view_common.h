#ifndef __OKAYLIB_DETAIL_VIEW_COMMON_H__
#define __OKAYLIB_DETAIL_VIEW_COMMON_H__

#include "okay/detail/addressof.h"
#include "okay/detail/template_util/empty.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/ranges/ranges.h"

namespace ok {
template <typename T, typename = void>
struct enable_view : public std::false_type
{};

// specialize enable_view or add is_view to range_definition
template <typename T>
struct enable_view<T,
                   std::enable_if_t<detail::range_definition_inner<T>::is_view>>
    : public std::true_type
{};

template <typename T> constexpr bool enable_view_v = enable_view<T>::value;

namespace detail {

template <typename range_t, typename = void> class owning_view;
template <typename range_t, typename = void> class ref_view;

template <typename range_t>
class owning_view<
    range_t, std::enable_if_t<is_moveable_v<range_t> && is_range_v<range_t>>>
{
  private:
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
class ref_view<
    range_t, std::enable_if_t<std::is_object_v<range_t> && is_range_v<range_t>>>
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
    is_range_v<std::decay_t<T>> && enable_view_v<T> && is_moveable_v<T>;

template <typename T> struct underlying_view_type
{
  private:
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

  public:
    using type = decltype(wrap_range_with_view(std::declval<T>()));
};

struct infinite_range_t
{
    static constexpr bool infinite = true;
};

struct finite_range_t
{
    static constexpr bool infinite = false;
};

/// Conditionally inherit range_definition from this to mark as sized.
/// range_t: the range which is being defined. must inherit from
/// owning_view or ref_view, and therefore have a get_view_reference() function.
template <typename range_t, typename parent_range_t> struct sized_range_t
{
    static constexpr size_t size(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return detail::range_definition_inner<parent_range_t>::size(
            i.template get_view_reference<range_t, parent_range_t>());
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct increment_only_range_t
{
    constexpr static void increment(const derived_range_t& i, cursor_t& c)
    {
        static_assert(std::is_same_v<cursor_t, cursor_type_for<parent_range_t>>,
                      "Cannot increment cursor if it requires a conversion of "
                      "any kind, even upcasting.");
        ok::increment(
            i.template get_view_reference<derived_range_t, parent_range_t>(),
            c);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct increment_decrement_range_t
    : public increment_only_range_t<derived_range_t, parent_range_t, cursor_t>
{
    constexpr static void decrement(const derived_range_t& i, cursor_t& c)
    {
        static_assert(std::is_same_v<cursor_t, cursor_type_for<parent_range_t>>,
                      "Cannot decrement cursor if it requires a conversion of "
                      "any kind, even upcasting.");
        ok::decrement(
            i.template get_view_reference<derived_range_t, parent_range_t>(),
            c);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
using propagate_increment_decrement_t = std::conditional_t<
    detail::range_definition_has_increment_v<parent_range_t>,
    std::conditional_t<
        detail::range_definition_has_decrement_v<parent_range_t>,
        increment_decrement_range_t<derived_range_t, parent_range_t, cursor_t>,
        increment_only_range_t<derived_range_t, parent_range_t, cursor_t>>,
    detail::empty_t>;

// derived_range_t: the range who is being implemented, and whose
// definition should be inheriting from this type. CRTP.
// parent_range_t: the range whos sizedness you want to propagate to the
// range definition of argument 1
template <typename derived_range_t, typename parent_range_t>
using propagate_sizedness_t =
    std::conditional_t<range_has_size_v<parent_range_t>,
                       sized_range_t<derived_range_t, parent_range_t>,
                       std::conditional<range_marked_infinite_v<parent_range_t>,
                                        infinite_range_t, finite_range_t>>;

// requires that cursor_t can be constructed from the
// cursor_type_for<parent_range_t>
template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_begin_t
{
    constexpr static cursor_t begin(const derived_range_t& i)
    {
        return cursor_t(ok::begin(
            i.template get_view_reference<derived_range_t, parent_range_t>()));
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_get_set_t
{
    template <typename T = parent_range_t>
    constexpr static auto get(const derived_range_t& range,
                              const cursor_t& cursor)
        -> std::enable_if_t<
            std::is_same_v<T, parent_range_t>,
            decltype(range_definition_inner<T>::get(
                std::declval<const T&>(),
                std::declval<const cursor_type_unchecked_for<T>>()))>
    {
        return range_definition_inner<T>::get(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor);
    }

    template <typename T = parent_range_t>
    constexpr static auto set(derived_range_t& range, const cursor_t& cursor,
                              value_type_for<T>&& value)
        -> std::enable_if_t<
            std::is_same_v<T, parent_range_t>,
            decltype(range_definition_inner<T>::set(
                std::declval<const T&>(),
                std::declval<const cursor_type_unchecked_for<T>>(),
                std::forward<value_type_for<T>>(value)))>
    {
        return range_definition_inner<T>::get(
            range.template get_view_reference<derived_range_t, T>(), cursor,
            std::forward<value_type_for<T>>(value));
    }

    template <typename T = parent_range_t>
    constexpr static auto get_ref(derived_range_t& range,
                                  const cursor_t& cursor)
        -> std::enable_if_t<
            std::is_same_v<T, parent_range_t>,
            decltype(range_definition_inner<T>::get_ref(
                std::declval<T&>(),
                std::declval<const cursor_type_unchecked_for<T>>()))>
    {
        return range_definition_inner<T>::get_ref(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor);
    }

    template <typename T = parent_range_t>
    constexpr static auto get_ref(const derived_range_t& range,
                                  const cursor_t& cursor)
        -> std::enable_if_t<
            std::is_same_v<T, parent_range_t>,
            decltype(range_definition_inner<T>::get_ref(
                std::declval<const T&>(),
                std::declval<const cursor_type_unchecked_for<T>>()))>
    {
        return range_definition_inner<T>::get_ref(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_boundscheck_t
{
    template <typename T = parent_range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<parent_range_t, T> &&
            detail::range_has_is_inbounds_v<parent_range_t>,
        bool>
    is_inbounds(const derived_range_t& i, const cursor_t& c)
    {
        return detail::range_definition_inner<T>::is_inbounds(
            i.template get_view_reference<derived_range_t, parent_range_t>(),
            cursor_type_for<parent_range_t>(c));
    }

    template <typename T = parent_range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<parent_range_t, T> &&
            detail::range_has_is_after_bounds_v<parent_range_t>,
        bool>
    is_after_bounds(const derived_range_t& i, const cursor_t& c)
    {
        return detail::range_definition_inner<T>::is_after_bounds(
            i.template get_view_reference<derived_range_t, parent_range_t>(),
            cursor_type_for<parent_range_t>(c));
    }

    template <typename T = parent_range_t>
    constexpr static std::enable_if_t<
        std::is_same_v<parent_range_t, T> &&
            detail::range_has_is_before_bounds_v<parent_range_t>,
        bool>
    is_before_bounds(const derived_range_t& i, const cursor_t& c)
    {
        return detail::range_definition_inner<T>::is_before_bounds(
            i.template get_view_reference<derived_range_t, parent_range_t>(),
            cursor_type_for<parent_range_t>(c));
    }
};

template <typename payload_t>
struct uninitialized_storage_default_constructible_t
{
    constexpr uninitialized_storage_default_constructible_t() OKAYLIB_NOEXCEPT
        : m_value(std::in_place)
    {
    }

    template <typename... args_t>
    constexpr uninitialized_storage_default_constructible_t(args_t&&... args)
        OKAYLIB_NOEXCEPT : m_value(std::in_place, std::forward<args_t>(args)...)
    {
    }

    uninitialized_storage_t<payload_t> m_value;
};
template <typename payload_t>
struct uninitialized_storage_deleted_default_constructor_t
{
    uninitialized_storage_deleted_default_constructor_t() = delete;

    template <typename... args_t>
    constexpr uninitialized_storage_deleted_default_constructor_t(
        args_t&&... args) OKAYLIB_NOEXCEPT
        : m_value(std::in_place, std::forward<args_t>(args)...)
    {
    }

    uninitialized_storage_t<payload_t> m_value;
};

template <typename payload_t>
using propagate_default_constructibility_t = std::conditional_t<
    std::is_default_constructible_v<payload_t>,
    uninitialized_storage_default_constructible_t<payload_t>,
    uninitialized_storage_deleted_default_constructor_t<payload_t>>;

/// type which wraps another type which does not have copy/move assignment and
/// gives it copy/move by destroying the reciever and then constructing over it
template <typename payload_t>
struct assignment_op_wrapper_t
    : public propagate_default_constructibility_t<payload_t>
{
  private:
    using parent_t = propagate_default_constructibility_t<payload_t>;
    using self_t = assignment_op_wrapper_t;
    constexpr parent_t* derived() OKAYLIB_NOEXCEPT
    {
        return static_cast<parent_t*>(this);
    }
    constexpr const parent_t* derived() const OKAYLIB_NOEXCEPT
    {
        return static_cast<const parent_t*>(this);
    }

  public:
    using parent_t::parent_t;

    constexpr payload_t& value() & OKAYLIB_NOEXCEPT
    {
        return derived()->m_value.value;
    }
    constexpr const payload_t& value() const& OKAYLIB_NOEXCEPT
    {
        return derived()->m_value.value;
    }
    constexpr payload_t&& value() && OKAYLIB_NOEXCEPT
    {
        return std::move(derived()->m_value.value);
    }
    constexpr payload_t& value() const&& = delete;

    constexpr assignment_op_wrapper_t(const assignment_op_wrapper_t& other) =
        default;
    constexpr assignment_op_wrapper_t(assignment_op_wrapper_t&& other) =
        default;

    constexpr self_t& operator=(const self_t& other) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_copy_constructible_v<payload_t>);
        if (this != ok::addressof(other)) {
            auto* ptr = ok::addressof(value());
            ptr->~payload_t();
            ::new (ptr) payload_t(other.value());
        }
        return *this;
    }

    constexpr self_t& operator=(self_t&& other) OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_nothrow_move_constructible_v<payload_t>);
        if (this != ok::addressof(other)) {
            auto* ptr = ok::addressof(value());
            ptr->~payload_t();
            ::new (ptr) payload_t(std::move(other).value());
        }
        return *this;
    }
};

} // namespace detail
} // namespace ok
#endif
