#ifndef __OKAY_POINTER_H__
#define __OKAY_POINTER_H__

/*
 * This file defines various types of pointers:
 * - ptr<T>, which is just a pointer but non-nullable
 * - arc<T> which is a std::shared_ptr equivalent
 * - weak<T> which is a std::weak_ptr equivalent
 */

#include "okay/detail/addressof.h"
#include "okay/detail/type_traits.h"

namespace ok {
template <typename T>
    requires stdc::is_pointer_v<T*> // T* should compile
class ptr
{
  public:
    constexpr explicit ptr(T& t) : m_realptr(ok::addressof(t)) {}

    ptr() = delete;
    constexpr ptr(const ptr&) = default;
    constexpr ptr& operator=(const ptr&) = default;
    constexpr ptr(ptr&&) = default;
    constexpr ptr& operator=(ptr&&) = default;

    constexpr T& operator*() const { return *m_realptr; }
    constexpr T* operator->() const { return m_realptr; }

  private:
    T* m_realptr;
};

class voidptr
{
  public:
  private:
    void* m_ptr;
    size_t m_typehash;
};

} // namespace ok

#endif
