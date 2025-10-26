#ifndef __OKAYLIB_RANGES_RANGES_H__
#define __OKAYLIB_RANGES_RANGES_H__

#include "okay/construct.h"
#include "okay/detail/concepts.h"
#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/is_std_container.h"
#include "okay/detail/traits/mathop_traits.h"
#include "okay/math/ordering.h"
#include "okay/ranges/range_definition.h"

/*
 * This header defines customization points and traits for ranges- their
 * value and cursor types, and operations on the range.
 *
 * `range_definition<T>`
 * `value_type_for<T>`
 * `cursor_type_for<T>`
 * `range_def_for<T>` (checked access to range_definition<T>)
 * `begin()`
 * `is_inbounds()`
 */

namespace ok {
namespace detail {

template <typename T> struct range_definition_inner_meta_t
{
    static_assert(std::is_same_v<T, remove_cvref_t<T>>);
    using inherited_id = typename inherited_range_type<T>::type;
    using type =
        std::conditional_t<has_inherited_range_type_v<T>,
                           range_definition<inherited_id>, range_definition<T>>;
    static_assert(!has_inherited_range_type_v<T> ||
                      is_derived_from_c<T, inherited_id>,
                  "inherited_range_type is not a base class of the range "
                  "for which it is specified.");
};

template <typename T>
using range_definition_inner_t =
    typename range_definition_inner_meta_t<remove_cvref_t<T>>::type;

template <typename T> constexpr range_strict_flags get_strict_flags_for_range()
{
    if constexpr (requires {
                      {
                          range_definition_inner_t<T>::strict_flags
                      } -> same_as_c<range_strict_flags>;
                  }) {
        return range_definition_inner_t<T>::strict_flags;
    } else {
        return {};
    }
}

template <typename T> constexpr range_flags get_flags_for_range()
{
    return range_definition_inner_t<T>::flags;
}

/// traits to check if something has a give strict flags, with ranges that don't
/// define strict flags defaulting to all flags off
template <typename T>
concept range_strictly_disallows_get_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_get;
};
template <typename T>
concept range_strictly_disallows_set_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_set;
};
template <typename T>
concept range_strictly_disallows_begin_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_begin;
};
template <typename T>
concept range_strictly_disallows_size_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_size;
};
template <typename T>
concept range_strictly_disallows_is_inbounds_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_is_inbounds;
};
template <typename T>
concept range_strictly_disallows_cursor_member_increment_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_cursor_member_increment;
};
template <typename T>
concept range_strictly_disallows_range_def_increment_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_range_def_increment;
};
template <typename T>
concept range_strictly_disallows_cursor_member_decrement_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_cursor_member_decrement;
};
template <typename T>
concept range_strictly_disallows_range_def_decrement_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_range_def_decrement;
};
template <typename T>
concept range_strictly_disallows_cursor_member_offset_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_cursor_member_offset;
};
template <typename T>
concept range_strictly_disallows_range_def_offset_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_range_def_offset;
};
template <typename T>
concept range_strictly_disallows_cursor_member_compare_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_cursor_member_compare;
};
template <typename T>
concept range_strictly_disallows_range_def_compare_c = requires {
    requires range_definition_inner_t<T>::strict_flags&
        range_strict_flags::disallow_range_def_compare;
};

template <typename T>
concept range_marked_arraylike_c = requires {
    requires range_definition_inner_t<T>::flags& range_flags::arraylike;
};

template <typename T>
concept range_marked_consuming_c = requires {
    requires range_definition_inner_t<T>::flags& range_flags::consuming;
};

template <typename T>
concept range_marked_as_implementing_set = requires {
    requires range_definition_inner_t<T>::flags& range_flags::implements_set;
    requires !range_strictly_disallows_set_c<T>;
};

template <typename T>
concept range_marked_producing_c = requires {
    requires range_definition_inner_t<T>::flags& range_flags::producing;
};

template <typename T>
concept has_value_type_c =
    requires { requires !is_void_c<typename T::value_type>; };

template <typename T>
concept range_marked_ref_wrapper_c = requires {
    requires range_definition_inner_t<T>::flags& range_flags::ref_wrapper;
};

template <typename T>
concept range_marked_infinite_c = requires() {
    requires range_definition_inner_t<T>::flags& range_flags::infinite;
};

