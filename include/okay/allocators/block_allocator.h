#ifndef __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"

namespace ok

{

class block_allocator_t : public ok::allocator_t
{

constexpr explicit block_allocator_t(bytes_t initial_buffer);
constexpr explicit block_allocator_t(bytes_t&& initial_buffer, ok);

};

} // namespace ok

#endif
