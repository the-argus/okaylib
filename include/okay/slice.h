#ifndef __OKAYLIB_SLICE_H__
#define __OKAYLIB_SLICE_H__

#include "okay/detail/abort.h"
#include "okay/detail/addressof.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/template_util/remove_cvref.h"
#include "okay/detail/template_util/uninitialized_storage.h"
#include "okay/detail/traits/is_instance.h"
#include "okay/detail/traits/is_std_container.h"
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

constexpr bit_slice_t raw_bit_slice(slice<uint8_t> bytes, size_t num_bits,
                                    uint8_t offset) OKAYLIB_NOEXCEPT;

constexpr const_bit_slice_t raw_bit_slice(slice<const uint8_t> bytes,
                                          size_t num_bits,
                                          uint8_t offset) OKAYLIB_NOEXCEPT;

/// Create a slice with no elements and no data, used for one (1) purpose which
/// is to allow arraylist to always return a valid slice even when it has no
/// items
template <typename viewed_t> slice<viewed_t> make_null_slice() OKAYLIB_NOEXCEPT;

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

    constexpr slice(viewed_t& data, size_t size) OKAYLIB_NOEXCEPT
        : m_data(ok::addressof(data)),
          m_elements(size)
    {
    }

    inline static detail::uninitialized_storage_t<viewed_t> null_slice_mem;
    inline static auto* null_slice_mem_start =
        ok::addressof(null_slice_mem.value);

    struct null_slice_tag
    {};

    constexpr slice(null_slice_tag) OKAYLIB_NOEXCEPT
        : m_data(null_slice_mem_start),
          m_elements(0)
    {
    }

  public:
    slice(const slice&) = default;
    slice& operator=(const slice&) = default;
    slice(slice&&) = default;
    slice& operator=(slice&&) = default;
    ~slice() = default;

    template <typename T> friend class slice;

    using viewed_type = viewed_t;

    // raw access to contents
    [[nodiscard]] constexpr viewed_type*
    address_of_first() const OKAYLIB_NOEXCEPT
    {
        return ok::addressof(first());
    }

    /// Guaranteed to never return nullptr, it is defined behavior to
    /// dereference this pointer but it is only defined behavior to write to it
    /// if the slice is not empty.
    [[nodiscard]] constexpr viewed_type*
    unchecked_address_of_first_item() const OKAYLIB_NOEXCEPT
    {
        __ok_assert(
            m_elements,
            "Attempted to call unchecked_address_of_first_item() but the "
            "slice points to no valid data.");
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

    [[nodiscard]] constexpr viewed_type& first() const OKAYLIB_NOEXCEPT
    {
        if (is_empty()) [[unlikely]] {
            __ok_abort("Attempt to get first() item from empty slice.");
        }
        return m_data[0];
    }

    [[nodiscard]] constexpr viewed_type& last() OKAYLIB_NOEXCEPT
    {
        if (is_empty()) [[unlikely]] {
            __ok_abort("Attempt to get last() item from empty slice.");
        }
        return m_data[m_elements - 1];
    }

    // implicitly take a slice of something with data() and size() functions
    template <typename U,
              std::enable_if_t<
                  detail::is_std_container_v<U> &&
                      std::is_same_v<decltype(*std::declval<const U&>().data()),
                                     viewed_type&>,
                  bool> = false>
    constexpr slice(const U& other) OKAYLIB_NOEXCEPT
    {
        static_assert(!detail::is_instance_v<U, ok::slice>);
        const size_t othersize = other.size();
        if (othersize == 0) {
            m_elements = 0;
            m_data = nullptr;
        } else {
            m_elements = othersize;
            m_data = other.data();
        }
    }

    // take slice of thing with data() and size(), nonconst variant
    template <typename U,
              std::enable_if_t<
                  // has data() and size()
                  detail::is_std_container_v<U> &&
                      // either the value type of the container is the
                      // same as our value type, or it is nonconst and
                      // we are const
                      (std::is_same_v<decltype(*std::declval<U&>().data()),
                                      viewed_type&> ||
                       std::is_same_v<decltype(*std::declval<U&>().data()),
                                      std::remove_cv_t<viewed_type>&>),
                  bool> = false>
    constexpr slice(U& other) OKAYLIB_NOEXCEPT
    {
        static_assert(!detail::is_instance_v<U, ok::slice>);
        const size_t othersize = other.size();
        if (othersize == 0) {
            m_elements = 0;
            m_data = nullptr;
        } else {
            m_elements = othersize;
            m_data = other.data();
        }
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

    // NOTE: no need for type deduction to work here as the above c array
    // converting constructor is what should provide deduction. you only
    // coerce to const if you explicitly provide type info like
    // int i[] = {1, 2, 3 4};
    // slice<const int> s(i);
    template <typename nonconst_viewed_type, size_t size,
              std::enable_if_t<
                  std::is_const_v<viewed_type> &&
                      std::is_same_v<const nonconst_viewed_type, viewed_type>,
                  bool> = true>
    constexpr slice(nonconst_viewed_type (&array)[size]) OKAYLIB_NOEXCEPT
        : m_data(array),
          m_elements(size)
    {
    }

    // can convert to const version of self
    constexpr operator slice<const viewed_type>() const OKAYLIB_NOEXCEPT
    {
        return slice<const viewed_type>(*m_data, m_elements);
    }

    template <typename V = viewed_type>
    [[nodiscard]] constexpr auto
    is_alias_for(const slice<const viewed_type>& other) OKAYLIB_NOEXCEPT
        -> std::enable_if_t<
            std::is_same_v<V, viewed_type> && !std::is_const_v<V>, bool>
    {
        return m_elements == other.m_elements && m_data == other.m_data;
    };

    [[nodiscard]] constexpr bool
    is_alias_for(const slice& other) OKAYLIB_NOEXCEPT
    {
        return m_elements == other.m_elements && m_data == other.m_data;
    };

    [[nodiscard]] constexpr viewed_type&
    operator[](size_t idx) const OKAYLIB_NOEXCEPT
    {
        if (idx >= m_elements) [[unlikely]] {
            __ok_abort("Out of bounds access into slice.");
        }
        return m_data[idx];
    }

    [[nodiscard]] constexpr viewed_type&
    unchecked_access(size_t idx) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(idx < m_elements, "Out of bounds access into slice.");
        return m_data[idx];
    }

    [[nodiscard]] constexpr slice
    subslice(const subslice_options_t& options) const OKAYLIB_NOEXCEPT
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

        return slice(*(m_data + options.start), options.length);
    }

    /// Can't default construct a slice since its always a reference to another
    /// thing.
    slice() = delete;

    template <typename T>
    friend slice<T> ok::raw_slice(T& data, size_t size) OKAYLIB_NOEXCEPT;
    template <typename T> friend slice<T> make_null_slice() OKAYLIB_NOEXCEPT;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<slice>;
