#ifndef __OKAYLIB_STATUS_H__
#define __OKAYLIB_STATUS_H__

#include "okay/detail/noexcept.h"
#include "okay/detail/traits/is_status_enum.h"
#include <type_traits>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {
/// Wrapper around a errcode to give it a similar interface to a result_t.
template <typename enum_t> struct status
{
  public:
    static_assert(
        detail::is_status_enum_v<enum_t>,
        "Bad enum errorcode type provided to status. Make sure it is only a "
        "byte in size, and that the okay entry is = 0.");

  private:
    enum_t m_status;

  public:
    using enum_type = enum_t;

    [[nodiscard]] constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return m_status == enum_t::okay;
    }
    [[nodiscard]] constexpr enum_t err() const OKAYLIB_NOEXCEPT
    {
        return m_status;
    }
    constexpr status(enum_t failure) OKAYLIB_NOEXCEPT { m_status = failure; }

    constexpr explicit status() OKAYLIB_NOEXCEPT
    {
        m_status = enum_t::no_value;
    }

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<status>;
#endif
};
} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename enum_t> struct fmt::formatter<ok::status<enum_t>>
{
    using status_template_t = ok::status<enum_t>;

    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const status_template_t& status,
                                    format_context& ctx) const
    {
        // TODO: use ctti to get nice typename for enum_t here
        if constexpr (fmt::is_formattable<enum_t>::value) {
            return fmt::format_to(ctx.out(), "[status::{}]", status.err());
        } else {
            if (status.okay()) {
                return fmt::format_to(ctx.out(), "[status::okay]");
            } else {
                return fmt::format_to(
                    ctx.out(), "[status::{}]",
                    std::underlying_type_t<enum_t>(status.err()));
            }
        }
    }
};
#endif
#endif
