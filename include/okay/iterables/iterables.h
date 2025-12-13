#ifndef __OKAYLIB_RANGES_ITERATOR_H__
#define __OKAYLIB_RANGES_ITERATOR_H__

#include "okay/detail/iterator_concepts.h"
#include "okay/detail/template_util/ref_as_const.h"
#include "okay/detail/utility.h"
#include "okay/math/ordering.h"
#include "okay/tuple.h"

namespace ok {

template <typename T, typename... args_t>
concept predicate_c = requires(const T& predicate, args_t&&... args) {
    {
        ok::invoke(predicate, ok::stdc::forward<args_t>(args)...)
    } -> ok::same_as_c<bool>;
};

/// Class which takes in its derived class as a template argument, and
/// implements common desireable features like STL style forward iteration (for
/// compatibility with standard foreach loops) and view member functions like
/// .enumerate() .reverse() etc
template <typename derived_t> struct iterator_common_impl_t;

template <typename iterable_t, cursor_c<iterable_t> cursor_t>
struct ref_iterator_t
    : public iterator_common_impl_t<ref_iterator_t<iterable_t, cursor_t>>
{
  private:
    iterable_t* iterable;
    cursor_t cursor;

  public:
    static constexpr inline bool is_infinite =
        infinite_cursor_c<cursor_t, iterable_t>;

    constexpr ref_iterator_t(iterable_t& i, cursor_t&& begin)
        : iterable(ok::addressof(i)), cursor(stdc::move(begin))
    {
    }

    using value_type = typename cursor_t::value_type;

    constexpr size_t index_impl() const
        requires stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    constexpr size_t size() const
        requires sized_cursor_c<cursor_t, iterable_t&>
    {
        return cursor.size(*iterable);
    }

    [[nodiscard]] constexpr opt<value_type> next()
    {
        return cursor.next(*iterable);
    }
};

template <typename iterable_t, cursor_c<iterable_t> cursor_t>
struct owning_iterator_t
    : public iterator_common_impl_t<owning_iterator_t<iterable_t, cursor_t>>
{
  private:
    using storage_type = stdc::remove_const_t<iterable_t>;

    storage_type iterable;
    cursor_t cursor;

  public:
    constexpr owning_iterator_t(storage_type&& i, cursor_t&& begin)
        : iterable(stdc::move(i)), cursor(stdc::move(begin))
    {
    }

    static constexpr inline bool is_infinite =
        infinite_cursor_c<cursor_t, iterable_t>;

    using value_type = typename cursor_t::value_type;

    constexpr size_t index_impl() const
        requires stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    constexpr size_t size() const
        requires sized_cursor_c<cursor_t, const iterable_t&>
    {
        return cursor.size(iterable);
    }

    [[nodiscard]] constexpr opt<value_type> next()
    {
        if constexpr (stdc::is_const_c<iterable_t>) {
            return cursor.next(static_cast<const storage_type&>(iterable));
        } else {
            return cursor.next(iterable);
        }
    }
};

template <typename iterable_t, arraylike_cursor_c<iterable_t> cursor_t>
struct ref_arraylike_iterator_t
    : public iterator_common_impl_t<
          ref_arraylike_iterator_t<iterable_t, cursor_t>>
{
  private:
    iterable_t* iterable;
    cursor_t cursor;

  public:
    static constexpr inline bool is_infinite =
        infinite_cursor_c<cursor_t, iterable_t>;

    constexpr ref_arraylike_iterator_t(iterable_t& i, cursor_t&& begin)
        : iterable(ok::addressof(i)), cursor(stdc::move(begin))
    {
    }

    using value_type = decltype(cursor.access(*iterable));

    [[nodiscard]] constexpr value_type access()
    {
        return cursor.access(*iterable);
    }

    constexpr void offset(int64_t offset)
    {
        return cursor.offset(
            static_cast<const stdc::remove_cvref_t<iterable_t>&>(*iterable),
            offset);
    }

    [[nodiscard]] constexpr size_t size() const
    {
        static_assert(sized_cursor_c<cursor_t, iterable_t>,
                      "requires constraints are too loose somewhere, this "
                      "should never be reached");
        return cursor.size(*iterable);
    }

    // iterator_common_impl_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return cursor.index(*iterable);
    }
};

template <typename iterable_t, arraylike_cursor_c<iterable_t> cursor_t>
struct owning_arraylike_iterator_t
    : public iterator_common_impl_t<
          owning_arraylike_iterator_t<iterable_t, cursor_t>>
{
  private:
    using storage_type = stdc::remove_const_t<iterable_t>;
    static_assert(!stdc::is_reference_v<storage_type>,
                  "Reference type in storage of owning view");

    storage_type iterable;
    cursor_t cursor;

  public:
    static constexpr inline bool is_infinite =
        infinite_cursor_c<cursor_t, iterable_t>;

    constexpr owning_arraylike_iterator_t(storage_type&& i, cursor_t&& begin)
        : iterable(stdc::move(i)), cursor(stdc::move(begin))
    {
    }

    using value_type = decltype(cursor.access(iterable));

    [[nodiscard]] constexpr value_type access()
    {
        if constexpr (stdc::is_const_c<iterable_t>) {
            return cursor.access(static_cast<const storage_type&>(iterable));
        } else {
            return cursor.access(iterable);
        }
    }

    constexpr void offset(int64_t offset)
    {
        return cursor.offset(static_cast<const storage_type&>(iterable),
                             offset);
    }

    [[nodiscard]] constexpr size_t size() const
        requires(!is_infinite)
    {
        return cursor.size(iterable);
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return cursor.index(iterable);
    }
};

namespace detail {
/// Class which wraps a std::vector, std::span, or std::array to make it
/// iterable.
template <typename cvref_qualified_container_t>
class stdlib_arraylike_container_iterable_compat_t
{
    using container_t = ok::remove_cvref_t<cvref_qualified_container_t>;

