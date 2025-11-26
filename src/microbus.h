// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MICROBUS_H
#define MICROBUS_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"


// =========================== //
// User configurable parameters

#define MAX_NODES 64 // Must be less than 255

// TODO:
#define MB_PACKET_SIZE 192 // (At 4.5Mhz => 355us per packet, at 9MHz => 177us per packet)
#define SLOT_TIME_US ((8 * MB_PACKET_SIZE * 10) / 45)

#define MAX_ACTIVE_TX_NODES 10 // Only for the master - how many nodes have tx packets waiting for them
#define SLIDING_WINDOW_SIZE 4 // The size of the sliding window in tx packets
#define SLIDING_WINDOW_PAUSE 3

#define MICROBUS_VERSION 1

// =========================== //
// Packets

#define MASTER_NODE_ID 0
#define FIRST_NODE_ID 1
#define UNALLOCATED_NODE_ID 0xFE // Used for signalling when newNodeId packets can be sent
#define INVALID_NODE_ID 0xFF // Shouldn't ever be used

// TODO: change empty packet to ACK PACKET - allowing master to ack multiple nodes
typedef enum {
    NULL_PACKET = 0,
    MASTER_DATA_PACKET = 1,
    NODE_DATA_PACKET = 2,
    MASTER_EMPTY_PACKET = 3,
    NODE_EMPTY_PACKET = 4,
    NEW_NODE_REQUEST_PACKET = 5,
    NEW_NODE_RESPONSE_PACKET = 6,
    MASTER_RESET_PACKET = 7,
    MAX_PACKET_TYPE = 8
} tPacketType; // Max of 15! - only 4 bits

#define MAX_TX_NODES_SCHEDULED 4

#define MB_HEADER_SIZE (6+(2*MAX_TX_NODES_SCHEDULED))

// Keep them the same 
#define MASTER_PACKET_DATA_SIZE (MB_PACKET_SIZE - MB_HEADER_SIZE)
#define NODE_PACKET_DATA_SIZE    MASTER_PACKET_DATA_SIZE

typedef uint8_t tNodeIndex; // Node 0 not allowed, Node 255 is unused for new nodes to advertise

// TODO: pragma struct align
typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    // These initial fields are read first independently of whether we need to process the rest of the packet
    // We necessarily want to CRC check the whole packet just for these fields so we use the checksum instead
    uint8_t nextTxNodeId[MAX_TX_NODES_SCHEDULED]; // Master only
    uint8_t nextTxNodeAckSeqNum[MAX_TX_NODES_SCHEDULED];
    // Remaining packet - only need to process if the packet is for us
    uint8_t dstNodeId;
    uint8_t wirelessDstNodeId;
    uint8_t data[MASTER_PACKET_DATA_SIZE];
} __attribute__((packed, aligned(2))) tMasterPacket;

// TODO: pragma struct align
typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    uint8_t ackSeqNum;
    uint8_t srcNodeId;
    uint8_t srcWirelessNodeId;
    uint8_t bufferLevel;
    uint8_t spare[2*MAX_TX_NODES_SCHEDULED-2]; // Not used
    uint8_t data[NODE_PACKET_DATA_SIZE];
} __attribute__((packed, aligned(2))) tNodePacket;

// TODO: pragma struct align
typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    uint8_t protocolVersionAndPacketType; // 4 bit version and 4 bit packet type
    uint8_t txSeqNum;
    uint8_t dataSize1; // Upper bits
    uint8_t dataSize2; // lower bits
    union {
        tMasterPacket master;
        tNodePacket node;
    };
}  __attribute__((packed, aligned(2))) tPacket;

#define MAX_PACKET_DATA_SIZE (MAX(MASTER_PACKET_DATA_SIZE, NODE_PACKET_DATA_SIZE))

