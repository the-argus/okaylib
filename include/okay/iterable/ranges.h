#ifndef __OKAYLIB_ITERABLE_RANGES_H__
#define __OKAYLIB_ITERABLE_RANGES_H__

#include "okay/detail/template_util/c_array_length.h"
#include "okay/iterable/traits.h"
#include <cstddef>

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

#define __OK_COMMA ,

namespace ok {
// eric neibler's std::begin (using function objects for begin /
// begin_for_cursor)
// https://ericniebler.com/2014/10/21/customization-point-design-in-c11-and-beyond/
namespace detail {
#define __OKAYLIB_ITERATOR_GETTER_FUNC_MEMBER_T(meta_type_name, func_name,     \
                                                array_code, ret_type)          \
    template <typename iterable_t, typename cursor_t, typename = void>         \
    struct meta_type_name                                                      \
    {};                                                                        \
                                                                               \
    template <typename iterable_t, typename cursor_t>                          \
    struct meta_type_name<                                                     \
        iterable_t, cursor_t,                                                  \
        std::enable_if_t<                                                      \
            is_input_or_output_cursor_for_iterable<iterable_t, cursor_t> &&    \
            std::is_same_v<                                                    \
                ret_type,                                                      \
                std::decay_t<decltype(std::declval<const iterable_t&>()        \
                                          .func_name())>>>>                    \
    {                                                                          \
        static inline constexpr cursor_t func_name(const iterable_t& iterable) \
        {                                                                      \
            return iterable.func_name();                                       \
        }                                                                      \
    };                                                                         \
                                                                               \
    template <typename iterable_t, typename cursor_t>                          \
    struct meta_type_name<                                                     \
        iterable_t, cursor_t,                                                  \
        std::enable_if_t<                                                      \
            is_input_or_output_cursor_for_iterable<iterable_t, cursor_t> &&    \
            std::is_same_v<                                                    \
                ret_type,                                                      \
                std::decay_t<decltype(std::declval<const iterable_t&>()        \
                                          .template func_name<cursor_t>())>>>> \
    {                                                                          \
        static inline constexpr cursor_t func_name(const iterable_t& iterable) \
        {                                                                      \
            return iterable.template func_name<cursor_t>();                    \
        }                                                                      \
    };                                                                         \
                                                                               \
    template <typename iterable_t>                                             \
    struct meta_type_name<                                                     \
        iterable_t, size_t,                                                    \
        std::enable_if_t<                                                      \
            std::is_array_v<iterable_t> ||                                     \
            std::is_array_v<std::remove_reference_t<iterable_t>>>>             \
    {                                                                          \
        static inline constexpr size_t func_name(const iterable_t& iterable)   \
        {                                                                      \
            array_code                                                         \
        }                                                                      \
    };

/// Template which has a static function ::begin(iterable) to call .begin() on
/// an iterable type, returning a cursor_t, if such a function exists. If the
/// type is an array, then return 0 as the beginning. Otherwise there is no such
/// function.
__OKAYLIB_ITERATOR_GETTER_FUNC_MEMBER_T(begin_meta_t, begin, return 0;
                                        , std::decay_t<cursor_t>)
/// Template to check for the same thing as begin_meta_t, but for end()
/// functions. array end() returns length of c array
__OKAYLIB_ITERATOR_GETTER_FUNC_MEMBER_T(
    end_meta_t, end, return c_array_length(iterable);
    , typename sentinel_type_for_iterable_and_cursor_meta_t<
          iterable_t __OK_COMMA cursor_t>::type)

#undef __OKAYLIB_ITERATOR_GETTER_FUNC_MEMBER_T

#define __OKAYLIB_FREE_FUNCTION_ITERATOR_GETTER_IMPL(meta_type_name,         \
                                                     func_name)              \
    template <typename iterable_t, typename cursor_t>                        \
    inline constexpr auto func_name(const iterable_t& i)                     \
        -> decltype(meta_type_name<                                          \
                    iterable_t, typename detail::default_cursor_type_meta_t< \
                                    iterable_t>::type>::func_name(i))        \
    {                                                                        \
        return meta_type_name<iterable_t,                                    \
                              typename detail::default_cursor_type_meta_t<   \
                                  iterable_t>::type>::func_name(i);          \
    }

