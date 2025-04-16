#ifndef __OKAYLIB_CONTAINERS_DYNAMIC_BITSET_H__
#define __OKAYLIB_CONTAINERS_DYNAMIC_BITSET_H__

#include "okay/allocators/allocator.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace bit_arraylist {
struct upcast_tag
{};
namespace detail {
struct preallocated_and_zeroed_t;
struct copy_booleans_from_range_t;
// pass a zero sized slice of this when no memory is available;
inline constexpr uint8_t dummy_mem = 0;
} // namespace detail
} // namespace bit_arraylist
template <typename backing_allocator_t = ok::allocator_t> class bit_arraylist_t
{
    static_assert(std::is_class_v<backing_allocator_t>,
                  "Do not pass a non-class type (reference, array, pointer) in "
                  "place of an allocator");
    static_assert(
        detail::is_derived_from_v<backing_allocator_t, ok::allocator_t>,
        "Invalid type given as allocator to bit_arraylist_t.");
    struct members_t
    {
        size_t num_bits;
        bytes_t allocation;
        backing_allocator_t* allocator;
    } m;

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (m.allocation.size() != 0) {
            m.allocator->deallocate(m.allocation);
        }
    }

  public:
    friend struct ok::bit_arraylist::detail::preallocated_and_zeroed_t;
    friend struct ok::bit_arraylist::detail::copy_booleans_from_range_t;

    template <typename other_allocator_t> friend class ok::bit_arraylist_t;

    bit_arraylist_t() = delete;

    using allocator_type = backing_allocator_t;

    explicit bit_arraylist_t(backing_allocator_t& allocator) OKAYLIB_NOEXCEPT
        : m(members_t{
              .allocation = raw_slice_create_null_empty_unsafe<uint8_t>(),
              .allocator = ok::addressof(allocator),
          })
    {
    }

    constexpr bit_arraylist_t(bit_arraylist_t&& other) noexcept : m(other.m)
    {
        other.m.allocation = raw_slice_create_null_empty_unsafe<uint8_t>();
#ifndef NDEBUG
        other.m.allocator = nullptr;
        other.m.num_bits = 0;
#endif
    }

    // allow upcasting to ok::allocator_t if you explicitly construct with
    // upcast_tag
    template <typename other_allocator_t,
              std::enable_if_t<std::is_convertible_v<other_allocator_t*,
                                                     backing_allocator_t*>,
                               bool> = true>
    constexpr bit_arraylist_t(const bit_arraylist::upcast_tag&,
                               bit_arraylist_t<other_allocator_t>&& other)
        : m(members_t{
              .num_bits = other.m.num_bits,
              .allocation =
                  std::exchange(other.m.allocation,
                                raw_slice_create_null_empty_unsafe<uint8_t>()),
              .allocator = static_cast<backing_allocator_t*>(other.m.allocator),
          })
    {
#ifndef NDEBUG
        other.m.num_bits = 0;
        other.m.allocator = nullptr;
#endif
    }

    template <typename other_allocator_t>
    constexpr auto operator=(bit_arraylist_t<other_allocator_t>&& other)
        -> std::enable_if_t<
            std::is_convertible_v<other_allocator_t*, backing_allocator_t*>,
            bit_arraylist_t&>
    {
        destroy();
        m = members_t{
            .num_bits = other.m.num_bits,
            .allocation =
                std::exchange(other.m.allocation,
                              raw_slice_create_null_empty_unsafe<uint8_t>()),
            .allocator = static_cast<backing_allocator_t*>(other.m.allocator),
        };
#ifndef NDEBUG
        other.m.num_bits = 0;
        other.m.allocator = nullptr;
#endif
        return *this;
    }

    [[nodiscard]] constexpr bit_slice_t items() & OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(m.allocation, m.num_bits, 0);
    }
    [[nodiscard]] constexpr const_bit_slice_t items() const& OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(m.allocation, m.num_bits, 0);
    }

    [[nodiscard]] constexpr uint8_t* data() & OKAYLIB_NOEXCEPT
    {
        return m.allocation.data();
    }
    uint8_t* data() && = delete;

    [[nodiscard]] constexpr const uint8_t* data() const& OKAYLIB_NOEXCEPT
    {
        return m.allocation.data();
    }
    const uint8_t* data() const&& = delete;

    constexpr operator bit_slice_t() & OKAYLIB_NOEXCEPT { return items(); }
    constexpr operator const_bit_slice_t() const& OKAYLIB_NOEXCEPT
    {
        return items();
    }

    // do not convert rvalue into slice EVER
    constexpr operator bit_slice_t() && = delete;
    constexpr operator bit_slice_t() const&& = delete;
    constexpr operator const_bit_slice_t() && = delete;
    constexpr operator const_bit_slice_t() const&& = delete;
    constexpr bit_slice_t items() && = delete;
    constexpr const_bit_slice_t items() const&& = delete;

    [[nodiscard]] constexpr size_t size() const noexcept { return m.num_bits; }

    constexpr void set_all_bits(bool value) OKAYLIB_NOEXCEPT
    {
        if (m.allocation.size() == 0) [[unlikely]] {
            return;
        }
        std::memset(m.allocation.data(), value ? char(-1) : char(0),
                    this->size_bytes());
    }

    constexpr size_t size_bytes() const OKAYLIB_NOEXCEPT
    {
        return round_up_to_multiple_of<8>(m.num_bits) / 8;
    }

    constexpr void set_bit(size_t idx, bool value) OKAYLIB_NOEXCEPT
    {
        items().set_bit(idx, value);
    }

    [[nodiscard]] constexpr bool get_bit(size_t idx) const OKAYLIB_NOEXCEPT
    {
        return items().get_bit(idx);
    }

    constexpr void toggle_bit(size_t idx) OKAYLIB_NOEXCEPT
    {
        items().toggle_bit(idx);
    }

    template <typename other_allocator_t>
    constexpr bool memcompare_with(
        const bit_arraylist_t<other_allocator_t>& other) const OKAYLIB_NOEXCEPT
    {
        // make sure both bitsets have data, if theyre both empty then this
        // returns true
        if (other.m.allocation.size() == 0 || m.allocation.size() == 0)
            [[unlikely]] {
            return other.m.allocation.size() == m.allocation.size();
        }

        return ok::memcompare(m.allocation.subslice({
                                  .length = this->size_bytes(),
                              }),
                              other.m.allocation.subslice({
                                  .length = other.size_bytes(),
                              }));
    }

    [[nodiscard]] constexpr status<alloc::error>
    ensure_additional_capacity() OKAYLIB_NOEXCEPT
    {
        if (m.allocation.size() == 0) {
            auto status = this->first_allocation();
            if (!status.okay()) [[unlikely]] {
                return status;
            }
        } else if (m.allocation.size() <= m.num_bits * 8) {
            auto status = reallocate(1, m.allocation.size() * 2);
            if (!status.okay()) [[unlikely]] {
                return status;
            }
        }
        return alloc::error::okay;
    }

    // TODO: these functions
    [[nodiscard]] constexpr status<alloc::error>
    insert_at(size_t idx, bool value) OKAYLIB_NOEXCEPT
    {
        __ok_internal_assert(this->capacity() >= this->size());
        if (auto status = this->ensure_additional_capacity(); !status.okay())
            [[unlikely]] {
            return status;
        }
        __ok_internal_assert(this->capacity() > this->size());
        constexpr uint8_t carry_in_mask = 0b001;
        constexpr uint8_t carry_check_mask = 0b10000000;

        constexpr auto shift_byte_zero_return_carry =
            [](uint8_t* byte, size_t bit_index, bool bit_is_on) -> bool {
            __ok_internal_assert(bit_index < 8);

            // mask of the bits that should be shifted
            const uint8_t shift_mask = (~uint8_t(0)) << bit_index;
            const bool carry = (*byte & carry_check_mask) != 0;
            const uint8_t shifted = (*byte & shift_mask) << 1;

            // zero stuff that was shifted, so only unmoved stuff is left in the
            // bit
            *byte &= ~shift_mask;
            // insert the shifted stuff
            *byte |= shifted;
            // insert the byte we're setting. if (!bit_is_on), this does nothing
            *byte |= (bit_is_on ? uint8_t(1) : uint8_t(0)) << bit_index;

            return carry;
        };

        const size_t first_byte_index = idx / 8;
        const size_t sub_byte_bit_index = idx % 8;

        bool carry = shift_byte_zero_return_carry(
            m.allocation.data() + first_byte_index, sub_byte_bit_index, value);

        // loop from zero to the number of bytes in use, shifting everything up
        // TODO: check if this gets optimized, maybe a good idea for
        // dynamic bitset to use u64 internally

        const size_t num_bytes =
            round_up_to_multiple_of<8>(m.num_bits - idx) / 8;

        for (size_t iter = first_byte_index + 2; iter < num_bytes; ++iter) {
            const size_t i = iter - 1;
            const bool new_carry = m.allocation.data()[i] & carry_check_mask;
            m.allocation.data()[i] <<= 1;
            m.allocation.data()[i] ^= carry_in_mask * carry;
            carry = new_carry;
        }

        return alloc::error::okay;
    }

    [[nodiscard]] constexpr status<alloc::error>
    append(bool value) OKAYLIB_NOEXCEPT
    {
        return this->insert_at(this->size(), value);
    }

    constexpr bool remove(size_t idx) OKAYLIB_NOEXCEPT
    {
        if (idx >= this->size()) [[unlikely]] {
            __ok_abort("Out of bounds access to bit_arraylist_t in remove()");
        }

        constexpr uint8_t carry_check_mask = 0b001;
        constexpr uint8_t carry_in_mask = 0b10000000;

        const size_t byte_index = idx / 8;
        const size_t sub_byte_bit_index = idx % 8;

        constexpr auto shift_last_byte_and_return_whether_bit_was_on =
            [](uint8_t* byte, size_t bit_index, bool removal_carry_in) -> bool {
            __ok_internal_assert(bit_index < 8);

            const uint8_t shift_mask = (~uint8_t(0)) << (bit_index + 1);
            const uint8_t bit_mask = uint8_t(1) << bit_index;
            const uint8_t out = *byte & bit_mask;
            const uint8_t shifted = (*byte & shift_mask) >> 1;

            *byte &= ~((~uint8_t(0)) << bit_index);
            *byte |= shifted;
            *byte |= carry_in_mask * removal_carry_in;
            return out;
        };

        // always carrying in a zero, it will be unused after this anyways
        // bc we decrease m.num_bits
        bool carry = false;
        const size_t num_bytes_in_use =
            round_up_to_multiple_of<8>(m.num_bits) / 8;
        // NOTE: skipping last byte in this for loop
        for (size_t i = num_bytes_in_use; i > byte_index; --i) {
            const bool new_carry =
                (m.allocation.data()[i] & carry_check_mask) != 0;
            m.allocation.data()[i] >> 1;
            // add most significant bit if it carried from above
            m.allocation.data()[i] |= (carry * carry_in_mask);
            carry = new_carry;
        }
        m.num_bits -= 1;
        return shift_last_byte_and_return_whether_bit_was_on(
            m.allocation.data() + byte_index, sub_byte_bit_index, carry);
    }

    constexpr status<alloc::error>
    increase_capacity_by(size_t new_spots) OKAYLIB_NOEXCEPT
    {
        if (new_spots == 0) [[unlikely]] {
            // TODO: can we just guarantee that all allocators do this?
            __ok_assert(false, "Attempt to increase capacity by 0.");
            return alloc::error::unsupported;
        }
        if (m.allocation.size() == 0) {
            return this->first_allocation(new_spots);
        } else {
            return this->reallocate(round_up_to_multiple_of<8>(new_spots) / 8,
                                    0);
        }
    }

    // constexpr opt<bool> remove_and_swap_last(size_t idx) OKAYLIB_NOEXCEPT;

    // constexpr void shrink_to_reclaim_unused_memory() OKAYLIB_NOEXCEPT;

    [[nodiscard]] constexpr status<alloc::error>
    resize(size_t new_size) OKAYLIB_NOEXCEPT;

    [[nodiscard]] constexpr size_t capacity() const OKAYLIB_NOEXCEPT
    {
        return m.allocation.size() * 8;
    }

    [[nodiscard]] constexpr bool is_empty() const OKAYLIB_NOEXCEPT
    {
        return this->size() == 0;
    }

    constexpr opt<bool> pop_last() OKAYLIB_NOEXCEPT
    {
        if (this->is_empty())
            return {};
        return this->remove(this->size() - 1);
    }

    [[nodiscard]] constexpr const backing_allocator_t&
    allocator() const OKAYLIB_NOEXCEPT
    {
        return *m.allocator;
    }

    [[nodiscard]] constexpr backing_allocator_t& allocator() OKAYLIB_NOEXCEPT
    {
        return *m.allocator;
    }

    constexpr void clear() OKAYLIB_NOEXCEPT { m.num_bits = 0; }

    ~bit_arraylist_t() { destroy(); }

  private:
    /// This function initializes m.allocation
    [[nodiscard]] constexpr status<alloc::error>
    first_allocation(size_t total_allocated_bits = 40) OKAYLIB_NOEXCEPT
    {
        __ok_assert(m.allocator,
                    "Attempt to operate on an invalid bit_arraylist_t");
        __ok_internal_assert(total_allocated_bits != 0);

        const size_t bytes_needed =
            round_up_to_multiple_of<8>(total_allocated_bits) / 8UL;

        alloc::result_t<bytes_t> result =
            m.allocator->allocate(alloc::request_t{
                .num_bytes = bytes_needed,
                .alignment = 1,
            });

        if (!result.okay()) [[unlikely]] {
            return result.err();
        }

        m.allocation = result.release_ref();

        return alloc::error::okay;
    }

    [[nodiscard]] constexpr status<alloc::error>
    reallocate(size_t bytes_required, size_t bytes_preferred) OKAYLIB_NOEXCEPT
    {
        using namespace alloc;

        result_t<bytes_t> res = m.allocator->reallocate(reallocate_request_t{
            .memory = m.allocation,
            .new_size_bytes = m.allocation.size() + bytes_required,
            .preferred_size_bytes = bytes_preferred == 0
                                        ? 0
                                        : m.allocation.size() + bytes_preferred,
            .flags = flags::expand_back,
        });

        if (!res.okay()) [[unlikely]] {
            return res.err();
        }

        m.allocation = res.release_ref();

        return alloc::error::okay;
    }
};

