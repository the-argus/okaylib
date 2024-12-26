#ifndef __OKAYLIB_DETAIL_TEMPLATE_UTIL_ENABLE_COPY_MOVE_H__
#define __OKAYLIB_DETAIL_TEMPLATE_UTIL_ENABLE_COPY_MOVE_H__

namespace ok::detail {

/// Template which disables copy, copy assignment, move, and move assignment
/// based on a set of booleans. used by inheriting from it
template <bool copy, bool copy_assignmnet, bool move, bool move_assignment,
          typename tag_t = void>
struct enable_copy_move
{};

template <typename tag_t>
struct enable_copy_move<false, true, true, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<true, false, true, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<false, false, true, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<true, true, false, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<false, true, false, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<true, false, false, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<false, false, false, true, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = default;
};

template <typename tag_t>
struct enable_copy_move<true, true, true, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<false, true, true, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<true, false, true, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<false, false, true, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<true, true, false, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<false, true, false, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = default;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<true, false, false, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = default;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};

template <typename tag_t>
struct enable_copy_move<false, false, false, false, tag_t>
{
    constexpr enable_copy_move() noexcept = default;
    constexpr enable_copy_move(enable_copy_move const&) noexcept = delete;
    constexpr enable_copy_move(enable_copy_move&&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move const&) noexcept = delete;
    enable_copy_move& operator=(enable_copy_move&&) noexcept = delete;
};
} // namespace ok::detail

#endif
