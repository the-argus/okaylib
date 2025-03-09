#ifndef __OKAYLIB_RES_H__
#define __OKAYLIB_RES_H__

#include "okay/detail/abort.h"
#include "okay/detail/noexcept.h"
#include "okay/detail/res.h"
#include "okay/detail/template_util/enable_copy_move.h"
#include "okay/detail/traits/is_nonthrowing.h"
#include "okay/detail/traits/is_status_enum.h"
#include "okay/opt.h"
#include "okay/slice.h"

#ifdef OKAYLIB_USE_FMT
#include <fmt/core.h>
#endif

namespace ok {
namespace detail {
template <typename T> struct make_inner_fn_t;
template <typename T, typename E> struct res_accessor_t;
} // namespace detail

namespace detail {
struct uninitialized_result_tag
{};
template <typename T, typename E>
using res_enable_copy_move_for_type_t = detail::enable_copy_move<
    std::is_copy_constructible_v<T>,
    std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
    std::is_move_constructible_v<T>,
    std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
    res<T, E>>; // unique tag

template <typename T> struct make_inner_fn_t;

} // namespace detail

template <typename T, typename E> struct res_internals_modifier_t;

template <typename contained_t, typename enum_t>
class res<contained_t, enum_t,
          std::enable_if_t<
              !detail::is_instance_v<std::remove_cv_t<contained_t>, slice>>>
    : private detail::res_base_t<contained_t, std::underlying_type_t<enum_t>>,
      private detail::res_enable_copy_move_for_type_t<contained_t, enum_t>
{
    using enum_int_t = std::underlying_type_t<enum_t>;

    inline static constexpr bool is_reference =
        std::is_lvalue_reference_v<contained_t>;

    constexpr res(detail::uninitialized_result_tag) OKAYLIB_NOEXCEPT {}

  public:
    using type = contained_t;
    using enum_type = enum_t;

    static_assert(
        std::is_enum_v<enum_t>,
        "The second type parameter to res must be a statuscode enum.");
    static_assert(std::is_same_v<std::decay_t<enum_t>, enum_t>,
                  "Do not cv or ref qualify statuscode type.");
    static_assert(!std::is_rvalue_reference_v<contained_t>,
                  "opt cannot store rvalue references");
    static_assert(is_reference || (std::is_object_v<contained_t> &&
                                   !std::is_array_v<contained_t>),
                  "Results can only store objects, and not c-style arrays.");
    static_assert(is_reference || detail::is_nonthrowing<contained_t>,
                  OKAYLIB_IS_NONTHROWING_ERRMSG);
    static_assert(
        !std::is_same_v<std::remove_cv_t<contained_t>,
                        std::remove_cv_t<enum_t>>,
        "Result cannot store an the same enum as payload as for statuscode.");
    static_assert(detail::is_status_enum_v<enum_t>,
                  OKAYLIB_IS_STATUS_ENUM_ERRMSG);
    static_assert(!is_std_constructible_v<contained_t, const enum_t&>,
                  "Cannot store type in res if it is constructible from its "
                  "own status code- this makes what assigning the statuscode "
                  "to the res will do ambiguous.");

    [[nodiscard]] constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return this->okay_payload();
    }

    // cheaply convert to opt if reference type
    template <typename T = contained_t>
    [[nodiscard]] constexpr std::enable_if_t<
        std::is_same_v<T, contained_t> && is_reference, opt<contained_t>>
    to_opt() const OKAYLIB_NOEXCEPT
    {
        if (this->okay_payload()) {
            return *this->pointer;
        } else {
            return nullopt;
        }
    }

    [[nodiscard]] constexpr enum_t err() const OKAYLIB_NOEXCEPT
    {
        return enum_t(this->get_error_payload());
    }

    // if contained type is a reference, then this just returns the reference.
    // otherwise it will perform a move.
    [[nodiscard]] constexpr std::conditional_t<is_reference, contained_t,
                                               contained_t&&>
    release() OKAYLIB_NOEXCEPT
    {
        if (!okay()) [[unlikely]] {
            __ok_abort("Attempt to release actual value from error result");
        }

        this->get_error_payload() = enum_int_t(enum_t::no_value);
        if constexpr (is_reference) {
            return *this->pointer;
        } else {
            return std::move(this->get_value_unchecked_payload());
        }
    }

