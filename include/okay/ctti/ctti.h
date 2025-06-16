#ifndef __OKAYLIB_CTTI_H__
#define __OKAYLIB_CTTI_H__
/// CTTI: Compile Time Type Information

#include "okay/detail/ctti/pretty_function.h"
#include "okay/slice.h"

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

/// Get a pointer to a string literal and a length which contains the name of
/// the given type.
template <typename T> constexpr slice<const char> nameof()
{
    constexpr auto cstr = detail::nameof_inner<T>;
    return ok::raw_slice<const char>(*cstr.data(), cstr.size());
}

} // namespace ok::ctti
#endif
