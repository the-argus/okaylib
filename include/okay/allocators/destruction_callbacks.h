#ifndef __OKAYLIB_ALLOCATORS_DESTRUCTION_CALLBACKS_H__
#define __OKAYLIB_ALLOCATORS_DESTRUCTION_CALLBACKS_H__

#include "okay/allocators/allocator.h"

namespace ok {
using callback_t = void (*)(void*);
struct context_callback_t
{
    void* context;
    callback_t callback;
};

struct destruction_callback_entry_node_t
{
    context_callback_t entry;
    destruction_callback_entry_node_t* previous = nullptr;
};

/// Given an allocator and some pointer to the end of a destruction callback
/// linked list, append one item to the list. This list can later be
/// traversed to call all callbacks. That should happen on clear and on
/// destruction.
/// Returns false on memory allocation failure.
template <typename T>
constexpr alloc::result_t<context_callback_t&>
append_destruction_callback(T& allocator,
                            destruction_callback_entry_node_t*& current_head,
                            context_callback_t& callback)
{
    static_assert(stdc::is_base_of_v<ok::allocator_t, T>,
                  "Cannot append destruction callback to allocator which "
                  "does not inherit from allocator_t");
    auto result =
        allocator.allocate_bytes(sizeof(destruction_callback_entry_node_t),
                                 alignof(destruction_callback_entry_node_t));
    if (!result)
        return alloc::error::oom;

    if (!current_head) {
        current_head =
            static_cast<destruction_callback_entry_node_t*>(result.data());
        current_head->previous = nullptr;
    } else {
        auto* const temp = current_head;
        current_head =
            static_cast<destruction_callback_entry_node_t*>(result.data());
        current_head->previous = temp;
    }

    current_head->entry = callback;
    return current_head->entry;
}

// traverse a linked list of destruction callbacks and call each one of
// them. Does not deallocate the space used by the destruction callback.
// Intended to be called when an allocator is destroyed.
static inline constexpr void
call_all_destruction_callbacks(destruction_callback_entry_node_t* current_head)
{
    destruction_callback_entry_node_t* iter = current_head;
    while (iter != nullptr) {
        iter->entry.callback(iter->entry.context);
        iter = iter->previous;
    }
}
} // namespace ok

#endif
