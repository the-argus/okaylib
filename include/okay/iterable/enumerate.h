#ifndef every_other_dummy
#define every_other_dummy

#include "okay/detail/ok_assert.h"
#include "okay/iterable/iterable.h"

namespace ok {

namespace detail {
template <template <typename> typename enumerated_iterable_t, typename inner_t>
struct sized_iterable_t;
template <template <typename> typename enumerated_t, typename iterable_t>
struct enumerated_iterable_definition_base_t;

template <typename iterable_t> struct enumerated_ref_t
{
    constexpr explicit enumerated_ref_t(iterable_t& iterable)
        : m_iterable(std::forward<iterable_t&>(iterable)) {};

    friend class enumerated_iterable_definition_base_t<enumerated_ref_t,
                                                       iterable_t>;
    friend class ok::detail::sized_iterable_t<enumerated_ref_t, iterable_t>;

  private:
    iterable_t& iterable() OKAYLIB_NOEXCEPT { return m_iterable; }
    const iterable_t& iterable() const OKAYLIB_NOEXCEPT { return m_iterable; }

    // breaking const correctness specifically in the case of enumerate
    // get_ref() where you need nonconst reference to internal iterable in order
    // to store a value_type& in the pair
    iterable_t& iterable_nonconst() const OKAYLIB_NOEXCEPT
    {
        return m_iterable;
    }

    iterable_t& m_iterable;
};

template <typename iterable_t> struct enumerated_owning_t
{
    constexpr explicit enumerated_owning_t(iterable_t&& iterable)
        : m_iterable(std::forward<iterable_t&&>(iterable)) {};

    friend class enumerated_iterable_definition_base_t<enumerated_owning_t,
                                                       iterable_t>;
    friend class ok::detail::sized_iterable_t<enumerated_owning_t, iterable_t>;

  private:
    iterable_t& iterable() OKAYLIB_NOEXCEPT { return m_iterable; }
    const iterable_t& iterable() const OKAYLIB_NOEXCEPT { return m_iterable; }
    iterable_t& iterable_nonconst() const OKAYLIB_NOEXCEPT
    {
        return m_iterable;
    }

    iterable_t&& m_iterable;
};

struct infinite_iterable_t
{
    static constexpr bool infinite = true;
};

struct finite_iterable_t
{
    static constexpr bool infinite = false;
};

template <template <typename> typename enumerated_iterable_t, typename inner_t>
struct sized_iterable_t
{
    template <typename T = inner_t>
    static constexpr std::enable_if_t<detail::iterable_has_size_v<T>, size_t>
    size(const enumerated_iterable_t<inner_t>& i) OKAYLIB_NOEXCEPT
    {
        return i.iterable().size();
    }
};

template <template <typename> typename enumerated_t, typename iterable_t>
struct enumerated_iterable_definition_base_t
{
    struct cursor_t
    {
      private:
        constexpr cursor_t(cursor_type_for<iterable_t>&& c)
            : m_index(0), m_inner(std::move(c))
        {
        }

      public:
        // inherit copy-ability from inner type
        cursor_t(const cursor_t&) = default;
        cursor_t& operator=(const cursor_t&) = default;

        friend class enumerated_iterable_definition_base_t;

        constexpr void operator++()
        {
            ++m_index;
            ++m_inner;
        }

        // only equality comparable to self if wrapped type is equality
        // comparable to self
        template <typename T = cursor_t>
        constexpr friend std::enable_if_t<
            std::is_same_v<cursor_t, T> &&
                detail::is_equality_comparable_to_v<
                    cursor_type_for<iterable_t>, cursor_type_for<iterable_t>>,
            bool>
        operator==(const cursor_t& a, const cursor_t& b)
        {
            return a.m_index == b.m_index && a.m_inner == b.m_inner;
        }

