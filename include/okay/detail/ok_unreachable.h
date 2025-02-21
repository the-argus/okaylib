#ifndef __OKAYLIB_DETAIL_OK_UNREACHABLE_H__
#define __OKAYLIB_DETAIL_OK_UNREACHABLE_H__

#include "okay/detail/ok_assert.h"

inline constexpr bool __reachable = false;

#define __ok_unreachable __ok_internal_assert(__reachable)

#endif
