#ifndef USEFULLIB_H
#define USEFULLIB_H

// #include "string.h"
// #include "stdbool.h"
// #include "stdint.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void myAssert(uint32_t predicate, char * msg);

#define INCR_AND_WRAP(tail, size) (((tail) + 1)) >= size ? (0) : ((tail) + 1))

#define BUFFER_FULL(head, tail, size) ((head) == INCR_AND_WRAP(tail, size))
#define BUFFER_EMPTY(head, tail, size) ((head) == (tail))
#define BUFFER_LENGTH(head, tail, size) (decrAndWrap(tail, head, size))
#define BUFFER_PEEK(entries, head) (&(entries)[(head)])
// #define BUFFER_APPEND(entries, head, tail, size) { \
//     myAssert(!BUFFER_FULL(head, tail, size), "Trying to append to full BUFFER"); \
//     entries[tail] = newEntry; \
//     tail = INCR_AND_WRAP(tail, size); \
// }
#define BUFFER_APPEND(entries, head, tail, size, newEntry) { \
    myAssert(!BUFFER_FULL(head, tail, size), "Trying to append to full BUFFER"); \
    entries[tail] = newEntry; \
    tail = INCR_AND_WRAP(tail, size); \
}
#define BUFFER_POP(head, tail, size) { \
    myAssert(!BUFFER_EMPTY(head, tail, size), "Trying to pop from empty BUFFER"); \
    head = INCR_AND_WRAP(head, size); \
}

// #define BUFFER_FULL(buffer) (QUEUE_FULL((buffer).start, (buffer).end, (buffer).size))
// #define BUFFER_EMPTY(buffer) (QUEUE_EMPTY((buffer).start, (buffer).end, (buffer).size))
// #define BUFFER_LENGTH(buffer) (QUEUE_LENGTH((buffer).start, (buffer).end, (buffer).size))
// #define BUFFER_APPEND(buffer, newEntry) (QUEUE_APPEND((buffer).entries, (buffer).start, (buffer).end, (buffer).size, newEntry))
// #define BUFFER_PEEK(buffer) (QUEUE_PEEK((buffer).entries, (buffer).start))
// #define BUFFER_POP(buffer) (QUEUE_POP((buffer).start, (buffer).end, (buffer).size))





#endif