template <typename T>
concept range_marked_finite_c = requires() {
    requires range_definition_inner_t<T>::flags& range_flags::finite;
};

template <typename T>
concept range_marked_sized_c = requires() {
    requires range_definition_inner_t<T>::flags& range_flags::sized;
};

template <typename T>
concept valid_range_cursor_c = std::is_object_v<T>;

template <typename T>
concept valid_range_value_type_c = std::is_object_v<T>;

template <typename T, typename = void> struct range_begin_rettype_or_void
{
    using type = void;
};
template <typename T>
struct range_begin_rettype_or_void<
    T, std::void_t<decltype(range_definition_inner_t<T>::begin(
           std::declval<const remove_cvref_t<T>&>()))>>
{
    using type = decltype(range_definition_inner_t<T>::begin(
        std::declval<const remove_cvref_t<T>&>()));
};

template <typename T>
using cursor_type_unchecked_for_t =
    std::conditional_t<range_marked_arraylike_c<T>, size_t,
                       typename range_begin_rettype_or_void<T>::type>;

template <typename T>
concept range_impls_begin_c = requires {
    requires !is_void_c<typename range_begin_rettype_or_void<T>::type>;
    requires !range_strictly_disallows_begin_c<T>;
};

template <typename T>
concept range_impls_increment_c = requires(
    const remove_cvref_t<T>& range, cursor_type_unchecked_for_t<T>& cursor) {
    { range_definition_inner_t<T>::increment(range, cursor) } -> is_void_c;
    requires !range_strictly_disallows_range_def_increment_c<T>;
};

template <typename T>
concept range_impls_decrement_c = requires(
    const remove_cvref_t<T>& range, cursor_type_unchecked_for_t<T>& cursor) {
    { range_definition_inner_t<T>::decrement(range, cursor) } -> is_void_c;
    requires !range_strictly_disallows_range_def_decrement_c<T>;
};

template <typename T>
concept range_impls_offset_c =
    requires(const remove_cvref_t<T>& range,
             cursor_type_unchecked_for_t<T>& cursor, int64_t offset) {
        {
            range_definition_inner_t<T>::offset(range, cursor, offset)
        } -> is_void_c;
        requires !range_strictly_disallows_range_def_offset_c<T>;
    };

template <typename T>
concept range_impls_compare_c =
    requires(const remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& a,
             const cursor_type_unchecked_for_t<T>& b) {
        {
            range_definition_inner_t<T>::compare(range, a, b)
        } -> same_as_c<ok::ordering>;
        requires !range_strictly_disallows_range_def_compare_c<T>;
    };

template <typename T>
concept range_impls_is_inbounds_c =
    requires(const remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& cursor) {
        {
            range_definition_inner_t<T>::is_inbounds(range, cursor)
        } -> same_as_c<bool>;
        requires !range_strictly_disallows_is_inbounds_c<T>;
    };

template <typename T>
concept range_impls_const_get_c = requires(
    const remove_cvref_t<T>& range, const cursor_type_unchecked_for_t<T>& c) {
    requires range_marked_producing_c<T>; // must be enabled
    requires !range_strictly_disallows_get_c<T>;
    {
        range_definition_inner_t<T>::get(range, c)
    } -> same_as_without_cvref_c<
          typename range_definition_inner_t<T>::value_type>;
};

/// Since nonconst to const demotion can occur, this concept really is only
/// checking if *any* form of get is implemented since it attempts to pass
/// a nonconst
template <typename T>
concept range_impls_const_or_nonconst_get_c = requires(
    remove_cvref_t<T>& range, const cursor_type_unchecked_for_t<T>& c) {
    requires range_marked_producing_c<T>; // must be enabled
    requires !range_strictly_disallows_get_c<T>;
    {
        range_definition_inner_t<T>::get(range, c)
    } -> same_as_without_cvref_c<
          typename range_definition_inner_t<remove_cvref_t<T>>::value_type>;
};

