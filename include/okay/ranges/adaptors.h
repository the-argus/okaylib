#ifndef __OKAYLIB_RANGES_ADAPTORS_H__
#define __OKAYLIB_RANGES_ADAPTORS_H__

#include "okay/detail/invoke.h"
#include "okay/detail/no_unique_addr.h"
#include "okay/detail/noexcept.h"
#include "okay/ranges/ranges.h"
#include <tuple>
#include <utility>

namespace ok::detail {

template <typename callable_t> struct range_adaptor_closure_t;

template <typename callable_t, typename... args_t> struct partial_called_t
{
    callable_t& callable;
    std::tuple<args_t...> args;

    // for CTAD- construct tuple here instead of passing it into brace
    // initializer
    constexpr partial_called_t(callable_t& c, args_t&&... a)
        : callable(c), args(std::move(a)...)
    {
    }

    template <typename range_t>
    constexpr auto operator()(range_t&& range) const&
    {
        auto forwarder = [this, &range](const args_t&... args) {
            return callable(std::forward<range_t>(range), args...);
        };
        return std::apply(forwarder, args);
    }

    template <typename range_t> constexpr auto operator()(range_t&& range) &&
    {
        auto forwarder = [this, &range](args_t&... args) {
            return callable(std::forward<range_t>(range), std::move(args)...);
        };
        return std::apply(forwarder, args);
    }

    template <typename range_t>
    constexpr auto operator()(range_t&& __r) const&& = delete;
};

template <typename callable_t> struct range_adaptor_t
{
  protected:
    OKAYLIB_NO_UNIQUE_ADDR callable_t callable;

  public:
    range_adaptor_t() = default;
    range_adaptor_t(callable_t&& _callable)
        : callable(std::forward<callable_t>(_callable))
    {
    }

    template <typename... args_t>
    constexpr auto operator()(args_t&&... args) const
    {
        if constexpr (std::is_invocable_v<callable_t, args_t...>) {
            // adaptor(range, args...)
            return ok::invoke(callable, std::forward<args_t>(args)...);
        } else {
            // adaptor(args...)(range)
            return range_adaptor_closure_t(
                partial_called_t(callable, std::move(args)...));
        }
    }
};

template <typename callable_t>
struct range_adaptor_closure_t : range_adaptor_t<callable_t>
{
    constexpr range_adaptor_closure_t(callable_t&& c) OKAYLIB_NOEXCEPT
        : range_adaptor_t<callable_t>(std::forward<callable_t>(c))
    {
    }

    range_adaptor_closure_t() = default;

    // support for C(R)
    template <typename range_t>
    constexpr auto
    operator()(range_t&& range) const OKAYLIB_NOEXCEPT->std::enable_if_t<
        range_c<range_t>,
        decltype(this->callable(std::forward<range_t>(range)))>
    {
        return this->callable(std::forward<range_t>(range));
    }

    // support for R | C to evaluate as C(R)
    template <typename range_t>
    friend constexpr decltype(auto)
    operator|(range_t&& range,
              std::enable_if_t<range_c<range_t> &&
                                   std::is_invocable_v<callable_t, range_t>,
                               const range_adaptor_closure_t&>
                  closure) OKAYLIB_NOEXCEPT
    {
        return closure.callable(std::forward<range_t>(range));
    }

    // support for C | D to produce a new range_adaptor_closure_t
    // so that (R | C) | D and R | (C | D) can be equivalent
    template <typename T>
    friend constexpr auto
    operator|(const range_adaptor_closure_t<T>& lhs,
              const range_adaptor_closure_t& rhs) OKAYLIB_NOEXCEPT
    {
        return range_adaptor_closure_t([lhs, rhs](auto&& r) {
            return std::forward<std::remove_reference_t<decltype(r)>>(r) | lhs |
                   rhs;
        });
    }
};

} // namespace ok::detail
#endif
