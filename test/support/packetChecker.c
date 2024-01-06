
#include "unity.h"
#include "simulator.h"
#include "microbus.h"

void initPacketChecker(tPacketChecker * checker) {
    // Start the IDs from 1. Any id of 0 is invalid
    checker->currentId = 1;
}

tPacket * createPacket(tPacketChecker * checker, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId) {
    // Find next free packet entry
    for (uint32_t i=0; i<MAX_PACKET_CHECKER_PACKETS; i++) {
        tPacketCheckerPacket * pkt = &checker->txPackets[i];
        if (!pkt->valid) {
            pkt->valid = true;
            pkt->id = checker->currentId;
            pkt->srcNodeSimId = srcNodeSimId;
            pkt->dstNodeSimId = dstNodeSimId;
            // data 0 is the dest node id
            // data 1 is the src node id
            myAssert(sizeof(pkt->id) < PACKET_SIZE-2, "ID too big for packet");
            memcpy(&pkt->packet.data[2], &pkt->id, sizeof(pkt->id));
            checker->currentId++;
            //printf("Creating packet %d -> %d, id:%d\n", srcNodeSimId, dstNodeSimId, pkt->id);
            return &pkt->packet;
        }
    }
    myAssert(0, "All tx packets entries are full");
    return NULL;
}

void processPacket(tPacketChecker * checker, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId, tPacket * packet) {
    uint32_t id = 0;
    memcpy(&id, &packet->data[2], sizeof(id));
    for (uint32_t i=0; i<MAX_PACKET_CHECKER_PACKETS; i++) {
        tPacketCheckerPacket * pkt = &checker->txPackets[i];
        if (pkt->valid 
            && (pkt->id == id)
            && (pkt->srcNodeSimId == srcNodeSimId)
            && (pkt->dstNodeSimId == dstNodeSimId)
            ) {
            checker->numCorrectRxPackets++;
            pkt->valid = false;
            //printf("Packet received %d -> %d, id:%d\n", srcNodeSimId, dstNodeSimId, id);
            return;
        }
    }
    printf("Failed: Packet received %d -> %d, id:%d\n", srcNodeSimId, dstNodeSimId, id);
    myAssert(0, "Failed to match rx packet to tx packet");
}

void checkAllPacketsReceived(tPacketChecker * checker) {
    bool failed = false;
    for (uint32_t i=0; i<MAX_PACKET_CHECKER_PACKETS; i++) {
        if (checker->txPackets[i].valid) {
            //printf("Packet failed %d -> %d, id:%d\n", checker->txPackets[i].srcNodeSimId, checker->txPackets[i].dstNodeSimId, checker->txPackets[i].id);
            failed = true;
        }
    }
    if (failed) {
        TEST_ASSERT_EQUAL_UINT(1, 0);
    }
}