    // using pointer instead of reference to get autogenerated constructors and
    // assignments for free, but we have to pay later with `if constexpr` to use
    // the right syntax
    using storage_type = stdc::conditional_t<
        stdc::is_lvalue_reference_v<cvref_qualified_container_t>,
        stdc::remove_reference_t<cvref_qualified_container_t>*, container_t>;

    storage_type m_container;

    template <typename iterable_t, typename value_type_t> struct cursor_t;

    template <typename iterable_t, typename value_type_t>
    friend struct cursor_t;

    template <typename iterable_t, typename value_type_t> struct cursor_t
    {
      private:
        size_t m_index = 0;

      public:
        constexpr cursor_t() = default;

        using value_type = value_type_t;

        constexpr size_t
        index(const stdlib_arraylike_container_iterable_compat_t&) const
        {
            return m_index;
        }
        constexpr size_t
        size(const stdlib_arraylike_container_iterable_compat_t& iterable) const
        {
            if constexpr (stdc::is_pointer_v<storage_type>) {
                return iterable.m_container->size();
            } else {
                return iterable.m_container.size();
            }
        }
        constexpr void
        offset(const stdlib_arraylike_container_iterable_compat_t&,
               int64_t offset_amount)
        {
            m_index += offset_amount;
        }

        constexpr value_type access(iterable_t& iterable)
        {
            __ok_assert(m_index < size(iterable),
                        "out of bounds iteration into stdlib style container");
            if constexpr (stdc::is_pointer_v<storage_type>) {
                stdc::remove_reference_t<cvref_qualified_container_t>&
                    container = *iterable.m_container;
                return container[m_index];
            } else {
                return iterable.m_container[m_index];
            }
        }
    };

  public:
    constexpr stdlib_arraylike_container_iterable_compat_t(
        cvref_qualified_container_t container)
        requires stdc::is_rvalue_reference_v<cvref_qualified_container_t>
        : m_container(stdc::move(container))
    {
    }

    constexpr stdlib_arraylike_container_iterable_compat_t(
        cvref_qualified_container_t container)
        requires stdc::is_lvalue_reference_v<cvref_qualified_container_t>
        : m_container(ok::addressof(container))
    {
    }

    constexpr stdlib_arraylike_container_iterable_compat_t(
        cvref_qualified_container_t container)
        requires(!stdc::is_reference_v<cvref_qualified_container_t>)
        : m_container(stdc::move(container))
    {
    }

    [[nodiscard]] constexpr auto iter() &&
    {
        using value_type_for_container = stdc::conditional_t<
            stdc::is_pointer_v<storage_type>,
            decltype(stdc::declval<stdc::remove_reference_t<
                         cvref_qualified_container_t>&>()[size_t{}]),
            decltype(m_container[size_t{}])>;

        using cursor = cursor_t<stdlib_arraylike_container_iterable_compat_t,
                                value_type_for_container>;

        using iterator = ok::owning_arraylike_iterator_t<
            stdlib_arraylike_container_iterable_compat_t, cursor>;

        return iterator{stdc::move(*this), {}};
    }
};

template <typename c_array_value_type> class c_array_iterable_compat_t
{
    c_array_value_type* m_array_start;
    size_t m_size;

    struct cursor_t;

    friend struct cursor_t;

    struct cursor_t
    {
      private:
        size_t m_index = 0;

      public:
        constexpr cursor_t() = default;

        using value_type = c_array_value_type&;

        constexpr size_t index(const c_array_iterable_compat_t&) const
        {
            return m_index;
        }
        constexpr size_t size(const c_array_iterable_compat_t& iterable) const
        {
            return iterable.m_size;
        }

        constexpr void offset(const c_array_iterable_compat_t&,
                              int64_t offset_amount)
        {
            m_index += offset_amount;
        }

        constexpr c_array_value_type&
        access(const c_array_iterable_compat_t& iterable)
        {
            __ok_assert(m_index < iterable.m_size,
                        "Out of bounds iteration into c-style array");
            return iterable.m_array_start[m_index];
        }
    };

  public:
    template <size_t N>
    constexpr c_array_iterable_compat_t(c_array_value_type (&array)[N])
        : m_array_start(array), m_size(N)
    {
    }

    [[nodiscard]] constexpr auto iter() const
    {
        // "owning" iterable which just copies us because we are a pointer, so
        // not actually owning
        return ok::owning_arraylike_iterator_t<c_array_iterable_compat_t,
                                               cursor_t>{
            c_array_iterable_compat_t(*this), {}};
    }
};

template <typename T>
concept const_index_operator_reference_semantics =
    requires(const stdc::remove_cv_t<T>& container, size_t index) {
        { container[index] } -> ok::same_as_c<typename T::value_type&>;
    };

template <typename T>
concept const_index_operator_value_semantics =
    requires(const stdc::remove_cv_t<T>& container, size_t index) {
        { container[index] } -> ok::same_as_c<const typename T::value_type&>;
    };

template <typename T>
concept stdlib_arraylike_container_c =
    requires(stdc::remove_cv_t<T>& container_nonconst,
             const stdc::remove_cv_t<T>& container_const, size_t index) {
        { container_const.size() } -> ok::same_as_c<size_t>;
        requires const_index_operator_value_semantics<T> ||
                     const_index_operator_reference_semantics<T>;
        { container_nonconst[index] } -> ok::same_as_c<typename T::value_type&>;
    };

template <typename T>
concept has_iter_memfun_c = requires(stdc::remove_cv_t<T>& iterable_nonconst,
                                     const stdc::remove_cv_t<T>& iterable_const,
                                     stdc::remove_cv_t<T>&& iterable_rvalue) {
    { iterable_nonconst.iter() } -> iterator_c;
    { iterable_const.iter() } -> iterator_c;
    { stdc::move(iterable_rvalue).iter() } -> iterator_c;
};

template <typename T, bool is_const> struct opt_cursor_t
{
    using container_t = stdc::conditional_t<is_const, const opt<T>&, opt<T>&>;
    bool has_called = false;

    using value_type = decltype(stdc::declval<container_t>().ref_unchecked());

    [[nodiscard]] constexpr opt<value_type>
    next(container_t container) OKAYLIB_NOEXCEPT
    {
        has_called = true;
        if (!has_called && container)
            return opt<value_type>(container.ref_unchecked());
        return opt<value_type>{};
    }
};

struct make_into_iterator_fn_t
{
    template <typename T, size_t N>
    [[nodiscard]] constexpr auto
    operator()(T (&array)[N]) const OKAYLIB_NOEXCEPT
    {
        return c_array_iterable_compat_t(array).iter();
    }

