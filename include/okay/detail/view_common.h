#ifndef __OKAYLIB_DETAIL_VIEW_COMMON_H__
#define __OKAYLIB_DETAIL_VIEW_COMMON_H__

#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/empty.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/ranges/ranges.h"

namespace ok {
template <typename T>
concept enable_view_c = requires {
    requires range_c<T>;
    requires detail::range_definition_inner_t<T>::is_view;
};

namespace detail {

/// Conditionally inherit range_definition from this to mark as sized.
/// range_t: the range which is being defined. must inherit from
/// owning_view or ref_view, and therefore have a get_view_reference() function.
template <typename range_t, typename parent_range_t> struct sized_range_t
{
    static constexpr size_t size(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return detail::range_definition_inner_t<parent_range_t>::size(
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
    detail::range_impls_increment_c<parent_range_t>,
    std::conditional_t<
        detail::range_impls_decrement_c<parent_range_t>,
        increment_decrement_range_t<derived_range_t, parent_range_t, cursor_t>,
        increment_only_range_t<derived_range_t, parent_range_t, cursor_t>>,
    detail::empty_t>;

// derived_range_t: the range who is being implemented, and whose
// definition should be inheriting from this type. CRTP.
// parent_range_t: the range whos sizedness you want to propagate to the
// range definition of argument 1
template <typename derived_range_t, typename parent_range_t>
using propagate_sizedness_t = std::conditional_t<
    range_can_size_c<parent_range_t>,
    sized_range_t<derived_range_t, parent_range_t>,
    infinite_static_def_t<range_marked_infinite_c<parent_range_t>>>;

// requires that cursor_t can be constructed from the
// cursor_type_for<parent_range_t>
template <typename derived_range_t, typename parent_range_t, typename cursor_t,
          bool propagate_arraylike = false>
struct propagate_begin_t
{
    [[nodiscard]] constexpr static cursor_t begin(const derived_range_t& i)
        requires(!propagate_arraylike ||
                 !detail::arraylike_range_c<parent_range_t>)
    {
        return cursor_t(ok::begin(
            i.template get_view_reference<derived_range_t, parent_range_t>()));
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_offset_t
{
    [[nodiscard]] static constexpr cursor_t offset(const derived_range_t& range,
                                                   cursor_t& cursor)
        requires(detail::range_impls_offset_c<parent_range_t>)
    {
        detail::range_definition_inner_t<parent_range_t>::offset(range, cursor);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_compare_t_t
{
    [[nodiscard]] static constexpr ok::ordering
    offset(const derived_range_t& range, const cursor_t& cursor_a,
           const cursor_t& cursor_b)
        requires(detail::range_impls_compare_c<parent_range_t>)
    {
        return detail::range_definition_inner_t<parent_range_t>::compare(
            range, cursor_a, cursor_b);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t>
struct propagate_get_set_t
{
    constexpr static decltype(auto) get(const derived_range_t& range,
                                        const cursor_t& cursor)
        requires range_impls_get_c<parent_range_t>
    {
        return range_definition_inner_t<parent_range_t>::get(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor);
    }

    template <typename... constructor_args_t>
    constexpr static void set(derived_range_t& range, const cursor_t& cursor,
                              constructor_args_t&&... args)
        requires range_impls_construction_set_c<parent_range_t,
                                                constructor_args_t...>
    {
        range_definition_inner_t<parent_range_t>::set(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor, std::forward<constructor_args_t>(args)...);
    }

    constexpr static decltype(auto) get_ref(derived_range_t& range,
                                            const cursor_t& cursor)
        requires(range_can_get_ref_c<parent_range_t> &&
                 !ref_wrapper_range_c<parent_range_t>)
    {
        return range_definition_inner_t<parent_range_t>::get_ref(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor);
    }

    constexpr static decltype(auto) get_ref(const derived_range_t& range,
                                            const cursor_t& cursor)
        requires range_can_get_ref_const_c<parent_range_t>
    {
        return range_definition_inner_t<parent_range_t>::get_ref(
            range
                .template get_view_reference<derived_range_t, parent_range_t>(),
            cursor);
    }
};

template <typename derived_range_t, typename parent_range_t, typename cursor_t,
          bool propagate_arraylike = false>
struct propagate_boundscheck_t
{
    [[nodiscard]] static constexpr bool is_inbounds(const derived_range_t& i,
                                                    const cursor_t& c)
        requires((!propagate_arraylike ||
                  !detail::arraylike_range_c<parent_range_t>) &&
                 detail::range_can_is_inbounds_c<parent_range_t>)
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

template <typename range_t> class owning_view;
/// If getting undefined template errors, maybe the type passed in is not a
/// range, or it is a reference to a range?
template <typename range_t> class ref_view;

template <typename range_t>
    requires(is_moveable_c<range_t> && range_c<range_t>)
class owning_view<range_t>
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
        static_assert(is_derived_from_c<derived_t, owning_view>);
        static_assert(is_convertible_to_c<derived_t&, owning_view&>);
        if constexpr (is_convertible_to_c<derived_t&, desired_t&>) {
            return *static_cast<derived_t*>(this);
        } else {
            static_assert(is_convertible_to_c<range_t&, desired_t&>);
            return m_range;
        }
    }
    template <typename derived_t, typename desired_reference_t>
    constexpr const remove_cvref_t<desired_reference_t>&
    get_view_reference() const& noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_c<derived_t, owning_view>);
        static_assert(
            is_convertible_to_c<const derived_t&, const owning_view&>);
        if constexpr (is_convertible_to_c<const derived_t&, const desired_t&>) {
            return *static_cast<const derived_t*>(this);
        } else {
            static_assert(
                is_convertible_to_c<const range_t&, const desired_t&>);
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
    requires(!std::is_reference_v<range_t> && range_c<range_t>)
class ref_view<range_t>
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
    constexpr ref_view(T&& t) OKAYLIB_NOEXCEPT
        requires(!ok::same_as_c<ref_view, T> &&
                 is_convertible_to_c<decltype(t), range_t&> &&
                 is_referenceable<decltype(t)>::value)
        : m_range(ok::addressof(static_cast<range_t&>(std::forward<T>(t))))
    {
    }

    template <typename derived_t, typename desired_reference_t>
    constexpr auto& get_view_reference() & noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_c<derived_t, ref_view>);
        static_assert(is_convertible_to_c<derived_t&, ref_view&>);
        if constexpr (is_convertible_to_c<derived_t&, desired_t&>) {
            // since derived_t is a ref_wrapper
            return *static_cast<desired_t*>(this);
        } else {
            return *m_range;
        }
    }

    template <typename derived_t, typename desired_reference_t>
    constexpr auto& get_view_reference() const& noexcept
    {
        using desired_t = remove_cvref_t<desired_reference_t>;
        static_assert(is_derived_from_c<derived_t, ref_view>);
        static_assert(is_convertible_to_c<const derived_t&, const ref_view&>);
        if constexpr (is_convertible_to_c<const derived_t&, const desired_t&>) {
            // since derived_t is a ref_wrapper
            return *static_cast<const desired_t*>(this);
        } else {
            return *m_range;
        }
    }
};
} // namespace detail

template <typename range_t>
struct ok::range_definition<detail::ref_view<range_t>>
    : detail::propagate_all_range_traits_t<detail::ref_view<range_t>, range_t>
{
    static constexpr bool is_ref_wrapper = !detail::range_impls_get_c<range_t>;
    using value_type = detail::range_deduced_value_type_t<range_t>;
};

namespace detail {

template <typename range_t> ref_view(range_t&) -> ref_view<range_t>;
template <typename range_t> owning_view(range_t&&) -> owning_view<range_t>;

template <typename T>
constexpr bool is_view_v = range_c<T> && enable_view_c<T> && is_moveable_c<T>;

template <typename T> struct underlying_view_type
{
  private:
    template <typename range_t>
    [[nodiscard]] static constexpr auto
    wrap_range_with_view(range_t&& view) OKAYLIB_NOEXCEPT
    {
        static_assert(
            is_view_v<range_t> || std::is_lvalue_reference_v<decltype(view)> ||
                std::is_rvalue_reference_v<decltype(view)>,
            "Attempt to wrap something like a value type which is not a view.");
        if constexpr (is_view_v<range_t>)
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
        : m_value(ok::in_place)
    {
    }

    template <typename... args_t>
    constexpr uninitialized_storage_default_constructible_t(args_t&&... args)
        OKAYLIB_NOEXCEPT : m_value(ok::in_place, std::forward<args_t>(args)...)
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
        : m_value(ok::in_place, std::forward<args_t>(args)...)
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

    constexpr ~assignment_op_wrapper_t() { value().~payload_t(); }
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

    constexpr derived_t& operator++() OKAYLIB_NOEXCEPT
        requires detail::has_pre_increment_c<cursor_type_for<parent_range_t>>
    {
        ++m_inner;
        derived()->increment();
        return *derived();
    }

    constexpr derived_t& operator--() OKAYLIB_NOEXCEPT
        requires detail::has_pre_decrement_c<cursor_type_for<parent_range_t>>
    {
        --m_inner;
        derived()->decrement();
        return *derived();
    }

    constexpr derived_t& operator+=(const size_t rhs) OKAYLIB_NOEXCEPT
        requires detail::random_access_range_c<parent_range_t>
    {
        m_inner += rhs;
        derived()->plus_eql(rhs);
        return *derived();
    }

    constexpr derived_t& operator-=(const size_t rhs) OKAYLIB_NOEXCEPT
        requires detail::random_access_range_c<parent_range_t>
    {
        m_inner -= rhs;
        derived()->minus_eql(rhs);
        return *derived();
    }

    constexpr derived_t& operator+(size_t rhs) const OKAYLIB_NOEXCEPT
        requires detail::random_access_range_c<parent_range_t>
    {
        derived_t out(*derived());
        out.inner() += rhs;
        out.plus_eql(rhs);
        return out;
    }

    constexpr derived_t& operator-(size_t rhs) const OKAYLIB_NOEXCEPT
        requires detail::random_access_range_c<parent_range_t>
    {
        derived_t out(*derived());
        out.inner() -= rhs;
        out.minus_eql(rhs);
        return out;
    }

  private:
    parent_cursor_t m_inner;
};

template <typename T, typename range_t>
concept predicate_for_c =
    is_std_invocable_r_c<bool, const value_type_for<range_t>&>;

} // namespace detail
} // namespace ok
#endif
