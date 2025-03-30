#ifndef __OKAYLIB_SLICE_H__
#define __OKAYLIB_SLICE_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/traits/is_container.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/math/rounding.h"
#include <cassert>

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {

struct subslice_options_t
{
    size_t start = 0;
    size_t length;
};

// forward decls
template <typename viewed_t> struct undefined_memory_t;
template <typename viewed_t> class slice;

class bit_slice_t;
class const_bit_slice_t;

// library implementation wants to be able to construct slices from data + size,
// particularly in stdmem and allocators
template <typename viewed_t>
slice<viewed_t> raw_slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT;

/// Only use this if you absolutely know what you're doing. data() will return
/// nullptr on the slice returned from this.
template <typename viewed_t>
slice<viewed_t> raw_slice_create_null_empty_unsafe() OKAYLIB_NOEXCEPT;

constexpr bit_slice_t raw_bit_slice(slice<uint8_t> bytes, size_t num_bits,
                                    uint8_t offset) OKAYLIB_NOEXCEPT;
constexpr const_bit_slice_t raw_bit_slice(slice<const uint8_t> bytes,
                                          size_t num_bits,
                                          uint8_t offset) OKAYLIB_NOEXCEPT;

/// A non-owning reference to a section of a contiguously allocated array of
/// type T. Intended to be passed around like a pointer.
/// The data pointed at by a slice can be expected to be "initialized"-
/// unless the viewed type is trivially constructible, in which case the type
/// provides no guarantees and that is also true of a slice of the type.
template <typename viewed_t> class slice
{
    static_assert(!std::is_reference_v<viewed_t>,
                  "Cannot create a slice of references.");

  private:
    // NOTE: opt may rely on the layout of these items, changing order may cause
    // UB (or... trigger problems in what is already UB...)
    size_t m_elements;
    viewed_t* m_data;

    constexpr slice(viewed_t* data, size_t size) OKAYLIB_NOEXCEPT
    {
        // allow nullptr data only if size is zero
        __ok_internal_assert(size == 0 || data);
        if (!data && size != 0) [[unlikely]] {
            __ok_abort("Something has gone horribly wrong with slice "
                       "implementation");
        }
        m_data = data;
        m_elements = size;
    }

  public:
    slice(const slice&) = default;
    slice& operator=(const slice&) = default;
    slice(slice&&) = default;
    slice& operator=(slice&&) = default;
    ~slice() = default;

    template <typename T> friend class slice;

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
    [[nodiscard]] constexpr size_t size_bytes() const OKAYLIB_NOEXCEPT
    {
        return m_elements * sizeof(viewed_t);
    }
    [[nodiscard]] constexpr size_t size_bits() const OKAYLIB_NOEXCEPT
    {
        return m_elements * sizeof(viewed_t) * 8;
    }

    [[nodiscard]] constexpr bool is_empty() const OKAYLIB_NOEXCEPT
    {
        return m_elements == 0;
    }

    [[nodiscard]] constexpr value_type& first() const OKAYLIB_NOEXCEPT
    {
        if (is_empty()) [[unlikely]] {
            __ok_abort("Attempt to get first() item from empty slice.");
        }
        return m_data[0];
    }

    [[nodiscard]] constexpr value_type& last() OKAYLIB_NOEXCEPT
    {
        if (is_empty()) [[unlikely]] {
            __ok_abort("Attempt to get last() item from empty slice.");
        }
        return m_data[m_elements - 1];
    }

    // implicitly take a slice of something with data() and size() functions
    // which is not also a slice.
    template <
        typename U,
        std::enable_if_t<
            detail::is_container_v<U> && !detail::is_instance<U, ok::slice>() &&
                std::is_same_v<decltype(*std::declval<const U&>().data()),
                               viewed_t&>,
            bool> = false>
    constexpr slice(const U& other) OKAYLIB_NOEXCEPT : m_data(other.data()),
                                                       m_elements(other.size())
    {
    }

    // take slice of thing with data() and size(), nonconst variant
    template <
        typename U,
        std::enable_if_t<
            // has data() and size()
            detail::is_container_v<U> && !detail::is_instance<U, ok::slice>() &&
                // either the value type of the container is the
                // same as our value type, or it is nonconst and
                // we are const
                (std::is_same_v<decltype(*std::declval<U&>().data()),
                                viewed_t&> ||
                 std::is_same_v<decltype(*std::declval<U&>().data()),
                                std::remove_cv_t<viewed_t>&>),
            bool> = false>
    constexpr slice(U& other) OKAYLIB_NOEXCEPT : m_data(other.data()),
                                                 m_elements(other.size())
    {
    }

    // mark rvalue reference overloads for array-to-slice constructors as
    // deleted, so we dont create a slice of something that's about to be
    // destroyed
    template <typename U,
              std::enable_if_t<
                  !detail::is_instance_v<detail::remove_cvref_t<U>, ok::slice>,
                  bool> = true>
    constexpr slice(const U&& other) = delete;
    template <typename U,
              std::enable_if_t<
                  !detail::is_instance_v<detail::remove_cvref_t<U>, ok::slice>,
                  bool> = true>
    constexpr slice(U&& other) = delete;

    // c array converting constructors
    template <size_t size>
    constexpr slice(viewed_t (&array)[size]) OKAYLIB_NOEXCEPT : m_data(array),
                                                                m_elements(size)
    {
    }

    template <
        typename nonconst_viewed_t, size_t size,
        std::enable_if_t<std::is_const_v<viewed_t> &&
                             std::is_same_v<const nonconst_viewed_t, viewed_t>,
                         bool> = true>
    constexpr slice(nonconst_viewed_t (&array)[size]) OKAYLIB_NOEXCEPT
        : m_data(array),
          m_elements(size)
    {
    }

    // can convert to const version of self
    constexpr operator slice<const viewed_t>() const OKAYLIB_NOEXCEPT
    {
        return slice<const viewed_t>(m_data, m_elements);
    }

    [[nodiscard]] constexpr bool
    is_alias_for(const slice<const viewed_t>& other) OKAYLIB_NOEXCEPT
    {
        return m_elements == other.size() && m_data == other.data();
    };

    // NOTE: not const-correct- constness of value_type& is determined by the
    // inner type, not by constness of the slice. same behavior as pointers
    [[nodiscard]] constexpr value_type&
    operator[](size_t idx) const OKAYLIB_NOEXCEPT
    {
        if (idx >= m_elements) [[unlikely]] {
            __ok_abort("Out of bounds access into slice.");
        }
        return m_data[idx];
    }

    [[nodiscard]] constexpr slice
    subslice(subslice_options_t options) const OKAYLIB_NOEXCEPT
    {
        // NOTE: checking both start and start + length, in case overflow occurs
        // have to do this to be technically safe, but this will probably never
        // happen...
        // TODO: 128 width math here?
        if (options.start >= m_elements) [[unlikely]] {
            __ok_abort("Attempt to create subslice but the starting value is "
                       "out of bounds.");
        }
        if (options.start + options.length > m_elements) [[unlikely]] {
            __ok_abort("Attempt to create subslice but the ending value is out "
                       "of bounds.");
        }

        return slice(m_data + options.start, options.length);
    }

    /// Can't default construct a slice since its always a reference to another
    /// thing.
    slice() = delete;

    template <typename T>
    friend slice<T> ok::raw_slice(T& data, size_t size) OKAYLIB_NOEXCEPT;
    template <typename T>
    friend slice<T> ok::raw_slice_create_null_empty_unsafe() OKAYLIB_NOEXCEPT;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<slice>;
#endif
};

template <typename viewed_t, size_t size>
slice(viewed_t (&)[size]) -> slice<viewed_t>;

template <typename T>
slice(T) -> slice<std::enable_if_t<
             detail::is_container_v<detail::remove_cvref_t<T>> &&
                 !detail::is_instance<detail::remove_cvref_t<T>, slice>(),
             typename detail::remove_cvref_t<T>::value_type>>;

using bytes_t = slice<uint8_t>;

class const_bit_slice_t
{
  protected:
    constexpr const_bit_slice_t(uint8_t* first_byte, size_t num_bits,
                                uint8_t offset)
        : m(members_t{
              .num_bits = num_bits,
              .first_byte = first_byte,
              .offset = offset,
          })
    {
        if (first_byte == nullptr && num_bits != 0) [[unlikely]] {
            __ok_abort("Attempt to use null data but size is not zero.");
        }
    }

    struct members_t
    {
        size_t num_bits;
        uint8_t* first_byte;
        // bytes from the start of first_byte that it starts
        uint8_t offset;
    } m;

  public:
#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<const_bit_slice_t>;
#endif

    friend constexpr const_bit_slice_t
    ok::raw_bit_slice(slice<const uint8_t> bytes, size_t num_bits,
                      uint8_t offset) OKAYLIB_NOEXCEPT;

    const_bit_slice_t() = delete;

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return m.num_bits;
    }

    [[nodiscard]] constexpr bool is_empty() const OKAYLIB_NOEXCEPT
    {
        return this->size() == 0;
    }

    [[nodiscard]] constexpr bool is_byte_aligned() const OKAYLIB_NOEXCEPT
    {
        return m.offset == 0;
    }

    /// Returns the number of bytes pointed at by this contiguous slice of bits
    [[nodiscard]] constexpr size_t num_bytes_occupied() const OKAYLIB_NOEXCEPT
    {
        return round_up_to_multiple_of<8>(m.num_bits + m.offset);
    }

    [[nodiscard]] constexpr bool
    get_bit(const size_t idx) const OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to const_bit_slice_t::get_bit.");
        }
        const size_t bit_idx = idx + m.offset;
        const size_t byte = bit_idx / 8;
        const size_t bit = bit_idx % 8;
        const uint8_t mask = uint8_t(1) << bit;
        return m.first_byte[byte] & mask;
    }

    [[nodiscard]] constexpr const_bit_slice_t
    subslice(const subslice_options_t& options) const OKAYLIB_NOEXCEPT
    {
        if (options.start + options.length >= this->size()) {
            __ok_abort("Out of bounds access in const_bit_slice_t::subslice");
        }

        const size_t first_byte_index = m.offset + options.start;
        const uint8_t new_offset = first_byte_index % 8UL;
        return const_bit_slice_t(m.first_byte + first_byte_index,
                                 options.length, new_offset);
    }
};

