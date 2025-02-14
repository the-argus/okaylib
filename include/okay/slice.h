#ifndef __OKAYLIB_SLICE_H__
#define __OKAYLIB_SLICE_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_container.h"
#include "okay/detail/traits/is_instance.h"
#include <cassert>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {

struct subslice_options_t
{
    size_t start;
    size_t length;
};

// forward decls
template <typename viewed_t> struct undefined_memory_t;
template <typename viewed_t> class slice_t;

// library implementation wants to be able to construct slices from data + size,
// particularly in stdmem and allocators
template <typename viewed_t>
slice_t<viewed_t> raw_slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT;

/// A non-owning reference to a section of a contiguously allocated array of
/// type T. Intended to be passed around like a pointer.
/// The data pointed at by a slice_t can be expected to be "initialized"-
/// unless the viewed type is trivially constructible, in which case the type
/// provides no guarantees and that is also true of a slice of the type.
template <typename viewed_t> class slice_t
{
    static_assert(!std::is_reference_v<viewed_t>,
                  "Cannot create a slice of references.");

  private:
    // NOTE: opt may rely on the layout of these items, changing order may cause
    // UB (or... trigger problems in what is already UB...)
    size_t m_elements;
    viewed_t* m_data;

    constexpr slice_t(viewed_t* data, size_t size) OKAYLIB_NOEXCEPT
    {
        if (!data) [[unlikely]]
            __ok_abort();
        m_data = data;
        m_elements = size;
    }

    /// Struct whose only purpose to exist in parameter lists as a way of doing
    /// enable_if...
    struct uninstantiable_t
    {
        friend struct slice_t;

      private:
        uninstantiable_t() = default;
    };

  public:
    slice_t(const slice_t&) = default;
    slice_t& operator=(const slice_t&) = default;
    slice_t(slice_t&&) = default;
    slice_t& operator=(slice_t&&) = default;
    ~slice_t() = default;

    using value_type = viewed_t;

    // raw access to contents
    [[nodiscard]] constexpr viewed_t* data() const OKAYLIB_NOEXCEPT
    {
        return m_data;
    }
    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return m_elements;
    }

    // implicitly take a slice of something with data() and size() functions
    // which is not also a slice.
    template <typename U>
    constexpr slice_t(
        const U& other,
        std::enable_if_t<detail::is_container_v<U> &&
                             !detail::is_instance<U, ok::slice_t>() &&
                             std::is_same_v<decltype(*other.data()), viewed_t&>,
                         uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
        : m_data(other.data()),
          m_elements(other.size())
    {
    }

    // take slice of thing with data() and size(), nonconst variant
    template <typename U>
    constexpr slice_t(
        U& other, std::enable_if_t<
                      // has data() and size()
                      detail::is_container_v<U> &&
                          !detail::is_instance<U, ok::slice_t>() &&
                          // either the value type of the container is the
                          // same as our value type, or it is nonconst and
                          // we are const
                          (std::is_same_v<decltype(*other.data()), viewed_t&> ||
                           std::is_same_v<decltype(*other.data()),
                                          std::remove_cv_t<viewed_t>&>),
                      uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
        : m_data(other.data()),
          m_elements(other.size())
    {
    }

    // c array converting constructor
    template <size_t size>
    constexpr slice_t(viewed_t (&array)[size]) OKAYLIB_NOEXCEPT
        : m_data(array),
          m_elements(size)
    {
    }

    // can convert to const version of self
    constexpr operator slice_t<const viewed_t>() const OKAYLIB_NOEXCEPT
    {
        return raw_slice(*static_cast<const viewed_t*>(m_data), m_elements);
    }

    [[nodiscard]] constexpr bool
    is_alias_for(const slice_t<const viewed_t>& other) OKAYLIB_NOEXCEPT
    {
        return m_elements == other.size() && m_data == other.data();
    };

    // NOTE: not const-correct- constness of value_type& is determined by the
    // inner type, not by constness of the slice. same behavior as pointers
    [[nodiscard]] constexpr value_type&
    operator[](size_t idx) const OKAYLIB_NOEXCEPT
    {
        if (idx >= m_elements) [[unlikely]] {
            __ok_abort();
        }
        return m_data[idx];
    }

    [[nodiscard]] constexpr slice_t subslice(subslice_options_t options)
    {
        // NOTE: checking both start and start + length, in case overflow occurs
        // have to do this to be technically safe, but this will probably never
        // happen...
        // TODO: 128 width math here?
        if (options.start >= m_elements) [[unlikely]] {
            __ok_abort();
        }
        if (options.start + options.length >= m_elements) [[unlikely]] {
            __ok_abort();
        }

        return slice_t(m_data + options.start, options.length);
    }

    /// Can't default construct a slice since its always a reference to another
    /// thing.
    slice_t() = delete;

    template <typename T>
    friend slice_t<T> ok::raw_slice(T& data, size_t size) OKAYLIB_NOEXCEPT;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<slice_t>;
#endif
};

template <typename viewed_t, size_t size>
slice_t(viewed_t (&)[size]) -> slice_t<viewed_t>;

template <typename T>
slice_t(T) -> slice_t<std::enable_if_t<
               detail::is_container_v<detail::remove_cvref_t<T>> &&
                   !detail::is_instance<detail::remove_cvref_t<T>, slice_t>(),
               typename detail::remove_cvref_t<T>::value_type>>;

using bytes_t = slice_t<uint8_t>;

/// Pointer to an array of items of type viewed_t, which are not initialized.
/// Not much can be done with this type besides decide how to initialize the
/// memory.
template <typename viewed_t> struct undefined_memory_t
{
  private:
    size_t m_elements;
    viewed_t* m_data;

  public:
    undefined_memory_t() = delete;
    undefined_memory_t& operator=(const undefined_memory_t&) = default;
    undefined_memory_t(const undefined_memory_t&) = default;
    undefined_memory_t(undefined_memory_t&&) = default;
    undefined_memory_t& operator=(undefined_memory_t&&) = default;
    ~undefined_memory_t() = default;

    static_assert(!std::is_reference_v<viewed_t>,
                  "Cannot create an undefined_memory slice of references.");
    static_assert(!std::is_const_v<viewed_t>,
                  "Useless undefined_memory slice of a const type- what're you "
                  "gonna do, read from it?");

    constexpr undefined_memory_t(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT
        : m_elements(size),
          m_data(ok::addressof(data))
    {
    }

    // Creates an undefined memory buffer from a buffer of bytes. Aborts if the
    // memory is not properly aligned or sized.
    [[nodiscard]] static constexpr undefined_memory_t
    from_bytes(const bytes_t& bytes) OKAYLIB_NOEXCEPT
    {
        if (bytes.size() % sizeof(viewed_t) != 0) [[unlikely]] {
            __ok_abort();
        }
        if (uintptr_t(bytes.data()) % alignof(viewed_t) != 0) [[unlikely]] {
            __ok_abort();
        }

        return undefined_memory_t(*reinterpret_cast<viewed_t*>(bytes.data()),
                                  bytes.size() / sizeof(viewed_t));
    }

    // If the type is trivially constructible, you can pretend it has been
    // initialized.
    template <typename T = viewed_t>
    [[nodiscard]] constexpr slice_t<
        std::enable_if_t<std::is_trivially_constructible_v<T>, T>>
    leave_undefined() const OKAYLIB_NOEXCEPT
    {
        return *reinterpret_cast<const slice_t<viewed_t>*>(this);
    }

    template <
        typename... args_t,
        std::enable_if_t<std::is_nothrow_constructible_v<viewed_t, args_t...>,
                         bool> = true>
    // Call the constructor of every item in the slice with the given arguments,
    // and return a slice to the now-initialized memory.
    [[nodiscard]] constexpr slice_t<viewed_t>
    construct_all(args_t&&... constructor_args) const OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < m_elements; ++i) {
            new (m_data + i)
                viewed_t(std::forward<args_t>(constructor_args)...);
        }

        return *reinterpret_cast<const slice_t<viewed_t>*>(this);
    }

    [[nodiscard]] constexpr viewed_t* data() const OKAYLIB_NOEXCEPT
    {
        return m_data;
    }

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return m_elements;
    }
};