// wrap metaprogramming types in a function which calls the best match
__OKAYLIB_FREE_FUNCTION_ITERATOR_GETTER_IMPL(begin_meta_t, begin)
__OKAYLIB_FREE_FUNCTION_ITERATOR_GETTER_IMPL(end_meta_t, end)

#undef __OKAYLIB_FREE_FUNCTION_ITERATOR_GETTER_IMPL

#define __OKAYLIB_EXPRESSION_VALID_TYPE(meta_type_name, expr)                \
    template <typename iterable_t, typename cursor_t, typename = void>       \
    struct meta_type_name : std::false_type                                  \
    {};                                                                      \
                                                                             \
    template <typename iterable_t, typename cursor_t>                        \
    struct meta_type_name<iterable_t, cursor_t, std::void_t<decltype(expr)>> \
        : std::true_type                                                     \
    {};

/// Templates to check if begin_meta_t and end_meta_t are specialized for a
/// given iterable and cursor pair, otherwise it can only rely on free function
/// implementations of begin() and end()
__OKAYLIB_EXPRESSION_VALID_TYPE(
    has_library_provided_begin_implementation_meta_t,
    begin_meta_t<iterable_t __OK_COMMA cursor_t>::begin(
        std::declval<iterable_t&>()))

__OKAYLIB_EXPRESSION_VALID_TYPE(has_library_provided_end_implementation_meta_t,
                                end_meta_t<iterable_t __OK_COMMA cursor_t>::end(
                                    std::declval<iterable_t&>()))

#undef __OKAYLIB_EXPRESSION_VALID_TYPE

#define __OKAYLIB_CHECK_CURSOR_GETTER_EXISTS(meta_type_name, func_name,    \
                                             ret_type)                     \
    template <typename iterable_t, typename cursor_t, typename = void>     \
    struct meta_type_name : std::false_type                                \
    {};                                                                    \
                                                                           \
    template <typename iterable_t, typename cursor_t>                      \
    struct meta_type_name<                                                 \
        iterable_t, cursor_t,                                              \
        std::enable_if_t<std::is_same_v<                                   \
            ret_type,                                                      \
            std::decay_t<decltype(std::declval<const iterable_t&>()        \
                                      .template func_name<cursor_t>())>>>> \
        : std::true_type                                                   \
    {};

/// Templates to check if you can call .(begin|end)<cursor_t>() on a const
/// reference to iterable_t, and get returned a cursor_t.
__OKAYLIB_CHECK_CURSOR_GETTER_EXISTS(
    has_member_function_begin_for_cursor_type_meta_t, begin,
    std::decay_t<cursor_t>)
__OKAYLIB_CHECK_CURSOR_GETTER_EXISTS(
    has_member_function_end_for_cursor_type_meta_t, end,
    typename sentinel_type_for_iterable_and_cursor_meta_t<
        iterable_t __OK_COMMA cursor_t>::type)

#undef __OKAYLIB_CHECK_CURSOR_GETTER_EXISTS
#undef __OK_COMMA

// function object type which just calls begin_meta_t default begin()
// implementation. operator() only exists if the above overload exists (the type
// is an array or defines .begin() for the given cursor type)
// This implementation always uses the default cursor type for the iterable
struct begin_fn_defaulted_t
{
    template <typename iterable_t>
    constexpr auto operator()(const iterable_t& iterable) const
    {
        static_assert(
            has_default_cursor_type_meta_t<iterable_t>::value,
            "Attempt to call begin() on iterable which does not have a "
            "deducible default cursor type. Try defining `using "
            "default_cursor_type = ...` in the class, or use "
            "begin_for_cursor<...>() to explicitly specify cursor type.");

        using default_cursor_t =
            typename detail::default_cursor_type_meta_t<iterable_t>::type;
        if constexpr (has_library_provided_begin_implementation_meta_t<
                          iterable_t, default_cursor_t>::value) {
            // use begin_meta_t implementation
            return begin_meta_t<iterable_t, default_cursor_t>::begin(iterable);
        } else {
            // allow free function overload- only for default cursor type.
            // NOTE: if this is fires, you need to define a free function
            // `begin()`, or a member function `.begin()`, either of which need
            // to return the default cursor type
            return begin(iterable);
        }
    }
};

