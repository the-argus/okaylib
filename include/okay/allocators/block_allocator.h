#ifndef __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__
#define __OKAYLIB_ALLOCATORS_BLOCK_ALLOCATOR_H__

#include "okay/allocators/allocator.h"

namespace ok
{
class block_allocator_t : public ok::allocator_t
{
    private:

        opt_t<allocator_t&> m_backing;
};
}

#endif
