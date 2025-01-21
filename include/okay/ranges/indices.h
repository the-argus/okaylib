#ifndef __OKAYLIB_RANGES_INDICES_H__
#define __OKAYLIB_RANGES_INDICES_H__

#include "okay/ranges/ranges.h"

namespace ok {

struct indices_t
{};

constexpr indices_t indices{};

template <> struct range_definition<indices_t>
{
    static constexpr bool infinite = true;

    static constexpr size_t begin(const indices_t& i) OKAYLIB_NOEXCEPT
    {
        return 0UL;
    }

    static constexpr bool is_inbounds(const indices_t& i,
                                      const size_t) OKAYLIB_NOEXCEPT
    {
        return true;
    }

    static constexpr size_t get(const indices_t& i,
                                const size_t c) OKAYLIB_NOEXCEPT
    {
        return c;
    }
};

} // namespace ok

#endif