    // if inner type is a value type, you can release an lvalue reference to the
    // contents of the result and perform operationgs in-place.
    template <typename maybe_t = contained_t>
        [[nodiscard]] constexpr std::enable_if_t<
            !is_reference && std::is_same_v<maybe_t, contained_t>, maybe_t&>
        release_ref() & OKAYLIB_NOEXCEPT
    {
        if (!okay()) [[unlikely]] {
            __ok_abort("Attempt to release_ref actual value from error result");
        }
        this->get_error_payload() = enum_int_t(enum_t::no_value);
        return this->get_value_unchecked_payload();
    }

    res(const res& other) = delete;
    res& operator=(const res& other) = delete;

    // can move construct (to allow returning from functions without RVO)
    res& operator=(res&& other) = default;

    // move construction is possible but not move assignment
    template <typename T = res,
              std::enable_if_t<std::is_same_v<T, res> &&
                                   (detail::is_moveable_v<contained_t> ||
                                    is_reference),
                               bool> = true>
    constexpr res(T&& other) OKAYLIB_NOEXCEPT
    {
        if (other.okay()) {
            if constexpr (is_reference) {
                this->construct_no_destroy_payload(
                    other.get_value_unchecked_payload());
            } else {
                this->construct_no_destroy_payload(
                    std::move(other.get_value_unchecked_payload()));
            }
        }
        this->get_error_payload() = std::exchange(other.get_error_payload(), 1);
    };

    template <typename T = contained_t>
    constexpr res(
        std::enable_if_t<!is_reference && std::is_default_constructible_v<T>,
                         std::in_place_t>) OKAYLIB_NOEXCEPT
    {
        this->get_error_payload() = 0;
        this->construct_no_destroy_payload();
    }

    template <typename... args_t,
              std::enable_if_t<
                  !is_reference && sizeof...(args_t) != 0 &&
                      is_infallible_constructible_v<contained_t, args_t...>,
                  bool> = true>
    constexpr res(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        this->get_error_payload() = 0;
        if constexpr (is_std_constructible_v<contained_t, args_t...>) {
            this->construct_no_destroy_payload(std::forward<args_t>(args)...);
        } else {
            this->get_value_unchecked_payload() =
                contained_t::construct(std::forward<args_t>(args)...);
        }
    }

    // reference from-reference constructor
    template <
        typename maybe_t = contained_t,
        std::enable_if_t<is_reference && std::is_same_v<maybe_t, contained_t>,
                         bool> = true>
    constexpr res(contained_t success) OKAYLIB_NOEXCEPT
    {
        this->get_error_payload() = 0;
        this->construct_no_destroy_payload(success);
    }

    /// A statuscode can also be implicitly converted to a result
    constexpr res(enum_t failure) OKAYLIB_NOEXCEPT
    {
        if (failure == enum_t::okay) [[unlikely]] {
            __ok_abort("Attempt to construct a result with an okay value");
        }

        this->get_error_payload() = enum_int_t(failure);
    }

    constexpr explicit res() OKAYLIB_NOEXCEPT
    {
        this->get_error_payload() = enum_int_t(enum_t::no_value);
    }

    friend struct ok::detail::res_accessor_t<contained_t, enum_t>;

#ifdef OKAYLIB_USE_FMT
    friend struct fmt::formatter<res>;
#endif
};

