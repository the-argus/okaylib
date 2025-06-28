#ifndef __OKAYLIB_RANGES_RANGES_H__
#define __OKAYLIB_RANGES_RANGES_H__

#include "okay/detail/addressof.h"
#include "okay/detail/construct_at.h"
#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/template_util/first_type_in_pack.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/is_std_container.h"
#include "okay/detail/traits/mathop_traits.h"
#include "okay/math/ordering.h"
#include "okay/slice.h"

/*
 * This header defines customization points and traits for ranges- their
 * value and cursor types, and operations on the range.
 *
 * `range_definition<T>`
 * `value_type_for<T>`
 * `cursor_type_for<T>`
 * `range_def_for<T>` (checked access to range_definition<T>)
 * `value_type iter_copyout(const range_t&, const cursor_t&)`
 * `const value_type& iter_get_temporary_ref(const range_t&, const
 * cursor_t&)`
 * `[const] value_type& iter_get_ref([const] range_t&, const cursor_t&)`
 * `void iter_set(range_t&, const cursor_t&, ...)`
 * `begin()`
 * `is_inbounds()`
 */

namespace ok {

template <typename range_t> struct range_definition
{
    // if a range definition includes a `static bool deleted = true`, then
    // it is ignored and the type is considered an invalid range.
    static constexpr bool deleted = true;
};

template <typename T, typename = void>
struct has_inherited_range_type : public std::false_type
{
    using type = void;
};
template <typename T>
struct has_inherited_range_type<T,
                                std::void_t<typename T::inherited_range_type>>
    : public std::true_type
{
    using type = typename T::inherited_range_type;
};

template <typename T>
constexpr bool has_inherited_range_type_v = has_inherited_range_type<T>::value;

// specialization for c-style arrays
template <typename input_range_t>
    requires std::is_array_v<detail::remove_cvref_t<input_range_t>>
struct range_definition<input_range_t>
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;

  public:
    using value_type = detail::c_array_value_type<range_t>;

    static constexpr bool is_arraylike = true;

    static constexpr value_type& get_ref(range_t& i, size_t c) OKAYLIB_NOEXCEPT
    {
        if (c >= size(i)) [[unlikely]] {
            __ok_abort("out of bounds access into c-style array");
        }
        return i[c];
    }

