# okaylib

The C++ STL is pretty bad. But this library... it's okay.

## goals

Replace some of the C++ STL with absolutely no concern for backwards
compatibility or familiarity, using C++20 features. Focus on mulithreading,
don't use exceptions, and everything is `std::pmr`. Improve compile times over
C++ STL. Basically eliminate standard constructors- instead have factory
functions which can fail and return an optional or error type.

And, honestly: something just feels kind of wrong when I use the C++ STL. Not sure
why, but it fires up some motivation in my programmer brain to make something better.
I often find myself reimplementing things that I wish were improved, so this is
an effort to organize that (not always necessary) work into one reusable product.

## Planned features

- [x] implicit `context()` for allocators, random number generators, current error
      message, etc
- [x] polymorphic allocator interface
- [ ] implementations for arena, block, slab, page, and remapping page allocators
- [ ] wrapper / import of jemalloc for the allocator interface
- [x] "result" type: optional with enum error value. like `std::expected`, kind of
- [x] "opt" type: optional but supports assigning in references which effectively
      become pointers. no exceptions
- [x] conditional support for constexpr + trivial traits for opt and result
- [x] slice type: like span but not nullable
- [x] defer statement
- [x] stdmem: functions for checking if slices are overlapping, contained
      within, etc
- [ ] context handle: serializable replacement for a reference which is a unique
      index of an allocation along with a generation / magic value. when dereferencing,
      it asks the context for the corresponding memory and compares magic number
      to try to detect invalid allocator or use-after-free.
- [ ] variants of context handle: explicit handle (dereferencing requires passing
      the allocator) and unique context handle
- [ ] standardized SIMD vector and matrix types, explicit by default but with
      optional operator overloading. inspired by DirectXMath
- [x] new iterators, with lower barrier to entry. c++17 compatible but not backwards
      compatible with algorithms that use legacy iterators or c++20 iterators. Designed
      for easy implementation, good codegen, and immediate rangelike support (type
      with iterator stuff should also be a range)
- [ ] A dynamic bit array and a static bit array with boolean-like iterators, to
      prove capability of new iterators
- [ ] sane `std::string` replacement, inspired a bit by Godot's `String` and
      maybe `StringName` as well if that can be pulled off.
- [ ] A low friction variant which is something like `std::variant<int, float, string>`
- [ ] A fast hashmap, maybe one of the flat hash sets / maps from Google, with
      support for emplace_back which can error by value
- [ ] A `std::vector` replacement with a better name (`ok::arraylist`?) which
      does not throw and supports emplace_back or push_back erroring by value. can
      yield its contents with some `slice<T> release()` function
- [x] A collection whose items can be accessed by a stable handle, instead of
      index, but keeps items in contiguous memory for fast iteration. Includes
      generation information in handle for lock and key type memory saftey and
      debugging.
- [ ] An arraylist type which does not store its elements contiguously but rather
      in roughly cache-line-sized blocks, then has an array of pointers to blocks.
      constant time lookup and less memory fragementation
- [ ] `std::ranges` reimplementation, with some new views. sliding window,
      jumping window (grouper? chunk?), lattice, bit_lattice (similar to sliding
      window, pass in 0b10101 to get the next three alternating items),
      enumerate, zip, into, take, drop, skip_if... support for allocators for
      stuff like into. explicit when something turns random access -> forward
      iterator
- [ ] fold/reduce function(s) compatible with above views
- [ ] reimplementation of `<algorithm>` stuff: `stable_sort`, `sort`, `copy_if`,
      `copy`, `move`, `count`, `count_if` `mismatch` `find`, `starts_with`, `ends_with`,
      `contains`, `fill`, `find_if`, `any_of`, `all_of`, `is_sorted`, `unique`, `shuffle`,
      `rotate`, `reverse`, `swap`, `binary_search`, `equal`, `max_element`, `max`,
      `min`, `min_element`, `minmax_element`, `clamp`, and copying vs. in-place
      variants for all algorithms. This is rangelike though- no need for
      begin() and end() as separate arguments
- [ ] threadpool compatibility for some views which are embarassingly parellel,
      like `count*` or `max_element` or `unique`. Specific threadsafe container
      iterator type?
- [ ] reimplementation of `<thread>` and `<atomic>` ? avoid exceptions where
      possible, and avoid implicit atomic load/stores
- [ ] standard coroutine types: task, generator
- [ ] coroutines which can use thread's context allocator
- [ ] coroutine-running threadpool with work queues and task stealing, for
      copying off Go's homework. (potentially put threadpool into context for
      submitting coroutines upon construction?)
- [ ] fmtlib included for IO, all types mentioned above include formatters
- [ ] all okaylib types have nlohmann json serialization defined
- [ ] context contains an "allocator allocator" so you can refer to allocators in
      a serializable way (not by pointer)?

## TODO

- [ ] Remove dependency on `memory` header due to `std::addressof` if possible?
