#ifndef __OKAYLIB_RANGES_ITERATOR_H__
#define __OKAYLIB_RANGES_ITERATOR_H__

#include "okay/detail/iterator_concepts.h"
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

template <typename iterable_t, typename cursor_t>
struct ref_iterator_t
    : public iterator_common_impl_t<ref_iterator_t<iterable_t, cursor_t>>
{
  private:
    iterable_t& iterable;
    cursor_t cursor;

  public:
    static constexpr inline bool is_infinite =
        infinite_cursor_c<cursor_t, iterable_t>;

    constexpr ref_iterator_t(iterable_t& i, cursor_t&& begin)
        : iterable(i), cursor(stdc::move(begin))
    {
    }

    using value_type = typename cursor_t::value_type;

    constexpr size_t index_impl() const
        requires stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    [[nodiscard]] constexpr opt<value_type> next()
    {
        return cursor.next(iterable);
    }
};

template <typename iterable_t, typename cursor_t>
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
    iterable_t& iterable;
    cursor_t cursor;

  public:
    static constexpr inline bool is_infinite =
        infinite_cursor_c<cursor_t, iterable_t>;

    constexpr ref_arraylike_iterator_t(iterable_t& i, cursor_t&& begin)
        : iterable(i), cursor(stdc::move(begin))
    {
    }

    using value_type = decltype(cursor.access(iterable));

    [[nodiscard]] constexpr value_type access()
    {
        return cursor.access(iterable);
    }

    constexpr void offset(int64_t offset)
    {
        return cursor.offset(
            static_cast<const stdc::remove_cvref_t<iterable_t>&>(iterable),
            offset);
    }

    [[nodiscard]] constexpr size_t size() const
        requires sized_cursor_c<cursor_t, iterable_t>
    {
        return cursor.size(iterable);
    }

    // iterator_common_impl_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return cursor.index(iterable);
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
        requires sized_cursor_c<cursor_t, iterable_t>
    {
        return cursor.size(iterable);
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return cursor.index(iterable);
    }
};

namespace adaptor {
template <iterator_c viewed_t, typename predicate_t>
struct keep_if_t
    : public iterator_common_impl_t<keep_if_t<viewed_t, predicate_t>>
{
    viewed_t iterator;
    const predicate_t& predicate;

    using value_type = typename viewed_t::value_type;

    constexpr keep_if_t(viewed_t&& input, const predicate_t& pred)
        : iterator(stdc::move(input)), predicate(pred)
    {
    }

    constexpr size_t index() const = delete;

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out = iterator.next();

        if (!out)
            return out;

        while (!predicate(out.ref_unchecked())) {
            out.reset();
            opt next = iterator.next();
            if (!next)
                return out;
            out.emplace(next.ref_unchecked());
        }

        return out;
    }
};

template <arraylike_iterator_c viewed_t>
    requires sized_iterator_c<viewed_t>
struct reverse_t : public iterator_common_impl_t<reverse_t<viewed_t>>
{
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
        requires sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterator.size() - iterator.index() - 1;
    }
};
template <typename viewed_t> struct drop_t;

template <iterator_c viewed_t>
struct drop_t<viewed_t> : public iterator_common_impl_t<drop_t<viewed_t>>
{
    viewed_t iterator;
    size_t skips_remaining;

    using value_type = typename viewed_t::value_type;

    constexpr drop_t(viewed_t&& input, size_t skips)
        : iterator(stdc::move(input)), skips_remaining(skips)
    {
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
            if (!out)
                return out;
            --skips_remaining;
        }

        __ok_internal_assert(out);

        return out;
    }
};

template <arraylike_iterator_c viewed_t>
struct drop_t<viewed_t> : public iterator_common_impl_t<drop_t<viewed_t>>
{
  private:
    viewed_t iterator;
    const size_t skips;

  public:
    using value_type = typename viewed_t::value_type;

    static constexpr bool is_infinite = infinite_iterator_c<viewed_t>;

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
        requires sized_iterator_c<viewed_t>
    {
        const size_t size = iterator.size();
        return ok::max(size, skips) - skips;
    }

    [[nodiscard]] constexpr size_t index() const
    {
        const size_t index = iterator.index();
        __ok_internal_assert(index >= skips);
        return index - skips;
    }

    [[nodiscard]] constexpr auto access() const { return iterator.access(); }
    [[nodiscard]] constexpr auto access() { return iterator.access(); }

