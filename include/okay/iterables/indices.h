#ifndef __OKAYLIB_RANGES_INDICES_H__
#define __OKAYLIB_RANGES_INDICES_H__

#include "okay/iterables/iterables.h"
#include <cstddef>
#include <cstdint>

namespace ok {

namespace detail {
struct indices_t
{
    struct cursor_t
    {
        static constexpr bool is_infinite = true;
        using value_type = size_t;

        size_t m_index;

        cursor_t() = delete;
        constexpr cursor_t(size_t index) : m_index(index) {}

        constexpr size_t access(const indices_t& /* iterable */) const
        {
            return m_index;
        }

        constexpr void offset(const indices_t& /* iterable */,
                              int64_t offset_amount)
        {
            m_index += offset_amount;
        }

        constexpr size_t index(const indices_t& /* iterable */) const
        {
            return m_index;
        }
    };
};
} // namespace detail

constexpr auto indices() OKAYLIB_NOEXCEPT
{
    using namespace detail;
    return owning_arraylike_iterator_t<indices_t, indices_t::cursor_t>{
        indices_t{}, indices_t::cursor_t{0}};
}
} // namespace ok

#endif
