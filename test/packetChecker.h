// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef PACKET_CHECKER_H
#define PACKET_CHECKER_H

#include "../src/microbus.h"

// NOTE: packets are added when Tx and removed when Rx

#define MAX_PACKET_CHECKER_PACKETS 500

#define PACKET_CHECKER_HEADER 6 // srcNodeSimId, dstNodeSimId, packetId

typedef struct {
    bool valid;
    bool mightBeDropped;
    uint32_t id;
    uint16_t size;
    tNodeIndex srcNodeSimId;
    tNodeIndex dstNodeSimId;
    uint8_t data[MB_PACKET_SIZE];
} tPacketCheckerPacket;

typedef struct {
    uint32_t currentId;
    uint32_t numCorrectRxPackets;
    uint32_t numPackets;
    tPacketCheckerPacket txPackets[MAX_PACKET_CHECKER_PACKETS];
} tPacketChecker;


void initPacketChecker(tPacketChecker * checker);
uint8_t * createTxPacket(tPacketChecker * checker, bool masterTx, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId, uint16_t size, bool mightBeDropped);
void processRxPacket(tPacketChecker * checker, uint8_t * data, uint16_t size);
void checkAllPacketsReceived(tPacketChecker * checker);

#endif