template <typename backing_allocator_t>
struct range_definition<bit_arraylist_t<backing_allocator_t>>
{
    using bit_arraylist_t = bit_arraylist_t<backing_allocator_t>;
    static constexpr size_t begin(const bit_arraylist_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }
    static constexpr bool is_inbounds(const bit_arraylist_t& bs,
                                      size_t cursor) OKAYLIB_NOEXCEPT
    {
        return cursor < bs.size();
    }

    static constexpr size_t size(const bit_arraylist_t& bs) OKAYLIB_NOEXCEPT
    {
        return bs.size();
    }

    static constexpr bool get(const bit_arraylist_t& range, size_t cursor)
    {
        return range.get_bit(cursor);
    }

    static constexpr void set(bit_arraylist_t& range, size_t cursor,
                              bool value)
    {
        return range.set_bit(cursor, value);
    }
};

namespace bit_arraylist {
namespace detail {
struct preallocated_and_zeroed_t
{
    struct options_t
    {
        size_t num_initial_bits = 0;
        size_t additional_capacity_in_bits = 0;
    };

    template <typename allocator_t, typename...>
    using associated_type =
        bit_arraylist_t<::ok::detail::remove_cvref_t<allocator_t>>;

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator, options);
    }

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(bit_arraylist_t<backing_allocator_t>& uninit,
                     backing_allocator_t& allocator,
                     const options_t& options) const OKAYLIB_NOEXCEPT
    {
        using type = bit_arraylist_t<backing_allocator_t>;

        const size_t total_bits =
            options.num_initial_bits + options.additional_capacity_in_bits;

        if (total_bits == 0) {
            new (ok::addressof(uninit)) type(allocator);
            return alloc::error::okay;
        }

        uninit.m.allocator = ok::addressof(allocator);

        auto status = uninit.first_allocation(total_bits);
        if (!status.okay()) [[unlikely]] {
            return status;
        }

        // last piece that needs to be initialized
        uninit.m.num_bits = options.num_initial_bits;

        return alloc::error::okay;
    }
};

