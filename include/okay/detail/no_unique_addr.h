#ifndef __OKAYLIB_DETAIL_NO_UNIQUE_ADDR_H__
#define __OKAYLIB_DETAIL_NO_UNIQUE_ADDR_H__

#if __cplusplus < 202002L
#define OKAYLIB_NO_UNIQUE_ADDR
#else
#ifdef _MSC_VER
#define OKAYLIB_NO_UNIQUE_ADDR [[msvc::no_unique_address]]
#else
#define OKAYLIB_NO_UNIQUE_ADDR [[no_unique_address]]
#endif
#endif

#endif