class bit_slice_t : public const_bit_slice_t
{
  private:
    constexpr bit_slice_t(uint8_t* first_byte, size_t num_bits, uint8_t offset)
        : const_bit_slice_t(first_byte, num_bits, offset)
    {
    }

  public:
#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<bit_slice_t>;
#endif

    friend constexpr bit_slice_t
    ok::raw_bit_slice(slice<uint8_t> bytes, size_t num_bits,
                      uint8_t offset) OKAYLIB_NOEXCEPT;

    constexpr void set_bit(const size_t idx,
                           const bool on) const OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to bit_slice_t::set_bit.");
        }
        const size_t bit_idx = idx + m.offset;
        const size_t byte = bit_idx / 8;
        const size_t bit = bit_idx % 8;

        const uint8_t mask = uint8_t(1) << bit;

        if (on) {
            m.first_byte[byte] |= mask;
        } else {
            m.first_byte[byte] &= ~mask;
        }
    }

    constexpr void toggle_bit(const size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to bit_slice_t::toggle_bit.");
        }
        const size_t bit_idx = idx + m.offset;
        const size_t byte = bit_idx / 8;
        const size_t bit = bit_idx % 8;
        const uint8_t mask = uint8_t(1) << bit;
        m.first_byte[byte] ^= mask;
    }

    [[nodiscard]] constexpr bit_slice_t
    subslice(const subslice_options_t& options) const OKAYLIB_NOEXCEPT
    {
        if (options.start + options.length >= this->size()) {
            __ok_abort("Out of bounds access in bit_slice_t::subslice");
        }

        const size_t first_byte_index = m.offset + options.start;
        const uint8_t new_offset = first_byte_index % 8UL;
        return bit_slice_t(m.first_byte + first_byte_index, options.length,
                           new_offset);
    }
};

