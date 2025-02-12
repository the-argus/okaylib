# okaylib

C++17/20 STL replacement for realtime and memory-constrained domains.

## goals

Replace some of the C++17 STL with absolutely no concern for backwards compatibility.
Backport and improve `std::ranges` ranges and range adaptors such as `enumerate`,
`sliding_window`, etc. Provide a variety of containers which all use polymorphic
allocators by default, and error by value instead of using exceptions. Additionally
provide multithreading primitives for C++20 users, such as a thread pool and coroutine
runtime. Optionally make use of C++20 modules for improved compile times without
waiting for C++23 `import std`. Provide serialization to string and JSON for all
types. Do bounds / error checking in both release and debug mode specifically to
detect undefined behavior (with an `OKAYLIB_FAST_UNSAFE` macro to disable it).

okaylib is a personal project which is intended to focus many disparate
efforts of mine to make C and C++ libraries into one mega-project. I have plan to
use it myself in some of my other projects, but any actual releases (along with
support for build systems that people actually use, like CMake) are a ways off.

## examples

This code demonstrates some views of an array: transforming, enumerating,
and reversing. C++17.

```cpp
int main()
{
    int forward[] = {5, 4, 3, 2, 1, 0};

    auto size_minus =
        transform([&](int i) { return ok::size(forward) - 1 - i; });

    fmt::println("Range using size_minus:");
    // std_for view to make okaylib ranges compatible with range based for
    for (auto [value, idx] : forward | size_minus | enumerate | std_for) {
        const char* sep = idx == ok::size(forward) - 1 ? "\n" : " -> ";
        fmt::print("{}: {}{}", value, idx, sep);
    }

    fmt::println("Range using reverse:");
    for (auto [value, idx] : forward | reverse | enumerate | std_for) {
        const char* sep = idx == ok::size(forward) - 1 ? "\n" : " -> ";
        fmt::print("{}: {}{}", value, idx, sep);
    }
}
```

Outputs:

```txt
Range using size_minus:
0: 0 -> 1: 1 -> 2: 2 -> 3: 3 -> 4: 4 -> 5: 5
Range using reverse:
0: 0 -> 1: 1 -> 2: 2 -> 3: 3 -> 4: 4 -> 5: 5
```

This code makes use of a polymorphic allocator, conditional `defer` statements,
the `res` (result) type, and formatting of error values.

```cpp
auto make_three_buffers = [](ok::allocator& allo)
    -> ok::allocator::res<std::array<ok::slice<u8>, 3>> {
    using namespace ok;
    // require allocator feature
    if (!(allo.features() & allocator::feature_flags::threadsafe)) {
        return allocator::error::unsupported;
    }

    // do allocation using raw allocator, no owning ptrs. rare, but acceptable
    // when combined with defers.
    auto first_mem = allo.allocate_bytes(100);
    if (!first_mem)
        return first_mem.err();

    maydefer free_first_mem([first_mem, &allo] {
        allo.deallocate_bytes(first_mem.data(), first_mem.size());
    });

    auto second_mem = allo.allocate_bytes(100);
    if (!second_mem)
        return second_mem.err();

    maydefer free_second_mem([second_mem, &allo] {
        allo.deallocate_bytes(second_mem.data(), second_mem.size());
    });

    auto third_mem = allo.allocate_bytes(100);
    if (!third_mem)
        return third_mem.err();

    // okay, all initialization is good, dont free anything
    free_first_mem.cancel();
    free_second_mem.cancel();

    return std::array{first_mem.slice(), second_mem.slice(), third_mem.slice()};
};

int main()
{
    ok::c_allocator my_allocator; // malloc and free

    auto buffer_result = make_three_buffers(my_allocator);
    if (!buffer_result.okay()) {
        fmt::println("Allocation failure: {}", buffer_result.err());
        return -1;
    }
    auto& buffers = buffer_result.release_ref();

    for (auto& slice : buffers) {
        // initialize to zero, just to demonstrate "stdmem" header
        ok::memfill(slice, 0);

        fmt::format_to_n(slice.data(), slice.size(),
                         "Hello, world! in slice {}", slice);

        // cstdio still works fine of course!
        printf("printed: %s\n", slice.data());
    }
}
```

Outputs:

```txt
printed: Hello, world! in slice [0x23c152a0 -> 100]
printed: Hello, world! in slice [0x23c15310 -> 100]
printed: Hello, world! in slice [0x23c15380 -> 100]
```

## todo

- [x] implicit `context()` for allocators, random number generators, current error
      message, etc.
- [x] modify context with `context_switch` type, which always restores changes when
      it is destroyed. it cannot be moved.
