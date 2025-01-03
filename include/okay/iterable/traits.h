#ifndef __OKAYLIB_ITERABLE_TRAITS_H__
#define __OKAYLIB_ITERABLE_TRAITS_H__

#include <type_traits>
#include <utility>

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

namespace detail {

// iterable traits ---------------

// template to check if a type T has T::value_type
template <typename, typename = void>
struct has_value_type_member_t : public std::false_type
{};
template <typename T>
struct has_value_type_member_t<T, std::void_t<typename T::value_type>>
    : public std::true_type
{};

template <typename, typename = void>
struct has_default_cursor_type_member : public std::false_type
{};
template <typename T>
struct has_default_cursor_type_member<
    T, std::void_t<typename T::default_cursor_type>> : public std::true_type
{};

template <typename T>
inline constexpr bool is_potential_iterable =
    std::is_object_v<T> && !std::is_array_v<T> &&
    has_value_type_member_t<T>::value;

// cursor traits ----------------

template <typename, typename = void>
class has_pre_increment_t : public std::false_type
{};
template <typename T>
class has_pre_increment_t<T, std::void_t<decltype(++std::declval<T&>())>>
    : public std::true_type
{};

template <typename, typename = void>
class has_pre_decrement_t : public std::false_type
{};
template <typename T>
class has_pre_decrement_t<T, std::void_t<decltype(--std::declval<T&>())>>
    : public std::true_type
{};

template <typename, typename = void>
class has_addition_with_size_t : public std::false_type
{};
template <typename T>
class has_addition_with_size_t<
    T, std::void_t<decltype(std::declval<const T&>() + std::size_t{},
                            std::size_t{} + std::declval<const T&>())>>
    : public std::true_type
{};

template <typename, typename = void>
class has_subtraction_with_size_t : public std::false_type
{};
template <typename T>
class has_subtraction_with_size_t<
    T, std::void_t<decltype(std::declval<const T&>() - std::size_t{},
                            std::size_t{} - std::declval<const T&>())>>
    : public std::true_type
{};

template <typename, typename = void>
class has_inplace_addition_with_size_t : public std::false_type
{};
template <typename T>
class has_inplace_addition_with_size_t<
    T, std::void_t<decltype(std::declval<T&>() += std::size_t{})>>
    : public std::true_type
{};

template <typename, typename = void>
class has_inplace_subtraction_with_size_t : public std::false_type
{};
template <typename T>
class has_inplace_subtraction_with_size_t<
    T, std::void_t<decltype(std::declval<T&>() -= std::size_t{})>>
    : public std::true_type
{};

template <typename, typename = void>
class has_comparison_operators : public std::false_type
{};
template <typename T>
class has_comparison_operators<
    T,
    std::enable_if_t<std::is_same_v<bool, decltype(std::declval<const T&>() <
                                                   std::declval<const T&>())> &&
                     std::is_same_v<bool, decltype(std::declval<const T&>() >
                                                   std::declval<const T&>())> &&
                     std::is_same_v<bool, decltype(std::declval<const T&>() <=
                                                   std::declval<const T&>())> &&
                     std::is_same_v<bool, decltype(std::declval<const T&>() >=
                                                   std::declval<const T&>())>>>
    : public std::true_type
{};

template <typename T>
inline constexpr bool is_potential_cursor =
    std::is_object_v<T> && !std::is_array_v<T> && has_pre_increment_t<T>::value;

// sentinel traits ------------

template <typename T, typename = void>
class has_sentinel_type_meta_t : public std::false_type
{};
template <typename T>
class has_sentinel_type_meta_t<T, std::void_t<typename T::sentinel_type>>
    : public std::true_type
{};

template <typename LHS, typename RHS, typename = void>
class is_equality_comparable_to_meta_t : public std::false_type
{};
template <typename LHS, typename RHS>
class is_equality_comparable_to_meta_t<
    LHS, RHS,
    std::enable_if_t<std::is_same_v<decltype(std::declval<const LHS&>() ==
                                             std::declval<const RHS&>()),
                                    bool>>> : public std::true_type
{};

template <typename LHS, typename RHS>
inline constexpr bool is_equality_comparable_to =
    is_equality_comparable_to_meta_t<LHS, RHS>::value;

/// Checks if a given iterable and cursor have a corresponding sentinel type.
template <typename iterable_t, typename cursor_t, typename = void>
struct sentinel_type_for_iterable_and_cursor_meta_t
{};

/// If iterable_t defines iterable_t::sentinel_type, and that type is comparable
/// to cursor_t, then iterable_t::sentinel_type is used.
template <typename iterable_t, typename cursor_t>
struct sentinel_type_for_iterable_and_cursor_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<has_sentinel_type_meta_t<iterable_t>::value &&
                     is_equality_comparable_to<
                         cursor_t, typename iterable_t::sentinel_type>>>
{
    using type = typename iterable_t::sentinel_type;
    static_assert(is_equality_comparable_to<cursor_t, type>,
                  "Invalid sentinel_type");
};

/// If iterable_t does not define iterable_t::sentinel_type or the given type is
/// not comparable, then the sentinel type is the cursor type
template <typename iterable_t, typename cursor_t>
struct sentinel_type_for_iterable_and_cursor_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<!has_sentinel_type_meta_t<iterable_t>::value &&
                     is_equality_comparable_to<cursor_t, cursor_t>>>
{
    using type = cursor_t;
};

/// Check if a given iterable and a cursor have a corresponding sentinel type
template <typename iterable_t, typename cursor_t, typename = void>
class have_sentinel_meta_t : public std::false_type
{};

// iterator and cursor only have a sentinel if the
// sentinel_type_for_iterable_and_cursor_meta_t trait is specialized
template <typename iterable_t, typename cursor_t>
class have_sentinel_meta_t<
    iterable_t, cursor_t,
    // default template for this does not have ::type member, should work like
    // enable_if
    std::void_t<typename sentinel_type_for_iterable_and_cursor_meta_t<
        iterable_t, cursor_t>::type>> : public std::true_type
{};

template <typename T>
inline constexpr bool is_potential_sentinel =
    std::is_object_v<T> && !std::is_array_v<T>;

/// Get the default cursor type of an iterable. Its ::type member will be the
/// type, unless no default cursor type can be determined, in which case there
/// is no ::type member;
template <typename T, typename = void> struct default_cursor_type_meta_t
{};

template <typename T>
struct default_cursor_type_meta_t<
    T, std::enable_if_t<is_potential_iterable<T> &&
                        has_default_cursor_type_member<T>::value>>
{
    using type = typename T::default_cursor_type;
    static_assert(is_potential_cursor<type>,
                  "Default cursor type cannot be used as a cursor with the "
                  "given iterable type.");
    static_assert(have_sentinel_meta_t<T, type>::value,
                  "Default cursor type does not have a corresponding sentinel "
                  "type with the given iterable.");
};

template <typename T>
struct default_cursor_type_meta_t<
    T, std::enable_if_t<std::is_array_v<T> ||
                        std::is_array_v<std::remove_reference_t<T>>>>
{
    using type = std::size_t;
};

/// Template to check if a given type has a default cursor type defined- either
/// through a ::default_cursor_type member type or by being an array.
template <typename T, typename = void>
struct has_default_cursor_type_meta_t : public std::false_type
{};

template <typename T>
struct has_default_cursor_type_meta_t<
    T, std::void_t<typename default_cursor_type_meta_t<T>::type>>
    : public std::true_type
{};

/// check if a sentinel is valid for an iterable and a cursor- does not actually
/// check if the iterable and cursor are valid.
template <typename I, typename C, typename S>
inline constexpr bool is_valid_sentinel =
    is_potential_sentinel<S> && have_sentinel_meta_t<I, C>::value &&
    std::is_same_v<
        S, typename sentinel_type_for_iterable_and_cursor_meta_t<I, C>::type>;

// template to check if an iterable has an `iterable_t::value_type` member type,
// and defines a `value_type iter_get(const cursor_t&) const;`
// function.
// Precondition: iterable_t and cursor_t are valid iterable and cursor pair
// (have a sentinel)
template <typename iterable_t, typename cursor_t, typename = void>
class iterable_has_iter_get_member_t : public std::false_type
{};

template <typename iterable_t, typename cursor_t>
class iterable_has_iter_get_member_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<const iterable_t&>().iter_get(
                           std::declval<const cursor_t&>())),
                       typename iterable_t::value_type>>>
    : public std::true_type
{};

