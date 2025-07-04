#ifndef __OKAYLIB_DETAIL_LIMITS_H__
#define __OKAYLIB_DETAIL_LIMITS_H__

#include "okay/detail/type_traits.h"
#include <stdint.h>

namespace ok {

template <typename T> struct numeric_limits;

template <> struct numeric_limits<uint64_t>
{
    inline static constexpr uint64_t max = -1;
};
template <> struct numeric_limits<uint32_t>
{
    inline static constexpr uint32_t max = -1;
};
template <> struct numeric_limits<uint16_t>
{
    inline static constexpr uint16_t max = -1;
};
template <> struct numeric_limits<uint8_t>
{
    inline static constexpr uint8_t max = -1;
};
// signed
template <> struct numeric_limits<int64_t>
{
    inline static constexpr int64_t max = uint64_t(-1) / 2;
};
template <> struct numeric_limits<int32_t>
{
    inline static constexpr int32_t max = uint32_t(-1) / 2;
};
template <> struct numeric_limits<int16_t>
{
    inline static constexpr int16_t max = uint16_t(-1) / 2;
};
template <> struct numeric_limits<int8_t>
{
    inline static constexpr int8_t max = uint8_t(-1) / 2;
};

} // namespace ok

#endif