template <typename contained_t, typename enum_t>
class res<contained_t, enum_t,
          std::enable_if_t<
              detail::is_instance_v<std::remove_cv_t<contained_t>, slice>>>
{
    size_t m_elements; // can also encode enum value
    void* m_data;

    using unqualified_t = std::remove_cv_t<contained_t>;
    using enum_int_t = std::underlying_type_t<enum_t>;

  public:
    using type = contained_t;
    using enum_type = enum_t;

    static_assert(!is_std_constructible_v<contained_t, const enum_t&>);
    static_assert(
        std::is_enum_v<enum_t>,
        "The second type parameter to res must be a statuscode enum.");
    static_assert(std::is_same_v<std::decay_t<enum_t>, enum_t>,
                  "Do not cv or ref qualify statuscode type.");
    static_assert(
        !std::is_same_v<std::remove_cv_t<contained_t>,
                        std::remove_cv_t<enum_t>>,
        "Result cannot store an the same enum as payload as for statuscode.");
    static_assert(detail::is_status_enum_v<enum_t>,
                  OKAYLIB_IS_STATUS_ENUM_ERRMSG);

    // allow only move construction
    constexpr res(res&& other) = default;
    constexpr res(const res& other) = delete;
    constexpr res& operator=(res&& other) = delete;
    constexpr res& operator=(const res& other) = delete;

    // NOTE: not defining std::in_place constructor here because slice cannot
    // default construct
    template <
        typename... args_t,
        std::enable_if_t<sizeof...(args_t) != 0 &&
                             is_std_constructible_v<contained_t, args_t...>,
                         bool> = true>
    constexpr res(args_t&&... args) OKAYLIB_NOEXCEPT
    {
        new (reinterpret_cast<unqualified_t*>(this))
            unqualified_t(std::forward<args_t>(args)...);
    }

    constexpr res(enum_t failure) OKAYLIB_NOEXCEPT
    {
        if (failure == enum_t::okay) [[unlikely]] {
            __ok_abort("Attempt to construct a result with an okay value.");
        }

        m_data = nullptr;
        m_elements = enum_int_t(failure);
    }

    constexpr explicit res() OKAYLIB_NOEXCEPT
        : m_data(nullptr),
          m_elements(enum_int_t(enum_t::no_value))
    {
    }

    [[nodiscard]] constexpr bool okay() const OKAYLIB_NOEXCEPT
    {
        return m_data != nullptr;
    }

    [[nodiscard]] constexpr enum_t err() const OKAYLIB_NOEXCEPT
    {
        __ok_internal_assert(
            okay() || m_elements <= std::numeric_limits<enum_int_t>::max());
        return okay() ? enum_t::okay : enum_t(enum_int_t(m_elements));
    }

    // perform copy of slice as optional slice
    [[nodiscard]] constexpr opt<contained_t> to_opt() const OKAYLIB_NOEXCEPT
    {
        // opt<slice>, slice, and res<slice> should all have the same layout
        static_assert(sizeof(opt<contained_t>) == sizeof(*this));
        static_assert(sizeof(contained_t) == sizeof(*this));
        // we have the same layout and abi as opt, no need to cast
        return *reinterpret_cast<const opt<contained_t>*>(this);
    }

    // copy inner slice out
    [[nodiscard]] constexpr unqualified_t release() OKAYLIB_NOEXCEPT
    {
        if (!okay()) [[unlikely]] {
            __ok_abort("Attempt to get an actual value from an error result.");
        }

        unqualified_t out = *reinterpret_cast<unqualified_t*>(this);
        m_elements = enum_int_t(enum_t::no_value);
        m_data = nullptr;
        return out;
    }

    [[nodiscard]] constexpr contained_t& release_ref() & OKAYLIB_NOEXCEPT
    {
        if (!okay()) [[unlikely]] {
            __ok_abort("Attempt to get an actual value from an error result.");
        }

        // NOTE: release_ref for slice is special: because the error is stored
        // in the type itself, we do not mark it no_value because that
        // would mean overwriting the data. so you can call release_ref on a
        // res<slice<>> as many times as you want.
        return *reinterpret_cast<unqualified_t*>(this);
    }
};

} // namespace ok

#ifdef OKAYLIB_USE_FMT
template <typename payload_t, typename enum_t>
struct fmt::formatter<ok::res<payload_t, enum_t>>
{
    using res_template_t = ok::res<payload_t, enum_t>;

    static_assert(
        res_template_t::is_reference || fmt::is_formattable<payload_t>::value,
        "Attempt to format an ok::res whose contents are not formattable.");
    constexpr format_parse_context::iterator parse(format_parse_context& ctx)
    {
        auto it = ctx.begin();
        // first character should just be closing brackets since we dont allow
        // anything else
        if (it != ctx.end() && *it != '}')
            throw_format_error("invalid format");
        return it;
    }

    format_context::iterator format(const res_template_t& result,
                                    format_context& ctx) const
    {
        if (result.okay()) {
            if constexpr (res_template_t::is_reference) {
                if constexpr (fmt::is_formattable<
                                  std::remove_reference_t<payload_t>>::value) {
                    return fmt::format_to(ctx.out(), "{}", *result.pointer);
                } else {
                    // TODO: use ctti to get nice typeof/typename here?
                    return fmt::format_to(ctx.out(), "{:p}",
                                          static_cast<void*>(result.pointer));
                }
            } else {
                return fmt::format_to(ctx.out(), "{}",
                                      result.get_value_unchecked_payload());
            }
        } else {
            if constexpr (fmt::is_formattable<enum_t>::value) {
                return fmt::format_to(ctx.out(), "{}",
                                      result.get_error_payload());
            } else {
                using enum_int_t = typename res_template_t::enum_int_t;
                return fmt::format_to(ctx.out(), "[res::statuscode::{}]",
                                      enum_int_t(result.get_error_payload()));
            }
        }
    }
};
#endif

#endif
