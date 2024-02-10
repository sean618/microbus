#ifndef USEFULLIB_H
#define USEFULLIB_H

// #include "string.h"
// #include "stdbool.h"
// #include "stdint.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Circular buffer stuff


void myAssert(uint32_t predicate, char * msg);

#endif