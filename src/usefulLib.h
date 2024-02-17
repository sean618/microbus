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
#define CIRCULAR_BUFFER_PEEK(entries, head) (&(entries)[(head)])

#define INCR_BUFFER_TAIL(head, tail, size) {\
    if (CIRCULAR_BUFFER_FULL(head, tail, size)) { \
        myAssert(0, "Trying to add to full CIRCULAR_BUFFER"); \
    } else { \
        tail = INCR_AND_WRAP(tail, 1, size); \
    } \
}
#define INCR_BUFFER_HEAD(head, tail, size) {\
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        myAssert(0, "Trying to remove from empty CIRCULAR_BUFFER"); \
    } else { \
        head = INCR_AND_WRAP(head, 1, size); \
    } \
}



// TODO: not very nice code
#define CIRCULAR_BUFFER_ALLOCATE(newPtr, entries, head, tail, size) { \
    if (CIRCULAR_BUFFER_FULL(head, tail, size)) { \
        myAssert(0, "Trying to append to full CIRCULAR_BUFFER"); \
    } else { \
        newPtr = &entries[tail]; \
        tail = INCR_AND_WRAP(tail, 1, size); \
    } \
}

#define CIRCULAR_BUFFER_APPEND(entries, head, tail, size, newEntry) { \
    myAssert(!CIRCULAR_BUFFER_FULL(head, tail, size), "Trying to append to full CIRCULAR_BUFFER"); \
    entries[tail] = newEntry; \
    tail = INCR_AND_WRAP(tail, 1, size); \
}
#define CIRCULAR_BUFFER_SHIFT(head, tail, size) { \
    myAssert(!CIRCULAR_BUFFER_EMPTY(head, tail, size), "Trying to shift from empty CIRCULAR_BUFFER"); \
    tail = DECR_AND_WRAP(tail, 1, size); \
}
#define CIRCULAR_BUFFER_POP(head, tail, size) { \
    myAssert(!CIRCULAR_BUFFER_EMPTY(head, tail, size), "Trying to pop from empty CIRCULAR_BUFFER"); \
    head = INCR_AND_WRAP(head, 1, size); \
}

// #define BUFFER_FULL(buffer) (QUEUE_FULL((buffer).start, (buffer).end, (buffer).size))
// #define BUFFER_EMPTY(buffer) (QUEUE_EMPTY((buffer).start, (buffer).end, (buffer).size))
// #define BUFFER_LENGTH(buffer) (QUEUE_LENGTH((buffer).start, (buffer).end, (buffer).size))
// #define BUFFER_APPEND(buffer, newEntry) (QUEUE_APPEND((buffer).entries, (buffer).start, (buffer).end, (buffer).size, newEntry))
// #define BUFFER_PEEK(buffer) (QUEUE_PEEK((buffer).entries, (buffer).start))
// #define BUFFER_POP(buffer) (QUEUE_POP((buffer).start, (buffer).end, (buffer).size))





#endif