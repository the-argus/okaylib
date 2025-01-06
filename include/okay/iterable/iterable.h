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
 * `sentinel_type_for<T>``
 * `iterable_for<T>` (checked access to iterable_definition<T>)
 * `value_type iter_copyout(const iterable_t&, const cursor_t&)`
 * `const value_type& iter_get_temporary_ref(const iterable_t&, const
 * cursor_t&)`
 * `[const] value_type& iter_get_ref([const] iterable_t&, const cursor_t&)`
 * `void iter_set(iterable_t&, const cursor_t&)`
 * `begin()`
 * `end()`
 */

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

template <typename derived_t, typename = void> class view_interface;

template <typename iterable_t, typename enable = void>
struct iterable_definition
{
    // if an iterable definition includes a `static bool deleted = true`, then
    // it is ignored and the type is considered an invalid iterable.
    inline static constexpr bool deleted = true;
};

// specialization for c-style arrays
template <typename input_iterable_t>
struct iterable_definition<input_iterable_t,
                           std::enable_if_t<std::is_array_v<
                               std::remove_reference_t<input_iterable_t>>>>
{
  private:
    using iterable_t = detail::remove_cvref_t<input_iterable_t>;

  public:
    using value_type = detail::c_array_value_type<iterable_t>;

    inline static constexpr value_type& get_ref(iterable_t& i,
                                                size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    inline static constexpr const value_type& get_ref(const iterable_t& i,
                                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    inline static constexpr size_t begin(const iterable_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    inline static constexpr size_t end(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return size(i);
    }

    inline static constexpr size_t size(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return detail::c_array_length(i);
    }

    inline static constexpr value_type* data(iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }

    inline static constexpr const value_type*
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
        !std::is_array_v<std::remove_reference_t<input_iterable_t>> &&
        // provides size_t .size() method and pointer data() method
        detail::is_container_v<input_iterable_t> &&
        // avoid recursion with view interface, which defines data and size
        // methods in terms of the functions provided here
        !detail::is_derived_from_v<input_iterable_t,
                                   view_interface<input_iterable_t>> &&
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

    inline static constexpr value_type& get_ref(iterable_t& i,
                                                size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    inline static constexpr const value_type& get_ref(const iterable_t& i,
                                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    inline static constexpr size_t begin(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    inline static constexpr size_t end(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return size();
    }

    inline static constexpr size_t size(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }

    inline static constexpr value_type* data(iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }

    inline static constexpr const value_type*
    data(const iterable_t& i) OKAYLIB_NOEXCEPT
    {
        return &i[0];
    }
};

namespace detail {
template <typename T>
inline constexpr bool is_valid_cursor_v =
    // NOTE: update static_assert in iterable_for if this changes
    std::is_object_v<T> && has_pre_increment_v<T> &&
    is_equality_comparable_to_v<T, T>;

template <typename T>
inline constexpr bool is_valid_value_type_v = std::is_object_v<T>;

// Check if sentinel is valid, given a cursor.
// Precondition: cursor passes is_valid_cursor_v
template <typename cursor_t, typename sentinel_t>
inline constexpr bool is_valid_sentinel_v =
    std::is_object_v<sentinel_t> &&
    is_equality_comparable_to_v<cursor_t, sentinel_t>;

template <typename T, typename = void>
class has_value_type : public std::false_type
{};
template <typename T>
class has_value_type<T, std::void_t<typename T::value_type>>
    : public std::true_type
{};

template <typename T, typename = void>
class iterable_has_begin : public std::false_type
{};
template <typename T>
class iterable_has_begin<
    T,
    std::enable_if_t<!std::is_same_v<
        void, decltype(iterable_definition<std::remove_reference_t<T>>::begin(
                  std::declval<const std::remove_reference_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
class iterable_has_end : public std::false_type
{};
template <typename T>
class iterable_has_end<
    T, std::enable_if_t<!std::is_same_v<
           void, decltype(iterable_definition<std::remove_reference_t<T>>::end(
                     std::declval<const std::remove_reference_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
class iterable_has_size : public std::false_type
{};
template <typename T>
class iterable_has_size<
    T,
    std::enable_if_t<std::is_same_v<
        size_t, decltype(iterable_definition<std::remove_reference_t<T>>::size(
                    std::declval<const std::remove_reference_t<T>&>()))>>>
    : public std::true_type
{};

template <typename T, typename = void>
class iterable_marked_infinite : public std::false_type
{};
template <typename T>
class iterable_marked_infinite<
    T, std::enable_if_t<iterable_definition<remove_cvref_t<T>>::infinite>>
    : public std::true_type
{};

template <typename T, typename = void>
class iterable_marked_finite : public std::false_type
{};
template <typename T>
class iterable_marked_finite<
    T, std::enable_if_t<!iterable_definition<remove_cvref_t<T>>::infinite>>
    : public std::true_type
{};

template <typename T, typename = void>
class iterable_has_data : public std::false_type
{};
template <typename T>
class iterable_has_data<
    T, std::enable_if_t<std::is_pointer_v<
           decltype(iterable_definition<remove_cvref_t<T>>::data(
               std::declval<const T&>()))>>> : public std::true_type
{};

template <typename T>
inline constexpr bool iterable_has_begin_v = iterable_has_begin<T>::value;
template <typename T>
inline constexpr bool iterable_has_end_v = iterable_has_end<T>::value;
template <typename T>
inline constexpr bool iterable_has_size_v = iterable_has_size<T>::value;
template <typename T>
inline constexpr bool iterable_marked_infinite_v =
    iterable_marked_infinite<T>::value;
template <typename T>
inline constexpr bool iterable_marked_finite_v =
    iterable_marked_finite<T>::value;

template <typename T>
using value_type_unchecked_for = typename iterable_definition<T>::value_type;
template <typename T>
using cursor_type_unchecked_for =
    decltype(iterable_definition<T>::begin(std::declval<const T&>()));
template <typename T>
using sentinel_type_unchecked_for =
    decltype(iterable_definition<T>::end(std::declval<const T&>()));

template <typename T, typename = void>
struct has_iterable_definition : std::true_type
{};
template <typename T>
struct has_iterable_definition<
    T, std::enable_if_t<iterable_definition<T>::deleted>> : std::false_type
{};

template <typename iterable_t, typename = void>
class iterable_has_get : public std::false_type
{};

template <typename iterable_t>
class iterable_has_get<
    iterable_t,
    std::enable_if_t<std::is_same_v<
        decltype(iterable_definition<iterable_t>::get(
            std::declval<const iterable_t&>(),
            std::declval<const cursor_type_unchecked_for<iterable_t>&>())),
        value_type_unchecked_for<iterable_t>>>> : public std::true_type
{};

template <typename iterable_t, typename = void>
class iterable_has_set : public std::false_type
{};

template <typename iterable_t>
class iterable_has_set<
    iterable_t,
    std::enable_if_t<std::is_same_v<
        decltype(iterable_definition<iterable_t>::set(
            std::declval<iterable_t&>(),
            std::declval<const cursor_type_unchecked_for<iterable_t>&>(),
            std::declval<value_type_unchecked_for<iterable_t>&&>())),
        void>>> : std::true_type
{};

template <typename iterable_t, typename = void>
class iterable_has_get_ref : public std::false_type
{};

template <typename iterable_t>
class iterable_has_get_ref<
    iterable_t,
    std::enable_if_t<std::is_same_v<
        decltype(iterable_definition<iterable_t>::get_ref(
            std::declval<iterable_t&>(),
            std::declval<const cursor_type_unchecked_for<iterable_t>&>())),
        value_type_unchecked_for<iterable_t>&>>> : public std::true_type
{};

template <typename iterable_t, typename = void>
class iterable_has_get_ref_const : public std::false_type
{};

template <typename iterable_t>
class iterable_has_get_ref_const<
    iterable_t,
    std::enable_if_t<std::is_same_v<
        decltype(iterable_definition<iterable_t>::get_ref(
            std::declval<const iterable_t&>(),
            std::declval<const cursor_type_unchecked_for<iterable_t>&>())),
        const value_type_unchecked_for<iterable_t>&>>> : public std::true_type
{};

template <typename iterable_t>
inline constexpr bool iterable_has_get_v = iterable_has_get<iterable_t>::value;
template <typename iterable_t>
inline constexpr bool iterable_has_set_v = iterable_has_set<iterable_t>::value;
template <typename iterable_t>
inline constexpr bool iterable_has_get_ref_v =
    iterable_has_get_ref<iterable_t>::value;
template <typename iterable_t>
inline constexpr bool iterable_has_get_ref_const_v =
    iterable_has_get_ref_const<iterable_t>::value;

template <typename T>
inline constexpr bool has_baseline_functions_v =
    (iterable_marked_finite_v<T> || iterable_marked_infinite_v<T> ||
     iterable_has_size_v<T>) &&
    iterable_has_end_v<T> && iterable_has_begin_v<T>;

template <typename T>
inline constexpr bool is_output_iterable_v =
    has_baseline_functions_v<T> &&
    (iterable_has_set_v<T> || iterable_has_get_ref_v<T>);

template <typename T>
inline constexpr bool is_input_iterable_v =
    has_baseline_functions_v<T> &&
    (iterable_has_get_v<T> || iterable_has_get_ref_const_v<T> ||
     iterable_has_get_ref_v<T>);

template <typename T>
inline constexpr bool is_input_or_output_iterable_v =
    is_input_iterable_v<T> || is_output_iterable_v<T>;

// single pass iterables are by definition the minimum iterable- so any valid
// input or output iterable is also a single pass iterable.
template <typename maybe_iterable_t>
inline constexpr bool is_single_pass_iterable_v =
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
inline constexpr bool is_multi_pass_iterable_v =
    is_single_pass_iterable_v<maybe_iterable_t> &&
    std::is_copy_constructible_v<cursor_or_void_t<maybe_iterable_t>> &&
    std::is_copy_assignable_v<cursor_or_void_t<maybe_iterable_t>>;

template <typename maybe_iterable_t>
inline constexpr bool is_bidirectional_iterable_v =
    is_multi_pass_iterable_v<maybe_iterable_t> &&
    // can also be decremented
    has_pre_decrement_v<cursor_or_void_t<maybe_iterable_t>>;

template <typename maybe_iterable_t>
inline constexpr bool is_random_access_iterable_v =
    is_bidirectional_iterable_v<maybe_iterable_t> &&
    // must also be able to do +, +=, -, -=, <, >, <=, >=
    has_addition_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_subtraction_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_inplace_addition_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_inplace_subtraction_with_size_v<cursor_or_void_t<maybe_iterable_t>> &&
    has_comparison_operators_v<cursor_or_void_t<maybe_iterable_t>>;
} // namespace detail

/// iterable_for is a way of accessing the iterable definition for a type, with
/// some additional verification that the definition being accessed is actually
/// valid.
template <typename T>
class iterable_for : public iterable_definition<std::remove_reference_t<T>>
{
    using noref = std::remove_reference_t<T>;
    inline static constexpr bool complete = detail::is_complete<noref>;
    static_assert(
        complete,
        "Refusing to access iterable definition for incomplete type.");

    inline static constexpr bool not_deleted =
        detail::has_iterable_definition<noref>::value;
    static_assert(not_deleted,
                  "Attempt to access iterable information for a type whose "
                  "iterable definition is marked deleted.");

    inline static constexpr bool has_begin =
        detail::iterable_has_begin_v<noref>;
    static_assert(has_begin,
                  "Iterable definition invalid- provide a begin(const "
                  "iterable_t&) function which returns a cursor type.");

    inline static constexpr bool has_end = detail::iterable_has_end_v<noref>;
    static_assert(
        has_end,
        "Iterable definition invalid- provide an end(const iterable_t&) "
        "function which returns a sentinel type that can be compared with "
        "operator == to the return value of begin().");

    inline static constexpr bool has_size = detail::iterable_has_size_v<noref>;
    inline static constexpr bool marked_infinite =
        detail::iterable_marked_infinite_v<noref>;
    inline static constexpr bool marked_finite =
        detail::iterable_marked_infinite_v<noref>;
    static_assert(
        has_size || marked_infinite || marked_finite,
        "Iterable definition invalid- provide a size_t size(const iterable_t&) "
        "method, or add a `static constexpr bool infinite = ...` which should "
        "be true if the iterable is intended to be infinite, and false if the "
        "iterable is intended to have some finite size that cannot be "
        "calculated in constant time.");

    inline static constexpr bool has_value_type =
        detail::has_value_type<iterable_definition<noref>>::value;
    static_assert(has_value_type,
                  "Iterable definition invalid- specify value_type.");

    inline static constexpr bool valid_value_type =
        detail::is_valid_value_type_v<
            typename iterable_definition<noref>::value_type>;
    static_assert(valid_value_type,
                  "Iterable definition invalid- value type is not an object.");

    inline static constexpr bool valid_cursor =
        detail::is_valid_cursor_v<detail::cursor_type_unchecked_for<T>>;
    static_assert(
        valid_cursor,
        "Iterable definition invalid- the return value from begin() (the "
        "cursor type) is either not an object or cannot be pre-incremented.");

    inline static constexpr bool valid_sentinel =
        detail::is_valid_sentinel_v<detail::cursor_type_unchecked_for<T>,
                                    detail::sentinel_type_unchecked_for<T>>;
    static_assert(
        valid_sentinel,
        "Iterable definition invalid- the return value from end() (the "
        "sentinel type) is either not an object or not comparable by == "
        "operator to the cursor type (the return type of begin()).");

    inline static constexpr bool input_or_output =
        detail::is_input_or_output_iterable_v<T>;
    static_assert(
        input_or_output,
        "Iterable definition invalid- it does not define any operations that "
        "can be done with the iterable like get(), set(), or get_ref()");

    inline static constexpr bool ref_or_get_are_mutually_exclusive =
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
/// Same as the checks performed in `iterator_for`
template <typename T>
inline constexpr bool is_iterable_impl_v =
    detail::is_complete<T> && has_iterable_definition<T>::value &&
    has_value_type<T>::value && iterable_has_begin_v<T> &&
    iterable_has_end_v<T> && is_valid_cursor_v<cursor_type_unchecked_for<T>> &&
    is_valid_value_type_v<value_type_unchecked_for<T>> &&
    is_valid_sentinel_v<cursor_type_unchecked_for<T>,
                        sentinel_type_unchecked_for<T>> &&
    is_input_or_output_iterable_v<T> &&
    (iterable_has_get_v<T> !=
     (iterable_has_get_ref_v<T> || iterable_has_get_ref_const_v<T>));
} // namespace detail

/// Check if a type has an iterable_definition which is valid, enabling a form
/// of either input or output iteration.
template <typename T>
inline constexpr bool is_iterable_v =
    detail::is_iterable_impl_v<std::remove_reference_t<T>>;

template <typename T>
using value_type_for = typename iterable_for<T>::value_type;
template <typename T>
using cursor_type_for =
    decltype(iterable_for<T>::begin(std::declval<const T&>()));
template <typename T>
using sentinel_type_for =
    decltype(iterable_for<T>::end(std::declval<const T&>()));

namespace detail {
struct iter_copyout_fn_t
{
    template <typename iterable_t>
    inline constexpr auto operator() [[nodiscard]] (
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

        using def = iterable_definition<iterable_t>;
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
    static inline constexpr void call(...) OKAYLIB_NOEXCEPT
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
    static inline constexpr type
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
    static inline constexpr type
    call(const iterable_t& iterable,
         const cursor_type_for<iterable_t>& cursor) OKAYLIB_NOEXCEPT
    {
        return iterable_definition<iterable_t>::get_ref(iterable, cursor);
    }
};

struct iter_get_temporary_ref_fn_t
{
    template <typename iterable_t>
    inline constexpr decltype(auto) operator() [[nodiscard]] (
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
    inline constexpr value_type_for<iterable_t>& operator() [[nodiscard]] (
        iterable_t& iterable,
        const cursor_type_for<iterable_t>& cursor) const OKAYLIB_NOEXCEPT
    {
        static_assert(detail::iterable_has_get_ref_v<iterable_t>,
                      "Unable to get_ref- no nonconst get_ref defined for this "
                      "iterable.");
        return iterable_definition<iterable_t>::get_ref(iterable, cursor);
    }

    template <typename iterable_t>
    inline constexpr const value_type_for<iterable_t>& operator()
        [[nodiscard]] (const iterable_t& iterable,
                       const cursor_type_for<iterable_t>& cursor) const
        OKAYLIB_NOEXCEPT
    {
        static_assert(
            detail::iterable_has_get_ref_const_v<iterable_t>,
            "Unable to get_ref- no const get_ref defined for this iterable.");
        return iterable_definition<iterable_t>::get_ref(iterable, cursor);
    }
};

struct iter_set_fn_t
{
    template <typename iterable_t>
    inline constexpr void
    operator()(iterable_t& iterable, const cursor_type_for<iterable_t>& cursor,
               value_type_for<iterable_t>&& value) const OKAYLIB_NOEXCEPT
    {
        if constexpr (detail::iterable_has_get_ref_v<iterable_t> &&
                      std::is_move_assignable_v<value_type_for<iterable_t>>) {
            iterable_definition<iterable_t>::get_ref(iterable, cursor) =
                std::move(value);
        } else {
            static_assert(
                detail::iterable_has_set_v<iterable_t>,
                "Cannot set for given iterable- it does not define iter_set, "
                "nor "
                "does not define get_ref + a move constructor for value type.");
            iterable_definition<iterable_t>::set(iterable, cursor,
                                                 std::move(value));
        }
    }
};

struct begin_fn_t
{
    template <typename iterable_t>
    inline constexpr auto operator()
        [[nodiscard]] (const iterable_t& iterable) const
        OKAYLIB_NOEXCEPT->decltype(iterable_for<iterable_t>::begin(iterable))
    {
        return iterable_for<iterable_t>::begin(iterable);
    }
};

struct end_fn_t
{
    template <typename iterable_t>
    inline constexpr auto operator()
        [[nodiscard]] (const iterable_t& iterable) const
        OKAYLIB_NOEXCEPT->decltype(iterable_for<iterable_t>::end(iterable))
    {
        return iterable_for<iterable_t>::end(iterable);
    }
};

struct size_fn_t
{
    template <typename iterable_t>
    inline constexpr auto
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
    inline constexpr auto operator()
        [[nodiscard]] (const iterable_t& iterable) const
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
inline constexpr detail::iter_copyout_fn_t iter_copyout{};

/// Get a temporary const reference to an item. More optimizable than iter_get
/// (its possible for any copying to be elided) but the reference is only valid
/// until the end of the execution of the calling function. Supported by all
/// types which implement at least one of `iter_get()`, `iter_deref() const`, or
/// `iter_deref()`.
/// Returns either a iterable_t::value_type, or a const iterable_t::value_type&.
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

inline constexpr detail::end_fn_t end{};

inline constexpr detail::size_fn_t size{};

inline constexpr detail::size_fn_t data{};

} // namespace ok

#endif
