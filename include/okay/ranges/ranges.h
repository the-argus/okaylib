#ifndef __OKAYLIB_RANGES_RANGES_H__
#define __OKAYLIB_RANGES_RANGES_H__

#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_complete.h"
#include "okay/detail/traits/is_container.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/mathop_traits.h"
#include "okay/math/ordering.h"
// provide opt range definition here to avoid circular includes
#include "okay/opt.h"

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

template <typename viewed_t> class slice;

template <typename range_t, typename enable = void> struct range_definition
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
struct range_definition<
    input_range_t,
    std::enable_if_t<std::is_array_v<detail::remove_cvref_t<input_range_t>>>>
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

    static constexpr value_type* data(range_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }

    static constexpr const value_type* data(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }
};

// specialization for things that can be indexed with size_t and have an
// range_t::value_type member and a size() and data() function (stuff like
// std::array, std::span, and std::vector)
template <typename input_range_t>
struct range_definition<
    input_range_t,
    std::enable_if_t<
        // not array
        !std::is_array_v<detail::remove_cvref_t<input_range_t>> &&
        // not specified to inherit some other range definition
        !has_inherited_range_type_v<detail::remove_cvref_t<input_range_t>> &&
        // provides size_t .size() method and pointer data() method
        detail::is_container_v<input_range_t> &&
        // iterator_t()[size_t{}] -> iterator_t::value_type&
        std::is_same_v<
            typename detail::remove_cvref_t<input_range_t>::value_type&,
            decltype(std::declval<detail::remove_cvref_t<input_range_t>&>()
                         [std::declval<size_t>()])>>>
{
  private:
    using range_t = detail::remove_cvref_t<input_range_t>;

  public:
    // value type for ranges is different than value type for vector or array.
    // a range over `const int` has a value_type of `int`, and jut only provides
    // `const int& get_ref(const range_t&)`
    using value_type = detail::remove_cvref_t<typename range_t::value_type>;
    static constexpr bool is_arraylike = true;

    // discrepancy between slice and std::array value_type, which may be const
    // qualified. If operator[] returns a const reference, then we shouldn't
    // enable a nonconst get_ref.
    template <typename T = range_t>
    static constexpr std::enable_if_t<
        std::is_same_v<T, range_t> &&
            std::is_same_v<value_type&, decltype(std::declval<range_t&>()
                                                     [std::declval<size_t>()])>,
        value_type&>
    get_ref(range_t& i, size_t c) OKAYLIB_NOEXCEPT
    {
        // slices have bounds checking already
        if constexpr (!detail::is_instance_v<range_t, ok::slice>) {
            if (c >= i.size()) [[unlikely]] {
                __ok_abort("Out of bounds access into arraylike container.");
            }
        }
        return i[c];
    }

    static constexpr const value_type& get_ref(const range_t& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        // slices have bounds checking already
        if constexpr (!detail::is_instance_v<range_t, ok::slice>) {
            if (c >= i.size()) [[unlikely]] {
                __ok_abort("Out of bounds access into arraylike container.");
            }
        }
        return i[c];
    }

    static constexpr size_t size(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }

    static constexpr value_type* data(range_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }

    static constexpr const value_type* data(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
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
                      is_derived_from_v<T, inherited_id>,
                  "inherited_range_type is not a base class of the range "
                  "for which it is specified.");
};

template <typename T>
using range_definition_inner =
    typename range_definition_inner_meta_t<remove_cvref_t<T>>::type;

template <typename T, typename = void>
struct range_is_arraylike_inner : std::false_type
{};

template <typename T>
struct range_is_arraylike_inner<
    T, std::enable_if_t<range_definition_inner<T>::is_arraylike>>
    : std::true_type
{};

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
    T, std::void_t<decltype(range_definition_inner<T>::begin(
           std::declval<const remove_cvref_t<T>&>()))>>
{
    using type = decltype(range_definition_inner<T>::begin(
        std::declval<const remove_cvref_t<T>&>()));
};

template <typename T>
using cursor_type_unchecked_for =
    std::conditional_t<range_is_arraylike_inner<T>::value, size_t,
                       typename range_begin_rettype_or_void<T>::type>;

