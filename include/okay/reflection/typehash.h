#ifndef __OKAYLIB_REFLECTION_TYPEHASH_H__
#define __OKAYLIB_REFLECTION_TYPEHASH_H__

#include "okay/reflection/nameof.h"

namespace ok {
namespace detail {
inline constexpr struct
{
} nameless_dummy;

inline constexpr uint64_t forbidden_hash =
    nameof<decltype(nameless_dummy)>().hash();
inline constexpr uint32_t forbidden_hash_32 =
    nameof<decltype(nameless_dummy)>().hash_32();
} // namespace detail

/// Get a (probably) unique 8 byte number for a given type, at compile time.
template <typename T> constexpr uint64_t typehash()
{
    /// typehash relies on hashing the name of the type. if it has no name,
    /// you get the same number back every time.
    static_assert(nameof<T>().hash() != detail::forbidden_hash,
                  "Attempt to get the typehash of an unnamed struct/class.");
    return nameof<T>().hash();
}

/// Get a (probably) unique 4 byte number for a given type, at compile time.
template <typename T> constexpr uint32_t typehash_32()
{
    static_assert(nameof<T>().hash_32() != detail::forbidden_hash_32,
                  "Attempt to get the typehash of an unnamed struct/class.");
    return nameof<T>().hash_32();
}

} // namespace ok

#endif