    constexpr void offset(int64_t offset_amount) const
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

template <iterator_c viewed_t> struct enumerate_t;

template <iterator_c viewed_t>
    requires(!index_providing_iterator_c<viewed_t>)
struct enumerate_t<viewed_t>
    : public iterator_common_impl_t<enumerate_t<viewed_t>>
{
    viewed_t iterator;
    size_t index = 0;

    using value_type = ok::tuple<typename viewed_t::value_type, size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterator(stdc::move(input)) {}

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
    requires index_providing_iterator_c<viewed_t>
struct enumerate_t<viewed_t>
    : public iterator_common_impl_t<enumerate_t<viewed_t>>
{
    viewed_t iterator;

    using value_type = tuple<typename viewed_t::value_type, size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterator(stdc::move(input)) {}

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        size_t idx = iterator.index();
        opt<typename viewed_t::value_type> opt = iterator.next();
        if (!opt)
            return out;
        out.emplace(opt.ref_or_panic(), idx);
        return out;
    }
};

template <typename... viewed_t> struct zip_t;

template <typename... viewed_t>
    requires((iterator_c<viewed_t> && ...) &&
             !(arraylike_iterator_c<viewed_t> && ...) &&
             sizeof...(viewed_t) > 1)
struct zip_t<viewed_t...> : public iterator_common_impl_t<zip_t<viewed_t...>>
{
  private:
    ok::tuple<viewed_t...> m_iterators;

  public:
    using value_type = ok::tuple<typename viewed_t::value_type...>;

    constexpr zip_t(stdc::add_rvalue_reference_t<viewed_t>... iterators)
        : m_iterators(stdc::move(iterators)...)
    {
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

            if constexpr ((index_providing_iterator_c<inner_viewed_t> && ...)) {
                __ok_assert((ok::get<indices>(iterators).index() == ...),
                            "Some of the iterators in the zip view are not at "
                            "the same index");
            }

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
  private:
    ok::tuple<viewed_t...> m_iterators;
    size_t m_size;

    // performs some funny lambda fold trickery to find the minimum value
    // returned by size() for all the iterators, unless they are all infinite in
    // which case nullopt is returned
    constexpr ok::opt<size_t> size_impl()
    {
        const auto with_indices =
            [&]<size_t... indices>(
                ok::stdc::index_sequence<indices...>) -> ok::opt<size_t> {
            ok::opt<size_t> out;
            (
                [&] {
                    if constexpr (sized_iterator_c<
                                      detail::nth_type<indices, viewed_t...>>) {
                        const auto size = ok::get<indices>();
                        if (!out || out.ref_unchecked() > size)
                            out = size;
                    }
                }(),
                ...);
            return out;
        };

        return with_indices(
            ok::stdc::make_index_sequence<sizeof...(viewed_t)>());
    }

  public:
    using value_type = ok::tuple<typename viewed_t::value_type...>;

    inline static constexpr bool is_infinite =
        (infinite_iterator_c<viewed_t> && ...);

    constexpr zip_t(stdc::add_rvalue_reference_t<viewed_t>... iterators)
        : m_iterators(stdc::move(iterators)...),
          m_size(size_impl().copy_out_or(-1))
    {
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

    constexpr value_type access() const
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

template <typename viewed_t, typename predicate_t> struct transform_t;

template <iterator_c viewed_t, typename predicate_t>
    requires(!arraylike_iterator_c<viewed_t>)
struct transform_t<viewed_t, predicate_t>
    : public iterator_common_impl_t<transform_t<viewed_t, predicate_t>>
{
    viewed_t iterator;
    const predicate_t& predicate;

    using value_type = decltype(predicate(
        stdc::declval<
            stdc::add_lvalue_reference_t<typename viewed_t::value_type>>()));

    constexpr transform_t(viewed_t&& input, const predicate_t& pred)
        : iterator(stdc::move(input)), predicate(pred)
    {
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        opt<typename viewed_t::value_type> opt = iterator.next();
        if (!opt)
            return out;
        out.emplace(predicate(opt.ref_unchecked()));
        return out;
    }
};

template <arraylike_iterator_c viewed_t, typename predicate_t>
struct transform_t<viewed_t, predicate_t>
    : public iterator_common_impl_t<transform_t<viewed_t, predicate_t>>
{
    viewed_t iterator;
    const predicate_t& predicate;

    using value_type = decltype(predicate(
        stdc::declval<
            stdc::add_lvalue_reference_t<typename viewed_t::value_type>>()));

    constexpr transform_t(viewed_t&& input, const predicate_t& pred)
        : iterator(stdc::move(input)), predicate(pred)
    {
    }

    [[nodiscard]] constexpr size_t index() const { return iterator.index(); }

    [[nodiscard]] constexpr size_t size() const
        requires sized_iterator_c<viewed_t>
    {
        return iterator.size();
    }

    constexpr value_type access() const
    {
        auto&& item = iterator.access();
        return predicate(stdc::forward<typename viewed_t::value_type>(item));
    }

    constexpr value_type access()
    {
        auto&& item = iterator.access();
        return predicate(stdc::forward<typename viewed_t::value_type>(item));
    }
};
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
    {
        return adaptor::keep_if_t<derived_t, predicate_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), predicate);
    }

