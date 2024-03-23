#ifndef USEFULLIB_H
#define USEFULLIB_H

// #include "string.h"
// #include "stdbool.h"
// #include "stdint.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void myAssert(uint32_t predicate, char * msg);


// #define INCR_AND_WRAP(val, max) ((((val) + 1) >= max) ? 0 : ((val) + 1))
// #define DECR_AND_WRAP(val, max) ((val) == 0 ? (max - 1) : ((val) - 1))

#define INCR_AND_WRAP(val, incr, max) (((val) >= (max - incr)) ? (incr - ((max) - (val))) : ((val) + (incr)))
#define DECR_AND_WRAP(val, decr, max) (((val) < (decr))        ? ((max) - (decr) + (val)) : ((val) - (decr)))

#define CIRCULAR_BUFFER_FULL(head, tail, size) ((head) == INCR_AND_WRAP(tail, 1, size))
#define CIRCULAR_BUFFER_EMPTY(head, tail, size) ((head) == (tail))
#define CIRCULAR_BUFFER_LENGTH(head, tail, size) (DECR_AND_WRAP(tail, head, size))

#define CIRCULAR_BUFFER_ALLOCATE(newPtr, entries, head, tail, size) { \
    if (CIRCULAR_BUFFER_FULL(head, tail, size)) { \
        myAssert(0, "Trying to append to full CIRCULAR_BUFFER"); \
    } else { \
        newPtr = &entries[tail]; \
        tail = INCR_AND_WRAP(tail, 1, size); \
    } \
}

#define CIRCULAR_BUFFER_APPEND(entries, head, tail, size, newEntry) { \
    if (CIRCULAR_BUFFER_FULL(head, tail, size)) { \
        myAssert(0, "Trying to append to full CIRCULAR_BUFFER"); \
    } else { \
        entries[tail] = newEntry; \
        tail = INCR_AND_WRAP(tail, 1, size); \
    } \
}

#define CIRCULAR_BUFFER_PEEK(entry, entries, head, tail, size) { \
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        entry = (&(entries)[(head)]); \
    } else { \
        entry = NULL; \
    } \
}

#define CIRCULAR_BUFFER_POP(head, tail, size) { \
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        myAssert(0, "Trying to pop] from empty CIRCULAR_BUFFER"); \
    } else { \
        head = INCR_AND_WRAP(head, 1, size); \
    } \
}

#define CIRCULAR_BUFFER_SHIFT(head, tail, size) { \
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        myAssert(0, "Trying to shift from empty CIRCULAR_BUFFER"); \
    } else { \
        tail = DECR_AND_WRAP(tail, 1, size); \
    } \
}


#endif