template <typename cursor_t> struct begin_fn_t
{
    template <typename iterable_t>
    constexpr auto operator()(const iterable_t& iterable) const
    {
        // if using the default type, use default implementation which will call
        // free function overloads.
        if constexpr (has_default_cursor_type_meta_t<iterable_t>::value &&
                      std::is_same_v<
                          typename default_cursor_type_meta_t<iterable_t>::type,
                          cursor_t>) {
            constexpr begin_fn_defaulted_t default_impl = {};
            return default_impl(iterable);
        } else {
            constexpr bool has_begin_member =
                has_member_function_begin_for_cursor_type_meta_t<
                    iterable_t, cursor_t>::value;
            static_assert(
                has_begin_member,
                "Attempt to use begin_for_cursor, which requires a `template "
                "<typename cursor_t> cursor_t begin() const;` member function "
                "to be available on the iterable type, but could not find one "
                "to call.");
            // NOTE: not ideal, but we have to use member function here because
            // using undeclared free function template is c++20 only.
            return iterable.template begin<cursor_t>();
        }
    }
};

struct end_fn_defaulted_t
{
    template <typename iterable_t>
    constexpr auto operator()(const iterable_t& iterable) const
    {
        static_assert(
            has_default_cursor_type_meta_t<iterable_t>::value,
            "Attempt to call end() on iterable which does not have a "
            "deducible default sentinel type. Try defining `using "
            "sentinel_type = ...` in the class, or use "
            "end_for_cursor<...>() to explicitly specify sentinel type.");

        using default_cursor_t =
            typename detail::default_cursor_type_meta_t<iterable_t>::type;
        if constexpr (has_library_provided_end_implementation_meta_t<
                          iterable_t, default_cursor_t>::value) {
            return end_meta_t<iterable_t, default_cursor_t>::end(iterable);
        } else {
            return end(iterable);
        }
    }
};

template <typename cursor_t> struct end_fn_t
{
    template <typename iterable_t>
    constexpr auto operator()(const iterable_t& iterable) const
    {
        if constexpr (has_default_cursor_type_meta_t<iterable_t>::value &&
                      std::is_same_v<
                          typename default_cursor_type_meta_t<iterable_t>::type,
                          cursor_t>) {
            constexpr end_fn_defaulted_t default_impl = {};
            return default_impl(iterable);
        } else {
            constexpr bool has_end_member =
                has_member_function_end_for_cursor_type_meta_t<iterable_t,
                                                               cursor_t>::value;
            static_assert(
                has_end_member,
                "Attempt to use end_for_cursor, which requires a `template "
                "<typename cursor_t> sentinel_type end() const;` member "
                "function to be available on the iterable type, but could not "
                "find one to call.");
            return iterable.template end<cursor_t>();
        }
    }
};
} // namespace detail

namespace {
/// Find the beginning of an iterable, using its default cursor type.
inline constexpr auto const& begin = detail::begin_fn_defaulted_t{};

/// Find the beginning of an iterable for a specific cursor type.
template <typename cursor_t>
inline constexpr auto const& begin_for_cursor = detail::begin_fn_t<cursor_t>{};

/// Find the sentinel of an iterable, using its default cursor/sentinel type.
/// Return type: sentinel associated with iterable and cursor. (default
/// sentinel)
inline constexpr auto const& end = detail::end_fn_defaulted_t{};

/// Find the sentinel of an iterable for a specific cursor type.
/// Return type: sentinel associated with iterable and cursor.
template <typename cursor_t>
inline constexpr auto const& end_for_cursor = detail::end_fn_t<cursor_t>{};
} // namespace

namespace detail {
template <typename, typename = void> struct is_valid_range_t : std::false_type
{};

template <typename range_t>
struct is_valid_range_t<
    range_t, std::void_t<decltype(ok::end(std::declval<const range_t&>()),
                                  ok::begin(std::declval<const range_t&>()))>>
    : std::true_type
{};
} // namespace detail

/// this will be true if its valid to call ok::begin() and ok::end() on the type
/// T
template <typename T>
inline constexpr bool is_valid_range = detail::is_valid_range_t<T>::value;

} // namespace ok

#endif