/// Checks whether both const and nonconst get can be called AND (they both
/// return a reference OR they both return a value)
template <typename T>
concept range_impls_const_or_nonconst_get_and_returntype_refness_matches_c =
    requires(remove_cvref_t<T>& range, const remove_cvref_t<T>& const_range,
             const cursor_type_unchecked_for_t<T>& c) {
        requires range_impls_const_or_nonconst_get_c<T>;
        requires stdc::is_reference_c<decltype(range_definition_inner_t<T>::get(
                     range, c))> ==
                     stdc::is_reference_c<decltype(range_definition_inner_t<
                                                   T>::get(const_range, c))>;
    };

// "get" can return any of `const value_type&`, `value_type&`, or `value_type`.
template <typename T>
concept range_impls_get_c = requires {
    requires range_marked_producing_c<T>; // must be enabled
    requires range_impls_const_or_nonconst_get_c<T>;
    requires !range_strictly_disallows_get_c<T>;

    // get must return a reference for both the const and nonconst overloads,
    // or return a value for both. no mixture of reference / value. it is also
    // okay if only one of the overloads is implemented
    requires range_impls_const_or_nonconst_get_and_returntype_refness_matches_c<
                 T> ||
                 !range_impls_const_get_c<T>;
};

template <typename T, typename... args_t>
concept range_impls_construction_set_c = requires {
    // must be enabled
    requires range_marked_consuming_c<T>;
    requires !range_strictly_disallows_set_c<T>;
    requires range_marked_as_implementing_set<T>;
    // either the value type in infallible constructible with the given
    // arguments and set() returns void
    requires requires(remove_cvref_t<T>& r,
                      const cursor_type_unchecked_for_t<T>& c, args_t... args) {
        requires is_infallible_constructible_c<
            typename range_definition_inner_t<T>::value_type, args_t...>;
        { range_definition_inner_t<T>::set(r, c, args...) } -> is_void_c;
    } ||
                 // OR the type is fallible constructible with these args, and
                 // set returns a status of some kind (and is hopefully marked
                 // [[nodiscard]])
                 requires(remove_cvref_t<T>& r,
                          const cursor_type_unchecked_for_t<T>& c,
                          args_t... args) {
                     requires is_fallible_constructible_c<
                         typename range_definition_inner_t<T>::value_type,
                         args_t...>;
                     {
                         range_definition_inner_t<T>::set(r, c, args...)
                     } -> status_type;
                 };
};

template <typename T>
concept range_impls_size_c = requires(const remove_cvref_t<T>& range) {
    requires range_marked_sized_c<T>;
    requires !range_strictly_disallows_size_c<T>;
    { range_definition_inner_t<T>::size(range) } -> same_as_c<size_t>;
};

template <typename T>
concept range_can_begin_c = requires {
    requires range_impls_begin_c<T> || range_marked_arraylike_c<T>;
};

template <typename T>
concept range_can_is_inbounds_c = requires {
    requires range_impls_is_inbounds_c<T> || range_marked_arraylike_c<T>;
};

template <typename T>
concept range_gets_const_from_nonconst_c = requires(
    remove_cvref_t<T>& range, const cursor_type_unchecked_for_t<T>& c) {
    {
        range_definition_inner_t<T>::get(range, c)
    } -> same_as_c<const typename range_definition_inner_t<
          remove_cvref_t<T>>::value_type&>;
};

template <typename T>
concept range_gets_by_ref_matching_constness_c =
    requires(remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& c) {
        {
            range_definition_inner_t<T>::get(range, c)
        }
        -> same_as_c<
            typename range_definition_inner_t<remove_cvref_t<T>>::value_type&>;
    } &&
    requires(const remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& c) {
        {
            range_definition_inner_t<T>::get(range, c)
        } -> same_as_c<const typename range_definition_inner_t<
              remove_cvref_t<T>>::value_type&>;
    };

template <typename T>
concept range_gets_by_value_c =
    requires(const remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& c) {
        {
            range_definition_inner_t<T>::get(range, c)
        } -> same_as_c<
              typename range_definition_inner_t<remove_cvref_t<T>>::value_type>;
    } &&
    (requires(const remove_cvref_t<T>& range,
              const cursor_type_unchecked_for_t<T>& c) {
        {
            range_definition_inner_t<T>::get(range, c)
        } -> same_as_c<
              typename range_definition_inner_t<remove_cvref_t<T>>::value_type>;
    });

// alias in case indirection is added later and (having size def) != (is sized)
template <typename T>
concept range_can_size_c = range_impls_size_c<T>;

