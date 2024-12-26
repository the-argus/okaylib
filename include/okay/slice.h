#ifndef __OKAYLIB_SLICE_H__
#define __OKAYLIB_SLICE_H__

#include "okay/detail/traits/is_instance.h"
#include <cassert>

#ifndef OKAYLIB_SLICE_NO_ITERATOR
#include <iterator>
#endif

#include "okay/detail/abort.h"
#include "okay/detail/traits/is_container.h"

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
[[nodiscard]] constexpr inline slice_t<viewed_t>
raw_slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT;

/// A non-owning reference to a section of a contiguously allocated array of
/// type T. Intended to be passed around like a pointer.
template <typename viewed_t> class slice_t
{
  private:
    // NOTE: opt may rely on the layout of these items, changing order may cause
    // UB
    size_t m_elements;
    viewed_t* m_data;

    inline constexpr slice_t(viewed_t* data, size_t size) OKAYLIB_NOEXCEPT
    {
        assert(data != nullptr);
        m_data = data;
        m_elements = size;
    }

    using nonconst_viewed_t =
        typename std::conditional_t<std::is_const_v<viewed_t>,
                                    std::remove_const_t<viewed_t>, viewed_t>;
    using const_viewed_t =
        typename std::conditional_t<std::is_const_v<viewed_t>, viewed_t,
                                    const viewed_t>;

    struct inner_constructor_t
    {};

    template <typename other_viewed_t>
    inline constexpr slice_t(inner_constructor_t,
                             const slice_t<other_viewed_t>& other)
        OKAYLIB_NOEXCEPT
    {
        static_assert(std::is_const_v<viewed_t> ||
                          (!std::is_const_v<other_viewed_t> &&
                           !std::is_const_v<viewed_t>),
                      "Instantiated const cast inner constructor incorrectly");
        m_data = const_cast<viewed_t*>(other.data());
        m_elements = other.size();
    }

    struct inner_single_constructor_t
    {};

    template <typename U = viewed_t>
    inline constexpr slice_t(inner_single_constructor_t,
                             const U& item) OKAYLIB_NOEXCEPT
    {
        static_assert(
            std::is_const_v<viewed_t> ||
                (!std::is_const_v<U> && !std::is_const_v<viewed_t>),
            "Instantiated const cast inner single constructor incorrectly");
        m_data = const_cast<viewed_t*>(std::addressof(item));
        m_elements = 1;
    }

    /// Struct whose only purpose to exist in parameter lists as a way of doing
    /// enable_if...
    struct uninstantiable_t
    {
        friend struct slice_t;

      private:
        uninstantiable_t() = default;
    };

    /// Internally, a slice can always be constructed from a non-const reference
    /// to a T.
    template <typename U>
    inline constexpr slice_t(
        U& other, std::enable_if_t<std::is_same_v<nonconst_viewed_t, U>,
                                   uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
        : slice_t(inner_single_constructor_t{}, other)
    {
    }

    /// Construct from a const reference to T ONLY if T is const. If it is
    /// non-const, then we can only be constructed from non-const references
    template <typename maybe_viewed_t = viewed_t>
    inline constexpr slice_t(
        std::enable_if_t<std::is_const_v<maybe_viewed_t>, maybe_viewed_t&>
            other) OKAYLIB_NOEXCEPT
        : slice_t(inner_single_constructor_t{}, other)
    {
    }

  public:
    using type = viewed_t;
    using value_type = viewed_t;

#ifndef OKAYLIB_SLICE_NO_ITERATOR
    struct iterator;
    struct const_iterator;

    using correct_iterator_t =
        std::conditional_t<std::is_const_v<viewed_t>, const_iterator, iterator>;

    inline constexpr correct_iterator_t begin() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t(m_data);
    }
    inline constexpr correct_iterator_t end() OKAYLIB_NOEXCEPT
    {
        return correct_iterator_t(m_data + m_elements);
    }

    // const variants of begin and end which are always const if the slice
    // itself is const
    inline constexpr const_iterator begin() const OKAYLIB_NOEXCEPT
    {
        return const_iterator(m_data);
    }

    inline constexpr const_iterator end() const OKAYLIB_NOEXCEPT
    {
        return const_iterator(m_data + m_elements);
    }
#endif

    // raw access to contents
    [[nodiscard]] inline constexpr viewed_t* data() const OKAYLIB_NOEXCEPT
    {
        return m_data;
    }
    [[nodiscard]] inline constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return m_elements;
    }

    /// Wrap a contiguous stdlib container which has data() and size() functions
    template <typename U>
    inline constexpr slice_t(
        U& other,
        std::enable_if_t<
            detail::is_container_v<U> &&
                !detail::is_instance<U, slice_t>::value &&
                (std::is_same_v<typename U::value_type, const_viewed_t> ||
                 std::is_same_v<typename U::value_type, nonconst_viewed_t>),
            uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
    {
        static_assert(!std::is_same_v<U, slice_t>,
                      "incorrect constructor selected");
        static_assert(!std::is_same_v<U, slice_t<nonconst_viewed_t>>,
                      "incorrect constructor selected");
        static_assert(!std::is_same_v<U, viewed_t>,
                      "incorrect constructor selected");
        static_assert(!std::is_same_v<U, nonconst_viewed_t>,
                      "incorrect constructor selected");
        m_data = other.data();
        m_elements = other.size();
    }

    template <typename U>
    static inline slice_t
    from_one(U& single_item,
             std::enable_if_t<std::is_same_v<nonconst_viewed_t, U>,
                              uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
    {
        return slice_t(single_item);
    }

    template <typename U>
    static inline slice_t
    from_one(U& single_item,
             std::enable_if_t<std::is_same_v<const_viewed_t, U> &&
                                  std::is_const_v<viewed_t>,
                              uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
    {
        return slice_t(single_item);
    }

    /// A slice can always be constructed from a nonconst variant of itself
    template <typename U>
    inline constexpr slice_t(
        U other, std::enable_if_t<std::is_same_v<slice_t<nonconst_viewed_t>, U>,
                                  uninstantiable_t> = {}) OKAYLIB_NOEXCEPT
        : slice_t(inner_constructor_t{}, other)
    {
    }

    /// If type is const, then we can be constructed from const
    template <typename maybe_viewed_t = viewed_t>
    inline constexpr slice_t(
        std::enable_if_t<std::is_const_v<maybe_viewed_t>, const slice_t&> other)
        OKAYLIB_NOEXCEPT : slice_t(inner_constructor_t{}, other)
    {
    }

    /// Take a slice of contiguous stdlib container from one index to another
    /// (from is less than to)
    template <typename container_t>
    inline constexpr slice_t<viewed_t>(container_t& container, size_t from,
                                       size_t to) OKAYLIB_NOEXCEPT
    {
        if (from > to || to > container.size()) [[unlikely]]
            OK_ABORT();
        m_elements = to - from;
        m_data = &container.data()[from];
    }

    inline constexpr friend bool operator==(const slice_t& a,
                                            const slice_t& b) OKAYLIB_NOEXCEPT
    {
        return a.m_elements == b.m_elements && a.m_data == b.m_data;
    };

    inline constexpr friend bool operator!=(const slice_t& a,
                                            const slice_t& b) OKAYLIB_NOEXCEPT
    {
        return a.m_elements != b.m_elements || a.m_data != b.m_data;
    };

    /// Can't default construct a slice since its always a reference to another
    /// thing.
    slice_t() = delete;

#ifndef OKAYLIB_SLICE_NO_ITERATOR
    struct iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = viewed_t;
        using pointer = value_type*;
        using reference = value_type&;

        inline constexpr pointer ptr() OKAYLIB_NOEXCEPT { return m_ptr; }

        inline constexpr iterator(pointer ptr) OKAYLIB_NOEXCEPT : m_ptr(ptr) {}

        inline constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            return *m_ptr;
        }

        inline constexpr pointer operator->() OKAYLIB_NOEXCEPT { return m_ptr; }

        // Prefix increment
        inline constexpr iterator& operator++() OKAYLIB_NOEXCEPT
        {
            ++m_ptr;
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        inline constexpr iterator operator++(int) OKAYLIB_NOEXCEPT
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        inline constexpr friend bool
        operator==(const iterator& a, const iterator& b) OKAYLIB_NOEXCEPT
        {
            return a.m_ptr == b.m_ptr;
        };
        inline constexpr friend bool
        operator!=(const iterator& a, const iterator& b) OKAYLIB_NOEXCEPT
        {
            return a.m_ptr != b.m_ptr;
        };

      private:
        pointer m_ptr;
    };

    struct const_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const viewed_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        inline constexpr pointer ptr() OKAYLIB_NOEXCEPT { return m_ptr; }

        inline constexpr const_iterator(pointer ptr) OKAYLIB_NOEXCEPT
            : m_ptr(ptr)
        {
        }

        inline constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            return *m_ptr;
        }

        inline constexpr pointer operator->() OKAYLIB_NOEXCEPT { return m_ptr; }

        // Prefix increment
        inline constexpr const_iterator& operator++() OKAYLIB_NOEXCEPT
        {
            ++m_ptr;
            return *this;
        }

        // Postfix increment
        // NOLINTNEXTLINE
        inline constexpr const_iterator operator++(int) OKAYLIB_NOEXCEPT
        {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        inline constexpr friend bool
        operator==(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            return a.m_ptr == b.m_ptr;
        };
        inline constexpr friend bool
        operator!=(const const_iterator& a,
                   const const_iterator& b) OKAYLIB_NOEXCEPT
        {
            return a.m_ptr != b.m_ptr;
        };

      private:
        pointer m_ptr;
    };
#endif

    friend constexpr slice_t ok::raw_slice<>(viewed_t& data,
                                             size_t size) OKAYLIB_NOEXCEPT;
#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<slice_t>;
#endif
};

/// Construct a slice point to a buffer of memory. Requires that data is not
/// nullptr. Aborts the program if data is nullptr.
template <typename viewed_t>
[[nodiscard]] constexpr inline slice_t<viewed_t>
raw_slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT
{
    return slice_t<viewed_t>(std::addressof(data), size);
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
