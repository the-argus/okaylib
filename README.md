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
- [ ] context contains an "allocator allocator" so you can refer to allocators in
      a serializable way (not by pointer)?
- [ ] standardized SIMD vector and matrix types, explicit by default but with
      optional operator overloading. inspired by DirectXMath
- [ ] iterator reimplementation (and maybe some redesign?)
- [ ] sane `std::string` replacement, inspired a bit by Godot's `String` and
      maybe `StringName` as well if that can be pulled off.
- [ ] A low friction variant which is something like `std::variant<int, float, string>`
- [ ] A fast hashmap, maybe one of the flat hash sets / maps from Google, with
      support for emplace_back which can error by value
- [ ] A `std::vector` replacement with a better name (`ok::arraylist`?) which
      does not throw and supports emplace_back erroring by value. can yield its
      contents with some `slice<T> release()` function
- [x] A collection whose items can be accessed by a stable handle, instead of
      index, but keeps item in contiguous memory for fast iteration. Includes
      generation information in handle for lock and key type memory saftey and
      debugging.
- [ ] An array type which does not store its elements contiguously but rather in
      roughly cache-line-sized blocks, then has an array of pointers to blocks.
      constant time lookup and less memory fragementation
- [ ] `std::ranges` reimplementation, with some new views. sliding window,
      jumping window (grouper?), lattice, bit_lattice (similar to sliding
      window, pass in 0b10101 to get the next three alternating items),
      enumerate, zip, into, take, drop, skip_if... support for allocators for
      stuff like into. explicit when something turns random access -> forward
      iterator
- [ ] fold/reduce function compatible with above views
- [ ] reimplementation of `stable_sort`, `sort`, `copy_if`, `copy`, `find`,
      `find_if`, potentially avoidng the need for `std::begin()` and `std::end()`
      as two arguments?
- [ ] reimplementation of `<thread>` and `<atomic>` ? avoid exceptions where
      possible, and implicit atomic load/stores
- [ ] standard coroutine types: task, generator
- [ ] coroutines which can use thread's context allocator
- [ ] coroutine-running threadpool with work queues and task stealing, for
      copying off Go's homework. (potentially put threadpool into context for
      submitting coroutines upon construction?)
- [ ] fmtlib included for IO, all types mentioned above include formatters
- [ ] all okaylib types have nlohmann json serialization defined
