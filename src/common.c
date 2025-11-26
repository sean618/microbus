// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.


#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include <stdio.h>

#include "microbus.h"

#if MICROBUS_LOGGING
    #include "assert.h"
#endif

void __attribute__((weak)) assertMessage(const char * msg, size_t msgLen) {
    #if MICROBUS_LOGGING
        fflush(logfile);
        assert(0);
    #endif
    while(1) {};
}

// Sometimes called by independent thread!
bool nodeQueueAdd(tNodeQueue * queue, tNodeIndex nodeId) {
    if (queue->numNodes < MAX_NODES) {
    	// If already exists then leave
        for (uint32_t i=0; i<queue->numNodes; i++) {
            // microbusAssert(queue->nodeIds[i] != nodeId, "");
            if (queue->nodeIds[i] == nodeId) {
            	return true;
            }
        }

        queue->nodeIds[queue->numNodes] = nodeId;
        // Do this at the end and atomically to cope with the other thread cutting in
        queue->numNodes++;
        return true;
    } else {
        microbusAssert(0, "");
        return false;
    }
}

void nodeQueueRemoveIfExists(tNodeQueue * queue, tNodeIndex nodeId) {
    if (queue->numNodes > 0) {
        for (uint8_t i=0; i<queue->numNodes; i++) {
            if (queue->nodeIds[i] == nodeId) {
                // Shift all entries down by 1
                for (uint8_t j=i; j<queue->numNodes-1; j++) {
                    queue->nodeIds[j] = queue->nodeIds[j+1];
                }
                queue->numNodes--;
                if (queue->lastIndex >= queue->numNodes) {
                    queue->lastIndex = 0;
                }
                break;
            }
        }
    }
}

void nodeQueueRemove(tNodeQueue * queue, tNodeIndex nodeId) {
    uint32_t prevNum = queue->numNodes;
    microbusAssert(queue->numNodes > 0, "");
    nodeQueueRemoveIfExists(queue, nodeId);
    microbusAssert(prevNum == queue->numNodes + 1, "");
}

bool queueReachedEnd(tNodeQueue * queue) {
    return ((queue->lastIndex + 1) >= queue->numNodes);
}

tNodeIndex getNextNodeInQueue(tNodeQueue * queue) {
    tNodeIndex node = queue->nodeIds[queue->lastIndex];
    queue->lastIndex++;
    if (queue->lastIndex >= queue->numNodes) {
        queue->lastIndex = 0;
    }
    return node;
}

void microbusPrintPacket(tPacket * packet, bool master, uint32_t nodeId, bool tx, uint8_t numScheduled) {
    if (packet == NULL) {
        return;
    }
    uint16_t size = GET_PACKET_DATA_SIZE(packet);
    if (MICROBUS_LOG_EMPTY_PACKETS || size > 0) {
        microbusAssert(size < MAX_PACKET_DATA_SIZE, "");
        if (master) {
            MB_PRINTF("Master, %s packet:%u, size:%u, txSeqNum:%u", 
                tx ? "Tx" : "Rx", 
                packet->protocolVersionAndPacketType & 0xF, 
                size, 
                packet->txSeqNum
            );
            if (tx) {
                MB_PRINTF_WITHOUT_NEW_LINE(", dstNode:%3u, nextTxNode:", packet->master.dstNodeId);
                for (uint8_t i=0; i<numScheduled; i++) {
                    MB_PRINTF_WITHOUT_NEW_LINE("%u,", packet->master.nextTxNodeId[i]);
                }
                MB_PRINTF_WITHOUT_NEW_LINE(" nextTxNodeAckSeqNum:");
                for (uint8_t i=0; i<numScheduled; i++) {
                    MB_PRINTF_WITHOUT_NEW_LINE("%u,", packet->master.nextTxNodeAckSeqNum[i]);
                }
            } else {
                MB_PRINTF_WITHOUT_NEW_LINE(", srcNode:%3u, ackSeqNum:%u, bufferLevel:%u", packet->node.srcNodeId, packet->node.ackSeqNum, packet->node.bufferLevel);
            }
        } else {
            MB_PRINTF("Node:%u,            %s packet:%u, size:%u, txSeqNum:%u", 
                nodeId, 
                tx ? "Tx" : "Rx", 
                packet->protocolVersionAndPacketType & 0xF, 
                size, 
                packet->txSeqNum
            );
            if (tx) {
                MB_PRINTF_WITHOUT_NEW_LINE(", ackSeqNum:%u, bufferLevel:%u", packet->node.ackSeqNum, packet->node.bufferLevel);
            } else {
                MB_PRINTF_WITHOUT_NEW_LINE(", ackSeqNum:%u", packet->master.nextTxNodeAckSeqNum[0]);
            }
        }
        if (size > 0) {
            MB_PRINTF_WITHOUT_NEW_LINE(", data:");
            for (uint32_t i=0; i<10; i++) {
                MB_PRINTF_WITHOUT_NEW_LINE("%02x", packet->master.data[i]);
            }
        }
        MB_PRINTF_WITHOUT_NEW_LINE("\n");
    }
}

