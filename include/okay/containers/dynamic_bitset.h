#ifndef __OKAYLIB_CONTAINERS_DYNAMIC_BITSET_H__
#define __OKAYLIB_CONTAINERS_DYNAMIC_BITSET_H__

#include "okay/allocators/allocator.h"
#include "okay/ranges/ranges.h"

namespace ok {
namespace dynamic_bitset::detail {
struct preallocated_and_zeroed_t;
struct copy_booleans_from_range_t;
} // namespace dynamic_bitset::detail
template <typename backing_allocator_t = ok::allocator_t> class dynamic_bitset_t
{
    static_assert(
        detail::is_derived_from_v<backing_allocator_t, ok::allocator_t>,
        "Invalid type given as allocator to dynamic_bitset_t.");
    struct members_t
    {
        size_t num_bits;
        uint8_t* data;
        size_t num_bytes_allocated;
        backing_allocator_t* allocator;
    } m;

    constexpr void destroy() OKAYLIB_NOEXCEPT
    {
        if (m.data) {
            m.allocator->deallocate(raw_slice(*m.data, m.num_bytes_allocated));
        }
    }

  public:
    friend struct ok::dynamic_bitset::detail::preallocated_and_zeroed_t;
    friend struct ok::dynamic_bitset::detail::copy_booleans_from_range_t;

    dynamic_bitset_t() = delete;

    explicit dynamic_bitset_t(backing_allocator_t& allocator) OKAYLIB_NOEXCEPT
        : m(members_t{.allocator = ok::addressof(allocator)})
    {
    }

    constexpr dynamic_bitset_t(dynamic_bitset_t&& other) noexcept : m(other.m)
    {
        other.m.data = nullptr;
#ifndef NDEBUG
        other.m = {};
#endif
    }

    template <typename other_allocator_t>
    constexpr auto operator=(dynamic_bitset_t<other_allocator_t>&& other)
        -> std::enable_if_t<
            std::is_same_v<backing_allocator_t, ok::allocator_t> ||
                std::is_same_v<backing_allocator_t, other_allocator_t>,
            dynamic_bitset_t&>
    {
        destroy();
        m = members_t{
            .num_bits = other.m.num_bits,
            .data = std::exchange(other.m.data, nullptr),
            .num_bytes_allocated = other.m.num_bytes_allocated,
            .allocator = static_cast<backing_allocator_t*>(other.allocator),
        };
#ifndef NDEBUG
        other.m = {};
#endif
    }

    [[nodiscard]] constexpr bit_slice_t items() & OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(raw_slice(*m.data, m.num_bytes_allocated),
                             m.num_bits, 0);
    }
    [[nodiscard]] constexpr const_bit_slice_t items() const& OKAYLIB_NOEXCEPT
    {
        return raw_bit_slice(raw_slice(*m.data, m.num_bytes_allocated),
                             m.num_bits, 0);
    }

    constexpr operator bit_slice_t() & OKAYLIB_NOEXCEPT { return items(); }
    constexpr operator const_bit_slice_t() const& OKAYLIB_NOEXCEPT
    {
        return items();
    }

    [[nodiscard]] constexpr size_t size() const noexcept { return m.num_bits; }

    // TODO: these functions
    [[nodiscard]] constexpr status<alloc::error>
    insert_at(size_t idx, bool value) OKAYLIB_NOEXCEPT;
    [[nodiscard]] constexpr status<alloc::error>
    append(bool value) OKAYLIB_NOEXCEPT;
    constexpr bool remove(size_t idx) OKAYLIB_NOEXCEPT;
    constexpr status<alloc::error>
    increase_capacity_by(size_t new_spots) OKAYLIB_NOEXCEPT;
    constexpr opt<bool> remove_and_swap_last(size_t idx) OKAYLIB_NOEXCEPT;
    constexpr void shrink_to_reclaim_unused_memory() OKAYLIB_NOEXCEPT;
    [[nodiscard]] constexpr status<alloc::error>
    resize(size_t new_size) OKAYLIB_NOEXCEPT;

    [[nodiscard]] constexpr size_t capacity() const OKAYLIB_NOEXCEPT
    {
        return m.num_bytes_allocated * 8;
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

    ~dynamic_bitset_t() { destroy(); }
};

namespace dynamic_bitset {
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
        dynamic_bitset_t<::ok::detail::remove_cvref_t<allocator_t>>;

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr auto
    operator()(backing_allocator_t& allocator,
               const options_t& options) const OKAYLIB_NOEXCEPT
    {
        return ok::make(*this, allocator);
    }

    template <typename backing_allocator_t>
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(dynamic_bitset_t<backing_allocator_t>& uninit,
                     backing_allocator_t& allocator,
                     const options_t& options) const OKAYLIB_NOEXCEPT
    {
        using type = dynamic_bitset_t<backing_allocator_t>;

        if (options.num_initial_bits + options.additional_capacity_in_bits ==
            0) {
            new (ok::addressof(uninit)) type(allocator);
            return alloc::error::okay;
        }

        const size_t bytes_needed =
            round_up_to_multiple_of<8>(options.num_initial_bits +
                                       options.additional_capacity_in_bits) /
            8;

        alloc::result_t<bytes_t> result = allocator.allocate(alloc::request_t{
            .num_bytes = bytes_needed,
            .alignment = 1,
            .flags = alloc::flags::leave_nonzeroed,
        });

        if (!result.okay()) [[unlikely]] {
            return result.err();
        }

        auto& bytes = result.release_ref();

        // zero data in use
        if (options.num_initial_bits != 0) {
            std::memset(bytes.data(), 0,
                        round_up_to_multiple_of<8>(options.num_initial_bits) /
                            8);
        }

        uninit.m = typename type::members_t{
            .num_bits = options.num_initial_bits,
            .data = bytes.data(),
            .num_bytes_allocated = bytes.size(),
            .allocator = ok::addressof(allocator),
        };
        return alloc::error::okay;
    }
};

struct copy_booleans_from_range_t
{
    template <typename backing_allocator_t, typename...>
    using associated_type = dynamic_bitset_t<backing_allocator_t>;

    template <typename backing_allocator_t, typename input_range_t>
    [[nodiscard]] constexpr status<alloc::error>
    make_into_uninit(dynamic_bitset_t<backing_allocator_t>& uninit,
                     backing_allocator_t& allocator,
                     const input_range_t& range) const OKAYLIB_NOEXCEPT
    {
        static_assert(ok::detail::range_definition_has_size_v<input_range_t>,
                      "Size of range unknown, refusing to use its items in "
                      "dynamic_bitset::copy_booleans_from_range constructor.");
        static_assert(
            std::is_convertible_v<value_type_for<input_range_t>, bool>,
            "The range given to dynamic_bitset::copy_booleans_from_range does "
            "not return something which can be converted to a boolean.");

        constexpr preallocated_and_zeroed_t other{};

        const size_t size = ok::size(range);
        auto result =
            other.make_into_uninit(uninit, allocator,
                                   preallocated_and_zeroed_t::options_t{
                                       .additional_capacity_in_bits = size});
        if (!result.okay()) [[unlikely]] {
            return result.err();
        }

        __ok_internal_assert(uninit.m.num_bytes_allocated * 8 >= size);

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

} // namespace dynamic_bitset

} // namespace ok

#endif
