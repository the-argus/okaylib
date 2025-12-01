#ifndef __OKAYLIB_RANGES_ITERATOR_H__
#define __OKAYLIB_RANGES_ITERATOR_H__

#include "okay/detail/iterator_concepts.h"
#include "okay/detail/utility.h"
#include "okay/math/ordering.h"
#include "okay/tuple.h"

namespace ok {

/// Class which takes in its derived class as a template argument, and
/// implements common desireable features like STL style forward iteration (for
/// compatibility with standard foreach loops) and view member functions like
/// .enumerate() .reverse() etc
template <typename derived_t> struct iterator_common_impl_t;

template <typename iterable_t, typename cursor_t>
struct ref_iterator_t
    : public iterator_common_impl_t<ref_iterator_t<iterable_t, cursor_t>>
{
    iterable_t& iterable;
    cursor_t cursor;

    constexpr size_t index_impl() const
        requires stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    constexpr ref_iterator_t(iterable_t& i, cursor_t&& begin)
        : iterable(i), cursor(stdc::move(begin))
    {
    }

    using value_type = typename cursor_t::value_type;

    [[nodiscard]] constexpr opt<value_type> next()
    {
        return cursor.next(iterable);
    }
};

template <typename iterable_t, typename cursor_t>
struct owning_iterator_t
    : public iterator_common_impl_t<owning_iterator_t<iterable_t, cursor_t>>
{
    using storage_type = stdc::remove_const_t<iterable_t>;

    storage_type iterable;
    cursor_t cursor;

    constexpr size_t index_impl() const
        requires stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    constexpr owning_iterator_t(storage_type&& i, cursor_t&& begin)
        : iterable(stdc::move(i)), cursor(stdc::move(begin))
    {
    }

    using value_type = typename cursor_t::value_type;

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
    iterable_t& iterable;
    cursor_t cursor;

    static constexpr bool is_infinite = infinite_cursor_c<iterable_t, cursor_t>;

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
    using storage_type = stdc::remove_const_t<iterable_t>;

    storage_type iterable;
    cursor_t cursor;

    static constexpr bool is_infinite = infinite_cursor_c<iterable_t, cursor_t>;

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
    viewed_t iterable;
    const predicate_t& predicate;

    using value_type = typename viewed_t::value_type;

    constexpr keep_if_t(viewed_t&& input, const predicate_t& pred)
        : iterable(stdc::move(input)), predicate(pred)
    {
    }

    constexpr size_t index() const = delete;

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out = iterable.next();

        if (!out)
            return out;

        while (!predicate(out.ref_unchecked())) {
            out.reset();
            opt next = iterable.next();
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
    viewed_t iterable;

    using value_type = typename viewed_t::value_type;

    constexpr reverse_t(viewed_t&& input) : iterable(stdc::move(input))
    {
        const size_t size = iterable.size();
        const size_t current = iterable.index();
        const size_t desired = size - current - 1;
        iterable.offset(int64_t(desired) - int64_t(current));
    }

    [[nodiscard]] constexpr value_type access() { return iterable.access(); }

    constexpr void offset(int64_t offset) { return iterable.offset(-offset); }

    [[nodiscard]] constexpr size_t size() const
        requires sized_iterator_c<viewed_t>
    {
        return iterable.size();
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterable.size() - iterable.index() - 1;
    }
};

template <iterator_c viewed_t>
struct drop_t : public iterator_common_impl_t<drop_t<viewed_t>>
{
    viewed_t iterable;
    size_t skips_remaining;

    using value_type = typename viewed_t::value_type;

    constexpr drop_t(viewed_t&& input, size_t skips)
        : iterable(stdc::move(input)), skips_remaining(skips)
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
            out = iterable.next();
            if (!out)
                return out;
            --skips_remaining;
        }

        __ok_internal_assert(out);

        return out;
    }
};

template <iterator_c viewed_t> struct enumerate_t;

template <iterator_c viewed_t>
    requires(!index_providing_iterator_c<viewed_t>)
struct enumerate_t<viewed_t>
    : public iterator_common_impl_t<enumerate_t<viewed_t>>
{
    viewed_t iterable;
    size_t index = 0;

    using value_type = ok::tuple<typename viewed_t::value_type, size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterable(stdc::move(input)) {}

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        opt<typename viewed_t::value_type> opt = iterable.next();
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
    viewed_t iterable;

    using value_type = tuple<typename viewed_t::value_type, size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterable(stdc::move(input)) {}

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        size_t idx = iterable.index();
        opt<typename viewed_t::value_type> opt = iterable.next();
        if (!opt)
            return out;
        out.emplace(opt.ref_or_panic(), idx);
        return out;
    }
};

template <iterator_c viewed_t, typename predicate_t>
struct transform_t
    : public iterator_common_impl_t<transform_t<viewed_t, predicate_t>>
{
    viewed_t iterable;
    const predicate_t& predicate;

    using value_type = decltype(predicate(
        stdc::declval<
            stdc::add_lvalue_reference_t<typename viewed_t::value_type>>()));

    constexpr transform_t(viewed_t&& input, const predicate_t& pred)
        : iterable(stdc::move(input)), predicate(pred)
    {
    }

    [[nodiscard]] constexpr opt<value_type> next_impl()
    {
        opt<value_type> out;
        opt<typename viewed_t::value_type> opt = iterable.next();
        if (!opt)
            return out;
        out.emplace(predicate(opt.ref_unchecked()));
        return out;
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

    template <typename callable_t>
    constexpr void for_each(const callable_t& callable) &&
        requires ok::detail::invocable_c<
            callable_t, decltype(this -> next().ref_unchecked())>
    {
        for (auto&& item : *this) {
            ok::invoke(
                callable,
                stdc::forward<
                    ok::remove_cvref_t<typename derived_t::value_type>>(item));
        }
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
                      "iterable does not implement .next_impl() but is also "
                      "not considered arraylike.");
        ok::opt<typename derived_t::value_type> out;
        auto* derived = static_cast<derived_t*>(this);
        if (this->index() >= derived->size())
            return out;
        out.emplace(derived->access());
        derived->offset(1);
        return out;
    }
};

} // namespace ok

#endif