        constexpr const cursor_type_for<iterable_t>&
        inner() const OKAYLIB_NOEXCEPT
        {
            return m_inner;
        }
        constexpr size_t index() const OKAYLIB_NOEXCEPT { return m_index; }

        template <typename T = iterable_t>
        constexpr std::enable_if_t<
            std::is_same_v<T, iterable_t> &&
            detail::is_bidirectional_iterable_v<iterable_t>>
        operator--()
        {
            --this->m_index;
            --this->m_inner;
        }

#define __okaylib_enumerator_enable_relop_if_random_access   \
    template <typename T = iterable_t>                       \
    constexpr friend std::enable_if_t<                       \
        std::is_same_v<T, iterable_t> &&                     \
            detail::is_random_access_iterable_v<iterable_t>, \
        bool>

        __okaylib_enumerator_enable_relop_if_random_access
        operator<(const cursor_t& lhs, const cursor_t& rhs)
        {
            const bool out = lhs.m_inner < rhs.m_inner;
            __ok_assert(out == lhs.m_index < lhs.m_index);
            return out;
        }

        __okaylib_enumerator_enable_relop_if_random_access
        operator<=(const cursor_t& lhs, const cursor_t& rhs)
        {
            const bool out = lhs.m_inner <= rhs.m_inner;
            __ok_assert(out == lhs.m_index <= lhs.m_index);
            return out;
        }

        __okaylib_enumerator_enable_relop_if_random_access
        operator>(const cursor_t& lhs, const cursor_t& rhs)
        {
            const bool out = lhs.m_inner > rhs.m_inner;
            __ok_assert(out == lhs.m_index > lhs.m_index);
            return out;
        }

        __okaylib_enumerator_enable_relop_if_random_access
        operator>=(const cursor_t& lhs, const cursor_t& rhs)
        {
            const bool out = lhs.m_inner >= rhs.m_inner;
            __ok_assert(out == lhs.m_index >= lhs.m_index);
            return out;
        }

#undef __okaylib_enumerator_enable_relop_if_random_access

#define __okaylib_enumerator_enable_member_if_random_access(rettype)       \
    template <typename T = iterable_t>                                     \
    constexpr std::enable_if_t<std::is_same_v<T, iterable_t> &&            \
                                   detail::is_random_access_iterable_v<T>, \
                               rettype>

        __okaylib_enumerator_enable_member_if_random_access(cursor_t&)
        operator+=(const size_t rhs) OKAYLIB_NOEXCEPT
        {
            m_index += rhs;
            m_inner += rhs;
            return *this;
        }

        __okaylib_enumerator_enable_member_if_random_access(cursor_t&)
        operator-=(const size_t rhs) OKAYLIB_NOEXCEPT
        {
            m_index -= rhs;
            m_inner -= rhs;
            return *this;
        }

        __okaylib_enumerator_enable_member_if_random_access(cursor_t)
        operator+(size_t rhs) const OKAYLIB_NOEXCEPT
        {
            cursor_t newcursor(m_index + rhs, m_inner + rhs);
            return newcursor;
        }

        __okaylib_enumerator_enable_member_if_random_access(cursor_t)
        operator-(size_t rhs) const OKAYLIB_NOEXCEPT
        {
            __ok_assert(rhs >= m_index);
            cursor_t newcursor(m_index - rhs, m_inner - rhs);
            return newcursor;
        }

#undef __okaylib_enumerator_enable_member_if_random_access

      private:
        constexpr cursor_t(size_t i, const cursor_type_for<iterable_t>& c)
            : m_index(i), m_inner(c) OKAYLIB_NOEXCEPT
        {
        }

        size_t m_index;
        cursor_type_for<iterable_t> m_inner;
    };

    constexpr static cursor_t begin(const enumerated_t<iterable_t>& i)
    {
        return cursor_t(ok::begin(i.iterable()));
    }

