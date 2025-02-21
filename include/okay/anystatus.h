#ifndef __OKAYLIB_ANYSTATUS_H__
#define __OKAYLIB_ANYSTATUS_H__

#include "okay/detail/noexcept.h"
#include "okay/detail/traits/is_status_enum.h"
#include <cstdint>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {

enum class nondescriptive_error : uint8_t
{
    okay = 0,
    no_value = 1,
};

struct anystatus_t
{
  private:
    uint8_t m_status;

    constexpr anystatus_t(bool is_okay) OKAYLIB_NOEXCEPT
        : m_status(is_okay ? 0 : 255)
    {
    }

  public:
    using enum_type = nondescriptive_error;

    [[nodiscard]] constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return m_status == 0;
    }

    /// Type is erased but we can at least give a number that could be mapped to
    /// the possible input error values.
    [[nodiscard]] constexpr uint8_t errcode() const OKAYLIB_NOEXCEPT
    {
        return m_status;
    }

    [[nodiscard]] constexpr enum_type err() const OKAYLIB_NOEXCEPT
    {
        return m_status == 0 ? nondescriptive_error::okay
                             : nondescriptive_error::no_value;
    }

    /// Can be constructed from a result or status
    template <typename T,
              std::enable_if_t<
                  detail::is_status_enum_v<
                      decltype(std::declval<const T&>().err())> &&
                      std::is_same_v<typename T::enum_type,
                                     decltype(std::declval<const T&>().err())>,
                  bool> = true>
    constexpr anystatus_t(const T& result) OKAYLIB_NOEXCEPT
    {
        m_status = uint8_t(result.err());
    }

    /// Can be constructed from a raw result errcode enum, which effectively
    /// just casts the enum value to a uint8_t.
    template <typename enum_t,
              std::enable_if_t<detail::is_status_enum_v<enum_t>, bool> = true>
    constexpr anystatus_t(enum_t status) OKAYLIB_NOEXCEPT
    {
        m_status = uint8_t(status);
    }

    // no_value constructor
    constexpr explicit anystatus_t() OKAYLIB_NOEXCEPT : m_status(1) {}

    static const anystatus_t success;
    static const anystatus_t failure;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<anystatus_t>;
#endif
};

inline const anystatus_t anystatus_t::success{true};
inline const anystatus_t anystatus_t::failure{false};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <> struct fmt::formatter<ok::anystatus_t>
{
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const ok::anystatus_t& result,
                                    format_context& ctx) const
    {
        if (result.okay()) {
            return fmt::format_to(ctx.out(), "[anystatus_t::okay]");
        } else {
            return fmt::format_to(ctx.out(), "[anystatus_t::{}]",
                                  uint8_t(result.err()));
        }
    }
};
#endif

#endif
