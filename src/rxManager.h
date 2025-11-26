// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef RXMANAGER_H
#define RXMANAGER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"


// Rx buffer - a circular buffer to allow access by other threads
typedef struct {
    uint8_t maxRxPacketEntries;
    tPacketEntry * rxPacketEntries;
    tPacketEntry ** rxPacketQueue;
    uint8_t start;
    uint8_t end;
    uint8_t rxBufferLevel;
} tRxPacketManager;

tPacketEntry * findFreeRxPacket(tRxPacketManager * rpm);
void addRxDataPacket(tRxPacketManager * rpm, tPacketEntry * packetEntry);
void rxManagerRemoveAllPackets(tRxPacketManager * rpm, tNodeIndex nodeId);
tPacket * peekNextRxDataPacket(tRxPacketManager * rpm);
bool popNextDataPacket(tRxPacketManager * rpm);
void rxManagerInit(tRxPacketManager * rpm, uint8_t maxRxPacketEntries, tPacketEntry rxPacketEntries[], tPacketEntry * rxPacketQueue[]);


#endif