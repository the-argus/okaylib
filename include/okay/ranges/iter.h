#ifndef __OKAYLIB_RANGES_ITER_H__
#define __OKAYLIB_RANGES_ITER_H__

#include "okay/detail/type_traits.h"
#include "okay/math/ordering.h"
#include "okay/opt.h"
#include "okay/tuple.h"

template <typename T>
concept iterable_c = requires(T& nonconst) {
    { nonconst.next() } -> ok::same_as_c<ok::opt<typename T::value_type>>;
};

template <typename T>
concept iter_sized_c = requires(const T& const_t) {
    { const_t.size() } -> ok::same_as_c<size_t>;
};

template <typename T, typename corresponding_iterable_t>
concept cursor_sized_c =
    requires(const T& const_t, const corresponding_iterable_t& iterable) {
        { const_t.size(iterable) } -> ok::same_as_c<size_t>;
    };

template <typename T>
concept iter_infinite_c = requires { requires T::is_infinite; };

template <typename T, typename corresponding_iterable_t>
concept cursor_infinite_c = requires { requires T::is_infinite; };

template <typename T, typename corresponding_iterable_t>
concept const_accessible_cursor_c =
    requires(const T& cursor, const corresponding_iterable_t& iterable) {
        { cursor.access(iterable) } -> ok::same_as_c<typename T::value_type>;
    };

template <typename T, typename corresponding_iterable_t>
concept nonconst_accessible_cursor_c =
    requires(const T& cursor, corresponding_iterable_t& iterable) {
        { cursor.access(iterable) } -> ok::same_as_c<typename T::value_type>;
    };

template <typename T, typename corresponding_iterable_t>
concept arraylike_cursor_c =
    requires(const T& cursor, T& nonconst_cursor,
             corresponding_iterable_t& nonconst_iterable,
             const corresponding_iterable_t& const_iterable, int64_t offset) {
        { cursor.index(const_iterable) } -> ok::same_as_c<size_t>;
        { nonconst_cursor.offset(const_iterable, offset) } -> ok::is_void_c;
        requires(const_accessible_cursor_c<T, corresponding_iterable_t> ||
                 nonconst_accessible_cursor_c<T, corresponding_iterable_t>);
        requires(cursor_sized_c<T, corresponding_iterable_t> ||
                 cursor_infinite_c<T, corresponding_iterable_t>);
    };

template <typename T>
concept arraylike_iterable_c =
    requires(T& nonconst, const T& const_t, int64_t offset) {
        { const_t.index() } -> ok::same_as_c<size_t>;
        { nonconst.offset(offset) } -> ok::is_void_c;
        { nonconst.access() } -> ok::same_as_c<typename T::value_type>;
        requires(iter_sized_c<T> || iter_infinite_c<T>);
    };

template <typename iterable_t>
concept index_provider_c = requires(const iterable_t& iterable) {
    { iterable.index() } -> ok::same_as_c<size_t>;
};

template <typename derived_t> struct complete_iter_t;

template <typename iterable_t, typename cursor_t>
struct iter_t : public complete_iter_t<iter_t<iterable_t, cursor_t>>
{
    iterable_t& iterable;
    cursor_t cursor;

    constexpr size_t index_impl() const
        requires ok::stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    constexpr iter_t(iterable_t& i, cursor_t&& begin)
        : iterable(i), cursor(ok::stdc::move(begin))
    {
    }

    using value_type = typename cursor_t::value_type;

    [[nodiscard]] constexpr ok::opt<value_type> next()
    {
        return cursor.next(iterable);
    }
};

template <typename iterable_t, typename cursor_t>
struct owning_iter_t
    : public complete_iter_t<owning_iter_t<iterable_t, cursor_t>>
{
    using storage_type = ok::stdc::remove_const_t<iterable_t>;

    storage_type iterable;
    cursor_t cursor;

    constexpr size_t index_impl() const
        requires ok::stdc::is_convertible_v<cursor_t, size_t>
    {
        return cursor;
    }

    constexpr owning_iter_t(storage_type&& i, cursor_t&& begin)
        : iterable(ok::stdc::move(i)), cursor(ok::stdc::move(begin))
    {
    }

    using value_type = typename cursor_t::value_type;

    [[nodiscard]] constexpr ok::opt<value_type> next()
    {
        if constexpr (ok::stdc::is_const_c<iterable_t>) {
            return cursor.next(static_cast<const storage_type&>(iterable));
        } else {
            return cursor.next(iterable);
        }
    }
};

template <typename iterable_t, arraylike_cursor_c<iterable_t> cursor_t>
struct arraylike_iter_t
    : public complete_iter_t<arraylike_iter_t<iterable_t, cursor_t>>
{
    iterable_t& iterable;
    cursor_t cursor;

    static constexpr bool is_infinite = cursor_infinite_c<iterable_t, cursor_t>;

    constexpr arraylike_iter_t(iterable_t& i, cursor_t&& begin)
        : iterable(i), cursor(ok::stdc::move(begin))
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
            static_cast<const ok::stdc::remove_cvref_t<iterable_t>&>(iterable),
            offset);
    }

    [[nodiscard]] constexpr size_t size() const
        requires cursor_sized_c<cursor_t, iterable_t>
    {
        return cursor.size(iterable);
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return cursor.index(iterable);
    }
};

