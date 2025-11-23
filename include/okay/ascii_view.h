#ifndef __OKAYLIB_ASCII_VIEW_H__
#define __OKAYLIB_ASCII_VIEW_H__

/*
 * Something like std::string_view.
 *
 * Called ascii_view because unqualified/generic names like "string" and "string
 * view" are reseved for the eventual utf8 encoded implementations.
 *
 * Included by reflection core library, so this includes as little as possible.
 */

#include "okay/detail/abort.h"
#include "okay/detail/ok_assert.h"
#include "okay/detail/utility.h"
#include <cstddef>
#include <cstdint>

namespace ok {
namespace detail {
// originally from C++11 "CTTI" library
// From https://github.com/foonathan/string_id
// See http://www.isthe.com/chongo/tech/comp/fnv/#FNV-param
constexpr uint64_t fnv_basis = 14695981039346656037ull;
constexpr uint64_t fnv_prime = 1099511628211ull;
constexpr uint32_t fnv_basis_32 = 0x811c9dc5;
constexpr uint32_t fnv_prime_32 = 0x01000193;

// FNV-1a 64 bit hash
constexpr uint64_t fnv1a_hash(size_t n, const char* str,
                              uint64_t hash = fnv_basis)
{
    return n > 0 ? fnv1a_hash(n - 1, str + 1, (hash ^ *str) * fnv_prime) : hash;
}

template <size_t N> constexpr uint64_t fnv1a_hash(const char (&array)[N])
{
    return fnv1a_hash(N - 1, &array[0]);
}

// FNV-1a 32 bit hash
constexpr uint32_t fnv1a_hash_32(size_t n, const char* str,
                                 uint32_t hash = fnv_basis_32)
{
    return n > 0 ? fnv1a_hash_32(n - 1, str + 1, (hash ^ *str) * fnv_prime_32)
                 : hash;
}

template <size_t N> constexpr uint32_t fnv1a_hash_32(const char (&array)[N])
{
    return fnv1a_hash_32(N - 1, &array[0]);
}
} // namespace detail

class ascii_view
{
  private:
    // substring constructor
    constexpr ascii_view(const char* begin, const char* end)
        : m_str(begin), m_length(static_cast<size_t>(end - begin))
    {
    }
    // from_raw constructor
    constexpr ascii_view(const char* begin, size_t len)
        : m_str(begin), m_length(len)
    {
    }

  public:
    // be std container-like, so we automatically get to be a range
    using value_type = char;

    constexpr ascii_view() = default;

    [[nodiscard]] constexpr static ascii_view from_raw(const char* chars,
                                                       size_t length)
    {
        return {chars, length};
    }

    template <size_t N>
        requires(N > 0)
    constexpr ascii_view(const char (&str)[N])
        : m_str(str), m_length(str[N - 1] == '\0' ? N - 1 : N)
    {
    }

    [[nodiscard]] constexpr uint64_t hash() const
    {
        return detail::fnv1a_hash(size(), m_str);
    }

    [[nodiscard]] constexpr uint64_t hash_32() const
    {
        return detail::fnv1a_hash_32(size(), m_str);
    }

    // NOTE: this function is dangerous not only because it exposes a raw
    // pointer but also because the string may not be null terminated
    // TODO: consider removing this and providing range-like-ness in the
    // range_definition.h header
    [[nodiscard]] constexpr const char* data() const { return m_str; }

    [[nodiscard]] constexpr size_t size() const { return m_length; }
    [[nodiscard]] constexpr bool is_empty() const { return m_length == 0; }

    [[nodiscard]] constexpr size_t index_of_back() const
    {
        return is_empty() ? 0 : size() - 1;
    }

    [[nodiscard]] constexpr const char&
    operator[](size_t i) const OKAYLIB_NOEXCEPT
    {
        if (!ok::stdc::is_constant_evaluated()) {
            if (i >= m_length) [[unlikely]]
                __ok_abort("Out of bounds access to ok::ascii_view");
        }
        return m_str[i];
    }

