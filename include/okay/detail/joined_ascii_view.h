#ifndef __OKAYLIB_DETAIL_JOINED_ASCII_VIEW_H__
#define __OKAYLIB_DETAIL_JOINED_ASCII_VIEW_H__

#include "okay/ascii_view.h"

namespace ok {
namespace detail {
template <typename T, size_t num_items> struct constexpr_array_t
{
    using value_type = T;

    [[nodiscard]] constexpr const value_type&
    operator[](size_t index) const& OKAYLIB_NOEXCEPT
    {
        if (index >= num_items) [[unlikely]] {
            __ok_abort("Out of bounds access into constexpr_array_t");
        }
        return __m_items[index];
    }

    [[nodiscard]] constexpr value_type& operator[](size_t index) &
        OKAYLIB_NOEXCEPT
    {
        if (index >= num_items) [[unlikely]] {
            __ok_abort("Out of bounds access into constexpr_array_t");
        }
        return __m_items[index];
    }

    [[nodiscard]] constexpr value_type* data() & noexcept { return __m_items; }
    [[nodiscard]] constexpr const value_type* data() const& noexcept
    {
        return __m_items;
    }

    [[nodiscard]] constexpr size_t size() const noexcept { return num_items; }

    T __m_items[num_items];
};

template <typename T, typename... pack>
    requires(stdc::is_same_v<T, pack> && ...)
constexpr_array_t(T, pack...) -> constexpr_array_t<T, 1 + sizeof...(pack)>;

template <constexpr_array_t V> struct make_static_constexpr_array_t
{
    static constexpr auto value = V;
};

/// thanks, glaze
template <const ascii_view&... ascii_views>
inline constexpr ascii_view ascii_view_join_constexpr_impl()
{
    constexpr auto joined_arr = []() {
        constexpr size_t len = (ascii_views.size() + ... + 0);
        detail::constexpr_array_t<char, len + 1> arr;
        auto append = [i = 0, &arr](const auto& s) mutable {
            for (size_t c = 0; c < s.size(); ++c)
                arr[i++] = s[c];
        };
        (append(ascii_views), ...);
        arr[len] = '\0';
        return arr;
    }();
    const auto& static_arr =
        detail::make_static_constexpr_array_t<joined_arr>::value;
    return ascii_view::from_raw(static_arr.data(), static_arr.size() - 1);
}
} // namespace detail

template <const ascii_view&... ascii_views>
inline constexpr auto joined_ascii_view =
    detail::ascii_view_join_constexpr_impl<ascii_views...>();
} // namespace ok

#endif
