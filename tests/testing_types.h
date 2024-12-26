#pragma once
#include <cstdint>
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