// template to check if iterable has an `iterable_t::value_type` member type,
// and defines a `void iter_set(const cursor_t&, value_type&&)`
// Precondition: iterable_t and cursor_t are valid iterable and cursor pair
// (have a sentinel)
template <typename iterable_t, typename cursor_t, typename = void>
class iterable_has_iter_set_member_t : public std::false_type
{};

template <typename iterable_t, typename cursor_t>
class iterable_has_iter_set_member_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<iterable_t&>().iter_set(
                           std::declval<const cursor_t&>(),
                           std::declval<typename iterable_t::value_type&&>())),
                       void>>> : std::true_type
{};

// template to check if iterable has an `interable_t::value_type` member type,
// and defines a `value_type& iter_get_ref(const cursor_t&);`
// Precondition: iterable_t and cursor_t are valid iterable and cursor pair
// (have a sentinel)
template <typename iterable_t, typename cursor_t, typename = void>
class iterable_has_iter_get_ref_member_t : public std::false_type
{};

template <typename iterable_t, typename cursor_t>
class iterable_has_iter_get_ref_member_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<iterable_t&>().iter_get_ref(
                           std::declval<const cursor_t&>())),
                       typename iterable_t::value_type&>>>
    : public std::true_type
{};

// template to check if iterable has an `interable_t::value_type` member type,
// and defines a `value_type& operator[](const cursor_t&);`. Functionally
// equivalent to iter_get_ref(), but included for backwards compatibility with
// STL containers. Precondition: iterable_t and cursor_t are valid iterable and
// cursor pair (have a sentinel)
template <typename iterable_t, typename cursor_t, typename = void>
class iterable_has_iter_get_ref_array_operator_t : public std::false_type
{};

