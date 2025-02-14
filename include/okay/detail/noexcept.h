#ifndef __OKAYLIB_DETAIL_NOEXCEPT_H__
#define __OKAYLIB_DETAIL_NOEXCEPT_H__

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#define OKAYLIB_NOEXCEPT_FORCE noexcept
#elif OKAYLIB_TESTING
#define OKAYLIB_NOEXCEPT_FORCE noexcept(false)
#else
static_assert(false,
              "Attempt to define OKAYLIB_NOEXCEPT but not OKAYLIB_TESTING");
#endif

#endif