#endif
};

template <typename viewed_t, size_t size>
slice(viewed_t (&)[size]) -> slice<viewed_t>;

namespace detail {
template <typename T, bool is_const>
using conditionally_const_t = std::conditional_t<is_const, const T, T>;
}

template <typename T>
slice(T)
    -> slice<std::enable_if_t<
        detail::is_std_container_v<detail::remove_cvref_t<T>>,
        detail::conditionally_const_t<
            detail::remove_cvref_t<decltype(*std::declval<T>().data())>,
            std::is_const_v<typename detail::remove_cvref_t<T>::value_type>>>>;

using bytes_t = slice<uint8_t>;

namespace detail {
struct null_bit_slice_tag
{};
inline static uint8_t dummy_byte = 0;
inline static uint8_t* const dummy_byte_ptr = &dummy_byte;
} // namespace detail

class bit
{
  public:
    explicit constexpr bit(bool b) : m_bool(b) {}

    constexpr void flip() noexcept { m_bool = !m_bool; }

    [[nodiscard]] constexpr ok::bit flipped() const noexcept
    {
        return ok::bit(!m_bool);
    }

    constexpr explicit operator bool() const { return m_bool; }

    [[nodiscard]] constexpr friend bool operator==(bit a, bit b) noexcept
    {
        return a.m_bool == b.m_bool;
    }

