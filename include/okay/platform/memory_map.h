#ifndef __OKAYLIB_PLATFORM_MEMORY_MAP_H__
#define __OKAYLIB_PLATFORM_MEMORY_MAP_H__
#include <assert.h>
#include <stdint.h>

#if defined(_WIN32)
#include <errhandlingapi.h>
#include <windows.h>
// LPVOID WINAPI VirtualAlloc(LPVOID lpaddress, SIZE_T size, DWORD
// flAllocationType, DWORD flProtect);
#elif defined(__linux__) || defined(__APPLE__)
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported OS for memory_map.h"
#endif

namespace ok::mmap {

struct map_result_t
{
    void* data;
    size_t bytes;
    /// Error-code. This will be 0 on success: check that before reading
    /// bytes and data. There is no compatibility of error codes between
    /// OSes. The only guaranteed is that it will be 0 on success on all
    /// platforms.
    int64_t code;
};

/// Get the system's memory page size in bytes.
/// Can fail on linux, in which case the returned
/// value is zero
inline uint64_t get_page_size()
{
#if defined(_WIN32)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
#elif defined(__linux__)
    long result = ::sysconf(_SC_PAGESIZE);
    if (result < 0) {
        return 0;
    }
    return (uint64_t)result;
#else
    return getpagesize();
#endif
}

/// Reserve a number of pages in virtual memory space. You cannot write to
/// memory allocated by this function. Call mm::commit_pages on each page
/// you want to write to first.
/// Address hint: a hint for where the OS should try to place the
/// reservation. May be ignored if there is not space there. nullptr
/// can be provided to let the OS choose where to place the reservation.
/// Num pages: the number of pages to reserve.
inline map_result_t reserve_pages(void* address_hint, size_t num_pages)
{
    const uint64_t result = get_page_size();
    if (result == 0) {
        // code 254 means unable to get page size... unlikely
        return map_result_t{.code = 254};
    }
    size_t size = num_pages * result;
#if defined(_WIN32)
    map_result_t res;
    res.data = VirtualAlloc(address_hint, size, MEM_RESERVE, PAGE_NOACCESS);
    res.bytes = size;
    res.code = 0;

    if (res.data == NULL) {
        res.code = GetLastError();
        assert(res.code != 0);
    }
    return res;
#else
    map_result_t res;
    res.data =
        ::mmap(address_hint, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    res.bytes = size;
    res.code = 0;

    if (res.data == MAP_FAILED) {
        res.code = errno;
        res.data = NULL;
    }
    return res;
#endif
}

/// Takes a pointer to a set of memory pages which you want to make
/// readable and writable by your process, specifically ones allocated by
/// mm_reserve_pages(). It is platform dependent behavior what happens if you
/// try to change the protection on pages *not* mapped with mm_reserve_pages(),
/// and in some cases you may see a success code when calling commit_pages on
/// pages not allocated with mm_reserve_pages(). Therefore it is up to the
/// caller to ensure that does not happen. Returns 0 on success, otherwise an
/// errcode.
inline int64_t commit_pages(void* address, size_t num_pages)
{
    if (!address) {
        return -1;
    }
    const uint64_t result = mmap::get_page_size();
    if (result == 0) {
        return -1;
    }

    size_t size = num_pages * result;
#if defined(_WIN32)
    int64_t err = 0;
    if (!VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE)) {
        err = GetLastError();
        assert(err != 0);
    }
    return err;
#else
    if (::mprotect(address, size, PROT_READ | PROT_WRITE) != 0) {
        // failure case, you probably passed in an address that is not page
        // aligned.
        int32_t res = errno;
        assert(res != 0);
        return res;
    }
    return 0;
#endif
}

/// Reserve and commit a number of pages in a single system call.
inline map_result_t alloc_pages(void* address_hint, size_t num_pages)
{
    const uint64_t result = get_page_size();
    if (result == 0) [[unlikely]] {
        // code 254 means unable to get page size...
        return map_result_t{.code = 254};
    }
    size_t size = num_pages * result;
#if defined(_WIN32)
    map_result_t res;
    res.data = VirtualAlloc(address_hint, size, MEM_RESERVE | MEM_COMMIT,
                            PAGE_READWRITE);
    res.bytes = size;
    res.code = 0;

    if (res.data == NULL) {
        res.code = GetLastError();
        assert(res.code != 0);
    }
    return res;
#else
    map_result_t res;
    res.data = ::mmap(address_hint, size, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    res.bytes = size;
    res.code = 0;

    if (res.data == MAP_FAILED) {
        res.code = errno;
        res.data = NULL;
    }
    return res;
#endif
}

/// Unmap pages starting at address and continuing for "size" bytes.
inline int64_t memory_unmap(void* address, size_t size)
{
#if defined(_WIN32)
    int64_t err = 0;
    if (!VirtualFree(address, 0, MEM_RELEASE)) {
        err = GetLastError();
    }
    return err;
#else
    if (::munmap(address, size) == 0) {
        return 0;
    } else {
        return errno;
    }
#endif
}
} // namespace ok::mmap
#endif