template <typename T>
concept range_marked_deleted_c =
    requires { requires range_definition_inner_t<T>::deleted; };

template <typename T>
concept range_has_definition_c = !range_marked_deleted_c<T>;

/// The parts of the range which describe what functions should be in the range
/// are present, and all the functions that are universal (is_inbounds, begin)
/// are present, and size() is present if and only if the range is marked sized.
template <typename T>
concept well_declared_range_c = requires {
    requires range_has_definition_c<T>;
    requires has_value_type_c<range_definition_inner_t<T>>;
    requires range_can_begin_c<T>;
    requires range_can_is_inbounds_c<T>;
    { range_definition_inner_t<T>::flags } -> same_as_c<const range_flags&>;
    requires !(range_marked_as_implementing_set<T> &&
               !range_marked_consuming_c<T>);
    requires range_marked_producing_c<T> || range_marked_consuming_c<T>;
    requires(int(range_marked_infinite_c<T>) + int(range_marked_finite_c<T>) +
                 int(range_marked_sized_c<T>) ==
             1);
    requires range_impls_size_c<T> == range_marked_sized_c<T>;
    requires range_impls_size_c<T> !=
                 (range_marked_finite_c<T> || range_marked_infinite_c<T>);
    requires range_strict_flags_validate(get_flags_for_range<T>(),
                                         get_strict_flags_for_range<T>());
};
} // namespace detail

template <detail::well_declared_range_c T>
using value_type_for = typename detail::range_definition_inner_t<T>::value_type;

namespace detail {
template <typename T> struct cursor_or_void
{
    using type = void;
};

template <typename T>
    requires requires { typename cursor_type_unchecked_for_t<T>; }
struct cursor_or_void<T>
{
    using type = cursor_type_unchecked_for_t<T>;
};

template <typename T> using cursor_or_void_t = typename cursor_or_void<T>::type;

template <typename T>
concept range_can_offset_c =
    range_impls_offset_c<T> ||
    (has_inplace_addition_with_i64_c<cursor_or_void_t<T>> &&
     !range_strictly_disallows_cursor_member_offset_c<T>);

template <typename T>
concept range_can_compare_c =
    range_impls_compare_c<T> ||
    (orderable_c<cursor_or_void_t<T>> &&
     !range_strictly_disallows_cursor_member_compare_c<T>);

// NOTE: offsetting and incrementing cannot be mutually exclusive, otherwise
// things without strict flags that use size_t or other numbers as a cursor
// wouldn't work since those cursors automatically can both increment and offset
template <typename T>
concept range_can_increment_c =
    range_can_offset_c<T> ||
    (range_impls_increment_c<T> !=
     (has_pre_increment_c<cursor_or_void_t<T>> &&
      !range_strictly_disallows_cursor_member_increment_c<T>));

template <typename T>
concept range_can_decrement_c =
    range_can_offset_c<T> ||
    (range_impls_decrement_c<T> !=
     (has_pre_decrement_c<cursor_or_void_t<T>> &&
      !range_strictly_disallows_cursor_member_decrement_c<T>));

template <typename T>
concept common_range_requirements_c = requires {
    requires is_complete_c<remove_cvref_t<T>>;
    requires well_declared_range_c<T>;
    requires detail::valid_range_value_type_c<value_type_for<T>>;
    requires detail::valid_range_cursor_c<
        detail::cursor_type_unchecked_for_t<T>>;
};

template <typename T>
concept consuming_range_c = requires {
    requires common_range_requirements_c<T>;
    requires range_can_increment_c<T>;
    requires range_definition_inner_t<T>::flags& range_flags::consuming;
};

template <typename T>
concept producing_range_c = requires {
    requires common_range_requirements_c<T>;
    requires range_can_increment_c<T>;
    requires get_flags_for_range<T>() & range_flags::producing;
    requires range_impls_get_c<T>;
};
} // namespace detail

template <typename T>
concept range_c = requires {
    // fail earlier by re-including this requirement which is also present in
    // the other requirements
    requires detail::common_range_requirements_c<T>;
    requires detail::producing_range_c<T> || detail::consuming_range_c<T>;
};

