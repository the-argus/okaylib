#ifndef __OKAYLIB_DETAIL_RANGE_CONCEPTS_H__
#define __OKAYLIB_DETAIL_RANGE_CONCEPTS_H__

#include "okay/detail/type_traits.h"

/*
 * Iterators are used to iterate on a series of values in a generic way.
 * In okaylib, there are:
 *
 * - Iterables -> things that produce iterators
 * - Cursors -> object which implements iteration
 * - Iterators -> thing with a .next() function that iterates over an iterable
 * - Iterator adaptors -> thing that wraps an iterator to become another
 *   iterator with different properties
 *
 * An iterable is something which has a function `.iter()` which produces an
 * iterator. That iterator will either have a reference to or internally own
 * the iterable.
 *
 * A "cursor" is a type which the iterable implementor puts the functions that
 * define how to move through and access the iterable, as well as the
 * pointer/index value one would typically call an "iterator" in standard C++.
 * Cursors are different from iterators in that they do not internally store
 * unstable pointers. That means that a cursor may store a pointer to an element
 * in a linked list, where the only way to break that pointer is to remove that
 * item from the linked list. But a cursor may *not* store a pointer to an item
 * in a vector, as there are other ways (reallocation) for that pointer to
 * become invalid. In this way cursors are able to avoid the UB associated with
 * iterator invalidation in dynamic arrays, which is their main advantage over
 * standard iterators.
 * A cursor is not really of concern to anyone except the person implementing
 * the iterable. It will be wrapped by the iterator.
 *
 * An "iterator" in okaylib is a type which internally contains an iterable and
 * a cursor. Iterators are provided by okaylib. They implement functions such as
 * .next(), .size(), .index(), etc, as well as adaptor functions like
 * .enumerate(), .keep_if(), .reverse(), etc. An iterator is templated on the
 * iterable and cursor types.
 *
 */

namespace ok {
template <typename payload_t> class opt;

template <typename T>
concept iterator_c = requires(T& nonconst) {
    { nonconst.next() } -> ok::same_as_c<ok::opt<typename T::value_type>>;
};

template <typename T>
concept sized_iterator_c = requires(const T& const_t) {
    { const_t.size() } -> ok::same_as_c<size_t>;
};

template <typename T>
concept infinite_iterator_c = requires { requires T::is_infinite; };

template <typename cursor_t, typename corresponding_iterable_t>
concept sized_cursor_c = requires(const cursor_t& const_t,
                                  const corresponding_iterable_t& iterable) {
    { const_t.size(iterable) } -> ok::same_as_c<size_t>;
};
template <typename T, typename corresponding_iterable_t>
concept infinite_cursor_c = requires { requires T::is_infinite; };

template <typename cursor_t, typename corresponding_iterable_t>
concept const_accessible_cursor_c =
    requires(const cursor_t& cursor, const corresponding_iterable_t& iterable) {
        { cursor.access(iterable) } -> ok::same_as_c<typename cursor_t::value_type>;
    };

template <typename cursor_t, typename corresponding_iterable_t>
concept nonconst_accessible_cursor_c =
    requires(const cursor_t& cursor, corresponding_iterable_t& iterable) {
        { cursor.access(iterable) } -> ok::same_as_c<typename cursor_t::value_type>;
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
} // namespace ok

#endif