    [[nodiscard]] constexpr auto drop(size_t num_to_drop) &&
    {
        return adaptor::drop_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), num_to_drop);
    }

    [[nodiscard]] constexpr auto enumerate() &&
    {
        return adaptor::enumerate_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    [[nodiscard]] constexpr auto reverse() &&
        requires arraylike_iterator_c<derived_t> && sized_iterator_c<derived_t>
    {
        return adaptor::reverse_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    template <typename predicate_t>
    [[nodiscard]] constexpr auto transform(const predicate_t& predicate) &&
    {
        return adaptor::transform_t<derived_t, predicate_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), predicate);
    }

    template <typename first_other_iterator_t, typename... extra_iterators_t>
        requires(iterator_c<stdc::remove_cvref_t<first_other_iterator_t>> &&
                 (iterator_c<stdc::remove_cvref_t<extra_iterators_t>> && ...))
    [[nodiscard]] constexpr auto zip(first_other_iterator_t&& other,
                                     extra_iterators_t&&... extras) &&
    {
        return adaptor::zip_t<derived_t,
                              stdc::remove_cvref_t<first_other_iterator_t>,
                              stdc::remove_cvref_t<extra_iterators_t>...>(
            stdc::move(*static_cast<derived_t*>(this)), stdc::move(other),
            stdc::move(extras)...);
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
        requires predicate_c<
            predicate_t,
            decltype(static_cast<derived_t*>(this) -> next().ref_unchecked())>
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
        requires arraylike_iterator_c<derived_t>
    {
        ok::opt<typename derived_t::value_type> out;
        auto* derived = static_cast<derived_t*>(this);

        if constexpr (sized_iterator_c<derived_t>) {
            if (this->index() >= derived->size())
                return out;
        }
        out.emplace(derived->access());
        derived->offset(1);
        return out;
    }
};

namespace detail {
/// Class which wraps a std::vector, std::span, or std::array to make it
/// iterable.
template <typename container_t>
class stdlib_arraylike_container_iterable_compat_t
{
    using storage_type =
        stdc::conditional_t<stdc::is_rvalue_reference_v<container_t>,
                            stdc::remove_reference_t<container_t>, container_t>;

    storage_type m_container;

    template <bool is_const> struct cursor_t;

    template <bool is_const> friend struct cursor_t;

    template <bool is_const> struct cursor_t
    {
      private:
        using noref_container_t = ok::remove_cvref_t<container_t>;
        size_t m_index = 0;

      public:
        constexpr cursor_t() = default;

        using value_type = typename noref_container_t::value_type&;

        constexpr size_t
        index(const stdlib_arraylike_container_iterable_compat_t&) const
        {
            return m_index;
        }
        constexpr size_t
        size(const stdlib_arraylike_container_iterable_compat_t& iterable) const
        {
            return iterable.m_container.size();
        }
        constexpr void
        offset(const stdlib_arraylike_container_iterable_compat_t&,
               int64_t offset_amount)
        {
            m_index += offset_amount;
        }

        constexpr typename noref_container_t::value_type& access(
            const stdlib_arraylike_container_iterable_compat_t& iterable) const
            requires(!is_const)
        {
            __ok_assert(m_index < size(iterable),
                        "out of bounds iteration into stdlib style container");
            return iterable.m_container[m_index];
        }

        constexpr const typename noref_container_t::value_type& access(
            const stdlib_arraylike_container_iterable_compat_t& iterable) const
        {
            __ok_assert(m_index < size(iterable),
                        "out of bounds iteration into stdlib style container");
            return iterable.m_container[m_index];
        }
    };

  public:
    constexpr stdlib_arraylike_container_iterable_compat_t(
        container_t container)
        : m_container(
              stdc::forward<ok::stdc::remove_cvref_t<container_t>>(container))
    {
    }

    [[nodiscard]] constexpr auto iter() &&
    {
        return ok::owning_arraylike_iterator_t<
            stdlib_arraylike_container_iterable_compat_t, cursor_t<true>>{
            stdc::move(*this), {}};
    }
};

template <typename c_array_value_type> class c_array_iterable_compat_t
{
    c_array_value_type* m_array_start;
    const size_t m_size;

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
        access(const c_array_iterable_compat_t& iterable) const
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
concept stdlib_arraylike_container_c =
    requires(T& container_nonconst, const T& container_const, size_t index) {
        { container_const.size() } -> ok::same_as_c<size_t>;
        {
            container_const[index]
        } -> ok::same_as_c<const typename T::value_type&>;
        { container_nonconst[index] } -> ok::same_as_c<typename T::value_type&>;
    };

struct make_into_iterator_fn_t
{
    template <typename T, size_t N>
    constexpr auto operator()(T (&array)[N]) const OKAYLIB_NOEXCEPT
    {
        return c_array_iterable_compat_t(array).iter();
    }

    template <stdlib_arraylike_container_c T>
    constexpr auto operator()(T&& arraylike) const OKAYLIB_NOEXCEPT
    {
        return stdlib_arraylike_container_iterable_compat_t<
                   decltype(arraylike)>(std::forward<T>(arraylike))
            .iter();
    }

    template <iterable_c T>
    constexpr auto operator()(T&& iterable) const OKAYLIB_NOEXCEPT
    {
        return stdc::forward<T>(iterable).iter();
    }
};

} // namespace detail

inline constexpr detail::make_into_iterator_fn_t iter{};

} // namespace ok

#endif