typedef struct {
    uint8_t protocolVersionAndPacketType; // 4 bit version and 4 bit packet type
    uint8_t txSeqNum;
    uint8_t dataSize1;
    uint8_t dataSize2;
    union {
        struct {
            uint8_t nextTxNodeId[4];
            uint8_t nextTxNodeAckSeqNum[4];
            uint8_t dstNodeId;
            uint8_t spare;
        } master;
        struct {
            uint8_t ackSeqNum;
            uint8_t srcNodeId;
            uint8_t bufferLevel;
            uint8_t spare[7];
        } node;
    };
} __attribute__((packed, aligned(2))) tPacketHeader;


typedef struct {
    bool inUse;
    // bool valid;
    tPacket packet;
} tPacketEntry;

typedef struct {
    tNodeIndex nodeIds[MAX_NODES]; // A list of nodes that we have outstanding packet for
    uint8_t numNodes;
    tNodeIndex lastIndex;
} tNodeQueue;

typedef struct {
    //uint64_t txPacketsSent;
    uint64_t txPackets;
    uint64_t txDataPackets;
    uint64_t emptyRx;
    uint64_t rxValid;
    uint64_t rxPacketEntries;
    uint64_t rxNodePackets;
    uint64_t rxDataPackets;
    uint64_t rxCrcFailures;
    uint64_t rxInvalidProtocol;
    uint64_t rxInvalidDataSize;
    uint64_t rxInvalidPacketType;
    uint64_t rxBufferFull;
    uint64_t txBufferFull;
    uint64_t txWindowRestarts;
    uint32_t nodeLeftNw;
    uint32_t nodeJoinedNw;
    uint32_t networkFullCount;

    uint32_t newNodeRequest; // Node
    uint32_t newNodeRequestRx; // Master
    uint32_t newNodeAllocated; // Master
    uint32_t newNodeAllocatedRx; // Node
} tNodeStats;

#define INVALID_SEQUENCE_NUM 255
#define NULL_SEQUENCE_NUM 255
#define MAX_SEQUENCE_NUM 255

#define SET_PROTOCOL_VERSION_AND_PACKET_TYPE(packet, packetType) ((packet)->protocolVersionAndPacketType = (MICROBUS_VERSION << 4) | (packetType & 0xF))
#define SET_PACKET_DATA_SIZE(packet, dataSize) (packet)->dataSize1 = (dataSize >> 8); (packet)->dataSize2 = (dataSize & 0xFF)

#define GET_PROTOCOL_VERSION(packet)  (((packet)->protocolVersionAndPacketType >> 4) & 0xF)
#define GET_PACKET_TYPE(packet)       (((packet)->protocolVersionAndPacketType     ) & 0xF)
#define GET_PACKET_DATA_SIZE(packet)  (((packet)->dataSize1 << 8) | ((packet)->dataSize2))

// =========================== //
// Useful

#ifndef MIN
	#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
	#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define INCR_AND_WRAP(val, incr, max) (((val) >= (max - incr)) ? (incr - ((max) - (val))) : ((val) + (incr)))
#define DECR_AND_WRAP(val, decr, max) (((val) < (decr))        ? ((max) - (decr) + (val)) : ((val) - (decr)))

#define CIRCULAR_BUFFER_FULL(head, tail, size) ((head) == INCR_AND_WRAP(tail, 1, size))
#define CIRCULAR_BUFFER_EMPTY(head, tail, size) ((head) == (tail))
#define CIRCULAR_BUFFER_LENGTH(head, tail, size) (DECR_AND_WRAP(tail, head, size))

#define CIRCULAR_BUFFER_APPEND(entries, head, tail, size, newEntry) { \
    if (CIRCULAR_BUFFER_FULL(head, tail, size)) { \
        microbusAssert(0, ""); /* Trying to append to full CIRCULAR_BUFFER */ \
    } else { \
        entries[tail] = newEntry; \
        tail = INCR_AND_WRAP(tail, 1, size); \
    } \
}

#define CIRCULAR_BUFFER_PEEK(entry, entries, head, tail, size) { \
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        entry = NULL; \
    } else { \
        entry = (&(entries)[(head)]); \
    } \
}