    [[nodiscard]] constexpr ascii_view
    substring(size_t begin, size_t end = -1) const OKAYLIB_NOEXCEPT
    {
        if (end > size()) [[unlikely]]
            end = size();
        if (!ok::stdc::is_constant_evaluated()) {
            if (end <= begin) [[unlikely]]
                __ok_abort("Out of bounds access in ok::ascii_view::substring");
            __ok_internal_assert(begin < size());
        }
        return ascii_view(m_str + begin, m_str + end);
    }

    [[nodiscard]] constexpr friend bool operator==(const ascii_view& lhs,
                                                   const ascii_view& rhs)
    {
        const size_t lsize = lhs.size();
        const size_t rsize = rhs.size();
        if (lsize != rsize)
            return false;

        for (size_t i = 0; i < lsize; ++i)
            if (lhs[i] != rhs[i])
                return false;
        return true;
    }

    [[nodiscard]] constexpr bool startswith(const ascii_view& prefix) const
    {
        if (size() < prefix.size())
            return false;
        return substring(0, prefix.size()) == prefix;
    }

    /// If the given prefix is found at the start of this ascii_view, then it is
    /// removed entirely (a new ascii_view is returned which points at
    /// everything after the prefix). Otherwise, a copy of the same ascii_view
    /// is returned.
    [[nodiscard]] constexpr ascii_view
    remove_prefix(const ascii_view& prefix) const
    {
        return startswith(prefix) ? substring(prefix.size(), size()) : *this;
    }

    /// Returns a new ascii_view pointing to the same memory, minus any
    /// whitespace at the start of the string.
    [[nodiscard]] constexpr ascii_view trim_front() const
    {
        size_t first_nonwhitespace_idx = 0;

        while (first_nonwhitespace_idx < size() &&
               (m_str[first_nonwhitespace_idx] == ' ' ||
                m_str[first_nonwhitespace_idx] == '\t' ||
                m_str[first_nonwhitespace_idx] == '\n'))
            ++first_nonwhitespace_idx;

        return substring(first_nonwhitespace_idx, size());
    }

    [[nodiscard]] constexpr ascii_view trim_back() const
    {
        int64_t last_nonwhitespace_idx = m_length - 1;

        while (last_nonwhitespace_idx >= 0 &&
               (m_str[last_nonwhitespace_idx] == ' ' ||
                m_str[last_nonwhitespace_idx] == '\t' ||
                m_str[last_nonwhitespace_idx] == '\n'))
            --last_nonwhitespace_idx;

        __ok_internal_assert(last_nonwhitespace_idx >= -1);

        return substring(0, last_nonwhitespace_idx + 1);
    }

    [[nodiscard]] constexpr ascii_view trim() const
    {
        return trim_back().trim_front();
    }

    [[nodiscard]] constexpr size_t find_or(const ascii_view& needle,
                                           size_t alternative) const
    {
        if (needle.is_empty())
            return 0;

        size_t needle_index = 0;

        for (size_t i = 0; i < this->size(); ++i) {
            if (needle[needle_index] == (*this)[i]) {
                ++needle_index;
                if (needle_index == needle.size())
                    return (i + 1) - needle.size();
            } else {
                needle_index = 0;
            }
        }

        return alternative;
    }

    [[nodiscard]] constexpr size_t reverse_find_or(const ascii_view& needle,
                                                   size_t alternative) const
    {
        if (needle.is_empty())
            return index_of_back();

        size_t needle_index = needle.index_of_back();

        for (size_t i = index_of_back(); i < this->size(); --i) {
            if (needle[needle_index] == (*this)[i]) {
                --needle_index;
                if (needle_index >= needle.size()) // overflow happened (safe)
                    return i;
            } else {
                needle_index = needle.index_of_back();
            }
        }

        return alternative;
    }

  private:
    static inline constexpr const char* default_string = "";

    const char* m_str = default_string;
    size_t m_length = 0;
};
} // namespace ok

#endif
