#ifndef __OKAYLIB_CONTAINERS_ARCPOOL_H__
#define __OKAYLIB_CONTAINERS_ARCPOOL_H__

#include "okay/allocators/allocator.h"
#include "okay/platform/atomic.h"

namespace ok {
/// An array_arcpool can be thought of as an allocator for objects of type T,
/// where you can only allocate the objects inside arcs (shared pointers). This
/// enables the pool to reuse the reference counters even when weak pointers to
/// the counters are still alive. Destroying the arcpool does not call any
/// destructors but in debug mode will assert that they are all dead (strong
/// reference count of 0).
/// Small side effect: attempting to promote a weak pointer to a strong pointer
/// may call the destructor of the object on the promoting thread, sort of
/// "stealing" the destructor call from a thread which destroyed a strong
/// reference at the exact same time the weak reference attempted promotion.
/// Also, this requires more compare exchanges than
template <typename T,
          ::ok::detail::is_derived_from_c<ok::allocator_t> allocator_impl_t>
class arcpool_t
{
    class counters_t
    {
        // even if dead, odd if alive
        ok::atomic_t<uint64_t> generation;
        ok::atomic_t<uint64_t> strongcount;
        // NOTE: atomic linked list, this value is only messed with right before
        // being added back to the linked list, when the generation has been
        // incremented
        size_t next_free;

        // 1. generation is zero, strongcount is zero, next_free is initialized,
        // object must be dead (even generation). the arcpool's first_free
        // points to this
        // 2. compare_exchange with the first_free of the arcpool, if it doesn't
        // change then we increment generation and set first_free to be this
        // object's next_free. now we must have exclusive access, because we
        // incremented generation and nobody could have gotten to this object
        // because it was only accessible for being initialized from the
        // first_free (weak pointers cannot "revive" what they point to). now
        // construct the object, increment strongcount to 1, return strong arc
        // pointing to this spot.
        // 3. strong count increments when copying strong arc- simple atomic
        // fetch_add
        // 4. weak pointer promotes to strong, incrementing strong count-
        // increments strong count then checks that generation is the same, if
        // it is then good we have a weak pointer, if it is not then we
        // decrement strongcount according to step 5 (IF generation is odd,
        // otherwise just decrement strongcount) and return null/error
        // 5. strong count decrements when destroying strong arc or failing to
        // promote weak pointer (step 4)- if the value became zero, increment
        // the generation and decrement strong count to zero, destroying the
        // object with newfound unique ownership. afterwards, do a
        // compare_exchange to set next_free to be the list's first_free and the
        // list's first_free to be the destroyed object

      public:
        // no need to check generation here, should only be called if
        // increment_strongcount(generation) returned true already
        constexpr void decrement_strongcount() noexcept;

        /// Returns true if there was a live object here with the same
        /// generation and now another owner of that object is registered. False
        /// otherwise
        [[nodiscard]] constexpr bool
        increment_strongcount(uint64_t generation) noexcept;

        template <typename... constructor_args_t>
        [[nodiscard]] constexpr bool construct_at(uint64_t generation, T& spot,
                                                  constructor_args_t&&... args);
    };

    class strong_arc_t
    {
        arcpool_t* pool;
        size_t index;
    };

    class handle_t
    {
        arcpool_t* pool;
        size_t index;
        uint64_t generation;
    };
};
} // namespace ok

#endif