#define CIRCULAR_BUFFER_POP(head, tail, size) { \
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        microbusAssert(0, ""); /* Trying to pop from empty CIRCULAR_BUFFER */ \
    } else { \
        head = INCR_AND_WRAP(head, 1, size); \
    } \
}

#define CIRCULAR_BUFFER_SHIFT(head, tail, size) { \
    if (CIRCULAR_BUFFER_EMPTY(head, tail, size)) { \
        microbusAssert(0, ""); /* "Tying to shift from empty CIRCULAR_BUFFER */ \
    } else { \
        tail = DECR_AND_WRAP(tail, 1, size); \
    } \
}


// =========================== //
// Logging

extern FILE * logfile;
extern bool loggingEnabled;
extern uint64_t cycleIndex;
extern uint64_t wCycleIndex;

#define MICROBUS_LOGGING 0
#define MICROBUS_LOG_PACKETS 0
#define MICROBUS_LOG_EMPTY_PACKETS 0
#define MICROBUS_LOG_NETWORK_MANGER 0
#define MICROBUS_LOG_TX_MANGER 0
#define MICROBUS_LOG_SCHEDULER 0

#if MICROBUS_LOGGING > 0
    #define MB_PRINTF_LL(useCycle, fmt, ...)  \
        if (loggingEnabled) { \
            if (useCycle) {\
                printf("[Cycle:%llu] " fmt, cycleIndex, ##__VA_ARGS__); \
                fprintf(logfile, "[Cycle:%llu] " fmt, cycleIndex, ##__VA_ARGS__); \
            } else {\
                printf(fmt, ##__VA_ARGS__); \
                fprintf(logfile, fmt, ##__VA_ARGS__); \
            } \
        }
    #define MB_PRINTF(...) MB_PRINTF_LL(true, ##__VA_ARGS__)
#else
    #define MB_PRINTF_LL(...) ;
    #define MB_PRINTF(...) ;
#endif


#define MB_PRINTF_WITHOUT_NEW_LINE(...) \
    MB_PRINTF_LL(false, __VA_ARGS__)

#define MB_SCHEDULER_PRINTF(...) \
    if (MICROBUS_LOG_SCHEDULER > 0) { \
        MB_PRINTF_LL(true, __VA_ARGS__) \
    }
#define MB_TX_MANAGER_PRINTF(...) \
    if (MICROBUS_LOG_TX_MANGER > 0) { \
        MB_PRINTF_LL(true, __VA_ARGS__) \
    }
#define MB_NETWORK_MANAGER_PRINTF(...) \
    if (MICROBUS_LOG_NETWORK_MANGER > 0) { \
        MB_PRINTF_LL(true, __VA_ARGS__) \
    }
#define MB_NETWORK_MANAGER_PRINTF_WITHOUT_NEW_LINE(...) \
    if (MICROBUS_LOG_NETWORK_MANGER > 0) { \
        MB_PRINTF_LL(false, __VA_ARGS__) \
    }

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define microbusAssert(_pred_, _msg_)       microbusAssertLL(_pred_, __FILE__ ": " TOSTRING(__LINE__) " - " _msg_)
#define microbusAssertLL(_pred_, _msg_) if ((_pred_) == false) assertMessage( _msg_ , sizeof(_msg_))

// =========================== //
// from common.c

void __attribute__((weak)) assertMessage(const char * msg, size_t msgLen);
bool nodeQueueAdd(tNodeQueue * queue, tNodeIndex nodeId);
void nodeQueueRemove(tNodeQueue * queue, tNodeIndex nodeId);
void nodeQueueRemoveIfExists(tNodeQueue * queue, tNodeIndex nodeId);
bool queueReachedEnd(tNodeQueue * queue);
tNodeIndex getNextNodeInQueue(tNodeQueue * queue);

void microbusPrintPacket(tPacket * packet, bool master, uint32_t nodeId, bool tx, uint8_t numScheduled);



#endif
