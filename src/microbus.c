#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

//#include "cexception.h"

#include "microbus.h"

void myAssert(uint8_t predicate, char * msg) {
    if (predicate == 0)
    {
        //Throw(1);
        printf("Assert: %s\n", msg);
        // uint8_t * ptr = NULL;
    	// *ptr = 0; // Hit debugger
        // return;
    }
}

void addTxPacket(tCommon * common, tPacket * packet) {
    if (common->numTxPackets < MAX_TX_PACKETS) {
        printf("Adding tx packet %d\n", packet->val);
        memcpy(&common->txPacket[common->numTxPackets], packet, sizeof(tPacket));
        common->numTxPackets++;
    } else {
        myAssert(0, "Tx buffer is full");
    }
}
tPacket * getTxPacket(tCommon * common) {
    if (common->numTxPackets == 0) {
        return NULL;
    } else {
        return &common->txPacket[0];
    }
}
void removeTxPacket(tCommon * common) {
    if (common->numTxPackets > 0) {
        for (uint32_t i=1; i<common->numTxPackets; i++) {
            memcpy(&common->txPacket[i-1], &common->txPacket[i], sizeof(tPacket));
        }
        common->numTxPackets--;
    } else {
        myAssert(0, "Tx buffer is empty");
    }
}

void addRxPacket(tCommon * common, tPacket * packet) {
    if (common->numRxPackets < MAX_RX_PACKETS) {
        memcpy(&common->rxPacket[common->numRxPackets], packet, sizeof(tPacket));
        common->numRxPackets++;
    } else {
        myAssert(0, "Rx buffer is full");
    }
}

// int main() {
    
// }

