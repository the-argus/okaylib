#ifndef __OKAYLIB_RANGES_RANGE_DEFINITION_H__
#define __OKAYLIB_RANGES_RANGE_DEFINITION_H__

#include "okay/detail/type_traits.h"

namespace ok {
// clang-format off
enum class range_flags
{
    none                = 0b00000000,
    producing           = 0b00000001,
    consuming           = 0b00000010,
    infinite            = 0b00000100,
    finite              = 0b00001000,
    sized               = 0b00010000,
    arraylike           = 0b00100000,
    implements_set      = 0b01000000,
    ref_wrapper         = 0b10000000,
};

/// Normally, range features are determined by what functions are implemented
/// on the range. range_strict_flags are an exception which prevent template
/// machinery from selecting certain operators. These flags exist because there
/// was a lot of machinery involved with deleting functions and it made writing
/// and testing views difficult
enum class range_strict_flags
{
    none                                = 0b00000000000000,
    disallow_range_def_increment        = 0b00000000000010,
    disallow_cursor_member_increment    = 0b00000000000100,
    disallow_range_def_decrement        = 0b00000000001000,
    disallow_cursor_member_decrement    = 0b00000000010000,
    disallow_range_def_offset           = 0b00000000100000,
    disallow_cursor_member_offset       = 0b00000001000000,
    disallow_range_def_compare          = 0b00000010000000,
    disallow_cursor_member_compare      = 0b00000100000000,
    disallow_get                        = 0b00001000000000,
    disallow_set                        = 0b00010000000000,
    disallow_begin                      = 0b00100000000000,
    disallow_size                       = 0b01000000000000,
    disallow_is_inbounds                = 0b10000000000000,
};
// clang-format on

constexpr range_flags operator|(range_flags a, range_flags b)
{
    using flags = range_flags;
    return static_cast<flags>(static_cast<std::underlying_type_t<flags>>(a) |
                              static_cast<std::underlying_type_t<flags>>(b));
}

/// Removes all flags in b from a
constexpr range_flags operator-(range_flags a, range_flags b)
{
    using flags = range_flags;
    const auto shared_flags = static_cast<std::underlying_type_t<flags>>(a) &
                              static_cast<std::underlying_type_t<flags>>(b);
    // only leave on ones that were exclusively on in `a`
    return static_cast<flags>(shared_flags ^
                              static_cast<std::underlying_type_t<flags>>(a));
}

constexpr range_flags& operator|=(range_flags& a, range_flags b)
{
    a = a | b;
    return a;
}

constexpr range_flags& operator-=(range_flags& a, range_flags b)
{
    a = a - b;
    return a;
}

constexpr bool operator&(range_flags a, range_flags b)
{
    using flags = range_flags;
    return static_cast<std::underlying_type_t<flags>>(a) &
           static_cast<std::underlying_type_t<flags>>(b);
}

constexpr range_strict_flags operator|(range_strict_flags a,
                                       range_strict_flags b)
{
    using flags = range_strict_flags;
    return static_cast<flags>(static_cast<std::underlying_type_t<flags>>(a) |
                              static_cast<std::underlying_type_t<flags>>(b));
}

/// Removes all flags in b from a
constexpr range_strict_flags operator-(range_strict_flags a,
                                       range_strict_flags b)
{
    using flags = range_strict_flags;
    const auto added = static_cast<std::underlying_type_t<flags>>(a) |
                       static_cast<std::underlying_type_t<flags>>(b);
    return static_cast<flags>(added ^
                              static_cast<std::underlying_type_t<flags>>(b));
}

constexpr range_strict_flags& operator|=(range_strict_flags& a,
                                         range_strict_flags b)
{
    a = a | b;
    return a;
}

constexpr range_strict_flags& operator-=(range_strict_flags& a,
                                         range_strict_flags b)
{
    a = a - b;
    return a;
}

constexpr bool operator&(range_strict_flags a, range_strict_flags b)
{
    using flags = range_strict_flags;
    return static_cast<std::underlying_type_t<flags>>(a) &
           static_cast<std::underlying_type_t<flags>>(b);
}

constexpr bool range_strict_flags_validate(range_flags rflags,
                                           range_strict_flags sflags)
{
    using sflags_e = range_strict_flags;
    using rflags_e = range_flags;

    // a range is only allowed to not implement begin if it is arraylike
    if (sflags & sflags_e::disallow_begin && !(rflags & rflags_e::arraylike))
        return false;

    // cases where you can avoid implementing size(): arraylike, infinite,
    // or finite
    if (sflags & sflags_e::disallow_size &&
        !(rflags & rflags_e::arraylike || rflags & rflags_e::finite ||
          rflags & rflags_e::infinite))
        return false;

    const bool disallows_get = sflags & sflags_e::disallow_get;
    const bool producing = rflags & rflags_e::producing;

    // a get() function is the only way for a range to produce something
    if (disallows_get && producing)
        return false;

    return true;
}

template <typename range_t> struct range_definition
{
    // if a range definition includes a `static bool deleted = true`, then
    // it is ignored and the type is considered an invalid range.
    static constexpr bool deleted = true;
};

template <typename T, typename = void>
struct inherited_range_type : public ok::false_type
{
    using type = void;
};
template <typename T>
struct inherited_range_type<T, std::void_t<typename T::inherited_range_type>>
    : public ok::true_type
{
    using type = typename T::inherited_range_type;
};

template <typename T>
constexpr bool has_inherited_range_type_v = inherited_range_type<T>::value;
} // namespace ok

#endif