    template <typename T = iterable_t>
    constexpr static std::enable_if_t<std::is_same_v<iterable_t, T> &&
                                          detail::iterable_has_is_inbounds_v<T>,
                                      bool>
    is_inbounds(const enumerated_t<iterable_t>& i, const cursor_t& c)
    {
        return detail::iterable_definition_inner<T>::is_inbounds(i.iterable(),
                                                                 c.inner());
    }

    template <typename T = iterable_t>
    constexpr static std::enable_if_t<
        std::is_same_v<iterable_t, T> &&
            detail::iterable_has_is_after_bounds_v<T>,
        bool>
    is_after_bounds(const enumerated_t<iterable_t>& i, const cursor_t& c)
    {
        return detail::iterable_definition_inner<T>::is_after_bounds(
            i.iterable(), c.inner());
    }

    template <typename T = iterable_t>
    constexpr static std::enable_if_t<
        std::is_same_v<iterable_t, T> &&
            detail::iterable_has_is_before_bounds_v<T>,
        bool>
    is_before_bounds(const enumerated_t<iterable_t>& i, const cursor_t& c)
    {
        return detail::iterable_definition_inner<T>::is_before_bounds(
            i.iterable(), c.inner());
    }

    constexpr static auto get(const enumerated_t<iterable_t>& i,
                              const cursor_t& c) OKAYLIB_NOEXCEPT
    {
        using inner_def = detail::iterable_definition_inner<iterable_t>;
        if constexpr (detail::iterable_has_get_ref_v<iterable_t> ||
                      detail::iterable_has_get_ref_const_v<iterable_t>) {
            using reftype =
                decltype(inner_def::get_ref(i.iterable_nonconst(), c.inner()));
            return std::pair<reftype, const size_t>(
                inner_def::get_ref(i.iterable_nonconst(), c.inner()),
                c.index());
        } else {
            using gettype = decltype(inner_def::get(i.iterable(), c.inner()));
            return std::pair<gettype, const size_t>(
                inner_def::get(i.iterable(), c.inner()), c.index());
        }
    }

    // satisfy requirement that get() returns value_type
    // TODO: value_type still needed?
    using value_type =
        decltype(get(std::declval<const enumerated_t<iterable_t>&>(),
                     std::declval<const cursor_t&>()));
};

} // namespace detail

template <typename iterable_t>
struct iterable_definition<detail::enumerated_ref_t<iterable_t>>
    : public detail::enumerated_iterable_definition_base_t<
          detail::enumerated_ref_t, iterable_t>,
      public std::conditional_t<
          detail::iterable_has_size_v<iterable_t>,
          detail::sized_iterable_t<detail::enumerated_ref_t, iterable_t>,
          std::conditional<detail::iterable_marked_infinite_v<iterable_t>,
                           detail::infinite_iterable_t,
                           detail::finite_iterable_t>>
{};

template <typename iterable_t>
struct iterable_definition<detail::enumerated_owning_t<iterable_t>>
    : public detail::enumerated_iterable_definition_base_t<
          detail::enumerated_owning_t, iterable_t>,
      public std::conditional_t<
          detail::iterable_has_size_v<iterable_t>,
          detail::sized_iterable_t<detail::enumerated_owning_t, iterable_t>,
          std::conditional<detail::iterable_marked_infinite_v<iterable_t>,
                           detail::infinite_iterable_t,
                           detail::finite_iterable_t>>
{};

namespace detail {

struct enumerate_fn_t
{
    template <typename iterable_t>
    constexpr decltype(auto) operator()(iterable_t& item) const OKAYLIB_NOEXCEPT
    {
        return enumerated_ref_t{item};
    }

    template <typename iterable_t>
    constexpr decltype(auto)
    operator()(iterable_t&& item) const OKAYLIB_NOEXCEPT
    {
        return enumerated_owning_t{std::forward<iterable_t&&>(item)};
    }
};
} // namespace detail

constexpr detail::enumerate_fn_t enumerate{};

} // namespace ok

#endif