    template <typename T>
        requires(stdlib_arraylike_container_c<stdc::remove_cvref_t<T>> &&
                 !has_iter_memfun_c<T>)
    [[nodiscard]] constexpr decltype(auto)
    operator()(T&& arraylike) const OKAYLIB_NOEXCEPT
    {
        return stdlib_arraylike_container_iterable_compat_t<
                   decltype(arraylike)>(std::forward<T>(arraylike))
            .iter();
    }

    template <typename T>
        requires(has_iter_memfun_c<T>)
    [[nodiscard]] constexpr decltype(auto)
    operator()(T&& iterable) const OKAYLIB_NOEXCEPT
    {
        return stdc::forward<T>(iterable).iter();
    }

    /// If a thing is already an iterator and its coming in as an rvalue,
    /// ok::iter() is just an identity function
    template <iterator_c T>
    [[nodiscard]] constexpr decltype(auto)
    operator()(T&& iterator) const OKAYLIB_NOEXCEPT
        requires(stdc::is_rvalue_reference_v<decltype(iterator)> &&
                 !stdlib_arraylike_container_c<T> && !has_iter_memfun_c<T>)
    {
        return stdc::forward<T>(iterator);
    }

    // if a thing is an lvalue iterator, then it just gets copied if it can
    template <iterator_c T>
    [[nodiscard]] constexpr T
    operator()(const T& iterator) const OKAYLIB_NOEXCEPT
        requires(stdc::is_copy_constructible_v<decltype(iterator)> &&
                 !stdlib_arraylike_container_c<T> && !has_iter_memfun_c<T>)
    {
        return iterator;
    }

    // retroactively make ok::opt iterable
    template <is_instance_c<ok::opt> T>
    [[nodiscard]] constexpr auto
    operator()(const T& optional) const OKAYLIB_NOEXCEPT
    {
        return ref_iterator_t<T, opt_cursor_t<typename T::value_type, true>>{
            optional, {}};
    }

    template <is_instance_c<ok::opt> T>
    [[nodiscard]] constexpr auto operator()(T& optional) const OKAYLIB_NOEXCEPT
    {
        return ref_iterator_t<T, opt_cursor_t<typename T::value_type, false>>{
            optional, {}};
    }

    template <is_instance_c<ok::opt> T>
    [[nodiscard]] constexpr auto operator()(T&& optional) const OKAYLIB_NOEXCEPT
    {
        return owning_iterator_t<T,
                                 opt_cursor_t<typename T::value_type, false>>{
            stdc::move(optional), {}};
    }
};

} // namespace detail

inline constexpr detail::make_into_iterator_fn_t iter{};

namespace detail {
template <typename T>
concept iterable_impl_c = requires(T obj) {
    requires detail::is_moveable_c<stdc::remove_cvref_t<T>> ||
                 stdc::is_lvalue_reference_v<T>;
    { ok::iter(stdc::forward<stdc::remove_cvref_t<T>>(obj)) } -> iterator_c;
};
template <typename T>
concept iterable_const_ref_impl_c = requires(T obj) {
    requires stdc::is_lvalue_reference_v<T>;
    requires stdc::is_const_c<stdc::remove_reference_t<T>>;
    { ok::iter(obj) } -> iterator_c;
};
} // namespace detail

/// An iterable is something that can have ok::iter called on it. (Technically
/// this means that iterators are also iterables).
template <typename T>
concept iterable_c =
    // the forward<remove_cvref_t<T>>(...) in the iterable_impl_c doesn't work
    // on c style array references, so we have to make an exception for them
    stdc::is_array_v<stdc::remove_cvref_t<T>> || detail::iterable_impl_c<T> ||
    // the forward also seems to not work on the const lvalue ref?
    detail::iterable_const_ref_impl_c<T>;

template <typename T>
concept arraylike_iterable_c = requires(T obj) {
    { ok::iter(obj) } -> arraylike_iterator_c;
};

namespace detail {
// the reason this is exists is because my version of clang-format with format
// everything incorrectly if I write this constraint in line
template <typename... Ts>
concept zip_constraints_c = (iterable_c<Ts> && ...);
} // namespace detail

template <iterable_c T>
using iterator_for =
    stdc::remove_cvref_t<decltype(ok::iter(stdc::declval<T>()))>;

template <iterable_c T>
using value_type_for = typename iterator_for<T>::value_type;

template <typename T>
inline constexpr bool is_iterable_infinite =
    detail::infinite_iterator_c<iterator_for<T>>;

template <typename T>
inline constexpr bool is_iterable_sized =
    detail::sized_iterator_c<iterator_for<T>>;

namespace adaptor {
template <typename T, typename corresponding_iterator_t>
concept keep_if_predicate_c =
    requires(const T& t, value_type_for<corresponding_iterator_t> value_type) {
        requires iterator_c<corresponding_iterator_t>;
        { invoke(t, value_type) } -> same_as_c<bool>;
    };

template <iterator_c viewed_t, keep_if_predicate_c<viewed_t> predicate_t>
struct keep_if_t;

template <typename viewed_t> struct flatten_t;

template <arraylike_iterator_c viewed_t>
    requires detail::sized_iterator_c<viewed_t>
struct reverse_t;
template <typename viewed_t> struct drop_t;
template <typename viewed_t> struct take_t;
template <typename viewed_t> struct enumerate_t;
template <typename... viewed_t> struct zip_t;
template <typename viewed_t, typename callable_t> struct transform_t;
template <typename viewed_t> struct as_const_t;
template <typename viewed_t, size_t index> struct get_t;
} // namespace adaptor