/// Make a slice of only a part of a contiguous container
/// container: std::array or std::vector, or another slice, etc
/// from: index to start subslice from
/// to: index to end subslice at (must be greater than from and less than
/// container.size())
template <typename container_t>
[[nodiscard]] constexpr auto
subslice(container_t& container,
         const subslice_options_t& options) OKAYLIB_NOEXCEPT
    -> std::enable_if_t<
        detail::is_container_v<container_t>,
        slice_t<std::remove_reference_t<decltype(*container.data())>>>
{
    if (options.start >= container.size()) [[unlikely]] {
        __ok_abort();
    }
    if (options.start + options.length > container.size()) [[unlikely]] {
        __ok_abort();
    }

    return raw_slice(*(container.data() + options.start), options.length);
}

/// Construct a slice from a starting item and a number of items. Generally a
/// bad idea, but useful when interfacing with things like c-style strings.
template <typename viewed_t>
[[nodiscard]] slice_t<viewed_t> raw_slice(viewed_t& data,
                                          size_t size) OKAYLIB_NOEXCEPT
{
    return slice_t<viewed_t>(ok::addressof(data), size);
}

template <typename T>
[[nodiscard]] constexpr slice_t<T> slice_from_one(T& item) OKAYLIB_NOEXCEPT
{
    return raw_slice(item, 1);
}

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename viewed_t> struct fmt::formatter<ok::slice_t<viewed_t>>
{
    constexpr auto
    parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        auto it = ctx.begin();

        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");

        // just immediately return the iterator to the ending valid character
        return it;
    }

    auto format(const ok::slice_t<viewed_t>& slice,
                format_context& ctx) const -> format_context::iterator
    {
        // TODO: use CTTI here to get nice typename before pointer
        return fmt::format_to(ctx.out(), "[{:p} -> {}]",
                              reinterpret_cast<void*>(slice.m_data),
                              slice.m_elements);
    }
};
#endif

#endif