// a multi pass range has a copyable cursor- enabling consumers to pause and
// come back to cursors at previous positions.
template <typename T>
concept multi_pass_range_c = requires {
    // fail earlier by re-including this requirement which is also present in
    // the single_pass_range_c requirement
    requires detail::common_range_requirements_c<T>;
    requires range_c<T>;
    requires detail::is_copy_constructible_c<detail::cursor_or_void_t<T>>;
    requires detail::is_copy_assignable_c<detail::cursor_or_void_t<T>>;
};

template <typename T>
concept bidirectional_range_c = requires {
    // fail earlier by re-including this requirement which is also present in
    // the multi_pass_range_c requirement
    requires detail::common_range_requirements_c<T>;
    requires multi_pass_range_c<T>;
    // can also be decremented
    requires detail::range_can_decrement_c<T>;
};

template <typename T>
concept random_access_range_c = requires {
    // fail earlier by re-including this requirement which is also present in
    // the bidirectional_range_c requirement
    requires detail::common_range_requirements_c<T>;
    requires bidirectional_range_c<T>;
    // must be able to offset cursor by arbitrary i64
    requires detail::range_can_offset_c<T>;
    // must be able to compare two cursors
    requires detail::range_can_compare_c<T>;
};

template <range_c T> using range_def_for = detail::range_definition_inner_t<T>;

template <range_c T>
using cursor_type_for =
    std::conditional_t<ok::detail::range_marked_arraylike_c<T>, size_t,
                       typename detail::range_begin_rettype_or_void<T>::type>;

namespace detail {

// forward declare these so we can make overloads that detect them and have
// static asserts with nice error messages
template <typename callable_t> struct range_adaptor_closure_t;
template <typename callable_t> struct range_adaptor_t;
template <typename callable_t, typename... args_t> struct partial_called_t;

#define __ok_range_function_assert_not_partially_called_member(funcname)      \
    template <typename T, typename... U>                                      \
        requires(!range_c<T> && (is_instance_c<T, range_adaptor_closure_t> || \
                                 is_instance_c<T, range_adaptor_t> ||         \
                                 is_instance_c<T, partial_called_t>))         \
    constexpr auto operator()(const T&, U&&...) const OKAYLIB_NOEXCEPT        \
    {                                                                         \
        static_assert(false,                                                  \
                      "Attempt to call " #funcname ", but the object "        \
                      "given as range hasn't finished being called. It "      \
                      "may be a view which takes additional arguments.");     \
    }

#define __ok_range_function_assert_correct_cursor_type_member                 \
    template <range_c T, typename U, typename... pack>                        \
                                                                              \
        requires(!is_convertible_to_c<const U&,                               \
                                      const cursor_type_unchecked_for_t<T>&>) \
    constexpr auto operator()(const T&, const U&, pack&&...)                  \
        const OKAYLIB_NOEXCEPT                                                \
    {                                                                         \
        static_assert(false,                                                  \
                      "Incorrect cursor type passed as second argument.");    \
    }

struct range_get_fn_t
{
    template <range_c range_t>
        requires range_impls_const_get_c<range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        return range_def_for<range_t>::get(range, cursor);
    }
};

struct range_get_best_fn_t
{
    template <range_c range_t>
    constexpr decltype(auto) operator()
        [[nodiscard]] (range_t& range, const cursor_type_for<range_t>& cursor)
        const OKAYLIB_NOEXCEPT
        requires range_impls_get_c<range_t>
    {
        return range_def_for<range_t>::get(range, cursor);
    }

    template <range_c range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
        requires range_impls_get_c<range_t>
    {
        return range_def_for<range_t>::get(range, cursor);
    }
};

struct range_get_nonconst_ref_fn_t
{
    template <range_c range_t>
    constexpr decltype(auto) operator()
        [[nodiscard]] (range_t& range, const cursor_type_for<range_t>& cursor)
        const OKAYLIB_NOEXCEPT
        // this function is specifically for getting a mutable reference, if
        // the range doesn't return a mutable reference then use regular
        // get()
        requires range_impls_get_c<range_t> && requires {
            {
                range_def_for<range_t>::get(range, cursor)
            } -> stdc::is_nonconst_reference_c;
        }
    {
        return range_def_for<range_t>::get(range, cursor);
    }

