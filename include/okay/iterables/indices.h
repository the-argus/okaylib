#ifndef __OKAYLIB_RANGES_INDICES_H__
#define __OKAYLIB_RANGES_INDICES_H__

#include "okay/iterables/iterables.h"
#include <cstddef>
#include <cstdint>

namespace ok {

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

        constexpr size_t index(indices_t& /* iterable */) const
        {
            return m_index;
        }
    };

    constexpr auto iter() const
    {
        // can be owning because every instance of indices_t is equivalent
        return owning_iterator_t<indices_t, cursor_t>{indices_t{}, cursor_t{0}};
    }

    constexpr auto iter_from(size_t first_index) const
    {
        return owning_iterator_t<indices_t, cursor_t>{
            indices_t{},
            cursor_t{first_index},
        };
    }
};

constexpr inline indices_t indices{};

} // namespace ok

#endif
