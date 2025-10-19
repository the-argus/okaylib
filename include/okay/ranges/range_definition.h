#ifndef __OKAYLIB_RANGES_RANGE_DEFINITION_H__
#define __OKAYLIB_RANGES_RANGE_DEFINITION_H__

#include "okay/detail/type_traits.h"

namespace ok {
// clang-format off
enum class range_flags
{
    producing           = 0b00000001,
    consuming           = 0b00000010,
    infinite            = 0b00000100,
    finite              = 0b00001000,
    sized               = 0b00010000,
    arraylike           = 0b00100000,
    implements_set      = 0b01000000,
    ref_wrapper         = 0b10000000,
};

/// more strict flags which, when present in the range definition, are used to
/// limit what implementations are detected/ignored. For example, if
/// can_increment and implements_increment are not defined, then the increment()
/// implementation in the range definition will be ignored.
enum class range_strict_flags
{
    none                        = 0b000000000000,
    // if use_cursor_increment is not present, then the range must define an
    // increment() function which will be used instead.
    use_cursor_increment        = 0b000000000010,
    use_def_decrement           = 0b000000000100,
    use_cursor_decrement        = 0b000000001000,
    use_def_offset              = 0b000000010000,
    use_cursor_offset           = 0b000000100000,
    use_def_compare             = 0b000001000000,
    use_cursor_compare          = 0b000010000000,
    can_get                     = 0b000100000000,
    can_set                     = 0b001000000000,
    // can only be absent if regular flags specify arraylike
    implements_begin            = 0b010000000000,
    // can only be absent if arraylike or finite or infinite
    implements_size             = 0b100000000000,
    // must always implement is_inbounds()
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
    const auto added = static_cast<std::underlying_type_t<flags>>(a) ^
                       static_cast<std::underlying_type_t<flags>>(b);
    return static_cast<flags>(added ^
                              static_cast<std::underlying_type_t<flags>>(b));
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
    if (!(sflags & sflags_e::implements_begin) &&
        !(rflags & rflags_e::arraylike))
        return false;

    // cases where you can avoid implementing size(): arraylike, infinite,
    // or finite
    if (!(sflags & sflags_e::implements_size) &&
        !(rflags & rflags_e::arraylike || rflags & rflags_e::finite ||
          rflags & rflags_e::infinite))
        return false;

    // do not try to use two implementations of decrement, offset, or compare
    if (sflags & sflags_e::use_cursor_decrement &&
        sflags & sflags_e::use_def_decrement)
        return false;
    if (sflags & sflags_e::use_cursor_offset &&
        sflags & sflags_e::use_def_offset)
        return false;
    if (sflags & sflags_e::use_cursor_compare &&
        sflags & sflags_e::use_def_compare)
        return false;

    const bool can_get = sflags & sflags_e::can_get;
    const bool can_set = sflags & sflags_e::can_set;
    const bool producing = rflags & rflags_e::producing;
    const bool consuming = rflags & rflags_e::consuming;

    // never have both can_set and can_get off
    if (!can_get && !can_set)
        return false;

    // if range can set(), then it is consuming
    if (can_set && !consuming)
        return false;

    // if range can get(), then it is producing
    if (can_get && !producing)
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