    template <range_c range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
        // this function is specifically for getting a mutable reference, if
        // the range doesn't return a mutable reference then use regular
        // get()
        requires range_impls_get_c<range_t> && requires {
            {
                range_def_for<range_t>::get(range, cursor)
            } -> stdc::is_nonconst_reference_c;
        }
    {
        return range_def_for<range_t>::get(range, cursor);
    }
};

struct range_get_ref_fn_t
{
    template <range_c range_t>
    constexpr decltype(auto) operator()
        [[nodiscard]] (range_t& range, const cursor_type_for<range_t>& cursor)
        const OKAYLIB_NOEXCEPT
        requires range_impls_get_c<range_t> && requires {
            {
                range_def_for<range_t>::get(range, cursor)
            } -> stdc::is_reference_c;
        }
    {
        return range_def_for<range_t>::get(range, cursor);
    }

    template <range_c range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
        requires range_impls_get_c<range_t> && requires {
            {
                range_def_for<range_t>::get(range, cursor)
            } -> stdc::is_reference_c;
        }
    {
        return range_def_for<range_t>::get(range, cursor);
    }

    __ok_range_function_assert_not_partially_called_member(range_get_ref)
        __ok_range_function_assert_correct_cursor_type_member
};

struct range_set_fn_t
{
    /// May return a status if set() call fails
    template <consuming_range_c range_t, typename... construction_args_t>
    [[nodiscard]] constexpr decltype(auto)
    operator()(range_t& range, const cursor_type_for<range_t>& cursor,
               construction_args_t&&... args) const OKAYLIB_NOEXCEPT
        requires(
            // can just call set, normally
            range_impls_construction_set_c<range_t, construction_args_t...> ||
            // the construction arguments are a single thing which can be
            // assigned into the existing items
            (((sizeof...(construction_args_t) == 1 &&
               requires {
                   // must be in requires block in case sizeof construction
                   // args is zero
                   stdc::is_assignable_v<value_type_for<range_t>,
                                         construction_args_t...>;
               }) ||
              // if not assignable, we can try stdstyle construction and
              // destruction
              is_infallible_constructible_c<value_type_for<range_t>,
                                            construction_args_t...> &&
                  is_std_destructible_c<value_type_for<range_t>>) &&
             requires {
                 {
                     range_def_for<range_t>::get(range, cursor)
                 } -> stdc::is_nonconst_reference_c;
             }))
    {
        if constexpr (range_impls_construction_set_c<range_t,
                                                     construction_args_t...>) {
            return range_def_for<range_t>::set(
                range, cursor, stdc::forward<construction_args_t>(args)...);
        } else {
            constexpr bool use_assignment =
                (sizeof...(construction_args_t) == 1 && requires {
                    requires stdc::is_assignable_v<value_type_for<range_t>,
                                                   construction_args_t...>;
                });

            auto& ref = range_def_for<range_t>::get(range, cursor);

            if constexpr (use_assignment) {
                const auto applyer = [&ref](auto&& a) {
                    if constexpr (stdc::is_rvalue_reference_v<decltype(a)>) {
                        ref = std::move(a);
                    } else {
                        ref = a;
                    }
                };
                applyer(stdc::forward<construction_args_t>(args)...);
            } else {
                ref.~value_type_for<range_t>();
                ok::make_into_uninitialized<value_type_for<range_t>>(
                    ref, stdc::forward<construction_args_t>(args)...);
            }
        }
    }

    __ok_range_function_assert_not_partially_called_member(range_set)
        __ok_range_function_assert_correct_cursor_type_member
};

struct begin_fn_t
{
    template <range_c range_t>
    constexpr cursor_type_for<range_t> operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_marked_arraylike_c<range_t>) {
            static_assert(
                !range_impls_begin_c<range_t> ||
                    range_strictly_disallows_begin_c<range_t>,
                "range which is marked arraylike implements begin. either "
                "remove the implementation or include a strict_flags "
                "declaration which omits range_strict_flags::implements_begin "
                "to explicitly declare that you want the implementation to be "
                "ignored.");
            return size_t(0);
        } else {
            return range_def_for<range_t>::begin(range);
        }
    }

    __ok_range_function_assert_not_partially_called_member(begin)
};

