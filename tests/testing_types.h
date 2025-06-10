#pragma once
#include "okay/detail/abort.h"
#include "okay/ranges/ranges.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

enum class StatusCodeA : uint8_t
{
    success,
    no_value,
    whatever,
    oom,
    bad_access,
};

enum class StatusCodeB : uint8_t
{
    success = 0,
    no_value,
    nothing = 250,
    more_nothing = 100,
};

struct trivial_t
{
    int whatever;
    const char* nothing;
};

struct moveable_t
{
    int whatever;
    char* nothing;

    moveable_t() noexcept { nothing = new char[150]; }
    ~moveable_t() noexcept { delete[] nothing; };

    // no copying
    moveable_t& operator=(const moveable_t& other) = delete;
    moveable_t(const moveable_t& other) = delete;

    // yay moving
    moveable_t& operator=(moveable_t&& other) noexcept
    {
        nothing = other.nothing;
        whatever = other.whatever;
        other.nothing = nullptr;
        other.whatever = 0;
        return *this;
    }
    moveable_t(moveable_t&& other) noexcept { *this = std::move(other); }
};

struct nonmoveable_t
{
    int whatever;
    const char* nothing;

    nonmoveable_t() { nothing = new char[150]; }
    ~nonmoveable_t() { delete[] nothing; };

    // trivially copyable
    nonmoveable_t& operator=(const nonmoveable_t& other) = default;
    nonmoveable_t(const nonmoveable_t& other) = default;

    // yay moving
    nonmoveable_t& operator=(nonmoveable_t&& other) = delete;
    nonmoveable_t(nonmoveable_t&& other) = delete;
};

class example_range_cstyle
{
  public:
    using value_type = uint8_t;
    // propagate our range definition to child  classes
    using inherited_range_type = example_range_cstyle;

    value_type& operator[](size_t index) OKAYLIB_NOEXCEPT
    {
        if (index >= num_bytes) {
            __ok_abort("Out of bounds access in example_range_cstyle");
        }
        return bytes[index];
    }

    const value_type& operator[](size_t index) const OKAYLIB_NOEXCEPT
    {
        return (*const_cast<example_range_cstyle*>(this))[index];
    }

    example_range_cstyle() OKAYLIB_NOEXCEPT
    {
        bytes = static_cast<uint8_t*>(malloc(100));
        std::memset(bytes, 0, 100);
        num_bytes = 100;
    }

    constexpr size_t size() const OKAYLIB_NOEXCEPT { return num_bytes; }

  private:
    uint8_t* bytes;
    size_t num_bytes;
};

class example_range_bidirectional
{
  public:
    using value_type = uint8_t;
    using inherited_range_type = example_range_bidirectional;

    friend class ok::range_definition<example_range_bidirectional>;

    struct cursor_t
    {
        constexpr cursor_t() = default;

        friend class example_range_bidirectional;

        constexpr cursor_t& operator++() OKAYLIB_NOEXCEPT
        {
            ++m_inner;
            return *this;
        }

        constexpr cursor_t& operator--() OKAYLIB_NOEXCEPT
        {
            if (m_inner == 0)
                __ok_abort("Integer overflow funny business in "
                           "example_range_bidirectional");
            --m_inner;
            return *this;
        }

        constexpr size_t inner() const OKAYLIB_NOEXCEPT { return m_inner; }

      private:
        size_t m_inner = 0;
    };

    constexpr value_type& get(cursor_t index) OKAYLIB_NOEXCEPT
    {
        if (index.inner() >= num_bytes)
            __ok_abort("Out of bounds access into bytes of "
                       "example_range_bidirectional");
        return bytes[index.inner()];
    }

    constexpr const value_type& get(cursor_t index) const OKAYLIB_NOEXCEPT
    {
        return (*const_cast<example_range_bidirectional*>(this)).get(index);
    }

    example_range_bidirectional() OKAYLIB_NOEXCEPT
    {
        bytes = static_cast<uint8_t*>(malloc(100));
        std::memset(bytes, 0, 100);
        num_bytes = 100;
    }

  private:
    uint8_t* bytes;
    size_t num_bytes;
};

class example_range_cstyle_child : public example_range_cstyle
{};

class fifty_items_unknown_size_t
{};

class fifty_items_unknown_size_no_pre_increment_t
{};

class fifty_items_bidir_no_pre_decrement_t
{};

// implement range trait for example_range_cstyle
namespace ok {
template <> struct range_definition<example_range_cstyle>
{
    using value_type = typename example_range_cstyle::value_type;

    static constexpr size_t size(const example_range_cstyle& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }

    static constexpr value_type& get_ref(example_range_cstyle& i,
                                         size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr const value_type& get_ref(const example_range_cstyle& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr size_t begin(const example_range_cstyle&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const example_range_cstyle& i,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < i.size();
    }
};

template <> struct range_definition<example_range_bidirectional>
{
    using value_type = typename example_range_bidirectional::value_type;

    constexpr static bool is_infinite = false;

    static constexpr value_type&
    get_ref(example_range_bidirectional& i,
            const example_range_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return i.get(c);
    }

    static constexpr const value_type&
    get_ref(const example_range_bidirectional& i,
            const example_range_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return i.get(c);
    }

    static constexpr example_range_bidirectional::cursor_t
    begin(const example_range_bidirectional&) OKAYLIB_NOEXCEPT
    {
        return {};
    }

    static constexpr bool
    is_inbounds(const example_range_bidirectional& i,
                const example_range_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return c.inner() < i.num_bytes;
    }
};

template <> struct range_definition<fifty_items_unknown_size_t>
{
    static constexpr bool is_infinite = false;

    static constexpr size_t
    begin(const fifty_items_unknown_size_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const fifty_items_unknown_size_t&,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < 50;
    }

    static constexpr size_t get(const fifty_items_unknown_size_t&,
                                size_t c) OKAYLIB_NOEXCEPT
    {
        return c + 1;
    }
};

template <> struct range_definition<fifty_items_unknown_size_no_pre_increment_t>
{
    using self_t = fifty_items_unknown_size_no_pre_increment_t;
    static constexpr bool is_infinite = false;

    struct cursor_t
    {
        constexpr cursor_t(size_t _inner) : inner(_inner) {}
        size_t inner;
    };

    static constexpr cursor_t begin(const self_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const self_t&,
                                      cursor_t c) OKAYLIB_NOEXCEPT
    {
        return c.inner < 50;
    }

    static constexpr size_t get(const self_t&, cursor_t c) OKAYLIB_NOEXCEPT
    {
        return c.inner + 1;
    }

    static constexpr void increment(const self_t&, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        ++c.inner;
    }
};

template <> struct range_definition<fifty_items_bidir_no_pre_decrement_t>
{
    using self_t = fifty_items_bidir_no_pre_decrement_t;
    static constexpr bool is_infinite = false;

    struct cursor_t
    {
        constexpr cursor_t(size_t _inner) : inner(_inner) {}
        size_t inner;
    };

    static constexpr cursor_t begin(const self_t&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const self_t&,
                                      cursor_t c) OKAYLIB_NOEXCEPT
    {
        return c.inner < 50;
    }

    static constexpr size_t get(const self_t&, cursor_t c) OKAYLIB_NOEXCEPT
    {
        return c.inner + 1;
    }

    static constexpr void increment(const self_t&, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        ++c.inner;
    }

    static constexpr void decrement(const self_t&, cursor_t& c) OKAYLIB_NOEXCEPT
    {
        --c.inner;
    }
};

} // namespace ok
