#ifndef __OKAYLIB_SHORT_ARITHMETIC_TYPES_H__
#define __OKAYLIB_SHORT_ARITHMETIC_TYPES_H__

#include <cstdint>

using u8 = uint8_t;
using i8 = int8_t;
using u16 = uint16_t;
using i16 = int16_t;
using u32 = uint32_t;
using i32 = int32_t;
using u64 = uint64_t;
using i64 = int64_t;

using f32 = float;
static_assert(
    sizeof(float) == 4,
    "f32 type alias not accurate, unable to compile for this platform");

using f64 = double;
static_assert(
    sizeof(double) == 8,
    "f64 type alias not accurate, unable to compile for this platform");

#endif
