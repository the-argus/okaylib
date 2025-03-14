#ifndef __OKAYLIB_STDMEM_H__
#define __OKAYLIB_STDMEM_H__

#include "okay/construct.h"
#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/traits/special_member_traits.h"
#include "okay/slice.h"
#include <cstdint>
#include <cstring>

namespace ok {
enum class mem_error : uint8_t
{
    okay,
    no_value,
    unsupported,
    usage,
};

template <typename T> struct memcopy_options_t
{
    slice<T> to;
    slice<T> from;
};

template <typename T>
memcopy_options_t(const slice<T>&, const slice<T>&) -> memcopy_options_t<T>;

template <typename T> struct memcontains_options_t
{
    slice<T> outer;
    slice<T> inner;
};

template <typename T>
memcontains_options_t(const slice<T>&,
                      const slice<T>&) -> memcontains_options_t<T>;

/// Copy the contents of "from" into "to", byte by byte, without invoking
/// any copy constructors or destructors.
/// T must be trivially copyable.
/// Returns a slice of the new, copied-to memory.
/// If the destination is smaller, or if they overlap, the function aborts.
template <typename T>
[[nodiscard]] constexpr slice<T>
memcopy(const memcopy_options_t<T>& options) OKAYLIB_NOEXCEPT;

/// Macro for memcopy to avoid writing the options typename
#define ok_memcopy(...) ok::memcopy(memcopy_options_t{__VA_ARGS__})

/// Compare two slices of memory. The types must not define operator== overloads
/// nor orderable or partial orderable traits (in that case, use the slices as
/// ranges, and ok::equals()). The memory can overlap and alias (in the latter
/// case the function just returns true). If the two slices of memory are
/// differently sized, the function immediately returns false.
template <typename T>
[[nodiscard]] constexpr bool memcompare(slice<T> a,
                                        slice<T> b) OKAYLIB_NOEXCEPT;

/// Check if slice "inner" points only to items also pointed at by slice
/// "outer"
template <typename T>
[[nodiscard]] constexpr bool
memcontains(const memcontains_options_t<T>&) OKAYLIB_NOEXCEPT;

/// Macro for memcontains to avoid writing the options typename
#define ok_memcontains(...) memcontains(memcontains_options_t{__VA_ARGS__})

/// Check if two slices of memory have any memory in common.
template <typename T>
[[nodiscard]] constexpr bool memoverlaps(ok::slice<T> a,
                                         ok::slice<T> b) OKAYLIB_NOEXCEPT;

/// Fills a block of memory of type T by calling the destructor and then
/// constructor of every item in the memory. The invoked constructor must be
/// nothrow, and the type T must be nothrow destructable.
template <typename slice_viewed_t, typename... constructor_args_t>
constexpr void memfill(ok::slice<slice_viewed_t> slice,
                       constructor_args_t&&... args) OKAYLIB_NOEXCEPT;

/// Converts a slice of any type into a slice of bytes. Doing this is usually a
/// bad idea.
template <typename T>
[[nodiscard]] constexpr bytes_t
reinterpret_as_bytes(ok::slice<T> slice) OKAYLIB_NOEXCEPT;

/// Given some slice of memory, convert it to a slice of a given type. Aborts if
/// the slice's size is not divisible by sizeof(T), or memory is not properly
/// aligned.
template <typename T>
[[nodiscard]] constexpr ok::slice<T>
reinterpret_bytes_as(bytes_t bytes) OKAYLIB_NOEXCEPT;

} // namespace ok

/// Returns the slice of memory copied into
template <typename T>
constexpr auto
ok::memcopy(const memcopy_options_t<T>& options) OKAYLIB_NOEXCEPT -> slice<T>
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "Cannot memcopy non-trivially copyable type.");
    if (options.to.size() < options.from.size() ||
        memoverlaps(options.to, options.from)) [[unlikely]] {
        __ok_abort(
            "Attempt to memcopy between two slices of memory which overlap.");
    }

    std::memcpy(options.to.data(), options.from.data(),
                options.from.size() * sizeof(T));

    return raw_slice(*options.to.data(), options.from.size());
}

template <typename T>
[[nodiscard]] constexpr bool
ok::memcompare(ok::slice<T> memory_1, ok::slice<T> memory_2) OKAYLIB_NOEXCEPT
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
[[nodiscard]] constexpr bool
ok::memcontains(const memcontains_options_t<T>& options) OKAYLIB_NOEXCEPT
{
    return options.outer.data() <= options.inner.data() &&
           options.outer.data() + options.outer.size() >=
               options.inner.data() + options.inner.size();
}

template <typename T>
[[nodiscard]] constexpr bool ok::memoverlaps(ok::slice<T> a,
                                             ok::slice<T> b) OKAYLIB_NOEXCEPT
{
    return a.data() < (b.data() + b.size()) && b.data() < (a.data() + a.size());
}

template <typename slice_viewed_t, typename... constructor_args_t>
constexpr void ok::memfill(ok::slice<slice_viewed_t> slice,
                           constructor_args_t&&... args) OKAYLIB_NOEXCEPT
{
    static_assert(!std::is_const_v<slice_viewed_t>,
                  "Cannot memfill a slice of const memory.");
    if constexpr (std::is_same_v<slice_viewed_t, uint8_t>) {
        static_assert(
            is_std_constructible_v<uint8_t, constructor_args_t...>,
            "No matching conversion from given arguments to a uint8_t.");
        std::memset(slice.data(),
                    uint8_t(std::forward<constructor_args_t>(args)...),
                    slice.size());
    } else {
        static_assert(is_infallible_constructible_v<slice_viewed_t,
                                                    constructor_args_t...> &&
                          std::is_nothrow_destructible_v<slice_viewed_t>,
                      "Refusing to call memfill if type is not able to (in a "
                      "non-failing way) be constructed or destroyed.");

        for (size_t i = 0; i < slice.size(); ++i) {
            auto& item = slice.data()[i];
            if constexpr (!std::is_trivially_destructible_v<slice_viewed_t>) {
                item.~slice_viewed_t();
            }
            ok::make_into_uninitialized<slice_viewed_t>(
                item, std::forward<constructor_args_t>(args)...);
        }
    }
}

template <typename T>
[[nodiscard]] constexpr ok::bytes_t
ok::reinterpret_as_bytes(ok::slice<T> slice) OKAYLIB_NOEXCEPT
{
    // NOTE: leave_undefined performs unnecessary copy here
    return ok::undefined_memory_t<uint8_t>(
               *reinterpret_cast<uint8_t*>(slice.data()),
               slice.size() * sizeof(T))
        .leave_undefined();
}

template <typename T>
[[nodiscard]] constexpr ok::slice<T>
ok::reinterpret_bytes_as(ok::bytes_t bytes) OKAYLIB_NOEXCEPT
{
    return ok::undefined_memory_t<T>::from_bytes(bytes).leave_undefined();
}

#endif