template <typename derived_t> struct iterator_common_impl_t
{
    struct sentinel_t
    {};
    struct iterator;
    friend struct iterator;

    struct iterator
    {
      private:
        derived_t& m_parent;
        mutable ok::opt<typename derived_t::value_type> m_last_output_value;

      public:
        // TODO: make these two std:: things work without STL
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = derived_t::value_type;
        using reference = ok::stdc::add_lvalue_reference_t<value_type>;
        using pointer = ok::stdc::add_pointer_t<value_type>;

        constexpr iterator() = delete;

        constexpr iterator(derived_t& parent)
            : m_parent(parent), m_last_output_value(parent.next())
        {
        }

        constexpr reference operator*() const OKAYLIB_NOEXCEPT
        {
            if (!m_last_output_value) [[unlikely]] {
                __ok_abort("out of bounds dereference of iterator");
            }
            return m_last_output_value.ref_unchecked();
        }

        constexpr iterator& operator++() OKAYLIB_NOEXCEPT
        {
            // resetting to avoid assigning elements, for example if elements
            // are references
            m_last_output_value.reset();
            ok::opt next = m_parent.next();
            if (!next)
                return *this;
            m_last_output_value.emplace(next.ref_unchecked());
            return *this;
        }

        constexpr iterator operator++(int) const OKAYLIB_NOEXCEPT
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr friend bool operator==(const iterator& a,
                                         const sentinel_t&) OKAYLIB_NOEXCEPT
        {
            return !a.m_last_output_value;
        }

        constexpr friend bool operator!=(const iterator& a,
                                         const sentinel_t& b) OKAYLIB_NOEXCEPT
        {
            return !(a == b);
        }
    };

    [[nodiscard]] constexpr iterator begin()
    {
        return iterator(*static_cast<derived_t*>(this));
    }

    [[nodiscard]] constexpr sentinel_t end() const { return {}; }

    [[nodiscard]] constexpr size_t index() const
        requires requires(const derived_t& d) {
            { d.index_impl() } -> ok::same_as_c<size_t>;
        }
    {
        return static_cast<const derived_t*>(this)->index_impl();
    }

    template <typename predicate_t>
    [[nodiscard]] constexpr auto keep_if(const predicate_t& predicate) &&
        requires detail::invocable_c<const predicate_t,
                                     typename derived_t::value_type>
    {
        return adaptor::keep_if_t<derived_t, predicate_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), predicate);
    }

    [[nodiscard]] constexpr auto drop(size_t num_to_drop) &&
    {
        return adaptor::drop_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), num_to_drop);
    }

    [[nodiscard]] constexpr auto take_at_most(size_t max_num_to_take) &&
    {
        return adaptor::take_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), max_num_to_take);
    }

    [[nodiscard]] constexpr auto enumerate() &&
    {
        return adaptor::enumerate_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    [[nodiscard]] constexpr auto reverse() &&
        requires arraylike_iterator_c<derived_t> &&
                 detail::sized_iterator_c<derived_t>
    {
        return adaptor::reverse_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    [[nodiscard]] constexpr auto flatten() &&
        requires iterable_c<value_type_for<derived_t>>
    {
        return adaptor::flatten_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    template <typename callable_t>
    [[nodiscard]] constexpr auto transform(const callable_t& transformer) &&
        requires detail::invocable_c<const callable_t,
                                     typename derived_t::value_type>
    {
        return adaptor::transform_t<derived_t, callable_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), transformer);
    }

    [[nodiscard]] constexpr auto as_const() &&
    {
        return adaptor::as_const_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    template <size_t index> [[nodiscard]] constexpr auto get_tuple_elem() &&
    {
        return adaptor::get_t<derived_t, index>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    template <typename first_other_iterable_t, typename... extra_iterables_t>
    [[nodiscard]] constexpr auto zip(first_other_iterable_t&& other,
                                     extra_iterables_t&&... extras) &&
        requires detail::zip_constraints_c<decltype(other), decltype(extras)...>
    {
        return adaptor::zip_t(
            stdc::move(*static_cast<derived_t*>(this)),
            iter(stdc::forward<first_other_iterable_t>(other)),
            iter(stdc::forward<extra_iterables_t>(extras))...);
    }

    template <typename callable_t>
    constexpr void for_each(const callable_t& callable) &&
        requires ok::detail::invocable_c<
            callable_t,
            decltype(static_cast<derived_t*>(this) -> next().ref_unchecked())>
    {
        for (auto&& item : *this) {
            ok::invoke(
                callable,
                stdc::forward<
                    ok::remove_cvref_t<typename derived_t::value_type>>(item));
        }
    }

    template <typename predicate_t>
    constexpr bool all_satisfy(const predicate_t& predicate) &&
        requires predicate_c<predicate_t, typename derived_t::value_type>
    {
        for (auto&& item : *this) {
            const bool satisfied_predicate = ok::invoke(
                predicate,
                stdc::forward<
                    ok::remove_cvref_t<typename derived_t::value_type>>(item));

            if (!satisfied_predicate) {
                return false;
            }
        }
        return true;
    }

    template <typename predicate_t>
    constexpr bool is_all_true() &&
        requires ok::stdc::convertible_to_c<typename derived_t::value_type,
                                            bool>
    {
        for (auto&& item : *this) {
            if (!stdc::forward<
                    ok::remove_cvref_t<typename derived_t::value_type>>(item)) {
                return false;
            }
        }
        return true;
    }

    template <typename predicate_t>
    constexpr bool any_satisfy(const predicate_t& predicate) &&
        requires predicate_c<
            predicate_t,
            decltype(static_cast<derived_t*>(this) -> next().ref_unchecked())>
    {
        for (auto&& item : *this) {
            const bool satisfied_predicate = ok::invoke(
                predicate,
                stdc::forward<
                    ok::remove_cvref_t<typename derived_t::value_type>>(item));

            if (satisfied_predicate) {
                return true;
            }
        }
        return false;
    }

    template <typename predicate_t>
    constexpr bool is_any_true() &&
        requires ok::stdc::convertible_to_c<typename derived_t::value_type,
                                            bool>
    {
        for (auto&& item : *this) {
            if (stdc::forward<
                    ok::remove_cvref_t<typename derived_t::value_type>>(item)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] constexpr auto next()
        requires detail::iterator_impl_c<derived_t>
    {
        return static_cast<derived_t*>(this)->next_impl();
    }

    [[nodiscard]] constexpr auto next()
        requires(!detail::iterator_impl_c<derived_t>)
    {
        static_assert(arraylike_iterator_c<derived_t>,
                      "The iterator inheriting from iterator_common_impl_t "
                      "does not have .next_impl(), nor is it arraylike");
        ok::opt<typename derived_t::value_type> out;
        auto* derived = static_cast<derived_t*>(this);

        if constexpr (arraylike_iterator_c<derived_t> &&
                      detail::sized_iterator_c<derived_t>) {
            if (this->index() >= derived->size())
                return out;
        }
        out.emplace(derived->access());
        derived->offset(1);
        return out;
    }
};

namespace adaptor {
template <iterator_c viewed_t, keep_if_predicate_c<viewed_t> predicate_t>
struct keep_if_t
    : public iterator_common_impl_t<keep_if_t<viewed_t, predicate_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;
    const predicate_t* predicate;

    using value_type = typename viewed_t::value_type;

    constexpr keep_if_t(viewed_t&& input, const predicate_t& pred)
        : iterator(stdc::move(input)), predicate(ok::addressof(pred))
    {
    }

    constexpr size_t index() const = delete;

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out = iterator.next();

        if (!out)
            return out;

        while (!(*predicate)(out.ref_unchecked())) {
            out.reset();
            opt next = iterator.next();
            if (!next)
                return out;
            out.emplace(next.ref_unchecked());
        }

        return out;
    }
};

template <iterator_c viewed_t>
    requires iterable_c<value_type_for<viewed_t>>
struct flatten_t<viewed_t> : public iterator_common_impl_t<flatten_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);

  private:
    using inner_iterator_t = iterator_for<typename viewed_t::value_type>;
    viewed_t iterator;
    opt<inner_iterator_t> inner_iterator;

  public:
    using value_type = typename inner_iterator_t::value_type;

    constexpr flatten_t(viewed_t&& input)
        : iterator(stdc::move(input)),
          inner_iterator(iterator.next().take_and_run(iter))
    {
    }

    constexpr size_t index() const = delete;

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        if (!inner_iterator)
            return out;

        if (!(out = inner_iterator.ref_unchecked().next())) {
            inner_iterator = iterator.next().take_and_run(iter);
            while (inner_iterator) {

                if ((out = inner_iterator.ref_unchecked().next()))
                    return out;

                inner_iterator = iterator.next().take_and_run(iter);
            }
        }

        return out;
    }
};

