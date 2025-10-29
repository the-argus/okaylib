#ifndef __OKAYLIB_CTTI_H__
#define __OKAYLIB_CTTI_H__
/// CTTI: Compile Time Type Information

#include "okay/detail/ctti/pretty_function.h"

namespace ok::ctti {

namespace detail {

template <typename T>
inline constexpr cstring nameof_inner =
    filter_typename_prefix(ctti::pretty_function::type<T>().pad(
        CTTI_TYPE_PRETTY_FUNCTION_LEFT, CTTI_TYPE_PRETTY_FUNCTION_RIGHT));

inline constexpr struct
{
} nameless_dummy;

inline constexpr uint64_t forbidden_hash =
    nameof_inner<decltype(nameless_dummy)>.hash();
inline constexpr uint32_t forbidden_hash_32 =
    nameof_inner<decltype(nameless_dummy)>.hash_32();

} // namespace detail

/// Get a (probably) unique 8 byte number for a given type, at compile time.
template <typename T> constexpr uint64_t typehash()
{
    /// typehash relies on hashing the name of the type. if it has no name,
    /// you get the same number back every time.
    static_assert(detail::nameof_inner<T>.hash() != detail::forbidden_hash,
                  "Attempt to get the typehash of an unnamed struct/class.");
    return detail::nameof_inner<T>.hash();
}

/// Get a (probably) unique 4 byte number for a given type, at compile time.
template <typename T> constexpr uint32_t typehash_32()
{
    static_assert(detail::nameof_inner<T>.hash_32() !=
                      detail::forbidden_hash_32,
                  "Attempt to get the typehash of an unnamed struct/class.");
    return detail::nameof_inner<T>.hash_32();
}

/// Get a c-style null terminated string (not necessarily unique) identifier for
/// a given type
template <typename T> constexpr char* nameof()
{
    constexpr auto cstr = detail::nameof_inner<T>;
    return cstr.data();
}

} // namespace ok::ctti
#endif