    static constexpr const value_type& get_ref(const range_t& i,
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

// specialization for things that can be indexed with size_t and have an
// range_t::value_type member and a size() and data() function (stuff like
// std::array, std::span, and std::vector)
template <typename input_range_t>
    requires(
        // not array
        !std::is_array_v<detail::remove_cvref_t<input_range_t>> &&
        // not specified to inherit some other range definition
        !has_inherited_range_type_v<detail::remove_cvref_t<input_range_t>> &&
        // provides size_t .size() method and pointer data() method
        detail::std_arraylike_container<input_range_t> &&
        // iterator_t()[size_t{}] -> iterator_t::value_type&
        std::is_same_v<
            typename detail::remove_cvref_t<input_range_t>::value_type&,
            decltype(std::declval<detail::remove_cvref_t<input_range_t>&>()
                         [std::declval<size_t>()])>)
struct range_definition<input_range_t>
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;

  public:
    static_assert(!detail::is_instance_c<range_t, slice>);
    // value type for ranges is different than value type for vector or array.
    // a range over `const int` has a value_type of `int`, and jut only provides
    // `const int& get_ref(const range_t&)`
    using value_type = detail::remove_cvref_t<typename range_t::value_type>;
    static constexpr bool is_arraylike = true;

    // dont enable mutable get_ref unless the index operator actually returns a
    // mutable reference (this applies for array<const T>)
    static constexpr value_type& get_ref(range_t& i, size_t c) OKAYLIB_NOEXCEPT
        requires std::is_same_v<
            value_type&,
            decltype(std::declval<range_t&>()[std::declval<size_t>()])>
    {
        if (c >= i.size()) [[unlikely]] {
            __ok_abort("Out of bounds access into arraylike container.");
        }
        return i[c];
    }

    static constexpr const value_type& get_ref(const range_t& i,
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

namespace detail {

template <typename T> struct range_definition_inner_meta_t
{
    static_assert(std::is_same_v<T, remove_cvref_t<T>>);
    using inherited_id = typename has_inherited_range_type<T>::type;
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

template <typename T>
concept arraylike_range_c =
    requires { requires range_definition_inner_t<T>::is_arraylike; };

template <typename T, typename = void>
struct has_value_type : public std::false_type
{};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>>
    : public std::true_type
{
    using type = typename T::value_type;
};

template <typename T, typename = void>
struct range_is_ref_wrapper : std::false_type
{};

template <typename T>
concept ref_wrapper_range_c = requires {
    requires range_definition_inner_t<T>::is_ref_wrapper;
    requires has_value_type<range_definition_inner_t<T>>::value;
};

template <typename T>
constexpr bool is_valid_cursor_v =
    // NOTE: update static_assert in range_def_for if this changes
    std::is_object_v<T>;

template <typename T>
constexpr bool is_valid_value_type_v = std::is_object_v<T>;

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
    std::conditional_t<arraylike_range_c<T>, size_t,
                       typename range_begin_rettype_or_void<T>::type>;

template <typename T>
concept range_impls_begin_c = requires {
    requires !is_void_c<typename range_begin_rettype_or_void<T>::type>;
};

template <typename T>
concept range_impls_is_inbounds_c =
    requires(const remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& cursor) {
        {
            range_definition_inner_t<T>::is_inbounds(range, cursor)
        } -> same_as_c<bool>;
    };

template <typename T>
concept range_impls_increment_c = requires(
    const remove_cvref_t<T>& range, cursor_type_unchecked_for_t<T>& cursor) {
    { range_definition_inner_t<T>::increment(range, cursor) } -> is_void_c;
};

template <typename T>
concept range_impls_decrement_c = requires(
    const remove_cvref_t<T>& range, cursor_type_unchecked_for_t<T>& cursor) {
    { range_definition_inner_t<T>::decrement(range, cursor) } -> is_void_c;
};

template <typename T>
concept range_impls_offset_c =
    requires(const remove_cvref_t<T>& range,
             cursor_type_unchecked_for_t<T>& cursor, int64_t offset) {
        {
            range_definition_inner_t<T>::offset(range, cursor, offset)
        } -> is_void_c;
    };

template <typename T>
concept range_impls_compare_c =
    requires(const remove_cvref_t<T>& range,
             const cursor_type_unchecked_for_t<T>& a,
             const cursor_type_unchecked_for_t<T>& b) {
        {
            range_definition_inner_t<T>::compare(range, a, b)
        } -> same_as_c<ok::ordering>;
    };

template <typename T>
concept range_impls_size_c = requires(const remove_cvref_t<T>& range) {
    { range_definition_inner_t<T>::size(range) } -> same_as_c<size_t>;
};

template <typename T>
concept range_marked_infinite_c =
    requires() { requires range_definition_inner_t<T>::is_infinite; };

template <typename T>
concept range_marked_finite_c =
    requires() { requires !range_definition_inner_t<T>::is_infinite; };

template <typename T>
concept range_disallows_cursor_member_offset_c = requires() {
    requires range_definition_inner_t<T>::disallow_cursor_member_offset;
};

template <typename T>
concept range_can_begin_c =
    requires { requires(range_impls_begin_c<T> || arraylike_range_c<T>); };

template <typename T>
concept range_can_is_inbounds_c = requires {
    requires(range_impls_is_inbounds_c<T> || arraylike_range_c<T>);
};

// alias in case indirection is added later and (having size def) != (is sized)
template <typename T>
concept range_can_size_c = range_impls_size_c<T>;

template <typename T>
concept range_marked_deleted_c =
    requires { requires range_definition_inner_t<T>::deleted; };

template <typename T>
concept range_has_definition_c = !range_marked_deleted_c<T>;

/// range_impls_get_c except it doesn't check that the return type is the value
/// type
template <typename range_t>
concept range_impls_get_unchecked_rettype_c = requires(
    const range_t& range, const cursor_type_unchecked_for_t<range_t>& cursor) {
    range_definition_inner_t<range_t>::get(range, cursor);
};

template <typename getting_range_t> struct range_get_return_type_meta_t
{
    using type = void;
};

template <typename getting_range_t>
    requires range_impls_get_unchecked_rettype_c<getting_range_t>
struct range_get_return_type_meta_t<getting_range_t>
{
    using type = decltype(range_definition_inner_t<getting_range_t>::get(
        std::declval<const getting_range_t&>(),
        std::declval<const cursor_type_unchecked_for_t<getting_range_t>&>()));
};

template <typename getting_range_t>
using range_get_return_type_t =
    typename range_get_return_type_meta_t<getting_range_t>::type;

template <typename range_t>
concept range_impls_get_ref_unchecked_rettype_c = requires(
    range_t& range, const cursor_type_unchecked_for_t<range_t>& cursor) {
    range_definition_inner_t<range_t>::get_ref(range, cursor);
};

template <typename getting_range_t> struct range_get_ref_return_type_meta_t
{
    using type = void;
    using deduced_nonrefwrap_value_type = void;
};

template <typename getting_range_t>
    requires range_impls_get_ref_unchecked_rettype_c<getting_range_t>
struct range_get_ref_return_type_meta_t<getting_range_t>
{
    using type = decltype(range_definition_inner_t<getting_range_t>::get_ref(
        std::declval<getting_range_t&>(),
        std::declval<const cursor_type_unchecked_for_t<getting_range_t>&>()));
    using deduced_nonrefwrap_value_type = remove_cvref_t<type>;
};

template <typename getting_range_t>
using range_get_ref_return_type_t =
    typename range_get_ref_return_type_meta_t<getting_range_t>::type;

template <range_impls_get_ref_unchecked_rettype_c getting_range_t>
using range_get_ref_deduced_nonrefwrap_value_type_t =
    typename range_get_ref_return_type_meta_t<
        getting_range_t>::deduced_nonrefwrap_value_type;

template <typename range_t>
concept range_impls_get_ref_const_unchecked_rettype_c = requires(
    const range_t& range, const cursor_type_unchecked_for_t<range_t>& cursor) {
    range_definition_inner_t<range_t>::get_ref(range, cursor);
};

template <typename getting_range_t>
struct range_get_ref_const_return_type_meta_t
{
    using type = void;
};

template <typename getting_range_t>
    requires range_impls_get_ref_const_unchecked_rettype_c<getting_range_t>
struct range_get_ref_const_return_type_meta_t<getting_range_t>
{
    using type = decltype(range_definition_inner_t<getting_range_t>::get_ref(
        std::declval<const getting_range_t&>(),
        std::declval<const cursor_type_unchecked_for_t<getting_range_t>&>()));
};

template <typename getting_range_t>
using range_get_ref_const_return_type_t =
    typename range_get_ref_const_return_type_meta_t<getting_range_t>::type;

template <range_impls_get_ref_const_unchecked_rettype_c getting_range_t>
using range_get_ref_const_deduced_nonrefwrap_value_type_t =
    remove_cvref_t<range_get_ref_const_return_type_t<getting_range_t>>;

/// Either range_t::value_type or, if that's not defined, the return type of
/// range_t::get()
template <typename range_t> struct range_deduced_value_type
{
  private:
    static constexpr bool impls_get =
        range_impls_get_unchecked_rettype_c<range_t>;
    static constexpr bool impls_get_ref =
        range_impls_get_ref_unchecked_rettype_c<range_t>;
    static constexpr bool impls_get_ref_const =
        range_impls_get_ref_const_unchecked_rettype_c<range_t>;

    using deduced_from_get_ref =
        range_get_ref_deduced_nonrefwrap_value_type_t<range_t>;
    using deduced_from_get_ref_const =
        range_get_ref_const_deduced_nonrefwrap_value_type_t<range_t>;

  public:
    using type = std::conditional_t<
        impls_get | impls_get_ref | impls_get_ref_const,
        std::conditional_t<
            impls_get_ref_const && !ref_wrapper_range_c<range_t>,
            std::conditional_t<impls_get_ref, deduced_from_get_ref,
                               deduced_from_get_ref_const>,
            range_get_return_type_t<range_t>>,
        void>;
};

template <typename range_t>
    requires(
        requires { typename range_definition_inner_t<range_t>::value_type; })
struct range_deduced_value_type<range_t>
{
    using type = typename range_definition_inner_t<range_t>::value_type;
};

template <typename T>
using range_deduced_value_type_t = typename range_deduced_value_type<T>::type;

template <typename T>
concept range_impls_get_c =
    range_impls_get_unchecked_rettype_c<T> &&
    std::is_same_v<range_get_return_type_t<T>, range_deduced_value_type_t<T>>;

template <typename range_t, typename... args_t>
concept range_impls_construction_set_c = requires(
    range_t& r, const cursor_type_unchecked_for_t<range_t>& c, args_t... args) {
    { range_definition_inner_t<range_t>::set(r, c, args...) } -> is_void_c;
};

template <typename T>
concept range_impls_move_construction_set_c = requires {
    requires range_impls_construction_set_c<T, range_deduced_value_type_t<T>&&>;
};

template <typename T>
concept range_impls_get_ref_c = range_impls_get_ref_unchecked_rettype_c<T> &&
                                std::is_same_v<range_get_ref_return_type_t<T>,
                                               range_deduced_value_type_t<T>&>;

template <typename T>
concept range_impls_get_ref_const_c =
    range_impls_get_ref_const_unchecked_rettype_c<T> &&
    std::is_same_v<range_get_ref_const_return_type_t<T>,
                   const range_deduced_value_type_t<T>&>;

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

// ref_or_void: avoid forming a const reference to void
template <typename T> struct ref_or_void
{
    using type = void;
};
template <typename T>
    requires requires { std::declval<T&>(); }
struct ref_or_void<T>
{
    using type = T&;
};
template <typename T> struct const_ref_or_void
{
    using type = void;
};
template <typename T>
    requires requires { std::declval<const T&>(); }
struct const_ref_or_void<T>
{
    using type = const T&;
};

template <typename T>
concept range_nonrefwrap_impls_get_ref_const_c =
    !ref_wrapper_range_c<T> &&
    range_impls_get_ref_const_unchecked_rettype_c<T> &&
    same_as_c<typename const_ref_or_void<range_deduced_value_type_t<T>>::type,
              range_get_ref_const_return_type_t<T>>;

template <typename T>
concept range_nonrefwrap_impls_get_ref_c =
    !ref_wrapper_range_c<T> && range_impls_get_ref_unchecked_rettype_c<T> &&
    std::is_same_v<typename ref_or_void<range_deduced_value_type_t<T>>::type,
                   range_get_ref_return_type_t<T>>;

template <typename T>
concept range_refwrap_impls_get_ref_c =
    ref_wrapper_range_c<T> &&
    range_impls_get_ref_const_unchecked_rettype_c<T> &&
    !is_const_c<range_deduced_value_type_t<T>> &&
    same_as_c<typename ref_or_void<range_deduced_value_type_t<T>>::type,
              range_get_ref_const_return_type_t<T>>;

template <typename T>
concept range_refwrap_impls_get_ref_const_c =
    ref_wrapper_range_c<T> &&
    range_impls_get_ref_const_unchecked_rettype_c<T> &&
    (std::is_same_v<typename ref_or_void<range_deduced_value_type_t<T>>::type,
                    range_get_ref_const_return_type_t<T>> ||
     std::is_same_v<
         typename const_ref_or_void<range_deduced_value_type_t<T>>::type,
         range_get_ref_const_return_type_t<T>>);

template <typename T>
concept range_can_get_ref_c =
    range_refwrap_impls_get_ref_c<T> || range_nonrefwrap_impls_get_ref_c<T>;

template <typename T>
concept range_can_get_ref_const_c =
    range_refwrap_impls_get_ref_const_c<T> ||
    range_nonrefwrap_impls_get_ref_const_c<T> || range_can_get_ref_c<T>;

template <typename T>
concept range_has_baseline_functions_c =
    (range_marked_finite_c<T> || range_marked_infinite_c<T> ||
     range_can_size_c<T>) &&
    range_can_is_inbounds_c<T> && range_can_begin_c<T>;

template <typename T>
concept range_can_offset_c =
    range_impls_offset_c<T> ||
    (!range_disallows_cursor_member_offset_c<T> &&
     has_inplace_addition_with_i64_c<cursor_or_void_t<T>>);

template <typename T>
concept range_can_compare_c =
    range_impls_compare_c<T> || is_orderable_v<cursor_or_void_t<T>>;

template <typename T>
concept range_can_increment_c =
    range_impls_increment_c<T> || has_pre_increment_c<cursor_or_void_t<T>> ||
    range_can_offset_c<T>;

template <typename T>
concept range_can_decrement_c =
    range_impls_decrement_c<T> || has_pre_decrement_c<cursor_or_void_t<T>> ||
    range_can_offset_c<T>;

// NOTE: a type might provide some other set() functions for different args,
// so this might not pick up that its a consuming range in that case.
// TODO: probably add some static constexpr bool impls_set = true; requirement
template <typename T>
concept consuming_range_c =
    range_has_baseline_functions_c<T> && range_can_increment_c<T> &&
    (range_impls_move_construction_set_c<T> || range_can_get_ref_c<T>);

template <typename T>
concept producing_range_c =
    range_has_baseline_functions_c<T> && range_can_increment_c<T> &&
    (range_impls_get_c<T> || range_can_get_ref_const_c<T> ||
     range_can_get_ref_c<T>);

template <typename T>
concept producing_or_consuming_range_c =
    producing_range_c<T> || consuming_range_c<T>;

// single pass ranges are by definition the minimum range- so any valid
// input or output range is also a single pass range.
template <typename maybe_range_t>
concept single_pass_range_c = producing_range_c<maybe_range_t>;

// a multi pass range has a copyable cursor- enabling consumers to pause and
// come back to cursors at previous positions.
template <typename maybe_range_t>
concept multi_pass_range_c =
    single_pass_range_c<maybe_range_t> &&
    is_copy_constructible_c<cursor_or_void_t<maybe_range_t>> &&
    is_copy_assignable_c<cursor_or_void_t<maybe_range_t>>;

template <typename maybe_range_t>
concept bidirectional_range_c = multi_pass_range_c<maybe_range_t> &&
                                // can also be decremented
                                range_can_decrement_c<maybe_range_t>;

template <typename maybe_range_t>
concept random_access_range_c =
    bidirectional_range_c<maybe_range_t> &&
    // must be able to offset cursor by arbitrary i64
    range_can_offset_c<maybe_range_t> &&
    // must be able to compare two cursors
    range_can_compare_c<maybe_range_t>;
} // namespace detail

/// range_def_for is a way of accessing the range definition for a type, with
/// some additional verification that the definition being accessed is actually
/// valid.
template <typename input_range_t>
class range_def_for : public detail::range_definition_inner_t<input_range_t>
{
  public:
    using value_type =
        typename detail::range_deduced_value_type<input_range_t>::type;

    static_assert(
        !std::is_reference_v<value_type> &&
            (!is_const_c<value_type> ||
             detail::ref_wrapper_range_c<input_range_t>),
        "A range's value_type (which is either defined explicitly or deduced "
        "as the output of range_definition::get() or get_ref()) cannot be a "
        "reference, and it can only be const if the range is also a reference "
        "wrapper.");

  private:
    using T = detail::remove_cvref_t<input_range_t>;
    static constexpr bool complete = detail::is_complete_c<T>;

    static_assert(complete,
                  "Refusing to access range definition for incomplete type.");

    static_assert(detail::range_has_definition_c<T>,
                  "Attempt to access range information for a type whose "
                  "range definition is marked deleted.");

    static_assert(detail::range_can_begin_c<T>,
                  "Range definition invalid- provide a begin(const range_t&) "
                  "function which returns a cursor type.");

    static_assert(detail::range_can_is_inbounds_c<T>,
                  "Range definition invalid- `is_inbounds()` not provided.");

    static constexpr bool has_size = detail::range_can_size_c<T>;
    static constexpr bool marked_infinite = detail::range_marked_infinite_c<T>;
    static constexpr bool marked_finite = detail::range_marked_finite_c<T>;
    static_assert(
        has_size || marked_infinite || marked_finite,
        "Range definition invalid- provide a size_t size(const range_t&) "
        "method, or add a `static constexpr bool is_infinite = ...` which "
        "should be true if the range is intended to be infinite, and false if "
        "the range is intended to have some finite size that cannot be "
        "calculated in constant time.");
    static_assert(int(has_size) + int(marked_finite) + int(marked_infinite) ==
                      1,
                  "Range definition invalid: specify only one of `static "
                  "bool infinite = ...` or `size()` method.");

    static_assert(
        !std::is_void_v<value_type>,
        "Range definition invalid- value_type is void. You may need "
        "to explicitly declare it in the range definition, or "
        "provide valid `get()`, `get_ref()`, or `get_ref() const` "
        "functions so that the value type can be deduced. Note: "
        "deduction only works if the range is not a reference wrapper.");

    static_assert(detail::is_valid_value_type_v<value_type>,
                  "Range definition invalid- value_type is not an object.");

    static_assert(
        !detail::ref_wrapper_range_c<T> || !detail::range_impls_get_c<T>,
        "Range is a reference wrapper, but it implements get(). Reference "
        "wrappers should only implement get_ref(const&). If you are returning "
        "a reference wrapper type, then your type is not a reference wrapper "
        "itself (its not responsibile for resolving the possibility that it "
        "may be const but the returned reference may not be).");

    static_assert(
        !detail::ref_wrapper_range_c<T> ||
            detail::range_impls_get_ref_const_unchecked_rettype_c<T>,
        "Range is marked as a reference wrapper, but it does not implement "
        "get_ref(const&), which is required of all reference wrappers.");

    // either it doesnt have the function at all, OR the function is totally
    // valid
    static_assert(!detail::range_impls_get_unchecked_rettype_c<T> ||
                      detail::range_impls_get_c<T>,
                  "Range definition invalid- `get()` function does not return "
                  "value_type.");

    static constexpr bool nonrefwrap_get_ref_const_function_returns_value_type =
        !detail::range_impls_get_ref_const_unchecked_rettype_c<T> ||
        detail::range_nonrefwrap_impls_get_ref_const_c<T>;

    static_assert(detail::ref_wrapper_range_c<T> ||
                      nonrefwrap_get_ref_const_function_returns_value_type,
                  "Range definition invalid- `get_ref(const&)` function does "
                  "not return `const value_type&`");

    static constexpr bool nonrefwrap_get_ref_function_returns_value_type =
        !detail::range_impls_get_ref_unchecked_rettype_c<T> ||
        detail::range_nonrefwrap_impls_get_ref_c<T>;

    static_assert(detail::ref_wrapper_range_c<T> ||
                      nonrefwrap_get_ref_function_returns_value_type,
                  "Range definition invalid- `get_ref()` function does not "
                  "return `value_type&`");

    static_assert(
        detail::is_valid_cursor_v<detail::cursor_type_unchecked_for_t<T>>,
        "Range definition invalid- the return value from begin() (the "
        "cursor type) is not an object.");

    static_assert(
        detail::has_pre_increment_c<detail::cursor_type_unchecked_for_t<T>> ||
            detail::range_impls_increment_c<T>,
        "Range definition invalid- given cursor type (the return type of "
        "begin()) is not able to be pre-incremented, nor is an increment(const "
        "range&, cursor&) function defined in the range definition.");

    static_assert(
        detail::producing_or_consuming_range_c<T>,
        "Range definition invalid- it does not define any operations that "
        "can be done with the range like get(range, cursor), set(range, "
        "cursor, value_type&&), or get_ref(range, cursor)");

    static constexpr bool ref_or_get_are_mutually_exclusive =
        (detail::range_impls_get_c<T> !=
         (detail::range_can_get_ref_c<T> ||
          detail::range_can_get_ref_const_c<T>)) ||
        // its also valid for the range to only define set()
        (!detail::range_impls_get_c<T> && !detail::range_can_get_ref_c<T> &&
         !detail::range_can_get_ref_const_c<T>);
    static_assert(
        ref_or_get_are_mutually_exclusive,
        "Range definition invalid- both get() and a get_ref() functions are "
        "defined. There is currently no protocol in okaylib to choose between "
        "these implementations, so remove one of them and use the copy "
        "constructor of value_type if both copyout and get_ref are needed.");
};

namespace detail {
template <typename T, typename value_type>
concept range_with_value_type_c = requires {
    requires is_complete_c<T>;
    requires range_has_definition_c<T>;
    // NOTE: not checking if has value type before doing this: if it
    // doesnt have it, it will be void, which is not a valid value type
    requires is_valid_value_type_v<value_type>;
    requires range_can_begin_c<T>;
    requires is_valid_cursor_v<cursor_type_unchecked_for_t<T>>;
    requires range_can_is_inbounds_c<T>;
    requires range_can_size_c<T> || range_marked_infinite_c<T> ||
                 range_marked_finite_c<T>;
    requires(int(range_can_size_c<T>) + int(range_marked_infinite_c<T>) +
             int(range_marked_finite_c<T>)) == 1;
    requires producing_or_consuming_range_c<T>;
    requires(range_impls_get_c<T> !=
             (range_can_get_ref_c<T> || range_can_get_ref_const_c<T>));
};
} // namespace detail

/// Check if a type has an range_definition which is valid, enabling a form
/// of either input or output iteration.
template <typename T>
concept range_c =
    detail::range_with_value_type_c<detail::remove_cvref_t<T>,
                                    detail::range_deduced_value_type_t<T>>;

template <typename T>
using value_type_for = typename range_def_for<T>::value_type;
template <typename T>
using cursor_type_for =
    std::conditional_t<ok::detail::arraylike_range_c<T>, size_t,
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

struct iter_copyout_fn_t
{
    template <range_c range_t>
        requires range_impls_get_c<range_t> ||
                 range_can_get_ref_const_c<range_t>
    constexpr auto operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        using namespace detail;
        using def = range_definition_inner_t<range_t>;
        if constexpr (range_impls_get_c<range_t>) {
            return def::get(range, cursor);
        } else {
            return def::get_ref(range, cursor);
        }
    }
};

template <typename range_t> struct iter_get_temporary_ref_meta_t
{
    using type = void;
    static constexpr void call(...) OKAYLIB_NOEXCEPT
    {
        // trigger static asserts in range_def_for
        constexpr bool checks = std::is_void_v<range_def_for<range_t>>;
    }
};

template <range_c range_t>
    requires(!range_can_get_ref_const_c<range_t>)
struct iter_get_temporary_ref_meta_t<range_t>
{
    using type = value_type_for<range_t>;
    static constexpr type
    call(const range_t& range,
         const cursor_type_for<range_t>& cursor) OKAYLIB_NOEXCEPT
    {
        constexpr auto copyout = iter_copyout_fn_t{};
        return copyout(range, cursor);
    }
};

template <range_c range_t>
    requires(range_can_get_ref_const_c<range_t>)
struct iter_get_temporary_ref_meta_t<range_t>
{
    using type = const value_type_for<range_t>&;
    static constexpr type
    call(const range_t& range,
         const cursor_type_for<range_t>& cursor) OKAYLIB_NOEXCEPT
    {
        return range_definition_inner_t<range_t>::get_ref(range, cursor);
    }
};

struct iter_get_temporary_ref_fn_t
{
    template <range_c range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        return iter_get_temporary_ref_meta_t<range_t>::call(range, cursor);
    }

    __ok_range_function_assert_not_partially_called_member(
        iter_get_temporary_ref)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_get_ref_fn_t
{
    template <range_c range_t>
        requires range_impls_get_ref_c<range_t>
    constexpr decltype(auto) operator()
        [[nodiscard]] (range_t& range, const cursor_type_for<range_t>& cursor)
        const OKAYLIB_NOEXCEPT
    {
        return range_definition_inner_t<range_t>::get_ref(range, cursor);
    }

    template <range_c range_t>
        requires range_impls_get_ref_const_c<range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        return range_definition_inner_t<range_t>::get_ref(range, cursor);
    }

    __ok_range_function_assert_not_partially_called_member(iter_get_ref)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_set_fn_t
{
    template <range_c range_t, typename... construction_args_t>
        requires(
            // can just call set, normally
            range_impls_construction_set_c<range_t, construction_args_t...> ||
            // the construction arguments are a single thing which can be
            // assigned into the existing items
            (sizeof...(construction_args_t) == 1 &&
             requires {
                 std::is_assignable_v<value_type_for<range_t>,
                                      construction_args_t...>;
             }) ||
            // can call get ref, and we can emulate assignment by destroying
            // the object that is already present and then constructing over
            // it
            (range_can_get_ref_c<range_t> &&
             is_std_constructible_c<value_type_for<range_t>,
                                    construction_args_t...>))
    constexpr void
    operator()(range_t& range, const cursor_type_for<range_t>& cursor,
               construction_args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_can_get_ref_c<range_t>) {
            auto& ref =
                range_definition_inner_t<range_t>::get_ref(range, cursor);

            // if you supplied only one arg, and it is assignable, and the range
            // uses get_ref, then set performs assignment. otherwise, it
            // manually calls destruction and then constructs in place
            if constexpr (sizeof...(construction_args_t) == 1) {
                using first_type = first_type_in_pack_t<construction_args_t...>;
                if constexpr (std::is_assignable_v<decltype(ref), first_type>) {
                    const auto applyer = [&ref](auto&& a) {
                        ref = std::forward<first_type>(a);
                    };
                    applyer(std::forward<construction_args_t>(args)...);
                } else {
                    // duplicated code with outer else case here
                    ref.~value_type_for<range_t>();
                    ok::construct_at(
                        ok::addressof(ref),
                        std::forward<construction_args_t>(args)...);
                }
            } else {
                ref.~value_type_for<range_t>();

                ok::construct_at(ok::addressof(ref),
                                 std::forward<construction_args_t>(args)...);
            }
        } else {
            range_definition_inner_t<range_t>::set(
                range, cursor, std::forward<construction_args_t>(args)...);
        }
    }

    __ok_range_function_assert_not_partially_called_member(iter_set)
        __ok_range_function_assert_correct_cursor_type_member
};

struct begin_fn_t
{
    template <range_c range_t>
    constexpr cursor_type_unchecked_for_t<range_t> operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
    {
        if constexpr (arraylike_range_c<range_t>) {
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
    template <range_c range_t>
    constexpr void
    operator()(const range_t& range,
               cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_impls_increment_c<range_t>) {
            range_def_for<range_t>::decrement(range, cursor);
        } else if constexpr (range_impls_offset_c<range_t>) {
            range_def_for<range_t>::offset(range, cursor, int64_t(-1));
        } else {
            static_assert(
                has_pre_increment_c<cursor_type_unchecked_for_t<range_t>>);
            --cursor;
        }
    }

    __ok_range_function_assert_not_partially_called_member(decrement)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_compare_fn_t
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
            static_assert(is_orderable_v<cursor_type_for<range_t>>);
            return ok::cmp(cursor_a, cursor_b);
        }
    }

    __ok_range_function_assert_not_partially_called_member(iter_compare)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_offset_fn_t
{
    template <range_c range_t>
        requires range_can_offset_c<range_t>
    constexpr ordering operator()(const range_t& range,
                                  cursor_type_for<range_t>& cursor,
                                  int64_t offset) const OKAYLIB_NOEXCEPT
    {
        if constexpr (range_impls_offset_c<range_t>) {
            return range_def_for<range_t>::offset(range, cursor, offset);
        } else {
            static_assert(has_inplace_addition_with_i64_c<range_t>);
            cursor += offset;
        }
    }

    __ok_range_function_assert_not_partially_called_member(iter_offset)
        __ok_range_function_assert_correct_cursor_type_member
};

struct is_inbounds_fn_t
{
    template <range_c range_t>
    constexpr bool operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor) const
    {
        if constexpr (range_impls_is_inbounds_c<range_t>) {
            return range_def_for<range_t>::is_inbounds(range, cursor);
        } else if constexpr (range_marked_infinite_c<range_t>) {
            return true;
        } else {
            return cursor < range_def_for<range_t>::size(range);
        }
    }

    __ok_range_function_assert_not_partially_called_member(is_inbounds)
        __ok_range_function_assert_correct_cursor_type_member
};

struct size_fn_t
{
    template <range_c range_t>
        requires(!range_marked_finite_c<range_t> &&
                 !range_marked_infinite_c<range_t>)
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

/// Get a copy of the data pointed at by an iterator. Works on ranges
/// implementing `iter_get()`, or `iter_deref() [const]` if value_type is copy
/// constructible.
/// Returns range_t::value_type.
inline constexpr detail::iter_copyout_fn_t iter_copyout{};

/// Get a temporary const reference to an item. More optimizable than iter_get
/// (its possible for any copying to be elided) but the reference is only valid
/// until the end of the execution of the calling function. Supported by all
/// types which implement at least one of `iter_get()`, `iter_deref() const`, or
/// `iter_deref()`.
/// Returns either a range_t::value_type, or a const range_t::value_type&.
/// If the return value is a reference, then it is the same as calling
/// iter_get_ref. It is always valid to store the return value in a `const
/// value_type&`
inline constexpr detail::iter_get_temporary_ref_fn_t iter_get_temporary_ref{};

/// Returns an lvalue reference to the internel representation of the data
/// pointed at by the iterator. This reference is invalidated based on
/// container-specific behavior.
inline constexpr detail::iter_get_ref_fn_t iter_get_ref{};

inline constexpr detail::iter_set_fn_t iter_set{};

inline constexpr detail::begin_fn_t begin{};

inline constexpr detail::is_inbounds_fn_t is_inbounds{};

inline constexpr detail::size_fn_t size{};

inline constexpr detail::increment_fn_t increment{};

inline constexpr detail::decrement_fn_t decrement{};

// functions for random access ranges ---------------------

inline constexpr detail::iter_compare_fn_t iter_compare{};

inline constexpr detail::iter_offset_fn_t iter_offset{};

} // namespace ok

#endif