template <arraylike_iterator_c viewed_t>
    requires detail::sized_iterator_c<viewed_t>
struct reverse_t : public iterator_common_impl_t<reverse_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;

    using value_type = typename viewed_t::value_type;

    constexpr reverse_t(viewed_t&& input) : iterator(stdc::move(input))
    {
        const size_t size = iterator.size();
        const size_t current = iterator.index();
        const size_t desired = size - current - 1;
        iterator.offset(int64_t(desired) - int64_t(current));
    }

    [[nodiscard]] constexpr value_type access() { return iterator.access(); }

    constexpr void offset(int64_t offset) { return iterator.offset(-offset); }

    [[nodiscard]] constexpr size_t size() const
        requires detail::sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterator.size() - iterator.index() - 1;
    }
};

template <iterator_c viewed_t>
    requires(!arraylike_iterator_c<viewed_t>)
struct take_t<viewed_t> : public iterator_common_impl_t<take_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;
    size_t consumptions_remaining;
    // TODO: this is only used if sized_iterator_c<viewed_t>, maybe remove it in
    // other cases
    size_t consumptions;

    using value_type = typename viewed_t::value_type;

    constexpr take_t(viewed_t&& input, size_t consumptions)
        : iterator(stdc::move(input)), consumptions(consumptions),
          consumptions_remaining(consumptions)
    {
    }

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
        requires detail::sized_iterator_c<viewed_t>
    {
        return ok::min(iterator.size(), consumptions);
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        if (consumptions_remaining == 0)
            return {};

        --consumptions_remaining;
        return iterator.next();
    }
};

template <arraylike_iterator_c viewed_t>
struct take_t<viewed_t> : public iterator_common_impl_t<take_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);

  private:
    viewed_t iterator;
    size_t clamped_length;

  public:
    using value_type = typename viewed_t::value_type;
    static_assert(stdc::is_same_v<decltype(stdc::declval<viewed_t>().access()),
                                  value_type>);

    // taking from an infinite iterator makes it finite
    static constexpr bool is_infinite = false;

    constexpr take_t(viewed_t&& input, size_t consumptions)
        : iterator(stdc::move(input)), clamped_length(consumptions)
    {
    }

    [[nodiscard]] constexpr size_t size() const
    {
        if constexpr (detail::sized_iterator_c<viewed_t>) {
            return ok::min(iterator.size(), clamped_length);
        } else {
            static_assert(detail::infinite_iterator_c<viewed_t>);
            return clamped_length;
        }
    }

    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterator.index();
    }

    [[nodiscard]] constexpr auto access()
    {
        __ok_internal_assert(index_impl() < clamped_length);
        return iterator.access();
    }

    constexpr void offset(int64_t offset_amount)
    {
        iterator.offset(offset_amount);
    }
};

template <iterator_c viewed_t>
    requires(!arraylike_iterator_c<viewed_t>)