constexpr bit_slice_t raw_bit_slice(bytes_t bytes, size_t num_bits,
                                    uint8_t offset) OKAYLIB_NOEXCEPT
{
    if (round_up_to_multiple_of<8>((num_bits + offset)) / 8 > bytes.size()) {
        __ok_abort("Invalid number of bits requested from slice of bytes in "
                   "raw_bit_slice().");
    }
    if (offset >= 8) {
        __ok_abort("Offset greater than 7 passed to raw_bit_slice.");
    }
    return bit_slice_t(bytes.data(), num_bits, offset);
}

constexpr const_bit_slice_t raw_bit_slice(slice<const uint8_t> bytes,
                                          size_t num_bits,
                                          uint8_t offset) OKAYLIB_NOEXCEPT
{
    if (round_up_to_multiple_of<8>((num_bits + offset)) / 8 > bytes.size()) {
        __ok_abort("Invalid number of bits requested from slice of bytes in "
                   "raw_bit_slice().");
    }
    if (offset >= 8) {
        __ok_abort("Offset greater than 7 passed to raw_bit_slice.");
    }
    return const_bit_slice_t(const_cast<uint8_t*>(bytes.data()), num_bits,
                             offset);
}

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
            __ok_abort(
                "Attempt to construct an undefined_memory_t from a bytes_t, "
                "but the given number of bytes is not divisible by sizeof(T) "
                "(ie. there would be some extra space).");
        }
        if (uintptr_t(bytes.data()) % alignof(viewed_t) != 0) [[unlikely]] {
            __ok_abort("Attempt to construct an undefined_memory_t of a type T "
                       "from a bytes_t, but the given bytes are not aligned "
                       "properly to store type T.");
        }

        return undefined_memory_t(*reinterpret_cast<viewed_t*>(bytes.data()),
                                  bytes.size() / sizeof(viewed_t));
    }

    // If the type is trivially constructible, you can pretend it has been
    // initialized.
    template <typename T = viewed_t>
    [[nodiscard]] constexpr slice<
        std::enable_if_t<std::is_trivially_constructible_v<T>, T>>
    leave_undefined() const OKAYLIB_NOEXCEPT
    {
        return *reinterpret_cast<const slice<viewed_t>*>(this);
    }

    template <
        typename... args_t,
        std::enable_if_t<std::is_nothrow_constructible_v<viewed_t, args_t...>,
                         bool> = true>
    // Call the constructor of every item in the slice with the given arguments,
    // and return a slice to the now-initialized memory.
    [[nodiscard]] constexpr slice<viewed_t>
    construct_all(args_t&&... constructor_args) const OKAYLIB_NOEXCEPT
    {
        for (size_t i = 0; i < m_elements; ++i) {
            new (m_data + i)
                viewed_t(std::forward<args_t>(constructor_args)...);
        }

        return *reinterpret_cast<const slice<viewed_t>*>(this);
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
        slice<std::remove_reference_t<decltype(*container.data())>>>
{
    if (options.start >= container.size()) [[unlikely]] {
        __ok_abort("Attempt to get a subslice of a container but the starting "
                   "value is out of range.");
    }
    if (options.start + options.length > container.size()) [[unlikely]] {
        __ok_abort("Attempt to get a subslice of a container but the ending "
                   "value is out of range.");
    }

    return raw_slice(*(container.data() + options.start), options.length);
}

/// Construct a slice from a starting item and a number of items. Generally a
/// bad idea, but useful when interfacing with things like c-style strings.
template <typename viewed_t>
[[nodiscard]] slice<viewed_t> raw_slice(viewed_t& data,
                                        size_t size) OKAYLIB_NOEXCEPT
{
    return slice<viewed_t>(ok::addressof(data), size);
}

template <typename viewed_t>
[[nodiscard]] slice<viewed_t>
raw_slice_create_null_empty_unsafe() OKAYLIB_NOEXCEPT
{
    return slice<viewed_t>(nullptr, 0);
}

template <typename T>
[[nodiscard]] constexpr slice<T> slice_from_one(T& item) OKAYLIB_NOEXCEPT
{
    return raw_slice(item, 1);
}

template <typename range_t, typename enable> struct range_definition;

template <> struct range_definition<const_bit_slice_t, void>
{
    static constexpr size_t begin(const const_bit_slice_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const const_bit_slice_t& bs,
                                      size_t cursor) OKAYLIB_NOEXCEPT
    {
        return cursor < bs.size();
    }

    static constexpr size_t size(const const_bit_slice_t& bs) OKAYLIB_NOEXCEPT
    {
        return bs.size();
    }

    static constexpr bool get(const const_bit_slice_t& range, size_t cursor)
    {
        return range.get_bit(cursor);
    }
};

template <> struct range_definition<bit_slice_t, void>
{
    static constexpr size_t begin(const bit_slice_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const bit_slice_t& bs,
                                      size_t cursor) OKAYLIB_NOEXCEPT
    {
        return cursor < bs.size();
    }

    static constexpr size_t size(const bit_slice_t& bs) OKAYLIB_NOEXCEPT
    {
        return bs.size();
    }

    static constexpr bool get(const bit_slice_t& range, size_t cursor)
    {
        return range.get_bit(cursor);
    }

    static constexpr void set(bit_slice_t& range, size_t cursor, bool value)
    {
        return range.set_bit(cursor, value);
    }
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename viewed_t> struct fmt::formatter<ok::slice<viewed_t>>
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

    auto format(const ok::slice<viewed_t>& slice,
                format_context& ctx) const -> format_context::iterator
    {
        // TODO: use CTTI here to get nice typename before pointer
        return fmt::format_to(ctx.out(), "[{:p} -> {}]",
                              reinterpret_cast<void*>(slice.m_data),
                              slice.m_elements);
    }
};

template <> struct fmt::formatter<ok::const_bit_slice_t>
{
    constexpr auto
    parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    auto format(const ok::const_bit_slice_t& bs,
                format_context& ctx) const -> format_context::iterator
    {
        fmt::format_to(ctx.out(), "0b");
        for (size_t i = 0; i < bs.size(); ++i) {
            fmt::format_to(ctx.out(), bs.get_bit(i) ? "1" : "0");
        }
        return fmt::format_to(ctx.out(), "");
    }
};
// bit slice inherits from const bit slice, and also from its fmt definition
template <>
struct fmt::formatter<ok::bit_slice_t>
    : public fmt::formatter<ok::const_bit_slice_t>
{};
#endif

#endif
