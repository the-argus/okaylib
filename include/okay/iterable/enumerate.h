#ifndef every_other_dummy
#define every_other_dummy

#include "okay/detail/ok_assert.h"
#include "okay/detail/view_common.h"
#include "okay/iterable/iterable.h"
#include "okay/iterable/ranges.h"

namespace ok {
namespace detail {
template <typename iterable_t> struct enumerated_view_t;

struct enumerate_fn_t
{
    template <typename iterable_t>
    constexpr decltype(auto)
    operator()(iterable_t&& iterable) const OKAYLIB_NOEXCEPT
    {
        static_assert(is_iterable_v<iterable_t>,
                      "Cannot enumerate given type- it is not iterable.");
        return enumerated_view_t<decltype(iterable)>{
            std::forward<iterable_t>(iterable)};
    }
};

// conditionally either a ref view wrapper or a owned view wrapper or just
// straight up inherits from the iterable_t if it's a view
template <typename iterable_t>
struct enumerated_view_t : public underlying_view_type<iterable_t>::type
{};

template <typename parent_iterable_t> struct enumerated_cursor_t
{
  private:
    using parent_cursor_t = cursor_type_for<parent_iterable_t>;

  public:
    explicit constexpr enumerated_cursor_t(parent_cursor_t&& c)
        : OKAYLIB_NOEXCEPT m_index(0), m_inner(std::move(c))
    {
    }

    // inherit copy-ability from inner type
    enumerated_cursor_t(const enumerated_cursor_t&) = default;
    enumerated_cursor_t& operator=(const enumerated_cursor_t&) = default;

    friend class iterable_definition<
        detail::enumerated_view_t<parent_iterable_t>>;

    constexpr void operator++()
    {
        ++m_index;
        ++m_inner;
    }

    // only equality comparable to self if wrapped type is equality
    // comparable to self
    template <typename T = enumerated_cursor_t>
    constexpr friend std::enable_if_t<std::is_same_v<enumerated_cursor_t, T> &&
                                          detail::is_equality_comparable_to_v<
                                              parent_cursor_t, parent_cursor_t>,
                                      bool>
    operator==(const enumerated_cursor_t& a, const enumerated_cursor_t& b)
    {
        return a.m_index == b.m_index && a.m_inner == b.m_inner;
    }

    constexpr const parent_cursor_t& inner() const OKAYLIB_NOEXCEPT
    {
        return m_inner;
    }

    explicit constexpr operator parent_cursor_t() const noexcept
    {
        return inner();
    }

    constexpr size_t index() const OKAYLIB_NOEXCEPT { return m_index; }

    template <typename T = parent_iterable_t>
    constexpr std::enable_if_t<
        std::is_same_v<T, parent_iterable_t> &&
        detail::is_bidirectional_iterable_v<parent_iterable_t>>
    operator--()
    {
        --this->m_index;
        --this->m_inner;
    }

#define __okaylib_enumerator_enable_relop_if_random_access \
    template <typename T = parent_iterable_t>              \
    constexpr friend std::enable_if_t<                     \
        std::is_same_v<T, parent_iterable_t> &&            \
            detail::is_random_access_iterable_v<T>,        \
        bool>

    __okaylib_enumerator_enable_relop_if_random_access
    operator<(const enumerated_cursor_t& lhs, const enumerated_cursor_t& rhs)
    {
        const bool out = lhs.m_inner < rhs.m_inner;
        __ok_assert(out == lhs.m_index < lhs.m_index);
        return out;
    }

    __okaylib_enumerator_enable_relop_if_random_access
    operator<=(const enumerated_cursor_t& lhs, const enumerated_cursor_t& rhs)
    {
        const bool out = lhs.m_inner <= rhs.m_inner;
        __ok_assert(out == lhs.m_index <= lhs.m_index);
        return out;
    }

    __okaylib_enumerator_enable_relop_if_random_access
    operator>(const enumerated_cursor_t& lhs, const enumerated_cursor_t& rhs)
    {
        const bool out = lhs.m_inner > rhs.m_inner;
        __ok_assert(out == lhs.m_index > lhs.m_index);
        return out;
    }

    __okaylib_enumerator_enable_relop_if_random_access
    operator>=(const enumerated_cursor_t& lhs, const enumerated_cursor_t& rhs)
    {
        const bool out = lhs.m_inner >= rhs.m_inner;
        __ok_assert(out == lhs.m_index >= lhs.m_index);
        return out;
    }

#undef __okaylib_enumerator_enable_relop_if_random_access

#define __okaylib_enumerator_enable_member_if_random_access(rettype)       \
    template <typename T = parent_iterable_t>                              \
    constexpr std::enable_if_t<std::is_same_v<T, parent_iterable_t> &&     \
                                   detail::is_random_access_iterable_v<T>, \
                               rettype>

    __okaylib_enumerator_enable_member_if_random_access(enumerated_cursor_t&)
    operator+=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_index += rhs;
        m_inner += rhs;
        return *this;
    }

