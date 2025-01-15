#ifndef __OKAYLIB_ITERABLE_ITERABLE_H__
#define __OKAYLIB_ITERABLE_ITERABLE_H__

#include "okay/detail/template_util/c_array_length.h"
#include "okay/detail/template_util/c_array_value_type.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_complete.h"
#include "okay/detail/traits/is_container.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/mathop_traits.h"
#include <type_traits>

/*
 * This header defines customization points and traits for iterables- their
 * value and cursor types, and operations on the iterable.
 *
 * `iterable_definition<T>`
 * `value_type_for<T>`
 * `cursor_type_for<T>`
 * `iterable_for<T>` (checked access to iterable_definition<T>)
 * `value_type iter_copyout(const iterable_t&, const cursor_t&)`
 * `const value_type& iter_get_temporary_ref(const iterable_t&, const
 * cursor_t&)`
 * `[const] value_type& iter_get_ref([const] iterable_t&, const cursor_t&)`
 * `void iter_set(iterable_t&, const cursor_t&)`
 * `begin()`
 * `is_inbounds()`
 */

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

namespace detail {
template <typename derived_t, typename = void> class view_interface;
}

template <typename iterable_t, typename enable = void>
struct iterable_definition
{
    // if an iterable definition includes a `static bool deleted = true`, then
    // it is ignored and the type is considered an invalid iterable.
    static constexpr bool deleted = true;
};

template <typename T, typename = void>
struct has_inherited_iterable_type : public std::false_type
{
    using type = void;
};
template <typename T>
struct has_inherited_iterable_type<
    T, std::void_t<typename T::inherited_iterable_type>> : public std::true_type
{
    using type = typename T::inherited_iterable_type;
};

template <typename T>
constexpr bool has_inherited_iterable_type_v =
    has_inherited_iterable_type<T>::value;

