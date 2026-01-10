#pragma once
#include "okay/containers/array.h"
#include "okay/detail/abort.h"
#include "okay/iterables/iterables.h"
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

    // no moving
    nonmoveable_t& operator=(nonmoveable_t&& other) = delete;
    nonmoveable_t(nonmoveable_t&& other) = delete;
};

class example_range_cstyle
{
  public:
    using value_type = uint8_t;

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

class example_iterable_forward_and_array
{
  public:
    using value_type = uint8_t;

    template <bool is_const> struct cursor_t
    {
        template <typename T>
        using maybe_const_t = ok::stdc::conditional_t<is_const, const T, T>;

        using value_type = maybe_const_t<uint8_t>&;

        size_t index = 0;

        constexpr ok::opt<value_type>
        next(maybe_const_t<example_iterable_forward_and_array>& iterable)
        {
            if (index >= iterable.size())
                return {};
            ok::opt<value_type> out(iterable[index]);
            ++index;
            return out;
        }
    };

    constexpr value_type& operator[](size_t index) OKAYLIB_NOEXCEPT
    {
        if (index >= num_bytes)
            __ok_abort("Out of bounds access into bytes of "
                       "example_iterable_forward");
        return bytes[index];
    }

    constexpr const value_type& operator[](size_t index) const OKAYLIB_NOEXCEPT
    {
        return (*const_cast<example_iterable_forward_and_array*>(this))[index];
    }

    constexpr auto iter() const&
    {
        return ok::ref_iterator_t<const example_iterable_forward_and_array,
                                  cursor_t<true>>{*this, {}};
    }

    constexpr auto iter() &
    {
        return ok::ref_iterator_t<example_iterable_forward_and_array,
                                  cursor_t<false>>{*this, {}};
    }

    constexpr auto iter() &&
    {
        return ok::owning_iterator_t<example_iterable_forward_and_array,
                                     cursor_t<false>>{ok::stdc::move(*this),
                                                      {}};
    }

    constexpr size_t size() const noexcept { return 100; }

    constexpr example_iterable_forward_and_array() OKAYLIB_NOEXCEPT
    {
        bytes = static_cast<uint8_t*>(malloc(size()));
        std::memset(bytes, 0, 100);
        num_bytes = 100;
    }

    constexpr ~example_iterable_forward_and_array() { free(bytes); }

    example_iterable_forward_and_array(
        const example_iterable_forward_and_array&) = delete;
    example_iterable_forward_and_array&
    operator=(const example_iterable_forward_and_array&) = delete;

    constexpr example_iterable_forward_and_array(
        example_iterable_forward_and_array&& other)
        : bytes(ok::stdc::exchange(other.bytes, nullptr)),
          num_bytes(other.num_bytes)
    {
    }

    constexpr example_iterable_forward_and_array&
    operator=(example_iterable_forward_and_array&& other)
    {
        free(bytes);
        bytes = ok::stdc::exchange(other.bytes, nullptr);
        num_bytes = other.num_bytes;
        return *this;
    }

  private:
    uint8_t* bytes;
    size_t num_bytes;
};

enum class size_mode
{
    known_sized,
    unknown_sized,
};

template <size_mode mode> class forward_iterable_size_test_t
{
    struct cursor_t
    {
        size_t idx;

        using value_type = size_t;

        [[nodiscard]] constexpr size_t
        size(const forward_iterable_size_test_t&) const
            requires(mode == size_mode::known_sized)
        {
            return 50;
        }

        ok::opt<value_type> next(const forward_iterable_size_test_t&)
        {
            if (idx >= 50)
                return {};
            ok::opt<value_type> out = idx;
            ++idx;
            return out;
        }
    };

  public:
    [[nodiscard]] auto iter() const&
    {
        return ok::ref_iterator_t{*this, cursor_t{}};
    }

    [[nodiscard]] auto iter() &
    {
        return ok::ref_iterator_t{*this, cursor_t{}};
    }

    [[nodiscard]] auto iter() &&
    {
        return ok::owning_iterator_t<forward_iterable_size_test_t, cursor_t>{
            ok::stdc::move(*this), {}};
    }
};

struct special_member_counters_t
{
    size_t copy_constructs;
    size_t move_constructs;
    size_t default_constructs;
    size_t destructs;
    size_t copy_assigns;
    size_t move_assigns;

    bool operator==(const special_member_counters_t&) const = default;
};

struct counter_type
{
    static special_member_counters_t counters;

    inline static void reset_counters() { counters = {}; }

