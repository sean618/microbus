// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "stdint.h"
#include "stdlib.h"
#include "assert.h"

#include "../src/microbus.h"
#include "packetChecker.h"

void initPacketChecker(tPacketChecker * checker) {
    // Start the IDs from 1. Any id of 0 is invalid
    checker->currentId = 1;
}

uint8_t * createTxPacket(tPacketChecker * checker, bool masterTx, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId, uint16_t size, bool mightBeDropped) {
    microbusAssert(size <= (masterTx ? MASTER_PACKET_DATA_SIZE : NODE_PACKET_DATA_SIZE), "");
    microbusAssert(size >= PACKET_CHECKER_HEADER, "");
    
    // Find next free packet entry
    for (uint32_t i=0; i<MAX_PACKET_CHECKER_PACKETS; i++) {
        tPacketCheckerPacket * pkt = &checker->txPackets[i];
        if (!pkt->valid) {
            pkt->valid = true;
            pkt->mightBeDropped = mightBeDropped;
            pkt->size = size;
            pkt->id = checker->currentId;
            pkt->srcNodeSimId = srcNodeSimId;
            pkt->dstNodeSimId = dstNodeSimId;
            pkt->data[0] = srcNodeSimId;
            pkt->data[1] = dstNodeSimId;
            memcpy(&pkt->data[2], &pkt->id, 4);
            for (uint32_t i=0; i<size-PACKET_CHECKER_HEADER; i++) {
                pkt->data[6+i] = rand() % 256;
            }

            checker->currentId++;
            // MB_PRINTF("Creating packet %d -> %d, id:%d\n", srcNodeSimId, dstNodeSimId, pkt->id);
            return pkt->data;
        }
    }
    microbusAssert(0, ""); // All tx packets entries are full
    return NULL;
}

void processRxPacket(tPacketChecker * checker, uint8_t * data, uint16_t size) {
    uint32_t packetId;
    uint8_t srcNodeSimId = data[0];
    uint8_t dstNodeSimId = data[1];
    memcpy(&packetId, &data[2], 4);

    for (uint32_t i=0; i<MAX_PACKET_CHECKER_PACKETS; i++) {
        tPacketCheckerPacket * pkt = &checker->txPackets[i];
        if (pkt->valid 
            && (pkt->id == packetId)
            && (pkt->srcNodeSimId == srcNodeSimId)
            && (pkt->dstNodeSimId == dstNodeSimId)
            ) {
            microbusAssert(pkt->size == size, "");
            // Check contents
            for (uint32_t i=0; i<size; i++) {
                microbusAssert(pkt->data[i] == data[i], "");
            }
            checker->numCorrectRxPackets++;
            MB_PRINTF("Packet received %d -> %d, id:%d", srcNodeSimId, dstNodeSimId, packetId);
            MB_PRINTF_WITHOUT_NEW_LINE(", data:");
            for (uint32_t z=0; z<10; z++) {
                MB_PRINTF_WITHOUT_NEW_LINE("%02x", pkt->data[z]);
            }
            MB_PRINTF_WITHOUT_NEW_LINE("\n");


            // Free packet
            pkt->valid = false;
            return;
        }
    }
    printf("Failed - no packet matched: %d -> %d, id:%d\n", srcNodeSimId, dstNodeSimId, packetId);
    microbusAssert(0, "");
}

void checkAllPacketsReceived(tPacketChecker * checker) {
    bool failed = false;
    for (uint32_t i=0; i<MAX_PACKET_CHECKER_PACKETS; i++) {
        if (checker->txPackets[i].valid && !checker->txPackets[i].mightBeDropped) {
            MB_PRINTF("Failed - packet never received %d -> %d, id:%d", checker->txPackets[i].srcNodeSimId, checker->txPackets[i].dstNodeSimId, checker->txPackets[i].id);

            MB_PRINTF_WITHOUT_NEW_LINE(", data:");
            for (uint32_t z=0; z<10; z++) {
                MB_PRINTF_WITHOUT_NEW_LINE("%02x", checker->txPackets[i].data[z]);
            }
            MB_PRINTF_WITHOUT_NEW_LINE("\n");
            
            microbusAssert(0, "");
        }
    }
}

