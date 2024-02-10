
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "usefulLib.h"



void myAssert(uint32_t predicate, char * msg) {
    if (predicate == 0)
    {
        //Throw(1);
        printf("Assert: %s\n", msg);
        // uint8_t * ptr = NULL;
    	// *ptr = 0; // Hit debugger
        // return;
    }
}