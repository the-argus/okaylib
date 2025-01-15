#ifndef __OKAYLIB_SLICE_H__
#define __OKAYLIB_SLICE_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/traits/is_container.h"
#include "okay/detail/traits/is_instance.h"
#include <cassert>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

#ifndef OKAYLIB_NOEXCEPT
#define OKAYLIB_NOEXCEPT noexcept
#endif

namespace ok {

// forward decls
template <typename viewed_t> class slice_t;
template <typename viewed_t>
[[nodiscard]] constexpr slice_t<viewed_t>
raw_slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT;

/// A non-owning reference to a section of a contiguously allocated array of
/// type T. Intended to be passed around like a pointer.
template <typename viewed_t> class slice_t
{
  private:
    // NOTE: opt may rely on the layout of these items, changing order may cause
    // UB (or... trigger problems in what is already UB...)
    // TODO: is the constexpr reinterpret_cast into a slice_t (in opt.h) valid
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
    static_assert(!std::is_reference_v<viewed_t>,
                  "Cannot create a slice of references.");

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

    /// Wrap a contiguous stdlib container which has data() and size() functions
    /// and the pointer result of data() is convertible to a viewed_t*.
    template <typename U>
    constexpr slice_t(
        U& other, std::enable_if_t<
                      // has data() and size()
                      detail::is_container_v<U> &&
                          !detail::is_instance<U, ok::slice_t>() &&
                          // either the value type of the container is the
                          // same as our value type, or
                          (std::is_same_v<decltype(*other.data()), viewed_t&> ||
                           std::is_same_v<decltype(*other.data()),
                                          std::remove_const_t<viewed_t>&>),
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
    constexpr operator ok::slice_t<const viewed_t>() const OKAYLIB_NOEXCEPT
    {
        return raw_slice(static_cast<const viewed_t&>(*m_data), m_elements);
    }

    constexpr friend bool operator==(const slice_t& a,
                                     const slice_t& b) OKAYLIB_NOEXCEPT
    {
        return a.m_elements == b.m_elements && a.m_data == b.m_data;
    };

    constexpr friend bool operator!=(const slice_t& a,
                                     const slice_t& b) OKAYLIB_NOEXCEPT
    {
        return a.m_elements != b.m_elements || a.m_data != b.m_data;
    };

    // NOTE: not const-correct- constness of value_type& is determined by the
    // inner type, not by constness of the slice. same behavior as pointers
    constexpr value_type& operator[](size_t idx) const OKAYLIB_NOEXCEPT
    {
        if (idx >= m_elements) [[unlikely]]
            __ok_abort();
        return m_data[idx];
    }

    /// Can't default construct a slice since its always a reference to another
    /// thing.
    slice_t() = delete;

    friend constexpr slice_t ok::raw_slice<>(viewed_t& data,
                                             size_t size) OKAYLIB_NOEXCEPT;
#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<slice_t>;
#endif
};

/// Construct a slice point to a buffer of memory. Requires that data is not
/// nullptr. Aborts the program if data is nullptr.
template <typename viewed_t>
[[nodiscard]] constexpr slice_t<viewed_t>
raw_slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT
{
    return slice_t<viewed_t>(ok::addressof(data), size);
}

/// Make a slice of only a part of a contiguous container
/// container: std::array or std::vector, or another slice, etc
/// from: index to start subslice from
/// to: index to end subslice at (must be greater than from and less than
/// container.size())
template <typename container_t>
constexpr std::enable_if_t<detail::is_container_v<container_t>,
                           slice_t<typename container_t::value_type>>
make_subslice(container_t& container, size_t from, size_t to) OKAYLIB_NOEXCEPT
{
    if (from > to || to > container.size()) [[unlikely]]
        __ok_abort();
    return raw_slice(container.data()[from], to - from);
}

template <typename T>
constexpr slice_t<T> make_slice_of_one(T& item) OKAYLIB_NOEXCEPT
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
