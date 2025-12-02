#ifndef __OKAYLIB_DETAIL_RANGE_CONCEPTS_H__
#define __OKAYLIB_DETAIL_RANGE_CONCEPTS_H__

#include "okay/detail/type_traits.h"
#include "okay/detail/utility.h"
#include <cstdint>

/*
 * Iterators are used to iterate on a series of values in a generic way.
 * In okaylib, there are:
 *
 * - Iterables -> things that produce iterators via a .iter() function
 * - Cursors -> object which implements iteration
 * - Iterators -> thing with a .next() function that iterates over an iterable
 * - Iterator adaptors -> thing that wraps an iterator to become another
 *   iterator with different properties
 *
 * An iterable is something which has a function `.iter()` which produces an
 * iterator. That iterator will either have a reference to or internally own
 * the iterable. `.iter()` is the default but most library functions should
 * provide an overload accepting an arbitrary callable to apply to the iterable.
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
concept iterator_c = requires(stdc::remove_cvref_t<T>& nonconst) {
    {
        nonconst.next()
    } -> ok::same_as_c<ok::opt<typename stdc::remove_cvref_t<T>::value_type>>;
};

namespace detail {
template <typename T>
concept iterator_impl_c = requires(stdc::remove_cvref_t<T>& d) {
    {
        d.next_impl()
    } -> ok::same_as_c<ok::opt<typename stdc::remove_cvref_t<T>::value_type>>;
};
} // namespace detail

template <typename T>
concept sized_iterator_c = requires(const stdc::remove_cvref_t<T>& const_t) {
    // not depending on this here so that arraylike iterators (which may not
    // also satisfy iterator_c, at least before resolving CRTP inheritance with
    // common iterator implementation) can still satisfy this
    //
    // requires iterator_c<T>;
    { const_t.size() } -> ok::same_as_c<size_t>;
};

template <typename T>
concept infinite_iterator_c = requires {
    // not depending on this here so that arraylike iterators (which may not
    // also satisfy iterator_c, at least before resolving CRTP inheritance with
    // common iterator implementation) can still satisfy this
    //
    // requires iterator_c<T>;
    requires T::is_infinite;
};

template <typename T, typename cv_qualified_corresponding_iterable_t>
concept sized_cursor_c =
    requires(const stdc::remove_cvref_t<T>& const_t,
             cv_qualified_corresponding_iterable_t& iterable) {
        { const_t.size(iterable) } -> ok::same_as_c<size_t>;
    };

template <typename T, typename cv_qualified_corresponding_iterable_t>
concept infinite_cursor_c =
    requires { requires stdc::remove_cvref_t<T>::is_infinite; };

template <typename T, typename cv_qualified_corresponding_iterable_t>
concept arraylike_cursor_c =
    requires(const stdc::remove_cvref_t<T>& cursor,
             stdc::remove_cvref_t<T>& nonconst_cursor,
             cv_qualified_corresponding_iterable_t& iterable, int64_t offset) {
        // can get the index of the cursor
        { cursor.index(iterable) } -> ok::same_as_c<size_t>;
        // can offset the cursor by an int64_t
        { nonconst_cursor.offset(iterable, offset) } -> ok::is_void_c;
        // can access the cursor with the iterable
        {
            cursor.access(iterable)
        } -> ok::same_as_c<typename stdc::remove_cvref_t<T>::value_type>;
        // it is either infinite or knows its size in constant time
        requires(sized_cursor_c<T, cv_qualified_corresponding_iterable_t> ||
                 infinite_cursor_c<T, cv_qualified_corresponding_iterable_t>);
    };

template <typename T>
concept arraylike_iterator_c =
    requires(stdc::remove_cvref_t<T>& nonconst,
             const stdc::remove_cvref_t<T>& const_t, int64_t offset) {
        { const_t.index() } -> ok::same_as_c<size_t>;
        { nonconst.offset(offset) } -> ok::is_void_c;
        // access on the iterator itself should always be nonconst, as the
        // iterator is a mutable object created by the .iter() and consumed
        // during iteration
        {
            nonconst.access()
        } -> ok::same_as_c<typename stdc::remove_cvref_t<T>::value_type>;
        requires(sized_iterator_c<T> || infinite_iterator_c<T>);
    };

template <typename T>
concept index_providing_iterator_c =
    requires(const stdc::remove_cvref_t<T>& iterator) {
        requires iterator_c<T> || arraylike_iterator_c<T>;
        { iterator.index() } -> ok::same_as_c<size_t>;
    };
} // namespace ok

#endif
