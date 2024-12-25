#ifndef __OKAYLIB_STDMEM_H__
#define __OKAYLIB_STDMEM_H__

#include "okay/slice.h"
#include <cstdint>
#include <cstring>

namespace ok {
/// Copy the contents of source into destination, byte by byte, without invoking
/// any copy constructors or destructors.
/// T must be trivially copyable.
/// If the two slices are not the same size, or if they overlap, the function
/// returns false and does nothing. Otherwise, it returns true.
template <typename T>
constexpr bool memcopy(ok::slice_t<T> destination,
                       ok::slice_t<T> source) noexcept;

/// Identical to memcopy, except it allows for sources which are smaller than
/// the destination. It will also allow copying of non-trivially-copyable types.
/// In this case, the types copy constructors will not be called, ::memcpy is
/// used
template <typename T>
constexpr bool memcopy_lenient(ok::slice_t<T> destination,
                               ok::slice_t<T> source) noexcept;

/// Compare two slices of memory, byte by byte, without invoking any == operator
/// overloads.
/// The memory can overlap and alias (in the latter case the function just
/// returns true).
/// If the two slices of memory are differently size, the function immediately
/// returns false;
template <typename T>
constexpr bool memcompare(ok::slice_t<T> memory_1,
                          ok::slice_t<T> memory_2) noexcept;

/// Check if slice "inner" points only to items also pointed at by slice "outer"
template <typename T>
constexpr bool memcontains(ok::slice_t<T> outer, ok::slice_t<T> inner) noexcept;

/// Check if a given T is contained entirely within a slice of SliceT.
template <typename SliceT, typename T>
constexpr bool memcontains_one(ok::slice_t<SliceT> outer,
                               const T* inner) noexcept;

/// Check if two slices of memory have any memory in common.
template <typename T>
constexpr bool memoverlaps(ok::slice_t<T> a, ok::slice_t<T> b) noexcept;

/// Fills a block of memory of type T by copying an instance of T into every
/// spot in that memory. T must be nothrow copy constructible. Does not invoke
/// destructors of any items already in the memory.
template <typename T>
constexpr void memfill(ok::slice_t<T> slice, T original) noexcept;
} // namespace ok

template <typename T>
inline constexpr bool ok::memcopy(ok::slice_t<T> destination,
                                  ok::slice_t<T> source) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "Cannot copy non-trivially copyable type.");
    if (destination.size() != source.size() ||
        memoverlaps(destination, source)) {
        return false;
    }
    return memcopy_lenient(destination, source);
}

template <typename T>
inline constexpr bool ok::memcopy_lenient(ok::slice_t<T> destination,
                                          ok::slice_t<T> source) noexcept
{
    if (destination.size() < source.size() ||
        memoverlaps(destination, source)) {
        return false;
    }

    std::memcpy(destination.data(), source.data(), source.size() * sizeof(T));
    return true;
}

template <typename T>
inline constexpr bool ok::memcompare(ok::slice_t<T> memory_1,
                                     ok::slice_t<T> memory_2) noexcept
{
    if (memory_1.size() != memory_2.size()) {
        return false;
    }
    if (memory_1.data() == memory_2.data()) {
        return true;
    }
    const size_t size = memory_1.size() * sizeof(T);
    const auto* const as_bytes_1 =
        reinterpret_cast<const uint8_t*>(memory_1.data());
    const auto* const as_bytes_2 =
        reinterpret_cast<const uint8_t*>(memory_2.data());

    for (size_t i = 0; i < size; ++i) {
        if (as_bytes_1[i] != as_bytes_2[i]) {
            return false;
        }
    }
    return true;
}

template <typename T>
inline constexpr bool ok::memcontains(ok::slice_t<T> outer,
                                      ok::slice_t<T> inner) noexcept
{
    return outer.begin().ptr() <= inner.begin().ptr() &&
           outer.end().ptr() >= inner.end().ptr();
}

template <typename SliceT, typename T>
inline constexpr bool ok::memcontains_one(ok::slice_t<SliceT> outer,
                                          const T* inner) noexcept
{
    return (uint8_t*)outer.begin().ptr() <= (uint8_t*)inner &&
           (uint8_t*)outer.end().ptr() >= (uint8_t*)(inner + 1);
}

template <typename T>
inline constexpr bool ok::memoverlaps(ok::slice_t<T> a,
                                      ok::slice_t<T> b) noexcept
{
    return a.begin().ptr() < b.end().ptr() && b.begin().ptr() < a.end().ptr();
}

template <typename T>
inline constexpr void ok::memfill(ok::slice_t<T> slice,
                                  const T original) noexcept
{
    static_assert(
        std::is_nothrow_copy_constructible_v<T>,
        "Cannot memfill a type which can throw when copy constructed.");
    for (T& item : slice) {
        new ((void*)std::addressof(item)) T(original);
    }
}

template <>
inline constexpr void ok::memfill(ok::slice_t<uint8_t> slice,
                                  const uint8_t original) noexcept
{
    std::memset(slice.data(), original, slice.size());
}
#endif
