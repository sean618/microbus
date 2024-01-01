#ifndef PACKET_CHECKER_H
#define PACKET_CHECKER_H

#include "unity.h"
//#include "simulator.h"
#include "microbus.h"

// NOTE: packets are added when Tx and removed when Rx

#define MAX_PACKET_CHECKER_PACKETS 200

typedef struct {
    bool valid;
    uint32_t id;
    tNodeIndex srcNodeSimId;
    tNodeIndex dstNodeSimId;
    tPacket packet;
} tPacketCheckerPacket;

typedef struct {
    uint32_t currentId;
    uint32_t numCorrectRxPackets;
    uint32_t numPackets;
    tPacketCheckerPacket txPackets[MAX_PACKET_CHECKER_PACKETS];
} tPacketChecker;


void initPacketChecker(tPacketChecker * checker);
tPacket * createPacket(tPacketChecker * checker, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId);
void processPacket(tPacketChecker * checker, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId, tPacket * packet);
void checkAllPacketsReceived(tPacketChecker * checker);

#endif