template <typename T, typename = void>
struct range_definition_has_begin : public std::false_type
{};
template <typename T>
struct range_definition_has_begin<
    T, std::enable_if_t<!std::is_same_v<
           void, typename range_begin_rettype_or_void<T>::type>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_definition_has_is_inbounds : public std::false_type
{};
template <typename T>
struct range_definition_has_is_inbounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(range_definition_inner<T>::is_inbounds(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_definition_has_increment : public std::false_type
{};
template <typename T>
struct range_definition_has_increment<
    T, std::enable_if_t<std::is_same_v<
           void, decltype(range_definition_inner<T>::increment(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_definition_has_decrement : public std::false_type
{};
template <typename T>
struct range_definition_has_decrement<
    T, std::enable_if_t<std::is_same_v<
           void, decltype(range_definition_inner<T>::decrement(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_definition_has_offset : public std::false_type
{};
template <typename T>
struct range_definition_has_offset<
    T, std::enable_if_t<std::is_same_v<
           void, decltype(range_definition_inner<T>::offset(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<cursor_type_unchecked_for<T>&>(),
                     std::declval<int64_t>()))>>> : public std::true_type
{};

template <typename T, typename = void>
struct range_definition_has_compare : public std::false_type
{};
template <typename T>
struct range_definition_has_compare<
    T, std::enable_if_t<std::is_same_v<
           ok::ordering,
           decltype(range_definition_inner<T>::compare(
               std::declval<const remove_cvref_t<T>&>(),
               std::declval<const cursor_type_unchecked_for<T>&>(),
               std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_definition_has_size : public std::false_type
{};
template <typename T>
struct range_definition_has_size<
    T, std::enable_if_t<std::is_same_v<
           size_t, decltype(range_definition_inner<T>::size(
                       std::declval<const remove_cvref_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_marked_infinite : public std::false_type
{};
template <typename T>
struct range_marked_infinite<
    T, std::enable_if_t<range_definition_inner<T>::infinite>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_marked_finite : public std::false_type
{};
template <typename T>
struct range_marked_finite<
    T, std::enable_if_t<!range_definition_inner<T>::infinite>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_disallows_cursor_member_offset : public std::false_type
{};
template <typename T>
struct range_disallows_cursor_member_offset<
    T,
    std::enable_if_t<range_definition_inner<T>::disallow_cursor_member_offset>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_has_data : public std::false_type
{};
template <typename T>
struct range_has_data<
    T,
    std::enable_if_t<std::is_pointer_v<decltype(range_definition_inner<T>::data(
        std::declval<const T&>()))>>> : public std::true_type
{};

template <typename T, typename = void>
struct range_is_arraylike : std::false_type
{};

template <typename T>
struct range_is_arraylike<T,
                          std::enable_if_t<range_is_arraylike_inner<T>::value>>
    : std::true_type
{
    // NOTE: an "arraylike" thing is a sized random access range whose begin()
    // function always returns zero. This means also that `cursor < size()` is
    // a valid bounds check. This is used to implement view optimizations, like
    // using the cursor as an index in `enumerate` and using only one cursor for
    // all views in zip.
    // "arraylike" does not specify which of set(), get(), or get_ref() the
    // range defines.
    static_assert(
        !range_definition_has_begin<T>::value,
        "Range marked arraylike, but it has a begin() function defined. "
        "Arraylike ranges must not define a begin() function- ok::begin() on "
        "an arraylike will always return zero.");
    static_assert(
        range_definition_has_size<T>::value,
        "Range marked arraylike, but its range definition does not define a "
        "`size()` function to determine its size in constant time.");
    static_assert(
        !range_definition_has_is_inbounds<T>::value,
        "Range marked arraylike, but it defines is_inbounds . An arraylike "
        "type must be able to be boundschecked with the provided arraylike "
        "function, which checks if the cursor is less than the array's size.");
    static_assert(
        !range_definition_has_increment<T>::value &&
            !range_definition_has_decrement<T>::value &&
            !range_definition_has_offset<T>::value,
        "Range marked arraylike, but it defines increment / "
        "decrement / offset. An arraylike type must use a size_t for its "
        "cursor and only the existing operator++ and operator+= etc.");
};

template <typename T>
constexpr bool range_is_arraylike_v = range_is_arraylike<T>::value;
template <typename T>
constexpr bool range_definition_has_begin_v =
    range_definition_has_begin<T>::value;
template <typename T>
constexpr bool range_can_begin_v =
    range_definition_has_begin_v<T> || range_is_arraylike_v<T>;
template <typename T>
constexpr bool range_definition_has_is_inbounds_v =
    range_definition_has_is_inbounds<T>::value;
template <typename T>
constexpr bool range_can_is_inbounds_v =
    range_definition_has_is_inbounds_v<T> || range_is_arraylike_v<T>;
template <typename T>
constexpr bool range_definition_has_offset_v =
    range_definition_has_offset<T>::value;
template <typename T>
constexpr bool range_definition_has_compare_v =
    range_definition_has_compare<T>::value;
template <typename T>
constexpr bool range_definition_has_increment_v =
    range_definition_has_increment<T>::value;
template <typename T>
constexpr bool range_definition_has_decrement_v =
    range_definition_has_decrement<T>::value;
template <typename T>
constexpr bool range_definition_has_size_v =
    range_definition_has_size<T>::value;
// alias in case indirection is added later and (having size def) != (is sized)
template <typename T>
constexpr bool range_can_size_v = range_definition_has_size_v<T>;
template <typename T>
constexpr bool range_marked_infinite_v = range_marked_infinite<T>::value;
template <typename T>
constexpr bool range_marked_finite_v = range_marked_finite<T>::value;
template <typename T>
constexpr bool range_disallows_cursor_member_offset_v =
    range_disallows_cursor_member_offset<T>::value;

template <typename T, typename = void>
struct has_range_definition : std::true_type
{};
template <typename T>
struct has_range_definition<
    T, std::enable_if_t<range_definition_inner<T>::deleted>> : std::false_type
{};

template <typename range_t, typename = void>
struct range_has_get_unchecked_rettype : public std::false_type
{
    using deduced_value_type = void;
    using return_type = void;
};

template <typename range_t>
struct range_has_get_unchecked_rettype<
    range_t,
    std::enable_if_t<!std::is_same_v<
        decltype(range_definition_inner<range_t>::get(
            std::declval<const range_t&>(),
            std::declval<const cursor_type_unchecked_for<range_t>&>())),
        void>>> : public std::true_type
{
    using return_type = decltype(range_definition_inner<range_t>::get(
        std::declval<const range_t&>(),
        std::declval<const cursor_type_unchecked_for<range_t>&>()));

    // get() returns value_type
    using deduced_value_type = return_type;
};

template <typename range_t, typename = void>
struct range_has_get_ref_unchecked_rettype : public std::false_type
{
    using deduced_value_type = void;
    using return_type = void;
};

template <typename range_t>
struct range_has_get_ref_unchecked_rettype<
    range_t,
    std::enable_if_t<std::is_same_v<
        std::void_t<decltype(range_definition_inner<range_t>::get_ref(
            std::declval<range_t&>(),
            std::declval<const cursor_type_unchecked_for<range_t>&>()))>,
        void>>> : public std::true_type
{
    using return_type = decltype(range_definition_inner<range_t>::get_ref(
        std::declval<range_t&>(),
        std::declval<const cursor_type_unchecked_for<range_t>&>()));
    // NOTE: remove_cvref_t is used here because the result may be const if we
    // hit the const reference overload. value_type is never const.
    using deduced_value_type = remove_cvref_t<return_type>;
};

template <typename range_t, typename = void>
struct range_has_get_ref_const_unchecked_rettype : public std::false_type
{
    using deduced_value_type = void;
    using return_type = void;
};

template <typename range_t>
struct range_has_get_ref_const_unchecked_rettype<
    range_t, std::void_t<decltype(range_definition_inner<range_t>::get_ref(
                 std::declval<const range_t&>(),
                 std::declval<const cursor_type_unchecked_for<range_t>&>()))>>
    : public std::true_type
{
    using return_type = decltype(range_definition_inner<range_t>::get_ref(
        std::declval<const range_t&>(),
        std::declval<const cursor_type_unchecked_for<range_t>&>()));
    using deduced_value_type = remove_cvref_t<return_type>;
};

template <typename range_t>
constexpr bool range_has_get_unchecked_rettype_v =
    range_has_get_unchecked_rettype<range_t>::value;
template <typename range_t>
constexpr bool range_has_get_ref_unchecked_rettype_v =
    range_has_get_ref_unchecked_rettype<range_t>::value;
template <typename range_t>
constexpr bool range_has_get_ref_const_unchecked_rettype_v =
    range_has_get_ref_const_unchecked_rettype<range_t>::value;

template <typename T, typename = void>
struct has_value_type : public std::false_type
{};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>>
    : public std::true_type
{
    using type = typename T::value_type;
};

/// Try to find the value type of an range, either from its
/// range_definition's value_type member type, or by deducing it from the
/// return values of get, get_ref_const, and get_ref (in that order). If unable
/// to deduce, the type is void.
template <typename range_t, typename = void> struct range_deduced_value_type
{
    using type = std::conditional_t<
        range_has_get_unchecked_rettype_v<range_t>,
        typename range_has_get_unchecked_rettype<range_t>::deduced_value_type,
        std::conditional_t<
            range_has_get_ref_const_unchecked_rettype_v<range_t>,
            typename range_has_get_ref_const_unchecked_rettype<
                range_t>::deduced_value_type,
            std::conditional_t<range_has_get_ref_unchecked_rettype_v<range_t>,
                               typename range_has_get_ref_unchecked_rettype<
                                   range_t>::deduced_value_type,
                               void>>>;
};

template <typename range_t>
struct range_deduced_value_type<
    range_t, std::void_t<typename range_definition_inner<range_t>::value_type>>
{
    using type = typename range_definition_inner<range_t>::value_type;
};

template <typename range_t>
using range_deduced_value_type_t =
    typename range_deduced_value_type<range_t>::type;

template <typename... construction_args_t> struct range_has_set
{
    static_assert(sizeof...(construction_args_t) != 0,
                  "No construction args passed to range_has_set<...>");
    template <typename range_t, typename = void>
    struct inner : public std::false_type
    {};

    template <typename range_t>
    struct inner<
        range_t,
        std::enable_if_t<std::is_same_v<
            decltype(range_definition_inner<range_t>::set(
                std::declval<range_t&>(),
                std::declval<const cursor_type_unchecked_for<range_t>&>(),
                std::declval<construction_args_t>()...)),
            void>>> : std::true_type
    {};
};

template <typename T, typename = void>
struct range_has_move_construction_set : public std::false_type
{};

template <typename T>
struct range_has_move_construction_set<
    T, std::enable_if_t<range_has_set<
           range_deduced_value_type_t<T>&&>::template inner<T>::value>>
    : public std::true_type
{};

template <typename range_t, typename... construction_args_t>
constexpr bool range_has_construction_set_v =
    range_has_set<construction_args_t...>::template inner<range_t>::value;

template <typename range_t>
constexpr bool range_has_move_construction_set_v =
    range_has_move_construction_set<range_t>::value;

template <typename T>
constexpr bool range_has_get_v =
    detail::range_has_get_unchecked_rettype_v<T> &&
    std::is_same_v<
        typename detail::range_has_get_unchecked_rettype<T>::return_type,
        detail::range_deduced_value_type_t<T>>;

template <typename T>
constexpr bool range_has_get_ref_v =
    detail::range_has_get_ref_unchecked_rettype_v<T> &&
    // must not return const- this range_has_get_ref is to check if it
    // explicitly has a nonconst overload for get_ref, not just any overload.
    !std::is_const_v<std::remove_reference_t<
        typename detail::range_has_get_ref_unchecked_rettype<
            T>::return_type>> &&
    std::is_same_v<typename detail::range_has_get_ref_unchecked_rettype<
                       T>::deduced_value_type,
                   detail::range_deduced_value_type_t<T>>;

// const_ref_or_void: avoid forming a const reference to void
template <typename T, typename = void> struct const_ref_or_void
{
    using type = void;
};
template <typename T> struct const_ref_or_void<T, std::void_t<const T&>>
{
    using type = const T&;
};

template <typename T>
constexpr bool range_has_get_ref_const_v =
    detail::range_has_get_ref_const_unchecked_rettype_v<T> &&
    std::is_same_v<typename detail::range_has_get_ref_const_unchecked_rettype<
                       T>::return_type,
                   typename const_ref_or_void<
                       detail::range_deduced_value_type_t<T>>::type>;

template <typename T>
constexpr bool has_baseline_functions_v =
    (range_marked_finite_v<T> || range_marked_infinite_v<T> ||
     range_can_size_v<T>) &&
    range_can_is_inbounds_v<T> && range_can_begin_v<T>;

template <typename T, typename = void> struct cursor_or_void
{
    using type = void;
};

template <typename T>
struct cursor_or_void<T, std::void_t<cursor_type_unchecked_for<T>>>
{
    using type = cursor_type_unchecked_for<T>;
};

template <typename T> using cursor_or_void_t = typename cursor_or_void<T>::type;

template <typename T>
constexpr bool range_can_offset_v =
    range_definition_has_offset_v<T> ||
    (!range_disallows_cursor_member_offset_v<T> &&
     has_inplace_addition_with_i64_v<cursor_or_void_t<T>>);

template <typename T>
constexpr bool range_can_compare_v =
    range_definition_has_compare_v<T> || is_orderable_v<cursor_or_void_t<T>>;

template <typename T>
constexpr bool range_can_increment_v =
    range_definition_has_increment_v<T> ||
    has_pre_increment_v<cursor_or_void_t<T>> || range_can_offset_v<T>;

template <typename T>
constexpr bool range_can_decrement_v =
    range_definition_has_decrement_v<T> ||
    has_pre_decrement_v<cursor_or_void_t<T>> || range_can_offset_v<T>;

template <typename T>
constexpr bool is_output_range_v =
    has_baseline_functions_v<T> && range_can_increment_v<T> &&
    (range_has_move_construction_set_v<T> || range_has_get_ref_v<T>);

template <typename T>
constexpr bool is_input_range_v =
    has_baseline_functions_v<T> && range_can_increment_v<T> &&
    (range_has_get_v<T> || range_has_get_ref_const_v<T> ||
     range_has_get_ref_v<T>);

template <typename T>
constexpr bool is_input_or_output_range_v =
    is_input_range_v<T> || is_output_range_v<T>;

// single pass ranges are by definition the minimum range- so any valid
// input or output range is also a single pass range.
template <typename maybe_range_t>
constexpr bool is_single_pass_range_v =
    is_input_or_output_range_v<maybe_range_t>;

// a multi pass range is copyable- enabling consumers to pause and come back
// to copies of ranges at previous positions.
template <typename maybe_range_t>
constexpr bool is_multi_pass_range_v =
    is_single_pass_range_v<maybe_range_t> &&
    std::is_copy_constructible_v<cursor_or_void_t<maybe_range_t>> &&
    std::is_copy_assignable_v<cursor_or_void_t<maybe_range_t>>;

template <typename maybe_range_t>
constexpr bool is_bidirectional_range_v =
    is_multi_pass_range_v<maybe_range_t> &&
    // can also be decremented
    range_can_decrement_v<maybe_range_t>;

template <typename maybe_range_t>
constexpr bool is_random_access_range_v =
    is_bidirectional_range_v<maybe_range_t> &&
    // must be able to offset cursor by arbitrary i64
    range_can_offset_v<maybe_range_t> &&
    // must be able to compre two cursors
    range_can_compare_v<maybe_range_t>;
} // namespace detail

/// range_def_for is a way of accessing the range definition for a type, with
/// some additional verification that the definition being accessed is actually
/// valid.
template <typename T>
class range_def_for : public detail::range_definition_inner<T>
{
  public:
    using value_type = typename detail::range_deduced_value_type<T>::type;

    // should never fire unless something is broken with ranges implementation.
    // value_type is never const- if your value type is "const", then you just
    // only provide the const get_ref overload.
    static_assert(!std::is_reference_v<value_type> &&
                  !std::is_const_v<value_type>);

  private:
    using noref = detail::remove_cvref_t<T>;
    static constexpr bool complete = detail::is_complete_v<noref>;

    static_assert(complete,
                  "Refusing to access range definition for incomplete type.");

    static constexpr bool not_deleted =
        detail::has_range_definition<noref>::value;
    static_assert(not_deleted,
                  "Attempt to access range information for a type whose "
                  "range definition is marked deleted.");

    static constexpr bool has_begin = detail::range_can_begin_v<noref>;
    static_assert(has_begin, "Range definition invalid- provide a begin(const "
                             "range_t&) function which returns a cursor type.");

    static constexpr bool has_inbounds = detail::range_can_is_inbounds_v<noref>;
    static_assert(has_inbounds,
                  "Range definition invalid- `is_inbounds()` not provided.");

    static constexpr bool has_size = detail::range_can_size_v<noref>;
    static constexpr bool marked_infinite =
        detail::range_marked_infinite_v<noref>;
    static constexpr bool marked_finite = detail::range_marked_finite_v<noref>;
    static_assert(
        has_size || marked_infinite || marked_finite,
        "Range definition invalid- provide a size_t size(const range_t&) "
        "method, or add a `static constexpr bool infinite = ...` which should "
        "be true if the range is intended to be infinite, and false if the "
        "range is intended to have some finite size that cannot be "
        "calculated in constant time.");
    static_assert(int(has_size) + int(marked_finite) + int(marked_infinite) ==
                      1,
                  "Range definition invalid: specify only one of `static "
                  "bool infinite = ...` or `size()` method.");

    static constexpr bool has_value_type = !std::is_same_v<value_type, void>;
    static_assert(
        has_value_type,
        "Range definition invalid- specify some object type for "
        "value_type, or provide valid `get()`, `get_ref()`, or `get_ref() "
        "const` functions so that the value type can be deduced.");

    static constexpr bool valid_value_type =
        detail::is_valid_value_type_v<value_type>;
    static_assert(valid_value_type,
                  "Range definition invalid- value_type is not an object.");

    // either it doesnt have the function at all, OR the function is good
    static constexpr bool get_function_returns_value_type =
        !detail::range_has_get_unchecked_rettype_v<noref> ||
        detail::range_has_get_v<noref>;

    static_assert(get_function_returns_value_type,
                  "Range definition invalid- `get()` function does not "
                  "return value_type.");

    static constexpr bool get_ref_const_function_returns_value_type =
        !detail::range_has_get_ref_const_unchecked_rettype_v<noref> ||
        detail::range_has_get_ref_const_v<noref>;

    static_assert(get_ref_const_function_returns_value_type,
                  "Range definition invalid- `get_ref() const` function "
                  "does not return `const value_type&`");

    static constexpr bool get_ref_function_returns_value_type =
        // can do get_ref(nonconst &), and it returns value_type with some
        // const or ref qualifiers
        !detail::range_has_get_ref_unchecked_rettype_v<noref> ||
        std::is_same_v<value_type,
                       typename detail::range_has_get_ref_unchecked_rettype<
                           noref>::deduced_value_type>;

    static_assert(get_ref_function_returns_value_type,
                  "Range definition invalid- `get_ref()` function does not "
                  "return `value_type&`");

    static constexpr bool valid_cursor =
        detail::is_valid_cursor_v<detail::cursor_type_unchecked_for<noref>>;
    static_assert(
        valid_cursor,
        "Range definition invalid- the return value from begin() (the "
        "cursor type) is not an object.");

    static constexpr bool can_increment_cursor =
        detail::has_pre_increment_v<detail::cursor_type_unchecked_for<noref>> ||
        detail::range_definition_has_increment_v<noref>;
    static_assert(
        can_increment_cursor,
        "Range definition invalid- given cursor type (the return type of "
        "begin()) is not able to be pre-incremented, nor is an increment(const "
        "range&, cursor&) function defined in the range definition.");

    static constexpr bool input_or_output =
        detail::is_input_or_output_range_v<noref>;
    static_assert(
        input_or_output,
        "Range definition invalid- it does not define any operations that "
        "can be done with the range like get(range, cursor), set(range, "
        "cursor, value_type&&), or get_ref(range, cursor)");

    static constexpr bool ref_or_get_are_mutually_exclusive =
        (detail::range_has_get_v<noref> !=
         (detail::range_has_get_ref_v<noref> ||
          detail::range_has_get_ref_const_v<noref>)) ||
        // its also valid for the range to only define set()
        (!detail::range_has_get_v<noref> &&
         !detail::range_has_get_ref_v<noref> &&
         !detail::range_has_get_ref_const_v<noref>);
    static_assert(
        ref_or_get_are_mutually_exclusive,
        "Range definition invalid- both get() and a get_ref() functions are "
        "defined. There is currently no protocol in okaylib to choose between "
        "these implementations, so remove one of them and use the copy "
        "constructor of value_type if both copyout and get_ref are needed.");
};

namespace detail {
template <typename T, typename value_type, typename = void>
struct is_range : std::false_type
{};
/// Same as the checks performed in `iterator_for`
template <typename T, typename value_type>
struct is_range<
    T, value_type,
    std::enable_if_t<
        is_complete_v<T> && has_range_definition<T>::value &&
        // NOTE: not checking if has value type before doing this: if it
        // doesnt have it, it will be void, which is not a valid value type
        is_valid_value_type_v<value_type> && range_can_begin_v<T> &&
        is_valid_cursor_v<cursor_type_unchecked_for<T>> &&
        range_can_is_inbounds_v<T> &&
        (range_can_size_v<T> || range_marked_infinite_v<T> ||
         range_marked_finite_v<T>) &&
        ((int(range_can_size_v<T>) + int(range_marked_infinite_v<T>) +
          int(range_marked_finite_v<T>)) == 1) &&
        is_input_or_output_range_v<T> &&
        (range_has_get_v<T> !=
         (range_has_get_ref_v<T> || range_has_get_ref_const_v<T>))>>
    : std::true_type
{};
} // namespace detail

/// Check if a type has an range_definition which is valid, enabling a form
/// of either input or output iteration.
template <typename T>
constexpr bool is_range_v =
    detail::is_range<detail::remove_cvref_t<T>,
                     detail::range_deduced_value_type_t<T>>::value;

template <typename T>
using value_type_for = typename range_def_for<T>::value_type;
template <typename T>
using cursor_type_for =
    std::conditional_t<ok::detail::range_is_arraylike_v<T>, size_t,
                       typename detail::range_begin_rettype_or_void<T>::type>;

// ok::opt range_definition. located here to avoid circular includes
template <typename payload_t> struct ok::range_definition<ok::opt<payload_t>>
{
    struct cursor_t
    {
        friend class range_definition;

      private:
        bool is_out_of_bounds = false;
    };

    using opt_range_t = opt<payload_t>;

    static constexpr cursor_t begin(const opt_range_t& range) OKAYLIB_NOEXCEPT
    {
        return cursor_t{};
    }

    static constexpr void increment(const opt_range_t& range,
                                    cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        cursor.is_out_of_bounds = true;
    }

    static constexpr bool is_inbounds(const opt_range_t& range,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return range.has_value() && !cursor.is_out_of_bounds;
    }

    static constexpr size_t size(const opt_range_t& range) OKAYLIB_NOEXCEPT
    {
        return size_t(range.has_value());
    }

    static constexpr auto& get_ref(opt_range_t& range, const cursor_t& cursor)
    {
        return range.ref_or_panic();
    }

    static constexpr const auto& get_ref(const opt_range_t& range,
                                         const cursor_t& cursor)
    {
        return range.ref_or_panic();
    }
};

namespace detail {

// forward declare these so we can make overloads that detect them and have
// static asserts with nice error messages
template <typename callable_t> struct range_adaptor_closure_t;
template <typename callable_t> struct range_adaptor_t;
template <typename callable_t, typename... args_t> struct partial_called_t;

#define __ok_range_function_assert_not_partially_called_member(funcname)    \
    template <typename T, typename... U>                                    \
    constexpr auto operator()(const T&, U&&...)                             \
        const OKAYLIB_NOEXCEPT->std::enable_if_t<                           \
            !is_range_v<T> && (is_instance_v<T, range_adaptor_closure_t> || \
                               is_instance_v<T, range_adaptor_t> ||         \
                               is_instance_v<T, partial_called_t>)>         \
    {                                                                       \
        static_assert(false,                                                \
                      "Attempt to call " #funcname ", but the object "      \
                      "given as range hasn't finished being called. It "    \
                      "may be a view which takes additional arguments.");   \
    }

#define __ok_range_function_assert_correct_cursor_type_member                  \
    template <typename T, typename U, typename... pack>                        \
    constexpr auto operator()(const T&, const U&,                              \
                              pack&&...) const OKAYLIB_NOEXCEPT                \
        ->std::enable_if_t<is_range_v<T> &&                                    \
                           !std::is_convertible_v<                             \
                               const U&, const cursor_type_unchecked_for<T>&>> \
    {                                                                          \
        static_assert(false,                                                   \
                      "Incorrect cursor type passed as second argument.");     \
    }

struct iter_copyout_fn_t
{
    template <typename range_t>
    constexpr auto operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_range_v<range_t>);
        using namespace detail;
        static_assert(
            range_has_get_v<range_t> || range_has_get_ref_const_v<range_t>,
            "Cannot copy out a value from given range- provide a `const& "
            "get_ref(const)` or `get(const)` function in its definition.");

        using def = range_definition_inner<range_t>;
        if constexpr (range_has_get_v<range_t>) {
            return def::get(range, cursor);
        } else {
            return def::get_ref(range, cursor);
        }
    }

    __ok_range_function_assert_not_partially_called_member(iter_copyout)
        __ok_range_function_assert_correct_cursor_type_member
};

template <typename range_t, typename = void>
struct iter_get_temporary_ref_meta_t
{
    using type = void;
    static constexpr void call(...) OKAYLIB_NOEXCEPT
    {
        static_assert(range_has_get_ref_v<range_t> &&
                          !range_has_get_ref_const_v<range_t>,
                      "Cannot call iter_get_temporary_ref- the function takes "
                      "a const reference but only nonconst get_ref is "
                      "implemented for this range.");
        // trigger static asserts in range_def_for- this range is invalid
        std::declval<range_def_for<range_t>>();
    }
};

template <typename range_t>
struct iter_get_temporary_ref_meta_t<
    range_t, std::enable_if_t<is_range_v<range_t> &&
                              !detail::range_has_get_ref_const_v<range_t>>>
{
    using type = value_type_for<range_t>;
    static constexpr type
    call(const range_t& range,
         const cursor_type_for<range_t>& cursor) OKAYLIB_NOEXCEPT
    {
        return iter_copyout_fn_t{}(range, cursor);
    }
};

template <typename range_t>
struct iter_get_temporary_ref_meta_t<
    range_t, std::enable_if_t<is_range_v<range_t> &&
                              detail::range_has_get_ref_const_v<range_t>>>
{
    using type = const value_type_for<range_t>&;
    static constexpr type
    call(const range_t& range,
         const cursor_type_for<range_t>& cursor) OKAYLIB_NOEXCEPT
    {
        return range_definition_inner<range_t>::get_ref(range, cursor);
    }
};

struct iter_get_temporary_ref_fn_t
{
    template <typename range_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        return detail::iter_get_temporary_ref_meta_t<range_t>::call(range,
                                                                    cursor);
        static_assert(is_range_v<range_t>,
                      "Invalid range passed in to iter_get_temporary_ref");
    }

    __ok_range_function_assert_not_partially_called_member(
        iter_get_temporary_ref)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_get_ref_fn_t
{
    template <typename range_t>
    constexpr auto operator()
        [[nodiscard]] (range_t& range, const cursor_type_for<range_t>& cursor)
        const OKAYLIB_NOEXCEPT
            ->decltype(range_definition_inner<range_t>::get_ref(range, cursor))
    {
        using return_type =
            decltype(range_definition_inner<range_t>::get_ref(range, cursor));
        static_assert(
            // if the return type is const, give a special exception to
            // static assert
            std::is_const_v<std::remove_reference_t<return_type>> ||
                range_has_get_ref_v<range_t>,
            "Unable to get_ref- no valid get_ref defined for this range.");
        return range_definition_inner<range_t>::get_ref(range, cursor);
    }

    template <typename range_t>
    constexpr const value_type_for<range_t>& operator() [[nodiscard]] (
        const range_t& range,
        const cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        static_assert(
            detail::range_has_get_ref_const_v<range_t>,
            "Unable to get_ref- no const get_ref defined for this range.");
        return range_definition_inner<range_t>::get_ref(range, cursor);
    }

    __ok_range_function_assert_not_partially_called_member(iter_get_ref)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_set_fn_t
{
    template <typename range_t, typename... construction_args_t>
    constexpr void
    operator()(range_t& range, const cursor_type_for<range_t>& cursor,
               construction_args_t&&... args) const OKAYLIB_NOEXCEPT
    {
        if constexpr (detail::range_has_get_ref_v<range_t>) {
            auto& ref = range_definition_inner<range_t>::get_ref(range, cursor);

            // if you supplied only one arg, and it is assignable, and the range
            // uses get_ref, then set performs assignment. otherwise, it
            // manually calls destruction and then constructs in place
            if constexpr (sizeof...(construction_args_t) == 1) {
                using first_type =
                    detail::first_type_in_pack_t<construction_args_t...>;
                if constexpr (std::is_assignable_v<decltype(ref), first_type>) {
                    const auto applyer = [&ref](auto&& a) {
                        ref = std::forward<first_type>(a);
                    };
                    applyer(std::forward<construction_args_t>(args)...);
                } else {
                    // duplicated code with outer else case here
                    ref.~value_type_for<range_t>();
                    new (ok::addressof(ref)) value_type_for<range_t>(
                        std::forward<construction_args_t>(args)...);
                }
            } else {
                // destroy whats there
                ref.~value_type_for<range_t>();

                new (ok::addressof(ref)) value_type_for<range_t>(
                    std::forward<construction_args_t>(args)...);
            }
        } else {
            static_assert(
                detail::range_has_construction_set_v<range_t,
                                                     construction_args_t...>,
                "Cannot set for given range- it does not define iter_set which "
                "takes the given arguments, nor "
                "does it define get_ref + a move constructor for value type.");
            range_definition_inner<range_t>::set(
                range, cursor, std::forward<construction_args_t>(args)...);
        }
    }

    __ok_range_function_assert_not_partially_called_member(iter_set)
        __ok_range_function_assert_correct_cursor_type_member
};

struct begin_fn_t
{
    template <typename range_t>
    constexpr auto operator() [[nodiscard]] (const range_t& range) const
        OKAYLIB_NOEXCEPT->std::enable_if_t<range_can_begin_v<range_t>,
                                           cursor_type_unchecked_for<range_t>>
    {
        if constexpr (range_is_arraylike_v<range_t>) {
            return size_t(0);
        } else {
            return range_def_for<range_t>::begin(range);
        }
    }

    __ok_range_function_assert_not_partially_called_member(begin)
};

struct increment_fn_t
{
    template <typename range_t>
    constexpr void
    operator()(const range_t& range,
               cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        if constexpr (detail::range_definition_has_increment_v<range_t>) {
            range_def_for<range_t>::increment(range, cursor);
        } else if constexpr (detail::range_definition_has_offset_v<range_t>) {
            range_def_for<range_t>::offset(range, cursor, int64_t(1));
        } else {
            static_assert(detail::has_pre_increment_v<
                          detail::cursor_type_unchecked_for<range_t>>);
            ++cursor;
        }
    }

    __ok_range_function_assert_not_partially_called_member(increment)
        __ok_range_function_assert_correct_cursor_type_member
};

struct decrement_fn_t
{
    template <typename range_t>
    constexpr void
    operator()(const range_t& range,
               cursor_type_for<range_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        if constexpr (detail::range_definition_has_increment_v<range_t>) {
            range_def_for<range_t>::decrement(range, cursor);
        } else if constexpr (detail::range_can_offset_v<range_t>) {
            range_def_for<range_t>::offset(range, cursor, int64_t(-1));
        } else {
            static_assert(detail::has_pre_increment_v<
                          detail::cursor_type_unchecked_for<range_t>>);
            --cursor;
        }
    }

    __ok_range_function_assert_not_partially_called_member(decrement)
        __ok_range_function_assert_correct_cursor_type_member
};

struct iter_compare_fn_t
{
    template <typename range_t>
    constexpr ordering
    operator()(const range_t& range, const cursor_type_for<range_t>& cursor_a,
               const cursor_type_for<range_t>& cursor_b) const OKAYLIB_NOEXCEPT
    {
        static_assert(
            range_can_compare_v<range_t>,
            "Cannot call iter_compare on a range which does not allow "
            "comparing cursors.");
        if constexpr (detail::range_definition_has_compare_v<range_t>) {
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
    template <typename range_t>
    constexpr ordering operator()(const range_t& range,
                                  cursor_type_for<range_t>& cursor,
                                  int64_t offset) const OKAYLIB_NOEXCEPT
    {
        static_assert(range_can_offset_v<range_t>,
                      "Cannot call iter_offset on a range which does not allow "
                      "offsetting cursors.");
        if constexpr (detail::range_definition_has_offset_v<range_t>) {
            return range_def_for<range_t>::offset(range, cursor, offset);
        } else {
            static_assert(has_inplace_addition_with_i64_v<range_t>);
            cursor += offset;
        }
    }

    __ok_range_function_assert_not_partially_called_member(iter_offset)
        __ok_range_function_assert_correct_cursor_type_member
};

struct is_inbounds_fn_t
{
    template <typename range_t>
    constexpr bool operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor) const
    {
        if constexpr (range_definition_has_is_inbounds_v<range_t>) {
            return range_def_for<range_t>::is_inbounds(range, cursor);
        } else {
            static_assert(range_is_arraylike_v<range_t>,
                          "Range doesnt have an `is_inbounds()` function, and "
                          "it's not arraylike (which would be the only valid "
                          "reason not to have one).");
            return cursor < range_def_for<range_t>::size(range);
        }
    }

    __ok_range_function_assert_not_partially_called_member(is_inbounds)
        __ok_range_function_assert_correct_cursor_type_member
};

struct size_fn_t
{
    template <typename range_t>
    constexpr auto operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
            ->std::enable_if_t<!range_marked_finite_v<range_t> &&
                                   !range_marked_infinite_v<range_t>,
                               decltype(range_def_for<range_t>::size(range))>
    {
        return range_def_for<range_t>::size(range);
    }

    template <typename range_t>
    constexpr auto operator() [[nodiscard]] (const range_t& range) const
        OKAYLIB_NOEXCEPT->std::enable_if_t<range_marked_finite_v<range_t>, void>
    {
        static_assert(false, "Attempt to get the size of a range whose size "
                             "can't be calculated in constant time.");
    }

    template <typename range_t>
    constexpr auto operator()
        [[nodiscard]] (const range_t& range) const OKAYLIB_NOEXCEPT
            ->std::enable_if_t<range_marked_infinite_v<range_t>, void>
    {
        static_assert(false,
                      "Attempt to get the size of an infinitely large range");
    }

    __ok_range_function_assert_not_partially_called_member(size)
};

struct data_fn_t
{
    template <typename range_t>
    constexpr auto operator() [[nodiscard]] (const range_t& range) const
        OKAYLIB_NOEXCEPT->decltype(range_def_for<range_t>::data(range))
    {
        return range_def_for<range_t>::data(range);
    }

    __ok_range_function_assert_not_partially_called_member(data)
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

inline constexpr detail::size_fn_t data{};

inline constexpr detail::increment_fn_t increment{};

inline constexpr detail::decrement_fn_t decrement{};

// functions for random access ranges ---------------------

inline constexpr detail::iter_compare_fn_t iter_compare{};

inline constexpr detail::iter_offset_fn_t iter_offset{};

} // namespace ok

#endif
