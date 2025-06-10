#ifndef __OKAYLIB_DETAIL_NO_UNIQUE_ADDR_H__
#define __OKAYLIB_DETAIL_NO_UNIQUE_ADDR_H__

#if __cplusplus < 202002L
#define OKAYLIB_NO_UNIQUE_ADDR
#else
// TODO: check for msvc, if so use [[msvc::no_unique_address]]
#define OKAYLIB_NO_UNIQUE_ADDR [[no_unique_address]]
#endif

#endif
