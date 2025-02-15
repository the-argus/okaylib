#ifndef __OKAYLIB_ANYSTATUS_H__
#define __OKAYLIB_ANYSTATUS_H__

#include "okay/detail/noexcept.h"
#include "okay/res.h"
#include "okay/status.h"
#include <cstdint>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {
struct anystatus_t
{
  private:
    uint8_t m_status;

    constexpr anystatus_t(bool is_okay) OKAYLIB_NOEXCEPT
        : m_status(is_okay ? 0 : 255)
    {
    }

  public:
    [[nodiscard]] inline constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return m_status == 0;
    }

    [[nodiscard]] inline constexpr uint8_t err() const OKAYLIB_NOEXCEPT
    {
        return m_status;
    }

    /// Can be constructed from a result, discarding the contents of the result
    /// and basically just storing the byte error code.
    template <typename payload_t, typename enum_t>
    inline constexpr anystatus_t(const res_t<payload_t, enum_t>& result)
        OKAYLIB_NOEXCEPT
    {
        m_status = uint8_t(result.err());
    }

    /// Can be constructed from a status, removing the type information of the
    /// status.
    template <typename enum_t>
    inline constexpr anystatus_t(status_t<enum_t> status) OKAYLIB_NOEXCEPT
    {
        m_status = uint8_t(status.err());
    }

    /// Can be constructed from a raw result errcode enum, which effectively
    /// just casts the enum value to a uint8_t.
    template <typename enum_t,
              std::enable_if_t<detail::is_status_enum_v<enum_t>, bool> = true>
    inline constexpr anystatus_t(enum_t status) OKAYLIB_NOEXCEPT
    {
        m_status = uint8_t(status);
    }

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
            return fmt::format_to(ctx.out(), "[anystatus_t::{}]", result.err());
        }
    }
};
#endif

#endif