struct copy_booleans_from_range_t
{
    template <typename backing_allocator_t, typename...>
    using associated_type =
        bit_arraylist_t<std::remove_reference_t<backing_allocator_t>>;

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(::ok::detail::is_derived_from_v<backing_allocator_t,
                                                      ok::allocator_t>,
                      "Invalid allocator passed to copy_booleans_from_range");
        static_assert(::ok::detail::is_input_range_v<input_range_t>,
                      "Non-range object passed to second argument of "
                      "copy_booleans_from_range.");
        return ok::make(*this, allocator, range);
    };

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(bit_arraylist_t<backing_allocator_t>& uninit,
                     backing_allocator_t& allocator,
                     const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(ok::detail::range_definition_has_size_v<input_range_t>,
                      "Size of range unknown, refusing to use its items in "
                      "bit_arraylist::copy_booleans_from_range constructor.");
        static_assert(
            std::is_convertible_v<value_type_for<input_range_t>, bool>,
            "The range given to bit_arraylist::copy_booleans_from_range does "
            "not return something which can be converted to a boolean.");

        constexpr preallocated_and_zeroed_t other{};

        const size_t size = ok::size(range);
        auto result =
            other.make_into_uninit(uninit, allocator,
                                   preallocated_and_zeroed_t::options_t{
                                       .additional_capacity_in_bits = size,
                                   });
        if (!result.okay()) [[unlikely]] {
            return result.err();
        }

        __ok_internal_assert(uninit.m.allocation.size() * 8 >= size);

        uninit.m.num_bits = size;

        size_t count = 0;
        for (auto c = ok::begin(range); ok::is_inbounds(range, c);
             ok::increment(range, c)) {
            uninit.items().set_bit(count,
                                   bool(iter_get_temporary_ref(range, c)));
            ++count;
        }

        return alloc::error::okay;
    }
};
} // namespace detail

inline constexpr detail::copy_booleans_from_range_t copy_booleans_from_range;
inline constexpr detail::preallocated_and_zeroed_t preallocated_and_zeroed;

} // namespace bit_arraylist

} // namespace ok

#endif
