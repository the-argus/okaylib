#ifndef __OKAYLIB_DETAIL_OK_ADDRESSOF_H__
#define __OKAYLIB_DETAIL_OK_ADDRESSOF_H__

namespace ok {
template <class T> constexpr T* addressof(T& arg) noexcept
{
    return reinterpret_cast<T*>(
        &const_cast<char&>(reinterpret_cast<char const volatile&>(arg)));
}
} // namespace ok

#endif