struct increment_fn_t
{
    template <range_c range_t>
    constexpr void
    operator()(const range_t& range,
               cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_impls_increment_c<range_t>) {
            range_def_for<range_t>::increment(range, cursor);
        } else if constexpr (range_impls_offset_c<range_t>) {
            range_def_for<range_t>::offset(range, cursor, int64_t(1));
        } else {
            static_assert(
                has_pre_increment_c<cursor_type_unchecked_for_t<range_t>>);
            ++cursor;
        }
    }

    __ok_range_function_assert_not_partially_called_member(increment)
        __ok_range_function_assert_correct_cursor_type_member
};

struct decrement_fn_t
{
    template <bidirectional_range_c range_t>
    constexpr void
    operator()(const range_t& range,
               cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_impls_decrement_c<range_t>) {
            range_def_for<range_t>::decrement(range, cursor);
        } else if constexpr (range_impls_offset_c<range_t>) {
            range_def_for<range_t>::offset(range, cursor, int64_t(-1));
        } else {
            static_assert(
                has_pre_decrement_c<cursor_type_unchecked_for_t<range_t>>);
            --cursor;
        }
    }

    __ok_range_function_assert_not_partially_called_member(decrement)
        __ok_range_function_assert_correct_cursor_type_member
};

struct range_compare_fn_t
{
    template <range_c range_t>
        requires range_can_compare_c<range_t>
    constexpr ordering
    operator()(const range_t& range, const cursor_type_for<range_t>& cursor_a,
               const cursor_type_for<range_t>& cursor_b) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_impls_compare_c<range_t>) {
            return range_def_for<range_t>::compare(range, cursor_a, cursor_b);
        } else {
            static_assert(orderable_c<cursor_type_for<range_t>>);
            return ok::cmp(cursor_a, cursor_b);
        }
    }

    __ok_range_function_assert_not_partially_called_member(range_compare)
        __ok_range_function_assert_correct_cursor_type_member
};

struct range_offset_fn_t
{
    template <range_c range_t>
        requires range_can_offset_c<range_t>
    constexpr void operator()(const range_t& range,
                              cursor_type_for<range_t>& cursor,
                              int64_t offset) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_impls_offset_c<range_t>) {
            range_def_for<range_t>::offset(range, cursor, offset);
        } else {
            static_assert(
                has_inplace_addition_with_i64_c<cursor_type_for<range_t>>);
            cursor += offset;
        }
    }

    __ok_range_function_assert_not_partially_called_member(range_offset)
        __ok_range_function_assert_correct_cursor_type_member
};

struct is_inbounds_fn_t
{
    template <range_c range_t>
    constexpr bool operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor) const
    {
        if constexpr (range_marked_arraylike_c<range_t> &&
                      !range_marked_infinite_c<range_t>) {
            return cursor < range_def_for<range_t>::size(range);
        } else if constexpr (range_marked_infinite_c<range_t>) {
            return true;
        } else if constexpr (range_impls_is_inbounds_c<range_t>) {
            return range_def_for<range_t>::is_inbounds(range, cursor);
        } else {
            static_assert(false, "no way to increment given range?");
        }
    }

    __ok_range_function_assert_not_partially_called_member(is_inbounds)
        __ok_range_function_assert_correct_cursor_type_member
};

struct size_fn_t
{
    template <range_c range_t>
        requires range_marked_sized_c<range_t>
    constexpr size_t operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
    {
        return range_def_for<range_t>::size(range);
    }

    template <range_c range_t>
        requires range_marked_finite_c<range_t>
    constexpr size_t operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(false, "Attempt to get the size of a range whose size "
                             "can't be calculated in constant time.");
        return 0;
    }

    template <range_c range_t>
        requires range_marked_infinite_c<range_t>
    constexpr auto operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(false,
                      "Attempt to get the size of an infinitely large range");
        return 0;
    }

    __ok_range_function_assert_not_partially_called_member(size)
};
} // namespace detail

// specialization for c-style arrays
template <typename input_range_t>
    requires std::is_array_v<detail::remove_cvref_t<input_range_t>>