    [[nodiscard]] constexpr friend bool operator!=(bit a, bit b) noexcept
    {
        return a.m_bool != b.m_bool;
    }

    [[nodiscard]] constexpr friend bool operator==(bit a, bool b) noexcept
    {
        return a.m_bool == b;
    }

    [[nodiscard]] constexpr friend bool operator==(bool b, bit a) noexcept
    {
        return a.m_bool == b;
    }

    [[nodiscard]] constexpr friend bool operator!=(bit a, bool b) noexcept
    {
        return a.m_bool != b;
    }

    [[nodiscard]] constexpr friend bool operator!=(bool b, bit a) noexcept
    {
        return a.m_bool != b;
    }

    // using functions here so that it can be constexpr, "static const" is other
    // option
    [[nodiscard]] constexpr static bit on() noexcept { return bit(true); }
    [[nodiscard]] constexpr static bit off() noexcept { return bit(false); }

  private:
    bool m_bool;
};

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

    const_bit_slice_t(detail::null_bit_slice_tag)
        : m(members_t{
              .num_bits = 0,
              .first_byte = detail::dummy_byte_ptr,
              .offset = 0,
          })
    {
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

    [[nodiscard]] constexpr bit get_bit(const size_t idx) const OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to const_bit_slice_t::get_bit.");
        }
        const size_t bit_idx = idx + m.offset;
        const size_t byte = bit_idx / 8;
        const size_t bit = bit_idx % 8;
        const uint8_t mask = uint8_t(1) << bit;
        return ok::bit(m.first_byte[byte] & mask);
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

    bit_slice_t(detail::null_bit_slice_tag)
        : const_bit_slice_t(detail::null_bit_slice_tag{})
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
                           const bit status) const OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to bit_slice_t::set_bit.");
        }
        const size_t bit_idx = idx + m.offset;
        const size_t byte = bit_idx / 8;
        const size_t bit = bit_idx % 8;

        const uint8_t mask = uint8_t(1) << bit;

        if (status == bit::on()) {
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
    if (bytes.is_empty()) [[unlikely]] {
        return bit_slice_t(detail::null_bit_slice_tag{});
    }
    if (round_up_to_multiple_of<8>((num_bits + offset)) / 8 > bytes.size())
        [[unlikely]] {
        __ok_abort("Invalid number of bits requested from slice of bytes in "
                   "raw_bit_slice().");
    }
    if (offset >= 8) [[unlikely]] {
        __ok_abort("Offset greater than 7 passed to raw_bit_slice.");
    }
    return bit_slice_t(bytes.unchecked_address_of_first_item(), num_bits,
                       offset);
}