// specialization for c-style arrays
template <typename input_iterable_t>
struct iterable_definition<
    input_iterable_t,
    std::enable_if_t<std::is_array_v<detail::remove_cvref_t<input_iterable_t>>>>
{
  private:
    using iterable_t = detail::remove_cvref_t<input_iterable_t>;
    static_assert(
        !has_inherited_iterable_type_v<iterable_t>,
        "Type that should be array has inherited iterable member type?");

  public:
    using value_type = detail::c_array_value_type<iterable_t>;

    static constexpr value_type& get_ref(iterable_t& i,
                                         size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr const value_type& get_ref(const iterable_t& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr size_t begin(const iterable_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const iterable_t& i,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < size(i);
    }

    static constexpr size_t size(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return detail::c_array_length(i);
    }

    static constexpr value_type* data(iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }

    static constexpr const value_type*
    data(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }
};

// specialization for things that can be indexed with size_t and have an
// iterable_t::value_type member and a size() and data() function (stuff like
// std::array, std::span, and std::vector)
template <typename input_iterable_t>
struct iterable_definition<
    input_iterable_t,
    std::enable_if_t<
        // not array
        !std::is_array_v<detail::remove_cvref_t<input_iterable_t>> &&
        // not specified to inherit some other iterable definition
        !has_inherited_iterable_type_v<
            detail::remove_cvref_t<input_iterable_t>> &&
        // provides size_t .size() method and pointer data() method
        detail::is_container_v<input_iterable_t> &&
        // avoid recursion with view interface, which defines data and size
        // methods in terms of the functions provided here
        !detail::is_derived_from_v<
            input_iterable_t, ok::detail::view_interface<input_iterable_t>> &&
        // iterator_t()[size_t{}] -> iterator_t::value_type&
        std::is_same_v<
            typename detail::remove_cvref_t<input_iterable_t>::value_type&,
            decltype(std::declval<detail::remove_cvref_t<input_iterable_t>&>()
                         [std::declval<size_t>()])>>>
{
  private:
    using iterable_t = detail::remove_cvref_t<input_iterable_t>;

  public:
    using value_type = typename iterable_t::value_type;

    static constexpr value_type& get_ref(iterable_t& i,
                                         size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr const value_type& get_ref(const iterable_t& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr size_t begin(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const iterable_t& i,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < size(i);
    }

    static constexpr size_t size(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }

    static constexpr value_type* data(iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }

    static constexpr const value_type*
    data(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }
};

namespace detail {

template <typename T> struct iterable_definition_inner_meta_t
{
    static_assert(std::is_same_v<T, remove_cvref_t<T>>);
    using inherited_id = typename has_inherited_iterable_type<T>::type;
    using type = std::conditional_t<has_inherited_iterable_type_v<T>,
                                    iterable_definition<inherited_id>,
                                    iterable_definition<T>>;
    static_assert(!has_inherited_iterable_type_v<T> ||
                      is_derived_from_v<T, inherited_id>,
                  "inherited_iterable_type is not a base class of the iterable "
                  "for which it is specified.");
};

template <typename T>
using iterable_definition_inner =
    typename iterable_definition_inner_meta_t<remove_cvref_t<T>>::type;

template <typename T>
constexpr bool is_valid_cursor_v =
    // NOTE: update static_assert in iterable_for if this changes
    std::is_object_v<T> && has_pre_increment_v<T>;

template <typename T>
constexpr bool is_valid_value_type_v = std::is_object_v<T>;

template <typename T, typename = void>
struct has_value_type : public std::false_type
{};
template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>>
    : public std::true_type
{};

template <typename T>
using value_type_unchecked_for =
    typename iterable_definition_inner<T>::value_type;
template <typename T>
using cursor_type_unchecked_for = decltype(iterable_definition_inner<T>::begin(
    std::declval<const remove_cvref_t<T>&>()));

template <typename T, typename = void>
struct iterable_has_begin : public std::false_type
{};
template <typename T>
struct iterable_has_begin<
    T, std::enable_if_t<!std::is_same_v<
           void, decltype(iterable_definition_inner<T>::begin(
                     std::declval<const remove_cvref_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_has_is_inbounds : public std::false_type
{};
template <typename T>
struct iterable_has_is_inbounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(iterable_definition_inner<T>::is_inbounds(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_has_is_after_bounds : public std::false_type
{};
template <typename T>
struct iterable_has_is_after_bounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(iterable_definition_inner<T>::is_after_bounds(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_has_is_before_bounds : public std::false_type
{};
template <typename T>
struct iterable_has_is_before_bounds<
    T, std::enable_if_t<std::is_same_v<
           bool, decltype(iterable_definition_inner<T>::is_before_bounds(
                     std::declval<const remove_cvref_t<T>&>(),
                     std::declval<const cursor_type_unchecked_for<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_has_size : public std::false_type
{};
template <typename T>
struct iterable_has_size<
    T, std::enable_if_t<std::is_same_v<
           size_t, decltype(iterable_definition_inner<T>::size(
                       std::declval<const remove_cvref_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_marked_infinite : public std::false_type
{};
template <typename T>
struct iterable_marked_infinite<
    T, std::enable_if_t<iterable_definition_inner<T>::infinite>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_marked_finite : public std::false_type
{};
template <typename T>
struct iterable_marked_finite<
    T, std::enable_if_t<!iterable_definition_inner<T>::infinite>>
    : public std::true_type
{};

template <typename T, typename = void>
struct iterable_has_data : public std::false_type
{};
template <typename T>
struct iterable_has_data<T, std::enable_if_t<std::is_pointer_v<
                                decltype(iterable_definition_inner<T>::data(
                                    std::declval<const T&>()))>>>
    : public std::true_type
{};

template <typename T>
constexpr bool iterable_has_begin_v = iterable_has_begin<T>::value;
template <typename T>
constexpr bool iterable_has_is_inbounds_v = iterable_has_is_inbounds<T>::value;
template <typename T>
constexpr bool iterable_has_is_after_bounds_v =
    iterable_has_is_after_bounds<T>::value;
template <typename T>
constexpr bool iterable_has_is_before_bounds_v =
    iterable_has_is_before_bounds<T>::value;
template <typename T>
constexpr bool iterable_has_size_v = iterable_has_size<T>::value;
template <typename T>
constexpr bool iterable_marked_infinite_v = iterable_marked_infinite<T>::value;
template <typename T>
constexpr bool iterable_marked_finite_v = iterable_marked_finite<T>::value;

template <typename T, typename = void>
struct has_iterable_definition : std::true_type
{};
template <typename T>
struct has_iterable_definition<
    T, std::enable_if_t<iterable_definition_inner<T>::deleted>>
    : std::false_type
{};

template <typename iterable_t, typename = void>
struct iterable_has_get : public std::false_type
{};

template <typename iterable_t>
struct iterable_has_get<
    iterable_t,
    std::enable_if_t<std::is_same_v<
        decltype(iterable_definition_inner<iterable_t>::get(
            std::declval<const iterable_t&>(),
            std::declval<const cursor_type_unchecked_for<iterable_t>&>())),
        value_type_unchecked_for<iterable_t>>>> : public std::true_type
{};

template <typename iterable_t, typename = void>
struct iterable_has_set : public std::false_type
{};

template <typename iterable_t>
struct iterable_has_set<
    iterable_t,
    std::enable_if_t<std::is_same_v<
        decltype(iterable_definition_inner<iterable_t>::set(
            std::declval<iterable_t&>(),
            std::declval<const cursor_type_unchecked_for<iterable_t>&>(),
            std::declval<value_type_unchecked_for<iterable_t>&&>())),
        void>>> : std::true_type
{};

template <typename iterable_t, typename = void>
struct iterable_has_get_ref : public std::false_type
{};

template <typename T>
using iterable_get_ref_return_type_t =
    decltype(iterable_definition_inner<T>::get_ref(
        std::declval<T&>(),
        std::declval<const cursor_type_unchecked_for<T>&>()));

template <typename iterable_t>
struct iterable_has_get_ref<
    iterable_t,
    std::enable_if_t<std::is_same_v<iterable_get_ref_return_type_t<iterable_t>,
                                    value_type_unchecked_for<iterable_t>&> &&
                     !std::is_const_v<std::remove_reference_t<
                         iterable_get_ref_return_type_t<iterable_t>>>>>
    : public std::true_type
{};

template <typename T>
using iterable_get_ref_const_return_type_t =
    decltype(iterable_definition_inner<T>::get_ref(
        std::declval<const T&>(),
        std::declval<const cursor_type_unchecked_for<T>&>()));

template <typename iterable_t, typename = void>
struct iterable_has_get_ref_const : public std::false_type
{};

template <typename iterable_t>
struct iterable_has_get_ref_const<
    iterable_t,
    std::enable_if_t<
        std::is_same_v<iterable_get_ref_const_return_type_t<iterable_t>,
                       const value_type_unchecked_for<iterable_t>&> &&
        std::is_const_v<std::remove_reference_t<
            iterable_get_ref_const_return_type_t<iterable_t>>>>>
    : public std::true_type
{};

template <typename iterable_t>
constexpr bool iterable_has_get_v = iterable_has_get<iterable_t>::value;
template <typename iterable_t>
constexpr bool iterable_has_set_v = iterable_has_set<iterable_t>::value;
template <typename iterable_t>
constexpr bool iterable_has_get_ref_v = iterable_has_get_ref<iterable_t>::value;
template <typename iterable_t>
constexpr bool iterable_has_get_ref_const_v =
    iterable_has_get_ref_const<iterable_t>::value;

template <typename T>
constexpr bool has_baseline_functions_v =
    (iterable_marked_finite_v<T> || iterable_marked_infinite_v<T> ||
     iterable_has_size_v<T>) &&
    // either provides an is_inbounds, XOR it provides after & before bounds
    // checks
    (iterable_has_is_inbounds_v<T> != (iterable_has_is_after_bounds_v<T> &&
                                       iterable_has_is_before_bounds_v<T>)) &&
    iterable_has_begin_v<T>;

template <typename T>
constexpr bool is_output_iterable_v =
    has_baseline_functions_v<T> &&
    (iterable_has_set_v<T> || iterable_has_get_ref_v<T>);

template <typename T>
constexpr bool is_input_iterable_v =
    has_baseline_functions_v<T> &&
    (iterable_has_get_v<T> || iterable_has_get_ref_const_v<T> ||
     iterable_has_get_ref_v<T>);

template <typename T>
constexpr bool is_input_or_output_iterable_v =
    is_input_iterable_v<T> || is_output_iterable_v<T>;

// single pass iterables are by definition the minimum iterable- so any valid
// input or output iterable is also a single pass iterable.
template <typename maybe_iterable_t>
constexpr bool is_single_pass_iterable_v =
    is_input_or_output_iterable_v<maybe_iterable_t>;

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

// a multi pass iterable is copyable- enabling consumers to pause and come back
// to copies of iterables at previous positions.
template <typename maybe_iterable_t>
constexpr bool is_multi_pass_iterable_v =
    is_single_pass_iterable_v<maybe_iterable_t> &&
    std::is_copy_constructible_v<cursor_or_void_t<maybe_iterable_t>> &&
    std::is_copy_assignable_v<cursor_or_void_t<maybe_iterable_t>>;

template <typename maybe_iterable_t>
constexpr bool is_bidirectional_iterable_v =
    is_multi_pass_iterable_v<maybe_iterable_t> &&
    // can also be decremented
    has_pre_decrement_v<cursor_or_void_t<maybe_iterable_t>>;

template <typename maybe_iterable_t>
constexpr bool is_random_access_iterable_v =
    is_bidirectional_iterable_v<maybe_iterable_t> &&
    // must also be able to do +, +=, -, -=, <, >, <=, >=
    has_addition_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_subtraction_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_inplace_addition_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_inplace_subtraction_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    is_equality_comparable_to_v<cursor_or_void_t<maybe_iterable_t>,
                                cursor_or_void_t<maybe_iterable_t>> &&
    has_comparison_operators_v<cursor_or_void_t<maybe_iterable_t>>;
} // namespace detail

/// iterable_for is a way of accessing the iterable definition for a type, with
/// some additional verification that the definition being accessed is actually
/// valid.
template <typename T>
class iterable_for : public detail::iterable_definition_inner<T>
{
    using noref = detail::remove_cvref_t<T>;
    static constexpr bool complete = detail::is_complete<noref>;

    static_assert(
        complete,
        "Refusing to access iterable definition for incomplete type.");

    static constexpr bool not_deleted =
        detail::has_iterable_definition<noref>::value;
    static_assert(not_deleted,
                  "Attempt to access iterable information for a type whose "
                  "iterable definition is marked deleted.");

    static constexpr bool has_begin = detail::iterable_has_begin_v<noref>;
    static_assert(has_begin,
                  "Iterable definition invalid- provide a begin(const "
                  "iterable_t&) function which returns a cursor type.");

    static constexpr bool has_inbounds =
        detail::iterable_has_is_inbounds_v<noref>;
    static constexpr bool has_after_bounds =
        detail::iterable_has_is_after_bounds_v<noref>;
    static constexpr bool has_before_bounds =
        detail::iterable_has_is_before_bounds_v<noref>;
    static_assert(has_inbounds != (has_after_bounds && has_before_bounds),
                  "Iterable definition invalid- provide exactly one of either "
                  "`is_inbounds()` "
                  "OR `is_after_bounds() and is_before_bounds()`.");

    static_assert(has_after_bounds == has_before_bounds,
                  "Iterable definition invalid- do not provide "
                  "is_after_bounds() or is_before_bounds() when is_inbounds() "
                  "is also provided, because the latter two will be ignored.");

    static constexpr bool has_size = detail::iterable_has_size_v<noref>;
    static constexpr bool marked_infinite =
        detail::iterable_marked_infinite_v<noref>;
    static constexpr bool marked_finite =
        detail::iterable_marked_finite_v<noref>;
    static_assert(
        has_size || marked_infinite || marked_finite,
        "Iterable definition invalid- provide a size_t size(const iterable_t&) "
        "method, or add a `static constexpr bool infinite = ...` which should "
        "be true if the iterable is intended to be infinite, and false if the "
        "iterable is intended to have some finite size that cannot be "
        "calculated in constant time.");
    static_assert(int(has_size) + int(marked_finite) + int(marked_infinite) ==
                      1,
                  "Iterable definition invalid: specify only one of `static "
                  "bool infinite = ...` or `size()` method.");

    static constexpr bool has_value_type =
        detail::has_value_type<detail::iterable_definition_inner<noref>>::value;
    static_assert(has_value_type,
                  "Iterable definition invalid- specify value_type.");

    static constexpr bool valid_value_type = detail::is_valid_value_type_v<
        typename detail::iterable_definition_inner<noref>::value_type>;
    static_assert(valid_value_type,
                  "Iterable definition invalid- value_type is not an object.");

    static constexpr bool valid_cursor =
        detail::is_valid_cursor_v<detail::cursor_type_unchecked_for<T>>;
    static_assert(
        valid_cursor,
        "Iterable definition invalid- the return value from begin() (the "
        "cursor type) is either not an object or cannot be pre-incremented.");

    static constexpr bool input_or_output =
        detail::is_input_or_output_iterable_v<T>;
    static_assert(
        input_or_output,
        "Iterable definition invalid- it does not define any operations that "
        "can be done with the iterable like get(), set(), or get_ref()");

    static constexpr bool ref_or_get_are_mutually_exclusive =
        detail::iterable_has_get_v<T> !=
        (detail::iterable_has_get_ref_v<T> ||
         detail::iterable_has_get_ref_const_v<T>);
    static_assert(
        ref_or_get_are_mutually_exclusive,
        "Iterable definition invalid- both get() and a get_ref() functions are "
        "defined. There is currently no protocol in okaylib to choose between "
        "these implementations, so remove one of them and use the copy "
        "constructor of value_type if both copyout and get_ref are needed.");
};

namespace detail {
template <typename T, typename = void> struct is_iterable : std::false_type
{};
/// Same as the checks performed in `iterator_for`
template <typename T>
struct is_iterable<
    T, std::enable_if_t<
           is_complete<T> && has_iterable_definition<T>::value &&
           has_value_type<iterable_definition_inner<T>>::value &&
           is_valid_value_type_v<value_type_unchecked_for<T>> &&
           iterable_has_begin_v<T> &&
           is_valid_cursor_v<cursor_type_unchecked_for<T>> &&
           (iterable_has_is_inbounds_v<T> !=
            (iterable_has_is_after_bounds_v<T> &&
             iterable_has_is_before_bounds_v<T>)) &&
           iterable_has_is_after_bounds_v<T> ==
               iterable_has_is_before_bounds_v<T> &&
           (iterable_has_size_v<T> || iterable_marked_infinite_v<T> ||
            iterable_marked_finite_v<T>) &&
           ((int(iterable_has_size_v<T>) + int(iterable_marked_infinite_v<T>) +
             int(iterable_marked_finite_v<T>)) == 1) &&
           is_input_or_output_iterable_v<T> &&
           (iterable_has_get_v<T> !=
            (iterable_has_get_ref_v<T> || iterable_has_get_ref_const_v<T>))>>
    : std::true_type
{};
} // namespace detail

/// Check if a type has an iterable_definition which is valid, enabling a form
/// of either input or output iteration.
template <typename T>
constexpr bool is_iterable_v =
    detail::is_iterable<detail::remove_cvref_t<T>>::value;

template <typename T>
using value_type_for = typename iterable_for<T>::value_type;
template <typename T>
using cursor_type_for =
    decltype(iterable_for<T>::begin(std::declval<const T&>()));

struct prefer_after_bounds_check_t
{};
struct prefer_before_bounds_check_t
{};

namespace detail {
struct iter_copyout_fn_t
{
    template <typename iterable_t>
    constexpr auto operator() [[nodiscard]] (
        const iterable_t& iterable,
        const cursor_type_for<iterable_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_iterable_v<iterable_t>);
        using namespace detail;
        static_assert(
            iterable_has_get_v<iterable_t> ||
                iterable_has_get_ref_const_v<iterable_t>,
            "Cannot copy out a value from given iterable- provide a `const& "
            "get_ref(const)` or `get(const)` function in its definition.");

        using def = iterable_definition_inner<iterable_t>;
        if constexpr (iterable_has_get_v<iterable_t>) {
            return def::get(iterable, cursor);
        } else {
            return def::get_ref(iterable, cursor);
        }
    }
};

template <typename iterable_t, typename = void>
struct iter_get_temporary_ref_meta_t
{
    using type = void;
    static constexpr void call(...) OKAYLIB_NOEXCEPT
    {
        static_assert(iterable_has_get_ref_v<iterable_t> &&
                          !iterable_has_get_ref_const_v<iterable_t>,
                      "Cannot call iter_get_temporary_ref- the function takes "
                      "a const reference but only nonconst get_ref is "
                      "implemented for this iterable.");
        // trigger static asserts in iterable_for- this iterable is invalid
        std::declval<iterable_for<iterable_t>>();
    }
};

template <typename iterable_t>
struct iter_get_temporary_ref_meta_t<
    iterable_t,
    std::enable_if_t<is_iterable_v<iterable_t> &&
                     !detail::iterable_has_get_ref_const_v<iterable_t>>>
{
    using type = value_type_for<iterable_t>;
    static constexpr type
    call(const iterable_t& iterable,
         const cursor_type_for<iterable_t>& cursor) OKAYLIB_NOEXCEPT
    {
        return iter_copyout(iterable, cursor);
    }
};

template <typename iterable_t>
struct iter_get_temporary_ref_meta_t<
    iterable_t,
    std::enable_if_t<is_iterable_v<iterable_t> &&
                     detail::iterable_has_get_ref_const_v<iterable_t>>>
{
    using type = const value_type_for<iterable_t>&;
    static constexpr type
    call(const iterable_t& iterable,
         const cursor_type_for<iterable_t>& cursor) OKAYLIB_NOEXCEPT
    {
        return iterable_definition_inner<iterable_t>::get_ref(iterable, cursor);
    }
};

struct iter_get_temporary_ref_fn_t
{
    template <typename iterable_t>
    constexpr decltype(auto) operator() [[nodiscard]] (
        const iterable_t& iterable,
        const cursor_type_for<iterable_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        return detail::iter_get_temporary_ref_meta_t<iterable_t>::call(iterable,
                                                                       cursor);
        static_assert(is_iterable_v<iterable_t>,
                      "Invalid iterable passed in to iter_get_temporary_ref");
    }
};

struct iter_get_ref_fn_t
{
    template <typename iterable_t>
    constexpr value_type_for<iterable_t>& operator() [[nodiscard]] (
        iterable_t& iterable,
        const cursor_type_for<iterable_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        static_assert(detail::iterable_has_get_ref_v<iterable_t>,
                      "Unable to get_ref- no nonconst get_ref defined for this "
                      "iterable.");
        return iterable_definition_inner<iterable_t>::get_ref(iterable, cursor);
    }

    template <typename iterable_t>
    constexpr const value_type_for<iterable_t>& operator() [[nodiscard]] (
        const iterable_t& iterable,
        const cursor_type_for<iterable_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        static_assert(
            detail::iterable_has_get_ref_const_v<iterable_t>,
            "Unable to get_ref- no const get_ref defined for this iterable.");
        return iterable_definition_inner<iterable_t>::get_ref(iterable, cursor);
    }
};

struct iter_set_fn_t
{
    template <typename iterable_t>
    constexpr void
    operator()(iterable_t& iterable, const cursor_type_for<iterable_t>& cursor,
               value_type_for<iterable_t>&& value) const OKAYLIB_NOEXCEPT
    {
        if constexpr (detail::iterable_has_get_ref_v<iterable_t> &&
                      std::is_move_assignable_v<value_type_for<iterable_t>>) {
            iterable_definition_inner<iterable_t>::get_ref(iterable, cursor) =
                std::move(value);
        } else {
            static_assert(
                detail::iterable_has_set_v<iterable_t>,
                "Cannot set for given iterable- it does not define iter_set, "
                "nor "
                "does not define get_ref + a move constructor for value type.");
            iterable_definition_inner<iterable_t>::set(iterable, cursor,
                                                       std::move(value));
        }
    }
};

struct begin_fn_t
{
    template <typename iterable_t>
    constexpr auto operator() [[nodiscard]] (const iterable_t& iterable) const
        OKAYLIB_NOEXCEPT->decltype(iterable_for<iterable_t>::begin(iterable))
    {
        return iterable_for<iterable_t>::begin(iterable);
    }
};

struct is_inbounds_fn_t
{
    template <typename iterable_t>
    constexpr bool operator()
        [[nodiscard]] (const iterable_t& iterable,
                       const cursor_type_for<iterable_t>& cursor) const
    {
        if constexpr (iterable_has_is_inbounds_v<iterable_t>) {
            return iterable_for<iterable_t>::is_inbounds(iterable, cursor);
        } else {
            using def = iterable_for<iterable_t>;
            return def::is_before_bounds(iterable, cursor) ||
                   def::is_after_bounds(iterable, cursor);
        }
    }

    template <typename iterable_t>
    constexpr bool operator()
        [[nodiscard]] (const iterable_t& iterable,
                       const cursor_type_for<iterable_t>& cursor,
                       prefer_after_bounds_check_t) const
    {
        if constexpr (iterable_has_is_after_bounds_v<iterable_t>) {
            return iterable_for<iterable_t>::is_after_bounds(iterable, cursor);
        } else {
            return this->operator()(iterable, cursor);
        }
    }

    template <typename iterable_t>
    constexpr bool operator()
        [[nodiscard]] (const iterable_t& iterable,
                       const cursor_type_for<iterable_t>& cursor,
                       prefer_before_bounds_check_t) const
    {
        if constexpr (iterable_has_is_after_bounds_v<iterable_t>) {
            return iterable_for<iterable_t>::is_before_bounds(iterable, cursor);
        } else {
            return this->operator()(iterable, cursor);
        }
    }
};

struct is_before_bounds_fn_t
{
    template <typename iterable_t>
    constexpr auto operator()
        [[nodiscard]] (const iterable_t& iterable,
                       const cursor_type_for<iterable_t>& cursor) const
        -> decltype(iterable_for<iterable_t>::is_before_bounds(iterable,
                                                               cursor))
    {
        return iterable_for<iterable_t>::is_before_bounds(iterable, cursor);
    }
};

struct is_after_bounds_fn_t
{
    template <typename iterable_t>
    constexpr auto operator()
        [[nodiscard]] (const iterable_t& iterable,
                       const cursor_type_for<iterable_t>& cursor) const
        -> decltype(iterable_for<iterable_t>::is_after_bounds(iterable, cursor))
    {
        return iterable_for<iterable_t>::is_after_bounds(iterable, cursor);
    }
};

struct size_fn_t
{
    template <typename iterable_t>
    constexpr auto
    operator() [[nodiscard]] (const iterable_t& iterable) const OKAYLIB_NOEXCEPT
        ->std::enable_if_t<!iterable_marked_finite_v<iterable_t> &&
                               !iterable_marked_infinite_v<iterable_t>,
                           decltype(iterable_for<iterable_t>::size(iterable))>
    {
        return iterable_for<iterable_t>::size(iterable);
    }
};

struct data_fn_t
{
    template <typename iterable_t>
    constexpr auto operator() [[nodiscard]] (const iterable_t& iterable) const
        OKAYLIB_NOEXCEPT->decltype(iterable_for<iterable_t>::data(iterable))
    {
        return iterable_for<iterable_t>::data(iterable);
    }
};

} // namespace detail

/// Get a copy of the data pointed at by an iterator. Works on iterables
/// implementing `iter_get()`, or `iter_deref() [const]` if value_type is copy
/// constructible.
/// Returns iterable_t::value_type.
constexpr detail::iter_copyout_fn_t iter_copyout{};

/// Get a temporary const reference to an item. More optimizable than iter_get
/// (its possible for any copying to be elided) but the reference is only valid
/// until the end of the execution of the calling function. Supported by all
/// types which implement at least one of `iter_get()`, `iter_deref() const`, or
/// `iter_deref()`.
/// Returns either a iterable_t::value_type, or a const iterable_t::value_type&.
/// If the return value is a reference, then it is the same as calling
/// iter_get_ref. It is always valid to store the return value in a `const
/// value_type&`
constexpr detail::iter_get_temporary_ref_fn_t iter_get_temporary_ref{};

/// Returns an lvalue reference to the internel representation of the data
/// pointed at by the iterator. This reference is invalidated based on
/// container-specific behavior.
constexpr detail::iter_get_ref_fn_t iter_get_ref{};

constexpr detail::iter_set_fn_t iter_set{};

constexpr detail::begin_fn_t begin{};

constexpr detail::is_inbounds_fn_t is_inbounds{};

constexpr detail::size_fn_t size{};

constexpr detail::size_fn_t data{};

} // namespace ok

#endif