struct drop_t<viewed_t> : public iterator_common_impl_t<drop_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;
    // TODO: only used it sized_iterator_c<viewed_t>, maybe remove this in other
    // cases
    size_t skips;
    size_t skips_remaining; // decremented on first call to next()

    using value_type = typename viewed_t::value_type;

    constexpr drop_t(viewed_t&& input, size_t skips)
        : iterator(stdc::move(input)), skips(skips), skips_remaining(skips)
    {
    }

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
        requires detail::sized_iterator_c<viewed_t>
    {
        const size_t size = iterator.size();
        return size > skips ? size - skips : 0;
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        // add one to skips remaining so we can use the same logic for skipping
        // as for getting a value with no skips
        skips_remaining = ok::max(1UL, skips_remaining);
        opt<value_type> out;

        while (skips_remaining) {
            out.reset();
            out = iterator.next();
            if (!out) {
                skips_remaining = 0;
                return out;
            }
            --skips_remaining;
        }

        __ok_internal_assert(out);

        return out;
    }
};

template <arraylike_iterator_c viewed_t>
struct drop_t<viewed_t> : public iterator_common_impl_t<drop_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);

  private:
    viewed_t iterator;
    size_t skips;

  public:
    using value_type = typename viewed_t::value_type;

    static constexpr bool is_infinite = detail::infinite_iterator_c<viewed_t>;

    constexpr drop_t(viewed_t&& input, size_t skips)
        : iterator(stdc::move(input)), skips(skips)
    {
        const size_t maybe_bad_index = iterator.index();
        if (maybe_bad_index < skips) {
            iterator.offset(skips - maybe_bad_index);
        }
        __ok_internal_assert(iterator.index() >= skips);
    }

    [[nodiscard]] constexpr size_t size() const
        requires(!is_infinite)
    {
        return ok::max(iterator.size(), skips) - skips;
    }

    [[nodiscard]] constexpr size_t index_impl() const
    {
        const size_t index = iterator.index();
        __ok_internal_assert(index >= skips);
        return index - skips;
    }

    [[nodiscard]] constexpr value_type access() { return iterator.access(); }

    constexpr void offset(int64_t offset_amount)
    {
        const size_t index = iterator.index();

        if (-int64_t(index) > offset_amount || index + offset_amount < skips) {
            // offset_amount is negative and is going to subtract by more than
            // the current index, overflow
            // OR
            // offset amount is negative, does not cause overflow, but the
            // caller would see it as overflow thanks to the transformation
            // caused by our .index(), so they have failed
            __ok_abort("index integer overflow in drop adaptor");
        }

        iterator.offset(offset_amount);
    }
};

template <iterator_c viewed_t>
    requires(!index_providing_iterator_c<viewed_t> &&
             (stdc::is_reference_v<value_type_for<viewed_t>> ||
              detail::is_move_constructible_c<value_type_for<viewed_t>>))
struct enumerate_t<viewed_t>
    : public iterator_common_impl_t<enumerate_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;
    size_t index = 0;

    using value_type = ok::tuple<typename viewed_t::value_type, const size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
        requires detail::sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        opt<typename viewed_t::value_type> opt = iterator.next();
        if (!opt)
            return out;
        out.emplace(opt.ref_or_panic(), index);
        ++index;
        return out;
    }
};

template <iterator_c viewed_t>
    requires(index_providing_iterator_c<viewed_t> &&
             !arraylike_iterator_c<viewed_t> &&
             (stdc::is_reference_v<value_type_for<viewed_t>> ||
              detail::is_move_constructible_c<value_type_for<viewed_t>>))
struct enumerate_t<viewed_t>
    : public iterator_common_impl_t<enumerate_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;

    using value_type = tuple<typename viewed_t::value_type, const size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
        requires detail::sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        size_t idx = iterator.index();
        opt<typename viewed_t::value_type> opt = iterator.next();
        if (!opt)
            return out;
        out.emplace(stdc::move(opt.ref_unchecked()), idx);
        return out;
    }
};

template <arraylike_iterator_c viewed_t>
    requires(stdc::is_reference_v<value_type_for<viewed_t>> ||
             detail::is_move_constructible_c<value_type_for<viewed_t>>)
struct enumerate_t<viewed_t>
    : public iterator_common_impl_t<enumerate_t<viewed_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;

    using value_type = tuple<typename viewed_t::value_type, const size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    [[nodiscard]] constexpr size_t size() const OKAYLIB_NOEXCEPT
    {
        return iterator.size();
    }

    [[nodiscard]] constexpr size_t index_impl() const OKAYLIB_NOEXCEPT
    {
        return iterator.index();
    }

    constexpr void offset(int64_t offset_amount) OKAYLIB_NOEXCEPT
    {
        return iterator.offset(offset_amount);
    }

    [[nodiscard]] constexpr value_type access() OKAYLIB_NOEXCEPT
    {
        return value_type{iterator.access(), iterator.index()};
    }
};

namespace detail {
template <typename... tuple_elems_t>
// performs some funny lambda fold trickery to find the minimum value
// returned by size() for all the iterators, unless they are all
// infinite/unsized in which case nullopt is returned
constexpr ok::opt<size_t>
size_impl(const ok::tuple<tuple_elems_t...>& iterators)
{
    const auto with_indices =
        [&]<size_t... indices>(
            ok::stdc::index_sequence<indices...>) -> ok::opt<size_t> {
        ok::opt<size_t> out;
        (
            [&] {
                if constexpr (ok::detail::sized_iterator_c<tuple_elems_t>) {
                    const size_t size = ok::get<indices>(iterators).size();
                    if (!out || out.ref_unchecked() > size)
                        out = size;
                }
            }(),
            ...);
        return out;
    };

    return with_indices(
        ok::stdc::make_index_sequence<sizeof...(tuple_elems_t)>());
}
} // namespace detail

template <typename... viewed_t>
    requires((iterator_c<viewed_t> && ...) &&
             !(arraylike_iterator_c<viewed_t> && ...) &&
             sizeof...(viewed_t) > 1)
struct zip_t<viewed_t...> : public iterator_common_impl_t<zip_t<viewed_t...>>
{
    static_assert((!stdc::is_const_c<viewed_t> && ...));