constexpr const_bit_slice_t raw_bit_slice(slice<const uint8_t> bytes,
                                          size_t num_bits,
                                          uint8_t offset) OKAYLIB_NOEXCEPT
{
    if (bytes.is_empty()) [[unlikely]] {
        return const_bit_slice_t(detail::null_bit_slice_tag{});
    }
    if (round_up_to_multiple_of<8>((num_bits + offset)) / 8 > bytes.size()) {
        __ok_abort("Invalid number of bits requested from slice of bytes in "
                   "raw_bit_slice().");
    }
    if (offset >= 8) {
        __ok_abort("Offset greater than 7 passed to raw_bit_slice.");
    }
    // const cast here to simplify const bit slice implementation (allows
    // regular bit slice to be a child class)
    return const_bit_slice_t(
        const_cast<uint8_t*>(bytes.unchecked_address_of_first_item()), num_bits,
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
        if (uintptr_t(bytes.unchecked_address_of_first_item()) %
                alignof(viewed_t) !=
            0) [[unlikely]] {
            __ok_abort("Attempt to construct an undefined_memory_t of a type T "
                       "from a bytes_t, but the given bytes are not aligned "
                       "properly to store type T.");
        }

        return undefined_memory_t(*reinterpret_cast<viewed_t*>(
                                      bytes.unchecked_address_of_first_item()),
                                  bytes.size() / sizeof(viewed_t));
    }

    // If the type is trivially constructible, you can pretend it has been
    // initialized.
    template <typename T = viewed_t>
    [[nodiscard]] constexpr slice<
        std::enable_if_t<std::is_trivially_constructible_v<T>, T>>
    leave_undefined() const OKAYLIB_NOEXCEPT
    {
        return raw_slice(*m_data, m_elements);
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

        return raw_slice(*m_data, m_elements);
    }

    template <typename T = viewed_t>
    [[nodiscard]] constexpr auto zero() const OKAYLIB_NOEXCEPT
        -> std::enable_if_t<std::is_same_v<T, viewed_t> &&
                                std::is_trivially_constructible_v<T>,
                            slice<viewed_t>>
    {
        std::memset((void*)m_data, 0, m_elements);
        return raw_slice(*m_data, m_elements);
    }

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return m_elements;
    }
};

/// Make a slice of only a part of a contiguous stdlib container
/// from: index to start subslice from
/// to: index to end subslice at (must be greater than `from` and less than
/// container.size())
template <typename container_t>
[[nodiscard]] constexpr auto
subslice(container_t& container,
         const subslice_options_t& options) OKAYLIB_NOEXCEPT
    -> std::enable_if_t<
        detail::is_std_container_v<container_t>,
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

template <typename viewed_t>
[[nodiscard]] constexpr auto
subslice(const slice<viewed_t>& slice,
         const subslice_options_t& options) OKAYLIB_NOEXCEPT
{
    return slice.subslice(options);
}

/// Construct a slice from a starting item and a number of items. Generally a
/// bad idea, but useful when interfacing with things like c-style strings.
template <typename viewed_t>
[[nodiscard]] slice<viewed_t> raw_slice(viewed_t& data,
                                        size_t size) OKAYLIB_NOEXCEPT
{
    return slice<viewed_t>(data, size);
}

template <typename viewed_t>
[[nodiscard]] slice<viewed_t> make_null_slice() OKAYLIB_NOEXCEPT
{
    return slice<viewed_t>(typename slice<viewed_t>::null_slice_tag{});
}

template <typename T>
[[nodiscard]] constexpr slice<T> slice_from_one(T& item) OKAYLIB_NOEXCEPT
{
    return raw_slice(item, 1);
}

template <typename range_t, typename enable> struct range_definition;

template <typename viewed_t> struct range_definition<slice<viewed_t>, void>
{
    static constexpr bool is_ref_wrapper = true;
    static constexpr bool is_arraylike = true;
    static constexpr bool is_view = false;

    using value_type = viewed_t;

    static constexpr size_t size(const slice<viewed_t>& slice) OKAYLIB_NOEXCEPT
    {
        return slice.size();
    }

    static constexpr value_type& get_ref(const slice<viewed_t>& range,
                                         size_t cursor) OKAYLIB_NOEXCEPT
    {
        return range[cursor];
    }
};

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

    static constexpr bit get(const const_bit_slice_t& range, size_t cursor)
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

    static constexpr bit get(const bit_slice_t& range, size_t cursor)
    {
        return range.get_bit(cursor);
    }

    static constexpr void set(bit_slice_t& range, size_t cursor, bit value)
    {
        return range.set_bit(cursor, value);
    }
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <> struct fmt::formatter<ok::bit>
{
    constexpr auto
    parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    auto format(const ok::bit& b,
                format_context& ctx) const -> format_context::iterator
    {
        return fmt::format_to(ctx.out(), b ? "1" : "0");
    }
};

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
        for (int64_t i = bs.size() - 1; i >= 0; --i) {
            fmt::format_to(ctx.out(), "{}", bs.get_bit(i));
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