    inline counter_type() { counters.default_constructs += 1; }
    inline counter_type(const counter_type&) { counters.copy_constructs += 1; }
    inline counter_type(counter_type&&) { counters.move_constructs += 1; }
    inline ~counter_type() { counters.destructs += 1; }
    inline counter_type& operator=(const counter_type&)
    {
        counters.copy_assigns += 1;
        return *this;
    }
    inline counter_type& operator=(counter_type&&)
    {
        counters.move_assigns += 1;
        return *this;
    }
};

inline special_member_counters_t counter_type::counters{};

/*
 * Iterable for testing that the correct cvref types are returned for something
 * which can iterate by reference (ie. it actually contains the values and is
 * not a generator)
 */
struct forward_iterable_reftype_test_t
{
    template <typename value_type_t, bool forward = true> struct cursor_t
    {
        using value_type = value_type_t&;

        size_t index;

        cursor_t() = delete;

        constexpr cursor_t(const forward_iterable_reftype_test_t&)
            requires forward
            : index(0)
        {
        }

        constexpr operator size_t() const { return index; }

        constexpr cursor_t(const forward_iterable_reftype_test_t& iterable)
            requires(!forward)
            : index(iterable.size())
        {
        }

        using iterable_arg_t =
            ok::stdc::conditional_t<ok::is_const_c<value_type_t>,
                                    const forward_iterable_reftype_test_t&,
                                    forward_iterable_reftype_test_t&>;

        constexpr ok::opt<value_type> next(iterable_arg_t iterable)
        {
            ok::opt<value_type> out;

            if constexpr (forward) {
                if (index >= iterable.size())
                    return out;
                out.emplace(iterable.items[index]);
                ++index;
            } else {
                if (index == 0)
                    return out;
                out.emplace(iterable.items[index - 1]);
                --index;
            }
            return out;
        }
    };

    constexpr auto iter() const&
    {
        return ok::ref_iterator_t{*this, cursor_t<const int>(*this)};
    }
    constexpr auto iter() &
    {
        return ok::ref_iterator_t{*this, cursor_t<int>(*this)};
    }
    constexpr auto iter() &&
    {
        return ok::owning_iterator_t<forward_iterable_reftype_test_t,
                                     cursor_t<int>>{
            std::move(*this),
            cursor_t<int>(*this),
        };
    }

    constexpr auto reverse_iter() const&
    {
        return ok::ref_iterator_t{*this, cursor_t<const int, false>(*this)};
    }

    constexpr auto reverse_iter() &
    {
        return ok::ref_iterator_t{*this, cursor_t<int, false>(*this)};
    }

    constexpr auto reverse_iter() &&
    {
        return ok::owning_iterator_t<forward_iterable_reftype_test_t,
                                     cursor_t<int, false>>{
            std::move(*this),
            cursor_t<int, false>(*this),
        };
    }

    [[nodiscard]] constexpr size_t size() const { return 10; }

    constexpr static ok::maybe_undefined_array_t expected{
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    int items[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
};

struct arraylike_iterable_reftype_test_t
{
    template <typename iterable_t> struct cursor_t
    {
        using value_type =
            decltype(ok::stdc::declval<iterable_t&>().items[size_t{}]);

        size_t m_index = 0;

        cursor_t() = default;

        constexpr auto& access(iterable_t& iterable) const
        {
            __ok_assert(m_index < iterable.size(),
                        "out of bounds access to arraylike iterable");
            return iterable.items[m_index];
        }

        constexpr size_t
        size(const arraylike_iterable_reftype_test_t& iterable) const
        {
            return iterable.size();
        }

        constexpr size_t index(const arraylike_iterable_reftype_test_t&) const
        {
            return m_index;
        }

        constexpr void offset(const arraylike_iterable_reftype_test_t& iterable,
                              int64_t offset)
        {
            m_index += offset;
        }
    };

    constexpr auto iter() &
    {
        return ok::ref_arraylike_iterator_t{
            *this, cursor_t<arraylike_iterable_reftype_test_t>{}};
    }

    constexpr auto iter() const&
    {
        return ok::ref_arraylike_iterator_t{
            *this, cursor_t<const arraylike_iterable_reftype_test_t>{}};
    }

    constexpr auto iter() &&
    {
        return ok::owning_arraylike_iterator_t<
            arraylike_iterable_reftype_test_t,
            cursor_t<arraylike_iterable_reftype_test_t>>{
            ok::stdc::move(*this),
            {},
        };
    }

    [[nodiscard]] constexpr size_t size() const { return 10; }

    int items[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
};
