#ifndef __OKAYLIB_ITERABLE_RANGES_H__
#define __OKAYLIB_ITERABLE_RANGES_H__

#include "okay/detail/no_unique_addr.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_derived_from.h"
#include "okay/detail/traits/special_member_traits.h"
#include <tuple>
#include <utility>

/*
 * C++17 backport of ranges using iterable/cursor/sentinel instead of
 * iterator/sentinel model
 */

namespace ok::detail {

struct is_adaptor_invokable_meta_t
{
    template <typename adaptor_t, typename... args_t>
    inline static const std::false_type test(char)
    {
        return {};
    }

    template <typename adaptor_t, typename... args_t>
    inline static constexpr std::enable_if_t<
        std::is_same_v<void, std::void_t<decltype(std::declval<adaptor_t>()(
                                 std::declval<args_t>()...))>>,
        std::true_type>
    test(int)
    {
        return {};
    }

    template <typename adaptor_t, typename... args_t>
    inline static constexpr bool value =
        decltype(test<adaptor_t, args_t...>(0))::value;
};

/// Template to check if a given type adaptor_t is callable with args_t
/// paramters
template <typename adaptor_t, typename... args_t>
inline constexpr bool is_adaptor_invokable_v =
    is_adaptor_invokable_meta_t::value<adaptor_t, args_t...>;

struct is_adaptor_partial_application_viable_meta_t
{
    template <typename adaptor_t, typename... args_t>
    inline static const std::false_type test(char)
    {
        return {};
    }

    template <typename adaptor_t, typename... args_t>
    inline static constexpr std::enable_if_t<
        (adaptor_t::ok_range_num_args > 1) &&
            (sizeof...(args_t) == adaptor_t::ok_range_num_args - 1) &&
            (is_constructible_from_v<std::decay_t<args_t>, args_t> && ...),
        std::true_type>
    test(int)
    {
        return {};
    }

    template <typename adaptor_t, typename... args_t>
    inline static constexpr bool value =
        decltype(test<adaptor_t, args_t...>(0))::value;
};

template <typename adaptor_t, typename... args_t>
inline constexpr bool is_adaptor_partial_application_viable_v =
    is_adaptor_partial_application_viable_meta_t::value<adaptor_t, args_t...>;

template <typename adaptor_t, typename... args_t> struct partial_called_t;

template <typename lhs_t, typename rhs_t> struct pipe_expression_t;

struct range_adaptor_closure_t
{
    // operator | with just a range is the same as operator()(range_t)
    template <typename self_t, typename range_t>
    friend constexpr auto operator|(range_t&& range, self_t&& self)
        -> std::enable_if_t<
            is_derived_from_v<remove_cvref_t<self_t>,
                              range_adaptor_closure_t> &&
                is_adaptor_invokable_v<self_t, range_t>,
            decltype(std::forward<self_t>(self)(std::forward<range_t>(range)))>
    {
        return std::forward<self_t>(self)(std::forward<range_t>(range));
    }

    // operator | from one range adaptor closure to another
    template <typename LHS, typename RHS>
    friend constexpr auto operator|(LHS lhs, RHS rhs)
        -> std::enable_if_t<is_derived_from_v<LHS, range_adaptor_closure_t> &&
                                is_derived_from_v<RHS, range_adaptor_closure_t>,
                            pipe_expression_t<LHS, RHS>>
    {
        return pipe_expression_t<LHS, RHS>{std::move(lhs), std::move(rhs)};
    }
};

// The base class of every range adaptor non-closure.
//
// The static data member derived_t::ok_range_num_args must contain the total
// number of arguments that the adaptor takes, and the class derived_t must
// introduce range_adaptor_t::operator() into the class scope via a
// using-declaration.
//
// The optional static data member derived_t::ok_has_simple_extra_args should
// be defined to true if the behavior of this adaptor is independent of the
// constness/value category of the extra arguments.  This data member could
// also be defined as a variable template parameterized by the types of the
// extra arguments.
//
// Optional static data member derived_t::ok_has_simple_call_op should be
// defined if the type does not overload the operator() based on the constness
// or value category of itself (derived_t), for its given arguments
template <typename derived_t> struct range_adaptor_t
{
    // Partially apply the arguments to the range adaptor,
    // returning a range adaptor closure object.
    template <typename... args_t>
    constexpr auto operator()(args_t&&... args) const
        -> std::enable_if_t<
            is_adaptor_partial_application_viable_v<derived_t, args_t...>,
            partial_called_t<derived_t, std::decay_t<args_t>...>>
    {
        return partial_called_t<derived_t, std::decay_t<args_t>...>{
            std::forward<args_t>(args)...};
    }
};

// template to check if closure has ::ok_has_simple_call_op defined ------------
template <typename adaptor_t, typename = void>
class closure_has_simple_call_op_meta_t : std::false_type
{};
template <typename adaptor_t>
class closure_has_simple_call_op_meta_t<
    adaptor_t, std::void_t<decltype(adaptor_t::ok_has_simple_call_op)>>
    : std::true_type
{};
template <typename T>
inline constexpr bool closure_has_simple_call_op_v =
    closure_has_simple_call_op_meta_t<T>::value;

// template to check if adaptor has ok_has_simple_extra_args defined, either
// normally or as a template w/ arguments for operator() for which it applies
struct adaptor_has_simple_extra_args_meta_t
{
    template <typename adaptor_t, typename... args_t>
    inline static const std::false_type test(char)
    {
        return {};
    }