template <typename iterable_t, arraylike_cursor_c<iterable_t> cursor_t>
struct owning_arraylike_iter_t
    : public complete_iter_t<owning_arraylike_iter_t<iterable_t, cursor_t>>
{
    using storage_type = ok::stdc::remove_const_t<iterable_t>;

    storage_type iterable;
    cursor_t cursor;

    static constexpr bool is_infinite = cursor_infinite_c<iterable_t, cursor_t>;

    constexpr owning_arraylike_iter_t(storage_type&& i, cursor_t&& begin)
        : iterable(ok::stdc::move(i)), cursor(ok::stdc::move(begin))
    {
    }

    using value_type = decltype(cursor.access(iterable));

    [[nodiscard]] constexpr value_type access()
    {
        if constexpr (ok::stdc::is_const_c<iterable_t>) {
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
        requires cursor_sized_c<cursor_t, iterable_t>
    {
        return cursor.size(iterable);
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return cursor.index(iterable);
    }
};

template <iterable_c viewed_t, typename predicate_t>
struct keep_if_t : public complete_iter_t<keep_if_t<viewed_t, predicate_t>>
{
    viewed_t iterable;
    const predicate_t& predicate;

    using value_type = typename viewed_t::value_type;

    constexpr keep_if_t(viewed_t&& input, const predicate_t& pred)
        : iterable(ok::stdc::move(input)), predicate(pred)
    {
    }

    constexpr size_t index() const = delete;

    [[nodiscard]] constexpr ok::opt<value_type> next_impl()
    {
        ok::opt<value_type> out = iterable.next();

        if (!out)
            return out;

        while (!predicate(out.ref_unchecked())) {
            out.reset();
            ok::opt next = iterable.next();
            if (!next)
                return out;
            out.emplace(next.ref_unchecked());
        }

        return out;
    }
};

template <arraylike_iterable_c viewed_t>
    requires iter_sized_c<viewed_t>
struct reverse_t : public complete_iter_t<reverse_t<viewed_t>>
{
    viewed_t iterable;

    using value_type = typename viewed_t::value_type;

    constexpr reverse_t(viewed_t&& input) : iterable(ok::stdc::move(input))
    {
        const size_t size = iterable.size();
        const size_t current = iterable.index();
        const size_t desired = size - current - 1;
        iterable.offset(int64_t(desired) - int64_t(current));
    }

    [[nodiscard]] constexpr value_type access() { return iterable.access(); }

    constexpr void offset(int64_t offset) { return iterable.offset(-offset); }

    [[nodiscard]] constexpr size_t size() const
        requires iter_sized_c<viewed_t>
    {
        return iterable.size();
    }

    // complete_iter_t will provide regular .index() for us
    [[nodiscard]] constexpr size_t index_impl() const
    {
        return iterable.size() - iterable.index() - 1;
    }
};

template <iterable_c viewed_t>
struct drop_t : public complete_iter_t<drop_t<viewed_t>>
{
    viewed_t iterable;
    size_t skips_remaining;

    using value_type = typename viewed_t::value_type;

    constexpr drop_t(viewed_t&& input, size_t skips)
        : iterable(ok::stdc::move(input)), skips_remaining(skips)
    {
    }

    [[nodiscard]] constexpr ok::opt<value_type> next_impl()
    {
        // add one to skips remaining so we can use the same logic for skipping
        // as for getting a value with no skips
        skips_remaining = ok::max(1UL, skips_remaining);
        ok::opt<value_type> out;

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

template <iterable_c viewed_t> struct enumerate_t;

template <iterable_c viewed_t>
    requires(!index_provider_c<viewed_t>)
struct enumerate_t<viewed_t> : public complete_iter_t<enumerate_t<viewed_t>>
{
    viewed_t iterable;
    size_t index = 0;

    using value_type = ok::tuple<typename viewed_t::value_type, size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterable(ok::stdc::move(input)) {}

    [[nodiscard]] constexpr ok::opt<value_type> next_impl()
    {
        ok::opt<value_type> out;
        ok::opt<typename viewed_t::value_type> opt = iterable.next();
        if (!opt)
            return out;
        out.emplace(opt.ref_or_panic(), index);
        ++index;
        return out;
    }
};

template <iterable_c viewed_t>
    requires index_provider_c<viewed_t>
struct enumerate_t<viewed_t> : public complete_iter_t<enumerate_t<viewed_t>>
{
    viewed_t iterable;

    using value_type = ok::tuple<typename viewed_t::value_type, size_t>;

    constexpr enumerate_t(viewed_t&& input) : iterable(ok::stdc::move(input)) {}

    [[nodiscard]] constexpr ok::opt<value_type> next_impl()
    {
        ok::opt<value_type> out;
        size_t idx = iterable.index();
        ok::opt<typename viewed_t::value_type> opt = iterable.next();
        if (!opt)
            return out;
        out.emplace(opt.ref_or_panic(), idx);
        return out;
    }
};

template <iterable_c viewed_t, typename predicate_t>
struct transform_t : public complete_iter_t<transform_t<viewed_t, predicate_t>>
{
    viewed_t iterable;
    const predicate_t& predicate;

    using value_type =
        decltype(predicate(ok::stdc::declval<ok::stdc::add_lvalue_reference_t<
                               typename viewed_t::value_type>>()));

    constexpr transform_t(viewed_t&& input, const predicate_t& pred)
        : iterable(ok::stdc::move(input)), predicate(pred)
    {
    }

    [[nodiscard]] constexpr ok::opt<value_type> next_impl()
    {
        ok::opt<value_type> out;
        ok::opt<typename viewed_t::value_type> opt = iterable.next();
        if (!opt)
            return out;
        out.emplace(predicate(opt.ref_unchecked()));
        return out;
    }
};

template <typename T>
concept iter_impls_next = requires(T& d) {
    { d.next_impl() } -> ok::same_as_c<ok::opt<typename T::value_type>>;
};

template <typename derived_t> struct complete_iter_t
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
        return keep_if_t<derived_t, predicate_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), predicate);
    }

    [[nodiscard]] constexpr auto drop(size_t num_to_drop) &&
    {
        return drop_t<derived_t>(ok::stdc::move(*static_cast<derived_t*>(this)),
                                 num_to_drop);
    }

    [[nodiscard]] constexpr auto enumerate() &&
    {
        return enumerate_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    [[nodiscard]] constexpr auto reverse() &&
        requires arraylike_iterable_c<derived_t> && iter_sized_c<derived_t>
    {
        return reverse_t<derived_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)));
    }

    template <typename predicate_t>
    [[nodiscard]] constexpr auto transform(const predicate_t& predicate) &&
    {
        return transform_t<derived_t, predicate_t>(
            ok::stdc::move(*static_cast<derived_t*>(this)), predicate);
    }

    [[nodiscard]] constexpr auto next()
        requires iter_impls_next<derived_t>
    {
        return static_cast<derived_t*>(this)->next_impl();
    }

    [[nodiscard]] constexpr auto next()
        requires(!iter_impls_next<derived_t>)
    {
        static_assert(arraylike_iterable_c<derived_t>,
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

struct myiterable_t
{
    template <typename value_type_t, bool forward = true> struct cursor_t
    {
        using value_type = value_type_t&;

        size_t index;

        cursor_t() = delete;

        constexpr cursor_t(const myiterable_t&)
            requires forward
            : index(0)
        {
        }

        constexpr operator size_t() const { return index; }

        constexpr cursor_t(const myiterable_t& iterable)
            requires(!forward)
            : index(iterable.size())
        {
        }

        constexpr ok::opt<value_type> next(const myiterable_t& iterable)
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
        return iter_t{*this, cursor_t<const int>(*this)};
    }
    constexpr auto iter() &&
    {
        return owning_iter_t<myiterable_t, cursor_t<const int>>{
            std::move(*this), cursor_t<const int>(*this)};
    }
    constexpr auto reverse_iter() const&
    {
        return iter_t{*this, cursor_t<const int, false>(*this)};
    }

    [[nodiscard]] constexpr size_t size() const { return 10; }

    int items[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
};

struct my_arraylike_iterable_t
{
    template <typename value_type_t> struct cursor_t
    {
        using value_type = value_type_t&;

        size_t m_index = 0;

        cursor_t() = default;

        constexpr const value_type_t&
        access(const my_arraylike_iterable_t& iterable) const
        {
            __ok_assert(m_index < iterable.size(),
                        "out of bounds access to arraylike iterable");
            return iterable.items[m_index];
        }

        constexpr value_type_t& access(my_arraylike_iterable_t& iterable) const
            requires(!ok::stdc::is_const_c<value_type_t>)
        {
            __ok_assert(m_index < iterable.size(),
                        "out of bounds access to arraylike iterable");
            return iterable.items[m_index];
        }

        constexpr size_t size(const my_arraylike_iterable_t& iterable) const
        {
            return iterable.size();
        }

        constexpr size_t index(const my_arraylike_iterable_t&) const
        {
            return m_index;
        }

        constexpr void offset(const my_arraylike_iterable_t& iterable,
                              int64_t offset)
        {
            m_index += offset;
        }
    };

    constexpr auto iter_const() const&
    {
        return arraylike_iter_t<const my_arraylike_iterable_t,
                                cursor_t<const int>>{*this, {}};
    }

    constexpr auto iter() &
    {
        return arraylike_iter_t<my_arraylike_iterable_t, cursor_t<int>>{*this,
                                                                        {}};
    }

    constexpr auto iter_const() &&
    {
        return owning_arraylike_iter_t<const my_arraylike_iterable_t,
                                       cursor_t<const int>>{
            ok::stdc::move(*this), {}};
    }

    constexpr auto iter() &&
    {
        return owning_arraylike_iter_t<my_arraylike_iterable_t, cursor_t<int>>{
            ok::stdc::move(*this), {}};
    }

    [[nodiscard]] constexpr size_t size() const { return 10; }

    int items[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
};

#endif
