#ifndef __OKAYLIB_RANGES_RANGES_H__
#define __OKAYLIB_RANGES_RANGES_H__

#include "okay/detail/ok_assert.h"
#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_complete.h"
#include "okay/detail/traits/is_container.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/mathop_traits.h"
#include "okay/math/ordering.h"
#include <type_traits>

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
 * `void iter_set(range_t&, const cursor_t&)`
 * `begin()`
 * `is_inbounds()`
 */

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

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

    static constexpr bool less_than_end_cursor_is_valid_boundscheck = true;

    static constexpr value_type& get_ref(range_t& i, size_t c) OKAYLIB_NOEXCEPT
    {
        __ok_assert(c < size(i)); // out of bounds on c style array
        return i[c];
    }

    static constexpr const value_type& get_ref(const range_t& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        __ok_assert(c < size(i)); // out of bounds on c style array
        return i[c];
    }

    static constexpr size_t begin(const range_t&) OKAYLIB_NOEXCEPT { return 0; }

    static constexpr bool is_inbounds(const range_t& i,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < size(i);
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
    using value_type = typename range_t::value_type;
    // specify this to allow the case where `value_type` is const to compile,
    // despite `get_ref` incorrectly returning a `const type&`. (ignores get_ref
    // as a range function and supresses the errors)
    static constexpr bool allow_get_ref_return_const_ref = true;
    // optimization: views do not have to check if cursor < begin
    static constexpr bool less_than_end_cursor_is_valid_boundscheck = true;

    static constexpr value_type& get_ref(range_t& i, size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr const value_type& get_ref(const range_t& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr size_t begin(const range_t& i) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const range_t& i,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < size(i);
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

template <typename T>
constexpr bool is_valid_cursor_v =
    // NOTE: update static_assert in range_def_for if this changes
    std::is_object_v<T>;

template <typename T>
constexpr bool is_valid_value_type_v = std::is_object_v<T>;

template <typename T>
using cursor_type_unchecked_for = decltype(range_definition_inner<T>::begin(
    std::declval<const remove_cvref_t<T>&>()));

template <typename T, typename = void>
struct range_has_begin : public std::false_type
{};
template <typename T>
struct range_has_begin<
    T, std::enable_if_t<!std::is_same_v<
           void, decltype(range_definition_inner<T>::begin(
                     std::declval<const remove_cvref_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_allows_get_ref_return_const_ref : public std::false_type
{};
template <typename T>
struct range_allows_get_ref_return_const_ref<
    T,
    std::enable_if_t<range_definition_inner<T>::allow_get_ref_return_const_ref>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_has_is_inbounds : public std::false_type
{};
template <typename T>
struct range_has_is_inbounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(range_definition_inner<T>::is_inbounds(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_has_is_after_bounds : public std::false_type
{};
template <typename T>
struct range_has_is_after_bounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(range_definition_inner<T>::is_after_bounds(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct range_has_is_before_bounds : public std::false_type
{};
template <typename T>
struct range_has_is_before_bounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(range_definition_inner<T>::is_before_bounds(
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
struct range_has_size : public std::false_type
{};
template <typename T>
struct range_has_size<
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
struct range_has_data : public std::false_type
{};
template <typename T>
struct range_has_data<
    T,
    std::enable_if_t<std::is_pointer_v<decltype(range_definition_inner<T>::data(
        std::declval<const T&>()))>>> : public std::true_type
{};

template <typename T>
constexpr bool range_has_begin_v = range_has_begin<T>::value;
template <typename T>
constexpr bool range_has_is_inbounds_v = range_has_is_inbounds<T>::value;
template <typename T>
constexpr bool range_has_is_after_bounds_v =
    range_has_is_after_bounds<T>::value;
template <typename T>
constexpr bool range_has_is_before_bounds_v =
    range_has_is_before_bounds<T>::value;
template <typename T>
constexpr bool range_definition_has_increment_v =
    range_definition_has_increment<T>::value;
template <typename T>
constexpr bool range_definition_has_decrement_v =
    range_definition_has_decrement<T>::value;
template <typename T>
constexpr bool range_has_size_v = range_has_size<T>::value;
template <typename T>
constexpr bool range_marked_infinite_v = range_marked_infinite<T>::value;
template <typename T>
constexpr bool range_marked_finite_v = range_marked_finite<T>::value;

template <typename T, typename = void>
struct range_can_boundscheck_with_less_than_end_cursor : public std::false_type
{};
template <typename T>
struct range_can_boundscheck_with_less_than_end_cursor<
    T, std::enable_if_t<range_definition_inner<
           T>::less_than_end_cursor_is_valid_boundscheck>>
    : public std::true_type
{
    static_assert(
        !range_has_is_before_bounds_v<T>,
        "Range marked as only needing on boundscheck operation (cursor < "
        "size()) but it also has a `is_before_bounds()` function "
        "defined. Just define an `is_inbounds()` function instead.");
};

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
    std::enable_if_t<
        std::is_same_v<
            std::void_t<decltype(range_definition_inner<range_t>::get_ref(
                std::declval<range_t&>(),
                std::declval<const cursor_type_unchecked_for<range_t>&>()))>,
            void>
        // most complex boolean logic ever written...
        // This function only checks if the object has a get_ref function, it
        // does not check the return type. UNLESS the return type is
        // incorrectly const& AND allow_get_ref_return_const_ref is defined. In
        // this case we ignore the get_ref function altogether.
        && (!range_allows_get_ref_return_const_ref<range_t>::value ||
            (range_allows_get_ref_return_const_ref<range_t>::value &&
             // allow_get_ref_return_const_ref is set, so do the constness check
             // here to avoid static_asserts later down the line
             !std::is_const_v<std::remove_reference_t<
                 decltype(range_definition_inner<range_t>::get_ref(
                     std::declval<range_t&>(),
                     std::declval<
                         const cursor_type_unchecked_for<range_t>&>()))>>))>>
    : public std::true_type
{
    using return_type = decltype(range_definition_inner<range_t>::get_ref(
        std::declval<range_t&>(),
        std::declval<const cursor_type_unchecked_for<range_t>&>()));
    using deduced_value_type = std::remove_reference_t<return_type>;
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

template <typename range_t, typename = void>
struct range_has_set : public std::false_type
{};

template <typename range_t>
struct range_has_set<
    range_t, std::enable_if_t<std::is_same_v<
                 decltype(range_definition_inner<range_t>::set(
                     std::declval<range_t&>(),
                     std::declval<const cursor_type_unchecked_for<range_t>&>(),
                     std::declval<range_deduced_value_type_t<range_t>&&>())),
                 void>>> : std::true_type
{};

template <typename range_t>
constexpr bool range_has_set_v = range_has_set<range_t>::value;

template <typename T>
constexpr bool range_has_get_v =
    detail::range_has_get_unchecked_rettype_v<T> &&
    std::is_same_v<
        typename detail::range_has_get_unchecked_rettype<T>::return_type,
        detail::range_deduced_value_type_t<T>>;

template <typename T>
constexpr bool range_has_get_ref_v =
    detail::range_has_get_ref_unchecked_rettype_v<T> &&
    // additional check to make sure returned value is not const
    !std::is_const_v<detail::range_deduced_value_type_t<T>> &&
    std::is_same_v<
        typename detail::range_has_get_ref_unchecked_rettype<T>::return_type,
        detail::range_deduced_value_type_t<T>&>;

template <typename T>
constexpr bool range_has_get_ref_const_v =
    detail::range_has_get_ref_const_unchecked_rettype_v<T> &&
    std::is_same_v<typename detail::range_has_get_ref_const_unchecked_rettype<
                       T>::return_type,
                   const detail::range_deduced_value_type_t<T>&>;

template <typename T>
constexpr bool has_baseline_functions_v =
    (range_marked_finite_v<T> || range_marked_infinite_v<T> ||
     range_has_size_v<T>) &&
    // either provides an is_inbounds, XOR it provides after & before bounds
    // checks
    (range_has_is_inbounds_v<T> !=
     (range_has_is_after_bounds_v<T> && range_has_is_before_bounds_v<T>)) &&
    // also make sure you dont have is_inbounds and one of after/before also
    // defined
    (range_has_is_after_bounds_v<T> == range_has_is_before_bounds_v<T>) &&
    range_has_begin_v<T>;

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
constexpr bool range_can_increment_v = range_definition_has_increment_v<T> ||
                                       has_pre_increment_v<cursor_or_void_t<T>>;

template <typename T>
constexpr bool range_can_decrement_v = range_definition_has_decrement_v<T> ||
                                       has_pre_decrement_v<cursor_or_void_t<T>>;

template <typename T>
constexpr bool is_output_range_v =
    has_baseline_functions_v<T> && range_can_increment_v<T> &&
    (range_has_set_v<T> || range_has_get_ref_v<T>);

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
    // must be able to do math on cursor without range context
    has_pre_increment_v<cursor_or_void_t<maybe_range_t>> &&
    has_pre_decrement_v<cursor_or_void_t<maybe_range_t>> &&
    // if user defines increment(), then that method is preferred so random
    // access is lost- increment with range context is now required
    !range_definition_has_increment_v<maybe_range_t> &&
    !range_definition_has_decrement_v<maybe_range_t> &&
    // must also be able to do +, +=, -, -=, <, >, <=, >=
    has_addition_with_size_v<cursor_or_void_t<maybe_range_t>> &&
    has_subtraction_with_size_v<cursor_or_void_t<maybe_range_t>> &&
    has_inplace_addition_with_size_v<cursor_or_void_t<maybe_range_t>> &&
    has_inplace_subtraction_with_size_v<cursor_or_void_t<maybe_range_t>> &&
    is_equality_comparable_to_v<cursor_or_void_t<maybe_range_t>,
                                cursor_or_void_t<maybe_range_t>> &&
    has_comparison_operators_v<cursor_or_void_t<maybe_range_t>>;
} // namespace detail

/// range_def_for is a way of accessing the range definition for a type, with
/// some additional verification that the definition being accessed is actually
/// valid.
template <typename T>
class range_def_for : public detail::range_definition_inner<T>
{
  public:
    using value_type = typename detail::range_deduced_value_type<T>::type;

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

    static constexpr bool has_begin = detail::range_has_begin_v<noref>;
    static_assert(has_begin, "Range definition invalid- provide a begin(const "
                             "range_t&) function which returns a cursor type.");

    static constexpr bool has_inbounds = detail::range_has_is_inbounds_v<noref>;
    static constexpr bool has_after_bounds =
        detail::range_has_is_after_bounds_v<noref>;
    static constexpr bool has_before_bounds =
        detail::range_has_is_before_bounds_v<noref>;
    static_assert(has_inbounds || (has_after_bounds && has_before_bounds),
                  "Range definition invalid- No bounds checking functions "
                  "provided. Provide either `is_inbounds()` function or both "
                  "`is_after_bounds` and `is_before_bounds`.");
    static_assert(
        has_inbounds != (has_after_bounds && has_before_bounds),
        "Range definition invalid- do not provide both of `is_inbounds()` "
        "*and* `is_after_bounds() and is_before_bounds()`.");

    static constexpr bool has_size = detail::range_has_size_v<noref>;
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

    static constexpr bool get_ref_function_returns_value_type =
        !detail::range_has_get_ref_unchecked_rettype_v<noref> ||
        detail::range_has_get_ref_v<noref>;

    static_assert(
        !(!get_ref_function_returns_value_type && std::is_const_v<value_type>),
        "Range definition invalid- `get_ref()` specified, and it returns "
        "`value_type&` correctly, but `value_type` is const. If your range "
        "definition is templated, consider adding `static constexpr bool "
        "allow_get_ref_return_const_ref = true` to your range_definition to "
        "ignore the invalid get_ref() altogether, and avoid this error "
        "message.");

    static_assert(get_ref_function_returns_value_type,
                  "Range definition invalid- `get_ref()` function does not "
                  "return `value_type&`");

    static constexpr bool get_ref_const_function_returns_value_type =
        !detail::range_has_get_ref_const_unchecked_rettype_v<noref> ||
        detail::range_has_get_ref_const_v<noref>;

    static_assert(get_ref_const_function_returns_value_type,
                  "Range definition invalid- `get_ref() const` function "
                  "does not return `const value_type&`");

    static constexpr bool valid_cursor =
        detail::is_valid_cursor_v<detail::cursor_type_unchecked_for<noref>>;
    static_assert(
        valid_cursor,
        "Range definition invalid- the return value from begin() (the "
        "cursor type) is not an object.");

    static constexpr bool before_and_after_or_inbounds_are_mutually_exclusive =
        detail::range_has_is_inbounds_v<noref> !=
        (detail::range_has_is_after_bounds_v<noref> &&
         detail::range_has_is_before_bounds_v<noref>);
    static_assert(
        before_and_after_or_inbounds_are_mutually_exclusive,
        "Range definition invalid- define either `is_inbounds()` XOR (both "
        "of `is_after_bounds()` and `is_before_bounds()`), but not both.");
    static constexpr bool before_and_after_are_both_defined_or_not =
        detail::range_has_is_after_bounds_v<noref> ==
        detail::range_has_is_before_bounds_v<noref>;
    static_assert(
        before_and_after_are_both_defined_or_not,
        "Range definition invalid- different status for `is_before_bounds` "
        "and `is_after_bounds`. either define both of `is_before_bounds()` and "
        "`is_after_bounds()`, or define neither.");

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
        "can be done with the range like get(), set(), or get_ref()");

    static constexpr bool ref_or_get_are_mutually_exclusive =
        detail::range_has_get_v<noref> !=
        (detail::range_has_get_ref_v<noref> ||
         detail::range_has_get_ref_const_v<noref>);
    static_assert(
        ref_or_get_are_mutually_exclusive,
        "Range definition invalid- both get() and a get_ref() functions are "
        "defined. There is currently no protocol in okaylib to choose between "
        "these implementations, so remove one of them and use the copy "
        "constructor of value_type if both copyout and get_ref are needed.");

    static constexpr bool valid_boundscheck_optimization_marker =
        !detail::range_can_boundscheck_with_less_than_end_cursor<
            noref>::value ||
        detail::is_random_access_range_v<noref>;
    static_assert(valid_boundscheck_optimization_marker,
                  "Attempt to specify that range can be boundschecked with "
                  "just a `... < size()` operation, but the type is not random "
                  "access so this optimization will be ignored.");
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
        is_valid_value_type_v<value_type> && range_has_begin_v<T> &&
        is_valid_cursor_v<cursor_type_unchecked_for<T>> &&
        (range_has_is_inbounds_v<T> !=
         (range_has_is_after_bounds_v<T> && range_has_is_before_bounds_v<T>)) &&
        range_has_is_after_bounds_v<T> == range_has_is_before_bounds_v<T> &&
        (range_has_size_v<T> || range_marked_infinite_v<T> ||
         range_marked_finite_v<T>) &&
        ((int(range_has_size_v<T>) + int(range_marked_infinite_v<T>) +
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
    decltype(range_def_for<T>::begin(std::declval<const T&>()));

struct prefer_after_bounds_check_t
{};
struct prefer_before_bounds_check_t
{};

namespace detail {
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
};

struct iter_get_ref_fn_t
{
    template <typename range_t>
    constexpr value_type_for<range_t>& operator()
        [[nodiscard]] (range_t& range, const cursor_type_for<range_t>& cursor)
        const OKAYLIB_NOEXCEPT
    {
        static_assert(detail::range_has_get_ref_v<range_t>,
                      "Unable to get_ref- no nonconst get_ref defined for this "
                      "range.");
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
};

struct iter_set_fn_t
{
    template <typename range_t>
    constexpr void
    operator()(range_t& range, const cursor_type_for<range_t>& cursor,
               value_type_for<range_t>&& value) const OKAYLIB_NOEXCEPT
    {
        if constexpr (detail::range_has_get_ref_v<range_t> &&
                      std::is_move_assignable_v<value_type_for<range_t>>) {
            range_definition_inner<range_t>::get_ref(range, cursor) =
                std::move(value);
        } else {
            static_assert(
                detail::range_has_set_v<range_t>,
                "Cannot set for given range- it does not define iter_set, nor "
                "does it define get_ref + a move constructor for value type.");
            range_definition_inner<range_t>::set(range, cursor,
                                                 std::move(value));
        }
    }
};

struct begin_fn_t
{
    template <typename range_t>
    constexpr auto operator() [[nodiscard]] (const range_t& range) const
        OKAYLIB_NOEXCEPT->decltype(range_def_for<range_t>::begin(range))
    {
        return range_def_for<range_t>::begin(range);
    }
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
        } else {
            static_assert(detail::has_pre_increment_v<
                          detail::cursor_type_unchecked_for<range_t>>);
            ++cursor;
        }
    }
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
        } else {
            static_assert(detail::has_pre_increment_v<
                          detail::cursor_type_unchecked_for<range_t>>);
            --cursor;
        }
    }
};

struct is_inbounds_fn_t
{
    template <typename range_t>
    constexpr bool operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor) const
    {
        if constexpr (range_has_is_inbounds_v<range_t>) {
            return range_def_for<range_t>::is_inbounds(range, cursor);
        } else {
            using def = range_def_for<range_t>;
            return !def::is_before_bounds(range, cursor) &&
                   !def::is_after_bounds(range, cursor);
        }
    }

    template <typename range_t>
    constexpr bool operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor,
                       prefer_after_bounds_check_t) const
    {
        if constexpr (range_has_is_after_bounds_v<range_t>) {
            return !range_def_for<range_t>::is_after_bounds(range, cursor);
        } else {
            return this->operator()(range, cursor);
        }
    }

    template <typename range_t>
    constexpr bool operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor,
                       prefer_before_bounds_check_t) const
    {
        if constexpr (range_has_is_after_bounds_v<range_t>) {
            return !range_def_for<range_t>::is_before_bounds(range, cursor);
        } else {
            return this->operator()(range, cursor);
        }
    }
};

struct is_before_bounds_fn_t
{
    template <typename range_t>
    constexpr auto operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor) const
        -> decltype(range_def_for<range_t>::is_before_bounds(range, cursor))
    {
        return range_def_for<range_t>::is_before_bounds(range, cursor);
    }
};

struct is_after_bounds_fn_t
{
    template <typename range_t>
    constexpr auto operator()
        [[nodiscard]] (const range_t& range,
                       const cursor_type_for<range_t>& cursor) const
        -> decltype(range_def_for<range_t>::is_after_bounds(range, cursor))
    {
        return range_def_for<range_t>::is_after_bounds(range, cursor);
    }
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
};

struct data_fn_t
{
    template <typename range_t>
    constexpr auto operator() [[nodiscard]] (const range_t& range) const
        OKAYLIB_NOEXCEPT->decltype(range_def_for<range_t>::data(range))
    {
        return range_def_for<range_t>::data(range);
    }
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

} // namespace ok

#endif
