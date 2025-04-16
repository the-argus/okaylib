#ifndef __OKAYLIB_DETAIL_VIEW_COMMON_H__
#define __OKAYLIB_DETAIL_VIEW_COMMON_H__

#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/ok_enable_if.h"
#include "okay/detail/template_util/c_array_value_type.h"
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

template <bool is_infinite> struct infinite_static_def_t
{
    static constexpr bool infinite = is_infinite;
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
using propagate_sizedness_t = std::conditional_t<
    range_can_size_v<parent_range_t>,
    sized_range_t<derived_range_t, parent_range_t>,
    infinite_static_def_t<range_marked_infinite_v<parent_range_t>>>;

// requires that cursor_t can be constructed from the
// cursor_type_for<parent_range_t>
template <typename derived_range_t, typename parent_range_t, typename cursor_t,
          bool propagate_arraylike = false>
struct propagate_begin_t
{
    __ok_enable_if_static(derived_range_t,
                          (!propagate_arraylike ||
                           !detail::range_is_arraylike_v<parent_range_t>),
                          cursor_t) begin(const T& i)
    {
        return cursor_t(ok::begin(
            i.template get_view_reference<derived_range_t, parent_range_t>()));
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_offset_t
{
    __ok_enable_if_static(cursor_t,
                          detail::range_definition_has_offset_v<parent_range_t>,
                          T)
        offset(const derived_range_t& range, cursor_t& cursor)
    {
        detail::range_definition_inner<parent_range_t>::offset(range, cursor);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_compare_t_t
{
    __ok_enable_if_static(
        cursor_t, detail::range_definition_has_compare_v<parent_range_t>, T)
        offset(const derived_range_t& range, const cursor_t& cursor_a,
               const cursor_t& cursor_b)
    {
        return detail::range_definition_inner<parent_range_t>::compare(
            range, cursor_a, cursor_b);
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
            std::is_same_v<T, parent_range_t> &&
                !std::is_const_v<std::remove_reference_t<T>>,
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
            std::is_same_v<T, parent_range_t> &&
                !std::is_const_v<std::remove_reference_t<T>>,
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
            std::is_same_v<T, parent_range_t> && range_has_get_ref_const_v<T>,
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

template <typename derived_range_t, typename parent_range_t, typename cursor_t,
          bool propagate_arraylike = false>
struct propagate_boundscheck_t
{
    __ok_enable_if_static(parent_range_t,
                          (!propagate_arraylike ||
                           !detail::range_is_arraylike_v<T>) &&
                              detail::range_can_is_inbounds_v<T>,
                          bool)
        is_inbounds(const derived_range_t& i, const cursor_t& c)
    {
        return ok::is_inbounds(
            i.template get_view_reference<derived_range_t, parent_range_t>(),
            cursor_type_for<parent_range_t>(c));
    }
};

template <typename derived_range_t, typename parent_range_t>
struct propagate_all_range_traits_t
    : public detail::propagate_sizedness_t<derived_range_t, parent_range_t>,
      public detail::propagate_begin_t<derived_range_t, parent_range_t,
                                       cursor_type_for<parent_range_t>>,
      public detail::propagate_get_set_t<derived_range_t, parent_range_t,
                                         cursor_type_for<parent_range_t>>,
      public detail::propagate_boundscheck_t<derived_range_t, parent_range_t,
                                             cursor_type_for<parent_range_t>>,
      public detail::propagate_increment_decrement_t<
          derived_range_t, parent_range_t, cursor_type_for<parent_range_t>>,
      public detail::propagate_offset_t<derived_range_t, parent_range_t,
                                        cursor_type_for<parent_range_t>>,
      public detail::propagate_compare_t_t<derived_range_t, parent_range_t,
                                           cursor_type_for<parent_range_t>>
{};

template <typename range_t, typename = void> class owning_view;
template <typename range_t, typename = void> class ref_view;

template <typename range_t>
class owning_view<
    range_t, std::enable_if_t<is_moveable_v<range_t> && is_range_v<range_t>>>
{
  private:
    range_t m_range;

  public:
    constexpr owning_view(range_t&& range) OKAYLIB_NOEXCEPT
        : m_range(std::move(range))
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

} // namespace detail

template <typename range_t>
struct ok::range_definition<detail::owning_view<range_t>>
    : detail::propagate_all_range_traits_t<detail::owning_view<range_t>,
                                           range_t>
{};

namespace detail {

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
                         empty_t> = {}) OKAYLIB_NOEXCEPT
        : m_range(ok::addressof(static_cast<range_t&>(std::forward<T>(t))))
    {
    }

    template <typename derived_t, typename desired_reference_t>
    constexpr auto get_view_reference() & noexcept
        // disable nonconst get_view_reference in the special case that we are a
        // ref wrapper around a const reference to a c array
        -> std::enable_if_t<
            !std::is_const_v<c_array_value_type_or_void<range_t>>,
            ok::detail::remove_cvref_t<desired_reference_t>&>
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
} // namespace detail

template <typename range_t>
struct ok::range_definition<detail::ref_view<range_t>>
    : detail::propagate_all_range_traits_t<detail::ref_view<range_t>, range_t>
{};

namespace detail {

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

    inline ~assignment_op_wrapper_t() { value().~payload_t(); }
};

// extend an arbitrary cursor type with additional members or functionality,
// regardless of whether the cursor is random access / bidirectional etc
// derived_t: the child of this class who implements increment(), decrement(),
// compare(), less_than(), less_than_eql(), greater_than(), greater_than_eql(),
// plus_eql(size_t), and minus_eql(size_t)
// parent_range_t: the range type whose cursor we need to extend
template <typename derived_t, typename parent_range_t> struct cursor_wrapper_t
{
  private:
    using parent_cursor_t = cursor_type_for<parent_range_t>;

    derived_t* derived() noexcept { return static_cast<derived_t*>(this); }

    const derived_t* derived() const noexcept
    {
        return static_cast<const derived_t*>(this);
    }

  public:
    explicit constexpr cursor_wrapper_t(parent_cursor_t&& c) OKAYLIB_NOEXCEPT
        : m_inner(std::move(c))
    {
    }

    cursor_wrapper_t(const cursor_wrapper_t&) = default;
    cursor_wrapper_t& operator=(const cursor_wrapper_t&) = default;
    cursor_wrapper_t(cursor_wrapper_t&&) = default;
    cursor_wrapper_t& operator=(cursor_wrapper_t&&) = default;

    constexpr const parent_cursor_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m_inner;
    }
    constexpr parent_cursor_t& inner() OKAYLIB_NOEXCEPT { return m_inner; }
    constexpr operator parent_cursor_t() const OKAYLIB_NOEXCEPT
    {
        return inner();
    }

    __ok_enable_if(parent_range_t,
                   detail::has_pre_increment_v<cursor_type_for<T>>,
                   derived_t&) operator++() OKAYLIB_NOEXCEPT
    {
        ++m_inner;
        derived()->increment();
        return *derived();
    }

    __ok_enable_if(parent_range_t,
                   detail::has_pre_decrement_v<cursor_type_for<T>>,
                   derived_t&) operator--() OKAYLIB_NOEXCEPT
    {
        --m_inner;
        derived()->decrement();
        return *derived();
    }

#define __okaylib_cursor_wrapper_enable_member_if_random_access(rettype) \
    template <typename T = parent_range_t>                               \
    constexpr std::enable_if_t<std::is_same_v<T, parent_range_t> &&      \
                                   detail::is_random_access_range_v<T>,  \
                               rettype>

    __okaylib_cursor_wrapper_enable_member_if_random_access(derived_t&)
    operator+=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_inner += rhs;
        derived()->plus_eql(rhs);
        return *derived();
    }

    __okaylib_cursor_wrapper_enable_member_if_random_access(derived_t&)
    operator-=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_inner -= rhs;
        derived()->minus_eql(rhs);
        return *derived();
    }

    __okaylib_cursor_wrapper_enable_member_if_random_access(derived_t)
    operator+(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        derived_t out(*derived());
        out.inner() += rhs;
        out.plus_eql(rhs);
        return out;
    }

    __okaylib_cursor_wrapper_enable_member_if_random_access(derived_t)
    operator-(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        derived_t out(*derived());
        out.inner() -= rhs;
        out.minus_eql(rhs);
        return out;
    }

#undef __okaylib_cursor_wrapper_enable_member_if_random_access

  private:
    parent_cursor_t m_inner;
};

} // namespace detail
} // namespace ok
#endif