struct range_definition<input_range_t>
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;

  public:
    // a c array can never have a const value type
    using value_type = detail::c_array_value_type<range_t>;
    static_assert(!is_const_c<value_type>);

    static constexpr range_flags flags =
        range_flags::arraylike | range_flags::sized | range_flags::consuming |
        range_flags::producing;

    static constexpr value_type& get(range_t& i, size_t c) OKAYLIB_NOEXCEPT
    {
        if (c >= size(i)) [[unlikely]] {
            __ok_abort("out of bounds access into c-style array");
        }
        return i[c];
    }

    static constexpr const value_type& get(const range_t& i,
                                           size_t c) OKAYLIB_NOEXCEPT
    {
        if (c >= size(i)) [[unlikely]] {
            __ok_abort("out of bounds access into c-style array");
        }
        return i[c];
    }

    static constexpr size_t size(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return detail::c_array_length(i);
    }
};

template <typename viewed_t> class slice;

// specialization for things that can be indexed with size_t and have an
// range_t::value_type member and a size() and data() function (stuff like
// std::array, std::span, and std::vector)
template <typename input_range_t>
    requires(
        // not array
        !stdc::is_array_v<detail::remove_cvref_t<input_range_t>> &&
        // not specified to inherit some other range definition
        !has_inherited_range_type_v<detail::remove_cvref_t<input_range_t>> &&
        // provides size_t .size() method and pointer data() method
        detail::std_arraylike_container<
            detail::remove_cvref_t<input_range_t>> &&
        stdc::is_same_v<
            typename detail::remove_cvref_t<input_range_t>::value_type&,
            decltype(std::declval<detail::remove_cvref_t<input_range_t>&>()
                         [std::declval<size_t>()])>)
struct range_definition<input_range_t>
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;
    static constexpr range_flags universal_flags =
        range_flags::arraylike | range_flags::sized | range_flags::producing;

  public:
    static_assert(!detail::is_instance_c<range_t, slice>);

    // std::array may have a const value type
    using value_type = typename range_t::value_type;

    static constexpr range_flags flags =
        is_const_c<typename range_t::value_type>
            ? universal_flags
            : universal_flags | range_flags::consuming;

    // dont enable mutable get_ref unless the index operator actually returns a
    // mutable reference (this applies for array<const T>)
    static constexpr value_type& get(range_t& i, size_t c) OKAYLIB_NOEXCEPT
        requires std::is_same_v<
            value_type&,
            decltype(std::declval<range_t&>()[std::declval<size_t>()])>
    {
        if (c >= i.size()) [[unlikely]] {
            __ok_abort("Out of bounds access into arraylike container.");
        }
        return i[c];
    }

    static constexpr const value_type& get(const range_t& i,
                                           size_t c) OKAYLIB_NOEXCEPT
    {
        if (c >= i.size()) [[unlikely]] {
            __ok_abort("Out of bounds access into arraylike container.");
        }
        return i[c];
    }

    static constexpr size_t size(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }
};

/// Call the range's get() function with a const reference to the range.
/// Returns value_type or const value_type&.
inline constexpr detail::range_get_fn_t range_get{};

/// Try to get a mutable reference, if not then try to get a const reference,
/// if not then try to get a value type. Basically just calls the range's get()
/// function with the given arguments.
inline constexpr detail::range_get_best_fn_t range_get_best{};

/// Call the range's get() function, attempting to get a `value_type&`. If it's
/// not possible to get a mutable reference from the given object there will
/// be no matching function call (compile error).
inline constexpr detail::range_get_nonconst_ref_fn_t range_get_nonconst_ref{};

/// Call the range's get() function (either const or nonconst variant depending
/// on the inputs) and only compile if the call returns a reference.
inline constexpr detail::range_get_ref_fn_t range_get_ref{};

/// Either call the range's set() function or, if not possible, try to
/// get_nonconst_ref() and assign into the existing item OR destroy the existing
/// item and then construct over it.
inline constexpr detail::range_set_fn_t range_set{};

inline constexpr detail::begin_fn_t begin{};

inline constexpr detail::is_inbounds_fn_t is_inbounds{};

inline constexpr detail::size_fn_t size{};

inline constexpr detail::increment_fn_t increment{};

inline constexpr detail::decrement_fn_t decrement{};

/// Compare two cursors of a given range (range, cursor_a, cursor_b)
inline constexpr detail::range_compare_fn_t range_compare{};

/// Offset a cursor by an i64 (range, cursor, i64 offset)
inline constexpr detail::range_offset_fn_t range_offset{};

} // namespace ok

#endif