- [x] polymorphic allocator interface
- [ ] implementations for arena, block, slab, page, and remapping page allocators
- [ ] wrapper / import of jemalloc for the allocator interface.
- [x] "result" type: optional with enum error value. like `std::expected`, kind of
- [x] "opt" type: optional but supports reference types with rebinding assignment
- [x] opt and result are constexpr + trivial, if their payloads are
- [x] slice type: like span but not nullable
- [x] defer statement
- [x] stdmem: functions for checking if slices are overlapping, contained
      within, etc
- [x] new iterators, with lower barrier to entry. c++17 compatible but not backwards
      compatible with algorithms that use legacy iterators or c++20 iterators. Designed
      for easy implementation, good codegen, and immediate rangelike support (type
      with iterator stuff should also be a range)
- [ ] [WIP](https://github.com/the-argus/vmath) SIMD vector and matrix types,
      explicit by default but with optional operator overloading. inspired by DirectXMath
- [ ] A dynamic bit array and a static bit array with boolean-like iterators, to
      prove capability of new iterators
- [x] `std::ranges` reimplementation, with some new views. enumerate, zip, take,
      drop, join, keep_if, reverse, transform. Template specialization / optimization
      when the viewed type is array-like.
- [ ] More views (which will require allocation + error handling): sliding window,
      chunking view, split view.
- [ ] "hresult" type: like result but instead of storing its error value, it points
      to a more complex (optionally polymorphic) error type. stands for "heavy
      result"
- [ ] "cresult" type, exactly like optional internally but with a different interface.
      On construction, it stores an info string in the thread context. Has a getter
      which returns a reference to the string in the context. Stands for "context
      result". Maybe instead "lresult" for "local result?". Potentially a debugmode
      check to make sure you haven't overwritten the value in the context when you
      access it from the result.
- [ ] context handle: serializable replacement for a reference which is a unique
      index of an allocation along with a generation / magic value. when dereferencing,
      it asks the context for the corresponding memory and compares magic number
      to try to detect invalid allocator or use-after-free.
- [ ] variants of context handle: explicit handle (dereferencing requires passing
      the allocator) and unique context handle
- [ ] sane `std::string` replacement, inspired a bit by Godot's `String`
- [ ] `static_string`: `const char*` replacement which stores its length and has
      a lot of nice string operations. never does allocation.
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
- [ ] fold/reduce function(s) compatible with above views
- [ ] reimplementation of `<algorithm>` stuff: `stable_sort`, `sort`, `copy_if`,
      `copy`, `move`, `count`, `count_if` `mismatch` `find`, `starts_with`, `ends_with`,
      `contains`, `fill`, `find_if`, `any_of`, `all_of`, `is_sorted`, `unique`, `shuffle`,
      `rotate`, `reverse`, `swap`, `binary_search`, `equal`, `max_element`, `max`,
      `min`, `min_element`, `minmax_element`, `clamp`, and copying vs. in-place
      variants for all algorithms. This is rangelike though- no need for
      begin() and end() as separate arguments
- [ ] threadpool compatibility for some views which are embarassingly parellel,
      like `count*` or `max_element`. Specific threadsafe container iterator type?
      iterables are all extremely templated, so this will be interesting.
- [ ] reimplementation of `<thread>` and `<atomic>` ? avoid exceptions where
      possible, and avoid implicit atomic load/stores
- [ ] standard coroutine types: task, generator
- [ ] coroutines which can use thread's context allocator
- [ ] coroutine-running threadpool with work queues and task stealing, for
      copying off Go's homework. (potentially put threadpool and runtime into
      context for submitting coroutines upon construction?)
- [ ] fmtlib included for IO, all types mentioned above include formatters
- [ ] all okaylib types have nlohmann json serialization defined
- [ ] Zig buildsystem module which makes it easy to import it into a zig project
      and propagate up information about compilation flags (get an error if you
      do something like enable bounds checking but a library youre calling into
      explicitly disables them)
- [ ] One day, far down the line: c++ modules support for zig build system, add
      c++ modules support to okaylib.
- [ ] (maybe) context contains an array of allocators so you can refer to allocators
      in a serializable way (not by pointer)? Could be possible for users who
      don't use pointers and only allocator handles to trivially serialize their
      whole program to binary

## misc improvements / backlog

- [ ] Remove dependency on `<memory>` header from `okay/detail/addressof.h`
- [ ] Add option to disable undefined behavior checks which are normally on in
      both release and debug mode (such as array bounds checks on iterators)
- [ ] Create "minimum viable" ranges for forward, multipass, bidirectional,
      random access, and contiguous ranges, to test conformance of all the views
- [ ] Add tests for all the views with a finite + random access range
- [ ] Make sure every constructor of opt and res (converting constructors esp.)
      have test coverage
- [ ] Add better static asserts for when you use an invalid range with a pipe operator-
      right now errors come from inside the range adaptor closure
- [ ] Add some concept of being infinite *and* arraylike. Currently infinite ranges
      like `ok::indices` are not arraylike, which makes `enumerate(array)` more
      space efficient than `zip(array, indices)`.