  private:
    ok::tuple<viewed_t...> m_iterators;
    // TODO: only used it all iterators are sized, optimize this away when not
    // needed?
    size_t m_size;

  public:
    using value_type = ok::tuple<typename viewed_t::value_type...>;

    inline static constexpr bool is_infinite =
        (ok::detail::infinite_iterator_c<viewed_t> && ...);

    template <typename... rvalues_t>
    constexpr zip_t(rvalues_t&&... iterators)
        : m_iterators(stdc::move(iterators)...)
    {
        if constexpr ((is_iterable_sized<viewed_t> && ...)) {
            m_size = adaptor::detail::size_impl(m_iterators).ref_unchecked();
        }
    }

    [[nodiscard]] constexpr size_t size() const
        requires(is_iterable_sized<viewed_t> && ...)
    {
        return m_size;
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        const auto with_indices =
            [&]<size_t... indices, typename... inner_viewed_t>(
                ok::stdc::index_sequence<indices...>,
                ok::tuple<inner_viewed_t...>& iterators) -> opt<value_type> {
            ok::tuple<ok::opt<typename viewed_t::value_type>...>
                iteration_results{ok::get<indices>(iterators).next()...};

            opt<value_type> out;

            if ((!ok::get<indices>(iteration_results).has_value() || ...))
                return out;

            out.emplace(ok::get<indices>(iteration_results).ref_unchecked()...);
            return out;
        };

        return with_indices(
            ok::stdc::make_index_sequence<sizeof...(viewed_t)>(), m_iterators);
    }
};

template <typename... viewed_t>
    requires((arraylike_iterator_c<viewed_t> && ...) && sizeof...(viewed_t) > 1)
struct zip_t<viewed_t...> : public iterator_common_impl_t<zip_t<viewed_t...>>
{
    static_assert((!stdc::is_const_c<viewed_t> && ...));

  private:
    ok::tuple<viewed_t...> m_iterators;
    size_t m_size;

  public:
    using value_type = ok::tuple<typename viewed_t::value_type...>;

    inline static constexpr bool is_infinite =
        (ok::detail::infinite_iterator_c<viewed_t> && ...);

    template <typename... rvalues_t>
    constexpr zip_t(rvalues_t&&... iterators)
        : m_iterators(stdc::move(iterators)...), m_size()
    {
        if constexpr (!is_infinite) {
            m_size = adaptor::detail::size_impl(m_iterators).ref_unchecked();
        }
    }

    [[nodiscard]] constexpr size_t index_impl() const
    {
        return ok::get<0>(m_iterators).index();
    }

    [[nodiscard]] constexpr size_t size() const
        requires(!is_infinite)
    {
        return m_size;
    }

    constexpr void offset(int64_t offset)
    {
        const auto with_indices =
            [&]<size_t... indices>(ok::stdc::index_sequence<indices...>) {
                ([&] { ok::get<indices>(m_iterators).offset(offset); }(), ...);
            };

        return with_indices(
            ok::stdc::make_index_sequence<sizeof...(viewed_t)>());
    }

    constexpr value_type access()
    {
        __ok_assert(ok::get<0>(m_iterators).index() < m_size,
                    "Out of bounds access to zipped view");

        const auto with_indices =
            [&]<size_t... indices>(
                ok::stdc::index_sequence<indices...>) -> value_type {
            return value_type{ok::get<indices>(m_iterators).access()...};
        };

        return with_indices(
            ok::stdc::make_index_sequence<sizeof...(viewed_t)>());
    }
};

template <typename... rvalues_t>
zip_t(rvalues_t&&... rvalues) -> zip_t<stdc::remove_cvref_t<rvalues_t>...>;

template <iterator_c viewed_t, typename callable_t>
    requires(!arraylike_iterator_c<viewed_t>)
struct transform_t<viewed_t, callable_t>
    : public iterator_common_impl_t<transform_t<viewed_t, callable_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);

  private:
    viewed_t iterator;
    const callable_t* transformer;

  public:
    using value_type = decltype((*transformer)(
        stdc::declval<typename viewed_t::value_type>()));

    constexpr transform_t(viewed_t&& input, const callable_t& transformer)
        : iterator(stdc::move(input)), transformer(ok::addressof(transformer))
    {
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        opt<typename viewed_t::value_type> opt = iterator.next();
        if (!opt)
            return out;
        out.emplace((*transformer)(
            stdc::forward<
                stdc::remove_reference_t<typename viewed_t::value_type>>(
                opt.ref_unchecked())));
        return out;
    }
};

template <arraylike_iterator_c viewed_t, typename callable_t>
struct transform_t<viewed_t, callable_t>
    : public iterator_common_impl_t<transform_t<viewed_t, callable_t>>
{
    static_assert(!stdc::is_const_c<viewed_t>);
    viewed_t iterator;
    const callable_t* transformer;

    using value_type = decltype((*transformer)(
        stdc::declval<
            stdc::add_lvalue_reference_t<typename viewed_t::value_type>>()));

    static inline constexpr bool is_infinite =
        ok::detail::infinite_iterator_c<viewed_t>;

    constexpr transform_t(viewed_t&& input, const callable_t& transformer)
        : iterator(stdc::move(input)), transformer(ok::addressof(transformer))
    {
    }

    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterator.index();
    }

    [[nodiscard]] constexpr size_t size() const
        requires ok::detail::sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    constexpr void offset(int64_t offset_amount) OKAYLIB_NOEXCEPT
    {
        iterator.offset(offset_amount);
    }

    constexpr value_type access() { return (*transformer)(iterator.access()); }
};

template <iterator_c viewed_t>
    requires(!arraylike_iterator_c<viewed_t> &&
             stdc::is_reference_v<value_type_for<viewed_t>>)
