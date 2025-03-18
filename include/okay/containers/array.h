#ifndef __OKAYLIB_CONTAINERS_array_t_H__
#define __OKAYLIB_CONTAINERS_array_t_H__

#include "okay/anystatus.h"
#include "okay/construct.h"
#include "okay/detail/abort.h"
#include "okay/detail/noexcept.h"

#include <cstddef>
#include <cstring>

namespace ok {

namespace array {
namespace detail {
template <typename T, size_t num_items> struct defaulted_or_zeroed_t;
template <typename T, size_t num_items> struct undefined_t;
} // namespace detail
} // namespace array

template <typename T, size_t num_items> class array_t
{
  private:
    constexpr array_t() = default;

  public:
    static_assert(!std::is_reference_v<T>,
                  "ok::array_t cannot store references.");
    static_assert(!std::is_void_v<T>, "Cannot create an array_t of void.");

    static_assert(num_items != 0, "Cannot create an array_t of zero items.");

    using value_type = T;

    constexpr array_t(const array_t&) = default;
    constexpr array_t& operator=(const array_t&) = default;
    constexpr array_t(array_t&&) = default;
    constexpr array_t& operator=(array_t&&) = default;

    // factory functions
    friend struct ok::array::detail::defaulted_or_zeroed_t<T, num_items>;
    friend struct ok::array::detail::undefined_t<T, num_items>;

    [[nodiscard]] constexpr value_type& operator[](size_t index) &
        OKAYLIB_NOEXCEPT
    {
        if (index >= num_items) [[unlikely]] {
            __ok_abort("Out of bounds access into ok::array_t");
        }
        return __m_items[index];
    }

    [[nodiscard]] constexpr const value_type&
    operator[](size_t index) const& OKAYLIB_NOEXCEPT
    {
        if (index >= num_items) [[unlikely]] {
            __ok_abort("Out of bounds access into ok::array_t");
        }
        return __m_items[index];
    }

    [[nodiscard]] constexpr value_type&& operator[](size_t index) &&
        OKAYLIB_NOEXCEPT
    {
        if (index >= num_items) [[unlikely]] {
            __ok_abort("Out of bounds access into ok::array_t");
        }
        return std::move(__m_items[index]);
    }

    constexpr value_type&& operator[](size_t index) const&& = delete;

    [[nodiscard]] constexpr value_type* data() & noexcept { return __m_items; }

    [[nodiscard]] constexpr const value_type* data() const& noexcept
    {
        return __m_items;
    }

    [[nodiscard]] constexpr size_t size() const noexcept { return num_items; }

    // this can't be private because we want aggregate initialization
    // please dont touch it :)
    T __m_items[num_items];
};

// aggregate initialization template deducation, so you can do
// `ok::array_t my_array_t = {...}`
template <typename T, typename... pack>
array_t(T, pack...)
    -> array_t<std::enable_if_t<(std::is_same_v<T, pack> && ...), T>,
               1 + sizeof...(pack)>;

namespace array {
namespace detail {
template <typename T, size_t num_items> struct defaulted_or_zeroed_t
{
    [[nodiscard]] constexpr auto make() const noexcept
    {
        array_t<T, num_items> out;
        if constexpr (std::is_trivially_constructible_v<T>) {
            std::memset(out.__m_items, 0, num_items * sizeof(T));
        }
        return out;
    };

    [[nodiscard]] constexpr auto operator()() const noexcept
    {
        return ok::make(*this);
    }
};
template <typename T, size_t num_items> struct undefined_t
{
    [[nodiscard]] constexpr auto make() const noexcept
    {
        return array_t<T, num_items>();
    };

    [[nodiscard]] constexpr auto operator()() const noexcept
    {
        return ok::make(*this);
    }
};
} // namespace detail

template <typename T, size_t num_items>
inline constexpr auto defaulted_or_zeroed =
    detail::defaulted_or_zeroed_t<T, num_items>{};

template <typename T, size_t num_items>
inline constexpr auto undefined = detail::undefined_t<T, num_items>{};

} // namespace array

} // namespace ok

#endif