    __okaylib_enumerator_enable_member_if_random_access(enumerated_cursor_t&)
    operator-=(const size_t rhs) OKAYLIB_NOEXCEPT
    {
        m_index -= rhs;
        m_inner -= rhs;
        return *this;
    }

    __okaylib_enumerator_enable_member_if_random_access(enumerated_cursor_t)
    operator+(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        return enumerated_cursor_t(m_index + rhs, m_inner + rhs);
    }

    __okaylib_enumerator_enable_member_if_random_access(enumerated_cursor_t)
    operator-(size_t rhs) const OKAYLIB_NOEXCEPT
    {
        __ok_assert(rhs >= m_index);
        return enumerated_cursor_t(m_index - rhs, m_inner - rhs);
    }

#undef __okaylib_enumerator_enable_member_if_random_access

  private:
    constexpr enumerated_cursor_t(size_t i, const parent_cursor_t& c)
        : m_index(i), m_inner(c) OKAYLIB_NOEXCEPT
    {
    }

    size_t m_index;
    parent_cursor_t m_inner;
};

} // namespace detail

template <typename iterable_t>
struct iterable_definition<detail::enumerated_view_t<iterable_t>>
    : public detail::propagate_sizedness_t<
          detail::enumerated_view_t<iterable_t>, iterable_t>,
      public detail::propagate_begin_t<detail::enumerated_view_t<iterable_t>,
                                       iterable_t,
                                       detail::enumerated_cursor_t<iterable_t>>,
      public detail::propagate_boundscheck_t<
          detail::enumerated_view_t<iterable_t>, iterable_t,
          detail::enumerated_cursor_t<iterable_t>>
{
    static constexpr bool is_view = true;

    using enumerated_t = detail::enumerated_view_t<iterable_t>;
    using cursor_t = detail::enumerated_cursor_t<iterable_t>;

    constexpr static auto get(const enumerated_t& i,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::iterable_definition_inner<iterable_t>;
        if constexpr (detail::iterable_has_get_ref_v<iterable_t>) {
            // if get_ref nonconst exists, do evil const cast to get to it
            using reftype = decltype(inner_def::get_ref(
                const_cast<enumerated_t&>(i)
                    .template get_view_reference<enumerated_t, iterable_t>(),
                c.inner()));
            return std::pair<reftype, const size_t>(
                inner_def::get_ref(
                    const_cast<enumerated_t&>(i)
                        .template get_view_reference<enumerated_t,
                                                     iterable_t>(),
                    c.inner()),
                c.index());
        } else if constexpr (detail::iterable_has_get_ref_const_v<iterable_t>) {
            using reftype = decltype(inner_def::get_ref(
                i.template get_view_reference<enumerated_t, iterable_t>(),
                c.inner()));
            return std::pair<reftype, const size_t>(
                inner_def::get_ref(
                    i.template get_view_reference<enumerated_t, iterable_t>(),
                    c.inner()),
                c.index());
        } else {
            using gettype = decltype(inner_def::get(
                i.template get_view_reference<enumerated_t, iterable_t>(),
                c.inner()));
            return std::pair<gettype, const size_t>(
                inner_def::get(
                    i.template get_view_reference<enumerated_t, iterable_t>(),
                    c.inner()),
                c.index());
        }
    }

    // satisfy requirement that get() returns value_type
    // TODO: value_type still needed?
    using value_type = decltype(get(std::declval<const enumerated_t&>(),
                                    std::declval<const cursor_t&>()));
};

constexpr detail::range_adaptor_closure_t<detail::enumerate_fn_t> enumerate;

} // namespace ok

#endif