struct as_const_t<viewed_t>
    : public iterator_common_impl_t<as_const_t<viewed_t>>
{
  private:
    viewed_t iterator;

  public:
    constexpr as_const_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    using value_type = ok::detail::ref_as_const_t<value_type_for<viewed_t>>;

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        auto parent = iterator.next();
        if (!parent)
            return {};
        return parent.ref_unchecked();
    }
};

template <arraylike_iterator_c viewed_t>
    requires stdc::is_reference_v<value_type_for<viewed_t>>
struct as_const_t<viewed_t>
    : public iterator_common_impl_t<as_const_t<viewed_t>>
{
    viewed_t iterator;

    static inline constexpr bool is_infinite =
        ok::detail::infinite_iterator_c<viewed_t>;

    using value_type = ok::detail::ref_as_const_t<value_type_for<viewed_t>>;

    constexpr as_const_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterator.index();
    }

    [[nodiscard]] constexpr size_t size() const
        requires ok::detail::sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    constexpr void offset(int64_t offset_amount) OKAYLIB_NOEXCEPT
    {
        iterator.offset(offset_amount);
    }

    constexpr value_type access() { return iterator.access(); }
};

template <iterator_c viewed_t, size_t index>
    requires(!arraylike_iterator_c<viewed_t> &&
             ok::detail::is_instance_c<value_type_for<viewed_t>, ok::tuple>)
struct get_t<viewed_t, index>
    : public iterator_common_impl_t<get_t<viewed_t, index>>
{
  private:
    viewed_t iterator;

  public:
    constexpr get_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    using value_type =
        decltype(ok::get<index>(stdc::declval<value_type_for<viewed_t>>()));

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        auto parent = iterator.next();
        if (!parent)
            return {};
        return ok::get<index>(parent.ref_unchecked());
    }
};

template <arraylike_iterator_c viewed_t, size_t index>
    requires ok::detail::is_instance_c<value_type_for<viewed_t>, ok::tuple>
struct get_t<viewed_t, index>
    : public iterator_common_impl_t<get_t<viewed_t, index>>
{
    viewed_t iterator;

    static inline constexpr bool is_infinite =
        ok::detail::infinite_iterator_c<viewed_t>;

    using value_type =
        decltype(ok::get<index>(stdc::declval<value_type_for<viewed_t>>()));

    constexpr get_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterator.index();
    }

    [[nodiscard]] constexpr size_t size() const
        requires ok::detail::sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    constexpr void offset(int64_t offset_amount) OKAYLIB_NOEXCEPT
    {
        iterator.offset(offset_amount);
    }

    constexpr value_type access() { return ok::get<index>(iterator.access()); }
};

} // namespace adaptor

inline constexpr auto zip = []<typename T, typename T2, typename... extras_t>(
                                T&& first, T2&& second, extras_t&&... extras)
    requires detail::zip_constraints_c<decltype(first), decltype(second),
                                       decltype(extras)...>
{
    return ok::iter(stdc::forward<T>(first))
        .zip(stdc::forward<T2>(second), stdc::forward<extras_t>(extras)...);
};

inline constexpr auto enumerate = []<typename T>(T&& iterable)
    requires iterable_c<decltype(iterable)>
{ return ok::iter(stdc::forward<T>(iterable)).enumerate(); };

inline constexpr auto transform =
    []<typename T, typename callable_t>(T&& iterable,
                                        const callable_t& transformer)
    requires(iterable_c<decltype(iterable)> &&
             detail::invocable_c<const callable_t,
                                 value_type_for<decltype(iterable)>>)
{ return ok::iter(stdc::forward<T>(iterable)).transform(transformer); };

inline constexpr auto reverse = []<typename T>(T&& iterable)
    requires arraylike_iterable_c<decltype(iterable)>
{ return ok::iter(stdc::forward<T>(iterable)).reverse(); };

inline constexpr auto flatten = []<typename T>(T&& iterable)
    requires iterable_c<decltype(iterable)>
{ return ok::iter(stdc::forward<T>(iterable)).flatten(); };

inline constexpr auto drop = []<typename T>(T&& iterable, size_t num_to_drop)
    requires iterable_c<decltype(iterable)>
{ return ok::iter(stdc::forward<T>(iterable)).drop(num_to_drop); };

inline constexpr auto take_at_most =
    []<typename T>(T&& iterable, size_t max_num_to_take)
    requires iterable_c<decltype(iterable)>
{ return ok::iter(stdc::forward<T>(iterable)).take_at_most(max_num_to_take); };

inline constexpr auto keep_if = []<typename T, typename predicate_t>(
                                    T&& iterable, const predicate_t& predicate)
    requires iterable_c<decltype(iterable)> &&
             adaptor::keep_if_predicate_c<predicate_t,
                                          iterator_for<decltype(iterable)>>
{ return ok::iter(stdc::forward<T>(iterable)).keep_if(predicate); };

namespace detail {
struct size_fn_t
{
    template <typename T, size_t N>
    [[nodiscard]] constexpr size_t operator()(T (&array)[N]) const noexcept
    {
        return N;
    }

    template <typename T>
        requires stdlib_arraylike_container_c<stdc::remove_cvref_t<T>>
    [[nodiscard]] constexpr size_t operator()(const T& arraylike) const noexcept
    {
        return arraylike.size();
    }

    // NOTE: use of iterable_c here means that this might select stuff also used
    // by other overloads, so we have to explicitly opt out of those (like
    // !stdlib_arraylike_container_c, etc)
    template <typename T>
        requires(iterable_c<const T&> && !iterator_c<T> &&
                 sized_iterator_c<iterator_for<const T&>> &&
                 !stdlib_arraylike_container_c<T>)
    [[nodiscard]] constexpr size_t operator()(const T& iterator) const noexcept
    {
        return ok::iter(iterator).size();
    }

    template <typename T>
        requires(iterator_c<T> && sized_iterator_c<T> &&
                 !stdlib_arraylike_container_c<T>)
    [[nodiscard]] constexpr size_t operator()(const T& iterator) const noexcept
    {
        return iterator.size();
    }
};
} // namespace detail

inline constexpr detail::size_fn_t size{};

} // namespace ok

#endif
