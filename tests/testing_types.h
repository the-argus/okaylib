#pragma once
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
        if (index >= num_bytes)
            OK_ABORT();
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

class example_iterable_cstyle_child : public example_iterable_cstyle
{};

// implement iterable trait for example_iterable_cstyle
namespace ok {
template <> struct iterable_definition<example_iterable_cstyle>
{
    using value_type = typename example_iterable_cstyle::value_type;

    inline static constexpr size_t size(const example_iterable_cstyle& i)
    {
        return i.size();
    }

    inline static constexpr value_type& get_ref(example_iterable_cstyle& i,
                                                size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    inline static constexpr const value_type&
    get_ref(const example_iterable_cstyle& i, size_t c) OKAYLIB_NOEXCEPT
    {
        return i[c];
    }

    inline static constexpr size_t
    begin(const example_iterable_cstyle&) OKAYLIB_NOEXCEPT
    {
        return 0;
    }

    inline static constexpr size_t
    end(const example_iterable_cstyle& i) OKAYLIB_NOEXCEPT
    {
        return i.size();
    }
};
} // namespace ok