template <typename iterable_t, typename cursor_t>
class iterable_has_iter_get_ref_array_operator_t<
    iterable_t, cursor_t,
    std::enable_if_t<std::is_same_v<
        decltype(std::declval<iterable_t&>()[std::declval<const cursor_t&>()]),
        typename iterable_t::value_type&>>> : public std::true_type
{};

// template to check if iterable has an `interable_t::value_type` member type,
// and defines a `const value_type& iter_get_ref(const cursor_t&) const;`
// Precondition: iterable_t and cursor_t are valid iterable and cursor pair
// (have a sentinel)
template <typename iterable_t, typename cursor_t, typename = void>
class iterable_has_iter_get_ref_const_member_t : public std::false_type
{};

template <typename iterable_t, typename cursor_t>
class iterable_has_iter_get_ref_const_member_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<const iterable_t&>().iter_get_ref(
                           std::declval<const cursor_t&>())),
                       const typename iterable_t::value_type&>>>
    : public std::true_type
{};

// template to check if iterable has an `interable_t::value_type` member type,
// and defines a `const value_type& operator[](const cursor_t&) const;`.
// Functionally equivalent to iter_get_ref() const but included for backwards
// compatibility with STL containers. Precondition: iterable_t and cursor_t are
// valid iterable and cursor pair (have a sentinel)
template <typename iterable_t, typename cursor_t, typename = void>
class iterable_has_iter_get_ref_const_array_operator_t : public std::false_type
{};

template <typename iterable_t, typename cursor_t>
class iterable_has_iter_get_ref_const_array_operator_t<
    iterable_t, cursor_t,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<const iterable_t&>()
                                    [std::declval<const cursor_t&>()]),
                       const typename iterable_t::value_type&>>>
    : public std::true_type
{};

// shorthand / helper template to check if an iterable has either operator[] or
// iter_get_ref() for getting lvalue references
template <typename iterable_t, typename cursor_t>
inline constexpr bool iterable_can_get_mutable_ref =
    iterable_has_iter_get_ref_array_operator_t<iterable_t, cursor_t>{} ||
    iterable_has_iter_get_ref_member_t<iterable_t, cursor_t>{};

// shorthand / helper template to check if an iterable has either operator[] or
// iter_get_ref() for getting *const* lvalue references
template <typename iterable_t, typename cursor_t>
inline constexpr bool iterable_can_get_const_ref =
    iterable_has_iter_get_ref_const_array_operator_t<iterable_t, cursor_t>{} ||
    iterable_has_iter_get_ref_const_member_t<iterable_t, cursor_t>{};

