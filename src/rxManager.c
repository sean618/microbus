// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"

#include "microbus.h"
#include "rxManager.h"


// ==================================================================== //
// Rx Packet memory manager

tPacketEntry * findFreeRxPacket(tRxPacketManager * rpm) {
    // Check the queue isn't full (it can be full when there are still empty entries because removing nodes doesn't shift down the queue)
    if (CIRCULAR_BUFFER_FULL(rpm->start, rpm->end, rpm->maxRxPacketEntries)) {
        return NULL;
    }
    for (uint32_t i=0; i<rpm->maxRxPacketEntries; i++) {
        if (rpm->rxPacketEntries[i].inUse == false) {
            rpm->rxPacketEntries[i].inUse = true;
            return &rpm->rxPacketEntries[i];
        }
    }
    return NULL;
}

void addRxDataPacket(tRxPacketManager * rpm, tPacketEntry * packetEntry) {
    // Leave in the buffer to be processed outside of this driver
    // Update the tail and use the next free entry
    CIRCULAR_BUFFER_APPEND(rpm->rxPacketQueue, rpm->start, rpm->end, rpm->maxRxPacketEntries, packetEntry);
    uint8_t rxBufferLevel = CIRCULAR_BUFFER_LENGTH(rpm->start, rpm->end, rpm->maxRxPacketEntries);
    rpm->rxBufferLevel = MAX(rpm->rxBufferLevel, rxBufferLevel);
}

void rxManagerRemoveAllPackets(tRxPacketManager * rpm, tNodeIndex nodeId) {
    // Rather than remove from the queue and having to shift everything up
    // instead just set packet invalid 
    for (uint32_t i=rpm->start; i != rpm->end; i++) {
        if (i >= rpm->maxRxPacketEntries) {
            i = 0;
        }

        if (rpm->rxPacketQueue[i]) {
            if (rpm->rxPacketQueue[i]->packet.node.srcNodeId == nodeId) {
                // Free the packet in memory
                rpm->rxPacketQueue[i]->inUse = false;
                // Invaliddate this entry in the queue
                rpm->rxPacketQueue[i] = NULL;
            }
        }
    }
    // microbusAssert(numValidRxPackets(rpm) == CIRCULAR_BUFFER_LENGTH(rpm->start, rpm->end, rpm->maxRxPacketEntries), "");
}

// ============================================ //
// User API - to the rx packet store

tPacket * peekNextRxDataPacket(tRxPacketManager * rpm) {
    for (uint32_t i=0; i<rpm->maxRxPacketEntries; i++) {
        if (CIRCULAR_BUFFER_EMPTY(rpm->start, rpm->end, rpm->maxRxPacketEntries)) {
            return NULL;
        }

        // Peek
        tPacketEntry * entry = rpm->rxPacketQueue[rpm->start];

        if (entry == NULL) {
            // If not valid then remove this entry from the queue and try again
            CIRCULAR_BUFFER_POP(rpm->start, rpm->end, rpm->maxRxPacketEntries);
        } else {
            // If valid return the packet
            return &entry->packet;
        }
    }
    // Shouldn't ever get here!
    microbusAssert(0, "");
    return NULL;
}

bool popNextDataPacket(tRxPacketManager * rpm) {
    // Find the packet and "free" it by setting the protocolversion byte to 0
    tPacketEntry ** entry;
    CIRCULAR_BUFFER_PEEK((entry), rpm->rxPacketQueue, rpm->start, rpm->end, rpm->maxRxPacketEntries);
    (*entry)->inUse = false;
    // Now move the start position of the buffer
    CIRCULAR_BUFFER_POP(rpm->start, rpm->end, rpm->maxRxPacketEntries);
    return true; //TODO
}

void rxManagerInit(tRxPacketManager * rpm, uint8_t maxRxPacketEntries, tPacketEntry rxPacketEntries[], tPacketEntry * rxPacketQueue[]) {
    memset(rxPacketEntries, 0, maxRxPacketEntries * sizeof(tPacketEntry));
    memset(rxPacketQueue, 0, maxRxPacketEntries * sizeof(tPacketEntry *));
    rpm->maxRxPacketEntries = maxRxPacketEntries;
    rpm->rxPacketEntries = rxPacketEntries;
    rpm->rxPacketQueue = rxPacketQueue;
}