    // overload that gets selected if non-template ok_has_simple_extra_args is
    // defined
    template <typename adaptor_t, typename... args_t>
    inline static constexpr std::enable_if_t<
        std::is_same_v<
            void, std::void_t<decltype(adaptor_t::ok_has_simple_extra_args)>>,
        std::true_type>
    test(int)
    {
        return {};
    }

    // extra overload for templated ok_has_simple_extra_args
    template <typename adaptor_t, typename... args_t>
    inline static constexpr std::enable_if_t<
        std::is_same_v<
            void,
            std::void_t<decltype(adaptor_t::template ok_has_simple_extra_args<
                                 args_t...>)>>,
        const std::true_type>
    test(int)
    {
        return {};
    }

    template <typename adaptor_t, typename... args_t>
    inline static constexpr bool value =
        decltype(test<adaptor_t, args_t...>(0))::value;
};

template <typename adaptor_t, typename... args_t>
inline constexpr bool adaptor_has_simple_extra_args_v =
    adaptor_has_simple_extra_args_meta_t::value<adaptor_t, args_t...>;

template <typename adaptor_t, typename... args_t>
struct partial_called_t : range_adaptor_closure_t
{
    std::tuple<args_t...> m_args;

    constexpr partial_called_t(args_t... args) : m_args(std::move(args)...) {}

    template <typename range_t>
    constexpr auto
    operator()(std::enable_if_t<
               is_adaptor_invokable_v<adaptor_t, range_t, const args_t&...>,
               range_t&&>
                   range) const&
    {
        auto forwarder = [&range](const auto&... args) {
            return adaptor_t{}(std::forward<range_t>(range), args...);
        };
        return std::apply(forwarder, m_args);
    }

    // non const rvalue category overload of operator() above
    template <typename range_t>
    constexpr auto operator()(
        std::enable_if_t<is_adaptor_invokable_v<adaptor_t, range_t, args_t...>,
                         range_t&&>
            range) &&
    {
        auto forwarder = [&range](auto&... args) {
            return adaptor_t{}(std::forward<range_t>(range),
                               std::move(args)...);
        };
        return std::apply(forwarder, m_args);
    }

    template <typename range_t>
    constexpr auto operator()(range_t&& __r) const&& = delete;
};

// overload for common case where adaptor only takes single argument
template <typename adaptor_t, typename arg_t>
struct partial_called_t<adaptor_t, arg_t> : range_adaptor_closure_t
{
    arg_t m_arg;

    constexpr partial_called_t(arg_t arg) : m_arg(std::move(arg)) {}

    template <typename range_t>
    constexpr auto operator()(
        std::enable_if_t<
            is_adaptor_invokable_v<adaptor_t, range_t, const arg_t&>, range_t&&>
            range) const&
    {
        return adaptor_t{}(std::forward<range_t>(range), m_arg);
    }

    template <typename range_t>
    constexpr auto operator()(
        std::enable_if_t<is_adaptor_invokable_v<adaptor_t, range_t, arg_t>,
                         range_t&&>
            range) &&
    {
        return adaptor_t{}(std::forward<range_t>(range), std::move(m_arg));
    }

    template <typename range_t>
    constexpr auto operator()(range_t&& __r) const&& = delete;
};

// TODO: port GCC template specialization of _Partial which requires that the
// adaptor has simple call args and all arguments are trivially copyable.
// according to a comment on it, doign that means you can have only `operator()
// const` which makes diagnostics more concise. doing it without `requires` is
// going to be a bit involved, though.

template <typename LHS, typename RHS, typename range_t, typename = void>
struct is_pipe_invokable_meta_t : std::false_type
{};
template <typename LHS, typename RHS, typename range_t>
struct is_pipe_invokable_meta_t<
    LHS, RHS, range_t,
    std::void_t<decltype(std::declval<RHS>()(
        std::declval<LHS>()(std::declval<range_t>())))>> : std::true_type
{};

template <typename LHS, typename RHS, typename range_t>
inline constexpr bool is_pipe_invokable_v =
    is_pipe_invokable_meta_t<LHS, RHS, range_t>::value;

// A range adaptor closure that represents composition of the range
// adaptor closures LHS and RHS.
template <typename LHS, typename RHS>
struct pipe_expression_t : range_adaptor_closure_t
{
    OKAYLIB_NO_UNIQUE_ADDR LHS m_lhs;
    OKAYLIB_NO_UNIQUE_ADDR RHS m_rhs;

    constexpr pipe_expression_t(LHS lhs, RHS rhs)
        : m_lhs(std::move(lhs)), m_rhs(std::move(rhs))
    {
    }

    // Invoke m_rhs(m_lhs(range)) according to the value category of this
    // range adaptor closure object.
    template <typename range_t>
    constexpr auto operator()(
        std::enable_if_t<is_pipe_invokable_v<const LHS&, const RHS&, range_t>,
                         range_t&&>
            range) const&
    {
        return m_rhs(m_lhs(std::forward<range_t>(range)));
    }

    template <typename range_t>
    // requires __pipe_invocable<_Lhs, _Rhs, _Range>
    constexpr auto operator()(
        std::enable_if_t<is_pipe_invokable_v<LHS, RHS, range_t>, range_t&&>
            range) &&
    {
        return std::move(m_rhs)(std::move(m_lhs)(std::forward<range_t>(range)));
    }

    template <typename range_t>
    constexpr auto operator()(range_t&& __r) const&& = delete;
};

} // namespace ok::detail
#endif