template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_input_cursor_for_iterable =
    is_potential_cursor<maybe_cursor_t> &&
    is_potential_iterable<maybe_iterable_t> &&
    have_sentinel_meta_t<maybe_iterable_t, maybe_cursor_t>::value &&
    (iterable_has_iter_get_member_t<maybe_iterable_t, maybe_cursor_t>::value ||
     iterable_can_get_const_ref<maybe_iterable_t, maybe_cursor_t> ||
     iterable_can_get_mutable_ref<maybe_iterable_t, maybe_cursor_t>);

template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_output_cursor_for_iterable =
    is_potential_cursor<maybe_cursor_t> &&
    is_potential_iterable<maybe_iterable_t> &&
    have_sentinel_meta_t<maybe_iterable_t, maybe_cursor_t>::value &&
    (iterable_has_iter_set_member_t<maybe_iterable_t, maybe_cursor_t>::value ||
     iterable_can_get_mutable_ref<maybe_iterable_t, maybe_cursor_t>);

/// Template to check if a type T satisfies `is_cursor_base_incrementable_t` and
/// the iterable_t defines either an `iter_get` or `iter_set` or `iter_get_ref`
/// function which accept a const T&.
/// No preconditions, checks if both the cursor and the iterable are valid and
/// if it is true then both are valid.
template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_input_or_output_cursor_for_iterable =
    is_input_cursor_for_iterable<maybe_iterable_t, maybe_cursor_t> ||
    is_output_cursor_for_iterable<maybe_iterable_t, maybe_cursor_t>;

// single pass iterables are by definition the minimum iterable- so any valid
// input or output iterable is also a single pass iterable.
template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_single_pass_iterable =
    is_input_or_output_cursor_for_iterable<maybe_iterable_t, maybe_cursor_t>;

// a multi pass iterable is copyable- enabling consumers to pause and come back
// to copies of iterables at previous positions.
template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_multi_pass_iterable =
    is_single_pass_iterable<maybe_iterable_t, maybe_cursor_t> &&
    std::is_copy_constructible_v<maybe_cursor_t> &&
    std::is_copy_assignable_v<maybe_cursor_t>;

template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_bidirectional_iterable =
    is_multi_pass_iterable<maybe_iterable_t, maybe_cursor_t> &&
    // can also be decremented
    has_pre_decrement_t<maybe_cursor_t>::value;

template <typename maybe_iterable_t, typename maybe_cursor_t>
inline constexpr bool is_random_access_iterable =
    is_bidirectional_iterable<maybe_iterable_t, maybe_cursor_t> &&
    // must also be able to do +, +=, -, -=, <, >, <=, >=
    has_addition_with_size_t<maybe_cursor_t>::value &&
    has_subtraction_with_size_t<maybe_cursor_t>::value &&
    has_inplace_addition_with_size_t<maybe_cursor_t>::value &&
    has_inplace_subtraction_with_size_t<maybe_cursor_t>::value &&
    has_comparison_operators<maybe_cursor_t>::value;

} // namespace detail

#define OKAYLIB_ITERABLE_INVALID_MSG "Invalid type passed in as iterable."
#define OKAYLIB_CURSOR_INVALID_MSG "Invalid type passed in as cursor."
#define OKAYLIB_IC_PAIR_INVALID_MSG                                           \
    "Unable to determine a sentinel type for the given iterable and cursor, " \
    "invalid pair. Try specifying a `using sentinel_type = ...` in the "      \
    "iterable declaration."

template <typename iterable_t, typename cursor_t>
inline constexpr const auto&
iter_get_const_ref(const iterable_t& iterable,
                   const cursor_t& cursor) OKAYLIB_NOEXCEPT
{
    static_assert(detail::is_potential_iterable<iterable_t>,
                  OKAYLIB_ITERABLE_INVALID_MSG);
    static_assert(detail::is_potential_cursor<cursor_t>,
                  OKAYLIB_CURSOR_INVALID_MSG);
    static_assert(detail::have_sentinel_meta_t<iterable_t, cursor_t>::value,
                  OKAYLIB_IC_PAIR_INVALID_MSG);
    static_assert(detail::iterable_can_get_const_ref<iterable_t, cursor_t>,
                  "Cannot get const reference from the given iterable type, "
                  "for that cursor type.");

    if constexpr (detail::iterable_has_iter_get_ref_const_member_t<
                      iterable_t, cursor_t>::value) {
        return iterable.get_ref(cursor);
    } else {
        static_assert(detail::iterable_has_iter_get_ref_const_array_operator_t<
                      iterable_t, cursor_t>::value);
        return iterable[cursor];
    }
}

