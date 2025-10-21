#ifndef __OKAYLIB_RANGES_INDICES_H__
#define __OKAYLIB_RANGES_INDICES_H__

#include "okay/ranges/range_definition.h"

namespace ok {

struct indices_t
{};

constexpr indices_t indices{};

template <> struct range_definition<indices_t>
{
    static constexpr range_flags flags =
        range_flags::producing | range_flags::arraylike | range_flags::infinite;

    using value_type = size_t;

    static constexpr size_t get(const indices_t& i, const size_t c) noexcept
    {
        return c;
    }
};

} // namespace ok

#endif
