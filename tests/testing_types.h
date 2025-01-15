#pragma once
#include "fmt/core.h"
#include "okay/detail/abort.h"
#include "okay/iterable/iterable.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

enum class StatusCodeA : uint8_t
{
    okay,
    result_released,
    whatever,
    oom,
    bad_access,
};

enum class StatusCodeB : uint8_t
{
    okay = 0,
    result_released,
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

class example_iterable_cstyle
{
  public:
    using value_type = uint8_t;
    // propagate our iterable definition to child  classes
    using inherited_iterable_type = example_iterable_cstyle;

    value_type& operator[](size_t index) OKAYLIB_NOEXCEPT
    {
        if (index >= num_bytes) {
            OK_ABORT();
        }
        return bytes[index];
    }

    const value_type& operator[](size_t index) const OKAYLIB_NOEXCEPT
    {
        return (*const_cast<example_iterable_cstyle*>(this))[index];
    }

    example_iterable_cstyle() OKAYLIB_NOEXCEPT
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

class example_iterable_bidirectional
{
  public:
    using value_type = uint8_t;
    using inherited_iterable_type = example_iterable_bidirectional;

    friend class ok::iterable_definition<example_iterable_bidirectional>;

    struct cursor_t
    {
        constexpr cursor_t() = default;

        friend class example_iterable_bidirectional;

        constexpr void operator++() OKAYLIB_NOEXCEPT { ++m_inner; }

        constexpr void operator--() OKAYLIB_NOEXCEPT
        {
            if (m_inner == 0)
                OK_ABORT();
            --m_inner;
        }

        constexpr size_t inner() const OKAYLIB_NOEXCEPT { return m_inner; }

      private:
        size_t m_inner = 0;
    };

    constexpr value_type& get(cursor_t index) OKAYLIB_NOEXCEPT
    {
        if (index.inner() >= num_bytes)
            OK_ABORT();
        return bytes[index.inner()];
    }

    constexpr const value_type& get(cursor_t index) const OKAYLIB_NOEXCEPT
    {
        return (*const_cast<example_iterable_bidirectional*>(this)).get(index);
    }

    example_iterable_bidirectional() OKAYLIB_NOEXCEPT
    {
        bytes = static_cast<uint8_t*>(malloc(100));
        std::memset(bytes, 0, 100);
        num_bytes = 100;
    }

  private:
    uint8_t* bytes;
    size_t num_bytes;
};

class example_iterable_cstyle_child : public example_iterable_cstyle
{};

// implement iterable trait for example_iterable_cstyle
namespace ok {
template <> struct iterable_definition<example_iterable_cstyle>
{
    using value_type = typename example_iterable_cstyle::value_type;

    static constexpr size_t
    size(const example_iterable_cstyle& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }

    static constexpr value_type& get_ref(example_iterable_cstyle& i,
                                         size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr const value_type& get_ref(const example_iterable_cstyle& i,
                                               size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    static constexpr size_t
    begin(const example_iterable_cstyle&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    static constexpr bool is_inbounds(const example_iterable_cstyle& i,
                                      size_t c) OKAYLIB_NOEXCEPT
    {
        return c < i.size();
    }
};

template <> struct iterable_definition<example_iterable_bidirectional>
{
    using value_type = typename example_iterable_bidirectional::value_type;

    constexpr static bool infinite = false;

    static constexpr value_type&
    get_ref(example_iterable_bidirectional& i,
            const example_iterable_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return i.get(c);
    }

    static constexpr const value_type&
    get_ref(const example_iterable_bidirectional& i,
            const example_iterable_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        return i.get(c);
    }

    static constexpr example_iterable_bidirectional::cursor_t
    begin(const example_iterable_bidirectional&) OKAYLIB_NOEXCEPT
    {
        return {};
    }

    static constexpr bool is_after_bounds(
        const example_iterable_bidirectional& i,
        const example_iterable_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        // no size() method exposed to iterator API, its just a finite range.
        // but we can internally figure out if an iterator is valid
        return c.inner() < i.num_bytes;
    }

    static constexpr bool is_before_bounds(
        const example_iterable_bidirectional& i,
        const example_iterable_bidirectional::cursor_t& c) OKAYLIB_NOEXCEPT
    {
        // unsigned type can never go below zero index
        return false;
    }
};
} // namespace ok