/// Get a copy of the data pointed at by an iterator. Works on iterables
/// implementing `iter_get()`, or `iter_deref() [const]` if value_type is copy
/// constructible.
/// Returns iterable_t::value_type.
template <typename iterable_t, typename cursor_t>
inline constexpr auto iter_copyout(const iterable_t& iterable,
                                   const cursor_t& cursor) OKAYLIB_NOEXCEPT
{
    static_assert(detail::is_potential_iterable<iterable_t>,
                  OKAYLIB_ITERABLE_INVALID_MSG);
    static_assert(detail::is_potential_cursor<cursor_t>,
                  OKAYLIB_CURSOR_INVALID_MSG);
    static_assert(detail::have_sentinel_meta_t<iterable_t, cursor_t>::value,
                  OKAYLIB_IC_PAIR_INVALID_MSG);

    constexpr bool has_iter_get =
        detail::iterable_has_iter_get_member_t<iterable_t, cursor_t>::value;
    constexpr bool can_get_const_ref =
        detail::iterable_can_get_const_ref<iterable_t, cursor_t>;
    constexpr bool can_get_mutable_ref =
        detail::iterable_can_get_mutable_ref<iterable_t, cursor_t>;
    static_assert(
        // valid: you have only iter_get
        (has_iter_get && (!can_get_const_ref && !can_get_mutable_ref))
            // valid: you have only const ref
            || (!can_get_mutable_ref && can_get_const_ref)
            // valid: you have both const and mutable ref
            || (can_get_const_ref && can_get_mutable_ref)
            // valid: you have none of them (checked in the next assert)
            || (!has_iter_get && !can_get_const_ref && !can_get_mutable_ref),
        "Invalid getters on iterable. If defining iter_get(), do not define "
        "any of operator[] or iter_get_ref() for that cursor. And, if you "
        "define a nonconst iter_get_ref() or operator[], define a const "
        "reference overload.");
    // thanks to above assert, can_get_const_ref implies can_get_mutable_ref
    static_assert(has_iter_get || can_get_const_ref,
                  "Cannot copy out a value from given iterable with given "
                  "cursor type- it is only an output cursor.");

    if constexpr (has_iter_get) {
        return iterable.iter_get(cursor);
    } else {
        static_assert(can_get_const_ref);
        if constexpr (detail::iterable_has_iter_get_ref_const_member_t<
                          iterable_t, cursor_t>::value) {
            return iterable.iter_get_ref(cursor);
        } else {
            static_assert(
                detail::iterable_has_iter_get_ref_const_array_operator_t<
                    iterable_t, cursor_t>::value);
            return iterable[cursor];
        }
    }
}

namespace detail {
template <typename iterable_t, typename cursor_t, typename = void>
struct iter_get_temporary_ref_meta_t
{
    using type = void;
    static inline constexpr void call(const iterable_t& iterable,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        static_assert(false,
                      "Attempt to call iter_get_temporary_ref, but iterable "
                      "type does not have a iterable_t::value_type defined.");
    }
};

template <typename iterable_t, typename cursor_t>
struct iter_get_temporary_ref_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<detail::has_value_type_member_t<iterable_t>::value &&
                     !detail::iterable_can_get_const_ref<iterable_t, cursor_t>>>
{
    using type = typename iterable_t::value_type;
    static inline constexpr type call(const iterable_t& iterable,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return iter_copyout(iterable, cursor);
    }
};

template <typename iterable_t, typename cursor_t>
struct iter_get_temporary_ref_meta_t<
    iterable_t, cursor_t,
    std::enable_if_t<detail::has_value_type_member_t<iterable_t>::value &&
                     detail::iterable_can_get_const_ref<iterable_t, cursor_t>>>
{
    using type = const typename iterable_t::value_type&;
    static inline constexpr type call(const iterable_t& iterable,
                                      const cursor_t& cursor) OKAYLIB_NOEXCEPT
    {
        return iter_get_const_ref(iterable, cursor);
    }
};

} // namespace detail

