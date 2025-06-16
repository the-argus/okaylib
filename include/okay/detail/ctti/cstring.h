#ifndef __OKAYLIB_DETAIL_CTTI_CSTRING_H__
#define __OKAYLIB_DETAIL_CTTI_CSTRING_H__

#include <cstddef>
#include <cstdint>

namespace ok::ctti::detail {
// From https://github.com/foonathan/string_id. As usually, thanks Jonathan.
using hash_t = uint64_t;

// See http://www.isthe.com/chongo/tech/comp/fnv/#FNV-param
constexpr hash_t fnv_basis = 14695981039346656037ull;
constexpr hash_t fnv_prime = 1099511628211ull;

// FNV-1a 64 bit hash
constexpr hash_t fnv1a_hash(size_t n, const char* str, hash_t hash = fnv_basis)
{
    return n > 0 ? fnv1a_hash(n - 1, str + 1, (hash ^ *str) * fnv_prime) : hash;
}

template <size_t N> constexpr hash_t fnv1a_hash(const char (&array)[N])
{
    return fnv1a_hash(N - 1, &array[0]);
}

// basically simpler version of slice<char> which includes .hash() and .pad()
class cstring
{
  public:
    template <size_t N>
    constexpr cstring(const char (&str)[N]) : m_str(str), m_length(N)
    {
    }

    constexpr cstring(const char* begin, const char* end)
        : m_str(begin), m_length(static_cast<size_t>(end - begin))
    {
    }

    [[nodiscard]] constexpr hash_t hash() const
    {
        return fnv1a_hash(size(), m_str);
    }

    [[nodiscard]] constexpr const char* data() const { return m_str; }

    [[nodiscard]] constexpr size_t size() const { return m_length; }

    [[nodiscard]] constexpr const char& operator[](size_t i) const
    {
        return m_str[i];
    }

    [[nodiscard]] constexpr cstring substring(size_t begin, size_t end) const
    {
        return cstring(m_str + begin, m_str + end);
    }

    [[nodiscard]] constexpr cstring pad(size_t begin_offset,
                                        size_t end_offset) const
    {
        return substring(begin_offset, size() - end_offset);
    }

  private:
    const char* m_str;
    size_t m_length;
};

constexpr bool operator==(const cstring& lhs, const cstring& rhs)
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

constexpr cstring filter_prefix(const cstring& str, const cstring& prefix)
{
    return str.size() >= prefix.size()
               ? (str.substring(0, prefix.size()) == prefix
                      ? str.substring(prefix.size(), str.size())
                      : str)
               : str;
}

constexpr cstring leftpad(const cstring& str)
{
    size_t first_nonspace_idx = 0;

    while (first_nonspace_idx < str.size() && str[first_nonspace_idx] == ' ')
        ++first_nonspace_idx;

    return str.substring(first_nonspace_idx, str.size());
}

constexpr cstring filter_class(const cstring& type_name)
{
    return leftpad(filter_prefix(leftpad(type_name), "class"));
}

constexpr cstring filter_struct(const cstring& type_name)
{
    return leftpad(filter_prefix(leftpad(type_name), "struct"));
}

constexpr cstring filter_typename_prefix(const cstring& type_name)
{
    return filter_struct(filter_class(type_name));
}

} // namespace ok::ctti::detail
#endif
