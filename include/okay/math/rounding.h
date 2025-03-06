#ifndef __OKAYLIB_MATH_ROUNDING_H__
#define __OKAYLIB_MATH_ROUNDING_H__

#include "okay/detail/ok_assert.h"
#include <cstddef>

namespace ok {
template <size_t multiple> constexpr size_t round_up_to_multiple_of(size_t size)
{
    static_assert(multiple != 0, "Cannot align to multiple of zero.");
    if (size == 0) [[unlikely]]
        return 0;
    return (((size - 1) / multiple) + 1) * multiple;
}
constexpr size_t runtime_round_up_to_multiple_of(size_t multiple, size_t size)
{
    __ok_assert(size != 0, "bad arg: size of zero to rround_up_to_multiple_of");
    __ok_assert(multiple != 0,
                "bad arg: multiple of zero to rround_up_to_multiple_of");
    return (((size - 1) / multiple) + 1) * multiple;
}
} // namespace ok

#endif