/// Get a temporary const reference to an item. More optimizable than iter_get
/// (its possible for any copying to be elided) but the reference is only valid
/// until the end of the execution of the calling function. Supported by all
/// types which implement at least one of `iter_get()`, `iter_deref() const`, or
/// `iter_deref()`.
/// Returns either a iterable_t::value_type, or a const iterable_t::value_type&.
/// If the return value is a reference, then it is the same as calling
/// iter_get_ref. It is always valid to store the return value in a `const
/// iterable_t::value_type&`
template <typename iterable_t, typename cursor_t>
inline constexpr
    typename detail::iter_get_temporary_ref_meta_t<iterable_t, cursor_t>::type
    iter_get_temporary_ref(const iterable_t& iterable,
                           const cursor_t& cursor) OKAYLIB_NOEXCEPT
{
    return detail::iter_get_temporary_ref_meta_t<iterable_t, cursor_t>::call(
        iterable, cursor);
    static_assert(detail::is_potential_iterable<iterable_t>,
                  OKAYLIB_ITERABLE_INVALID_MSG);
    static_assert(detail::is_potential_cursor<cursor_t>,
                  OKAYLIB_CURSOR_INVALID_MSG);
    static_assert(detail::have_sentinel_meta_t<iterable_t, cursor_t>::value,
                  OKAYLIB_IC_PAIR_INVALID_MSG);
    static_assert(detail::is_input_cursor_for_iterable<iterable_t, cursor_t>,
                  "Attempt to read from non-input iterable cursor");
}

/// Returns an lvalue reference to the internel representation of the data
/// pointed at by the iterator. This reference is invalidated based on
/// container-specific behavior.
template <typename iterable_t, typename cursor_t>
inline constexpr auto& iter_get_ref(iterable_t& iterable,
                                    const cursor_t& cursor) OKAYLIB_NOEXCEPT
{
    static_assert(detail::is_potential_iterable<iterable_t>,
                  OKAYLIB_ITERABLE_INVALID_MSG);
    static_assert(detail::is_potential_cursor<cursor_t>,
                  OKAYLIB_CURSOR_INVALID_MSG);
    static_assert(detail::have_sentinel_meta_t<iterable_t, cursor_t>::value,
                  OKAYLIB_IC_PAIR_INVALID_MSG);
    static_assert(detail::iterable_can_get_mutable_ref<iterable_t, cursor_t>,
                  "Attempt to get mutable reference from iterable and cursor "
                  "which do not support mutable dereference.");
    if constexpr (detail::iterable_has_iter_get_ref_member_t<iterable_t,
                                                             cursor_t>::value) {
        return iterable.get_ref(cursor);
    } else {
        static_assert(detail::iterable_has_iter_get_ref_array_operator_t<
                      iterable_t, cursor_t>::value);
        return iterable[cursor];
    }
}

template <typename iterable_t, typename cursor_t>
inline constexpr void
iter_set(iterable_t& iterable, const cursor_t& cursor,
         typename iterable_t::value_type&& value) OKAYLIB_NOEXCEPT
{
    static_assert(detail::is_potential_iterable<iterable_t>,
                  OKAYLIB_ITERABLE_INVALID_MSG);
    static_assert(detail::is_potential_cursor<cursor_t>,
                  OKAYLIB_CURSOR_INVALID_MSG);
    static_assert(detail::have_sentinel_meta_t<iterable_t, cursor_t>::value,
                  OKAYLIB_IC_PAIR_INVALID_MSG);
    static_assert(
        detail::is_output_cursor_for_iterable<iterable_t, cursor_t>,
        "Attempt to call iter_set() on iterable and cursor which cannot "
        "take output.");

    if constexpr (detail::iterable_can_get_mutable_ref<iterable_t, cursor_t>) {
        iter_get_ref(iterable, cursor) = std::move(value);
    } else {
        static_assert(detail::iterable_has_iter_set_member_t<iterable_t,
                                                             cursor_t>::value);
        iterable.iter_set(std::move(value));
    }
}

} // namespace ok

#endif
