#ifndef __OKAYLIB_MACROS_FOREACH_H__
#define __OKAYLIB_MACROS_FOREACH_H__

// taken from a comment on
// https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
// in turn taken from https://www.chiark.greenend.org.uk/~sgtatham/mp/

#define __OK_LN(l, x) x##l // creates unique labels
#define __OK_L(x, y) __OK_LN(x, y)
#define ok_foreach(decl, iterable)                                            \
    for (bool _run = true; _run; _run = false)                                \
        for (auto cursor = ok::begin(iterable); cursor != ok::end(iterable);  \
             ++cursor)                                                        \
            if (1) {                                                          \
                _run = true;                                                  \
                goto __OK_L(__LINE__, body);                                  \
                __OK_L(__LINE__, cont) : _run = true;                         \
                continue;                                                     \
                __OK_L(__LINE__, finish) : break;                             \
            } else                                                            \
                while (1)                                                     \
                    if (1) {                                                  \
                        if (!_run)                                            \
                            goto __OK_L(                                      \
                                __LINE__,                                     \
                                cont); /* we reach here if the block          \
                                          terminated normally/via continue */ \
                        goto __OK_L(__LINE__,                                 \
                                    finish); /* we reach here if the block    \
                                                terminated by break */        \
                    } else                                                    \
                        __OK_L(__LINE__, body)                                \
                            : for (decl = ok::iter_get_ref(iterable, cursor); \
                                   _run; _run = false) /* block following the \
                                                          expanded macro */

#endif
