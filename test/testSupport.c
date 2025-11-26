// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "../src/microbus.h"
#include "../src/master.h"
#include "../src/node.h"

#include "../src/scheduler.h"
#include "../src/networkManager.h"
#include "../src/txManager.h"

#include "packetChecker.h"

static const tPacket nullPacket = {0};

void * myMalloc(size_t numBytes) {
    void * result = malloc(numBytes);
    assert(result);
    return result;
}


#define DEFAULT_SINGLE_CHANNEL_NUM_TX_NODES_SCHEDULED 4
tMaster * createMaster(uint32_t txQueueSize, uint32_t rxQueueSize, bool singleChannel) {
    tMaster * master = myMalloc(sizeof(tMaster));
    tPacketEntry * txPacketEntries = myMalloc(txQueueSize * sizeof(tPacketEntry));
    tPacketEntry * rxPackets = myMalloc(rxQueueSize * sizeof(tPacketEntry));
    tPacketEntry ** rxPacketQueue = myMalloc(rxQueueSize * sizeof(tPacketEntry *));
    uint8_t numTxNodesScheduled = singleChannel ? DEFAULT_SINGLE_CHANNEL_NUM_TX_NODES_SCHEDULED : 1;
    masterInit(master, numTxNodesScheduled, txQueueSize, txPacketEntries, rxQueueSize, rxPackets, rxPacketQueue);
    return master;
}

tNode * createNode(uint32_t txQueueSize, uint32_t rxQueueSize, uint64_t uniqueId) {
    tNode * node = myMalloc(sizeof(tNode));
    assert(node);
    tPacketEntry * txPacketEntries = myMalloc(txQueueSize * sizeof(tPacketEntry));
    tPacketEntry * rxPackets = myMalloc(rxQueueSize * sizeof(tPacketEntry));
    tPacketEntry ** rxPacketQueue = myMalloc(rxQueueSize * sizeof(tPacketEntry *));
    if (uniqueId == 0) {
        uniqueId = ((uint64_t)rand() << 32) | (uint64_t)rand();
    }
    nodeInit(node, uniqueId, txQueueSize, txPacketEntries, rxQueueSize, rxPackets, rxPacketQueue);
    return node;
}

void freeNode(tNode * node) {
    free(node->txManager.packetStore.entries);
    free(node->rxPacketManager.rxPacketEntries);
    free(node->rxPacketManager.rxPacketQueue);
    free(node);
}

void freeMaster(tMaster * master) {
    free(master->tx.txManager.packetStore.entries);
    free(master->rx.rxPacketManager.rxPacketEntries);
    free(master->rx.rxPacketManager.rxPacketQueue);
    free(master);
}

void initSystem(tPacketChecker * checker, tMaster ** master, tNode * nodes[MAX_NODES], uint32_t numNodes, bool singleChannel) {
    cycleIndex = 0;
    initPacketChecker(checker);
    *master = createMaster(10, 10, singleChannel); // 20*300 = 6KB

    for (uint32_t i=1; i<numNodes+1; i++) {
        nodes[i] = createNode(5, 4, 0); // 8*300 = 2.4KB
    }
}

// =================================================== //

void checkAllNodesHaveDistinctIds(uint8_t i, tNode * nodes[], uint32_t numNodes) {
    // Check all nodes have distinct IDs
    for (uint32_t n=1; n<numNodes; n++) {
        if (n != i && nodes[n]->nodeId != UNALLOCATED_NODE_ID) {
            if (nodes[i]->nodeId == nodes[n]->nodeId) {
                if ((nodes[i]->timeToLive > 0) && (nodes[n]->timeToLive > 0)) {
                    MB_PRINTF("node:%u, i:%u, n:%u, ttl:%d, %d", nodes[i]->nodeId, i, n, nodes[i]->timeToLive, nodes[n]->timeToLive);
                    microbusAssert(nodes[i]->nodeId != nodes[n]->nodeId, "");
                }
            }
        }
    }
}


void runSingleChannel(tMaster * master, tNode * nodes[], bool ignoreNodes[], uint32_t numNodes, uint32_t numFrames, bool allowNodeTxOverlaps) {
    for (uint32_t j=0; j<numFrames; j++) {
        cycleIndex++;
        
        tPacket * nodeTxData = NULL;
        uint32_t numNodeTxPackets = 0;
        tPacket * masterTxPacket = NULL;
        bool masterTx = master->currentTxNodeId == MASTER_NODE_ID;

        masterUpdateTimeUs(master, SLOT_TIME_US);

        if (masterTx) {
            masterNoDelaySingleChannelProcessTx(master, &masterTxPacket);
            MB_PRINTF("Master Tx mode, txPacket:%u\n", masterTxPacket != NULL);
            if (masterTxPacket != NULL) {
                numNodeTxPackets++;
            }
        }

        for (uint32_t i=0; i<numNodes; i++) {
            tNode * node = nodes[i];
            tPacket * nodeTxPacket = NULL;

            nodeUpdateTimeUs(node, SLOT_TIME_US);

            if (ignoreNodes == NULL || !ignoreNodes[i]) {
                bool nodeTx = nodeIsTxMode(node);

                if (nodeTx) {
                    nodeNoDelaySingleChannelProcessTx(node, &nodeTxPacket);
                    MB_PRINTF("node Tx mode:%u, txPacket:%u\n", node->nodeId, nodeTxPacket != NULL);

                    if (nodeTxPacket != NULL) {
                        if (masterTxPacket != NULL) {
                            MB_PRINTF("Single channel master and node overlap occurred\n");
                            assert(0);
                        }

                        numNodeTxPackets++;
                        nodeTxData = nodeTxPacket;

                        if(numNodeTxPackets > 1) {
                            if (MICROBUS_LOGGING) {
                                MB_PRINTF("Overlap occurred\n");
                            }
                            if (allowNodeTxOverlaps) {
                                nodeTxData = NULL;
                            } else {
                                assert(0);
                            }
                        }
                    }
                } else {
                    tPacket * nodeRxPacket = NULL;
                    nodeRxPacket = nodeGetRxPacketMemory(node);
                    // Record Master -> Node packet
                    memcpy(nodeRxPacket, masterTxPacket ? masterTxPacket : &nullPacket, sizeof(tPacket));
                    nodeNoDelaySingleChannelProcessRx(node, false);
                }
            }
            checkAllNodesHaveDistinctIds(i, nodes, numNodes);
        }

        // Record Node -> Master packet
        if (!masterTx) {
            tPacket * masterRxPacket = masterGetRxPacketMemory(master);
            memcpy(masterRxPacket, nodeTxData ? nodeTxData : &nullPacket, sizeof(tPacket));
            masterNoDelaySingleChannelProcessRx(master, false);
        }
    }
}

void runDualChannel(tMaster * master, tNode * nodes[], bool ignoreNodes[], uint32_t numNodes, uint32_t numFrames, bool allowNodeTxOverlaps) {
    for (uint32_t j=0; j<numFrames; j++) {
        cycleIndex++;
        
        tPacket * nodeTxData = NULL;
        uint32_t numNodeTxPackets = 0;

        tPacket * masterRxPacket = NULL;
        tPacket * masterTxPacket = NULL;
        masterUpdateTimeUs(master, SLOT_TIME_US);
        // Process previous packet
        masterDualChannelPipelinedPostProcess(master);
        // Get next packets
        masterDualChannelPipelinedPreProcess(master, &masterTxPacket, &masterRxPacket, false);

        for (uint32_t i=0; i<numNodes; i++) {
            tNode * node = nodes[i];
            tPacket * nodeTxPacket = NULL;
            tPacket * nodeRxPacket = NULL;

            nodeUpdateTimeUs(node, SLOT_TIME_US);

            if (ignoreNodes == NULL || !ignoreNodes[i]) {
                // Process previous packet
                nodeDualChannelPipelinedPostProcess(node);
                // Get next packets
                nodeDualChannelPipelinedPreProcess(node, &nodeTxPacket, &nodeRxPacket, false);

                if (nodeTxPacket != NULL) {
                    numNodeTxPackets++;
                    nodeTxData = nodeTxPacket;

                    if(numNodeTxPackets > 1) {
                        if (MICROBUS_LOGGING) {
                            MB_PRINTF("Overlap occurred\n");
                        }
                        if (allowNodeTxOverlaps) {
                            nodeTxData = NULL;
                        } else {
                            assert(0);
                        }
                    }
                }

                // Record Master -> Node packet
                memcpy(nodeRxPacket, masterTxPacket ? masterTxPacket : &nullPacket, sizeof(tPacket));
            }

            checkAllNodesHaveDistinctIds(i, nodes, numNodes);
        }

        // Record Node -> Master packet
        memcpy(masterRxPacket, nodeTxData ? nodeTxData : &nullPacket, sizeof(tPacket));
    }
}


void run(tMaster * master, tNode * nodes[], bool ignoreNodes[], uint32_t numNodes, uint32_t numFrames, bool allowNodeTxOverlaps, bool singleChannel) {
    if (singleChannel) {
        runSingleChannel(master, nodes, ignoreNodes, numNodes, numFrames, allowNodeTxOverlaps);
    } else {
        runDualChannel(master, nodes, ignoreNodes, numNodes, numFrames, allowNodeTxOverlaps);
    }
}


void runUntilAllNodesOnNetwork(tMaster ** master, tNode * nodes[MAX_NODES], uint32_t numNodes, bool disableLogging, bool singleChannel) {
    if (disableLogging) {
        loggingEnabled = false;
    }
    // Run until nodes are on the network
    for (uint32_t j=0; j<300*numNodes; j++) {
        run(*master, &nodes[1], NULL, numNodes, 1, true, singleChannel);
        // Check if all nodes allocated
        bool allAllocated = true;
        for (uint32_t i=1; i<numNodes+1; i++) {
            if (nodes[i]->nodeId == UNALLOCATED_NODE_ID || nodes[i]->timeToLive <= 0) {
                allAllocated = false;
            }
        }
        if (allAllocated) {
            break;
        }
    }
    for (uint32_t i=1; i<numNodes+1; i++) {
        assert(nodes[i]->nodeId != UNALLOCATED_NODE_ID);
        assert(nodes[i]->timeToLive > 0);
    }
    if (disableLogging) {
        loggingEnabled = true;
    }
}


// =================================================== //

bool attemptMasterTxRandomPacket(tPacketChecker * checker, tMaster * master, tNode * nodes[], uint8_t dstSimNodeId, bool mightBeDropped) {
    uint8_t * masterTxData = masterAllocateTxPacket(master);
    if (masterTxData == NULL) {
        return false;
    }
    uint16_t size = PACKET_CHECKER_HEADER + rand() % (MASTER_PACKET_DATA_SIZE - PACKET_CHECKER_HEADER);
    uint8_t * data = createTxPacket(checker, true, 0, dstSimNodeId, size, mightBeDropped);
    memcpy(masterTxData, data, size);
    // MB_PRINTF("Checker - created master -> %u (%u) packet: ", nodes[dstSimNodeId]->nodeId, size);
    // for (uint32_t i=0; i<10; i++) {
    //     MB_PRINTF_WITHOUT_NEW_LINE("%02x", data[i]);
    // }
    // MB_PRINTF_WITHOUT_NEW_LINE("\n");
    masterSubmitAllocatedTxPacket(master, nodes[dstSimNodeId]->nodeId, size);
    return true;
}

bool attemptNodeTxRandomPacket(tPacketChecker * checker, tNode * node, uint8_t srcSimNodeId, bool mightBeDropped) {
    uint8_t * nodeTxData = nodeAllocateTxPacket(node);
    if (nodeTxData == NULL) {
        return false;
    }
    uint16_t size = PACKET_CHECKER_HEADER + rand() % (NODE_PACKET_DATA_SIZE - PACKET_CHECKER_HEADER);
    uint8_t * data = createTxPacket(checker, false, srcSimNodeId, 0, size, mightBeDropped);
    memcpy(nodeTxData, data, size);
    nodeSubmitAllocatedTxPacket(node, 0, size);
    return true;
}

void fillTxBuffersWithRandomPackets(
        uint32_t numPackets,
        tPacketChecker * checker,
        tMaster * master,
        tNode * nodes[MAX_NODES],
        uint32_t numNodes,
        uint32_t packetsSent[MAX_NODES],
        uint32_t * numPacketsSent,
        bool onlyMaster,
        bool onlyFirstNode,
        bool mightBeDropped) {
            
    if (!onlyFirstNode) {
        while ((*numPacketsSent) < numPackets) {
        // Add master packet if we can
            uint8_t dstSimNodeId = 1+(rand() % numNodes);
            bool txSucess = attemptMasterTxRandomPacket(checker, master, nodes, dstSimNodeId, mightBeDropped);
            if (txSucess) {
                (*numPacketsSent)++;
                packetsSent[0]++;
            } else {
                break;
            }
        }
    }
    
    // Add node packet if we can
    if (!onlyMaster) {
        for (uint32_t i=1; i<numNodes+1; i++) {
            if (i == 1 || !onlyFirstNode) {
                while ((*numPacketsSent) < numPackets) {
                    bool txSucess = attemptNodeTxRandomPacket(checker, nodes[i], i, mightBeDropped);
                    if (txSucess) {
                        (*numPacketsSent)++;
                        packetsSent[i]++;
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

bool areAllTxBuffersEmpty(tMaster * master, tNode * nodes[MAX_NODES], uint32_t numNodes, bool printFalse) {
    for (uint32_t i=1; i<numNodes+1; i++) {
        if (nodes[i]->timeToLive > 0) {
            uint32_t num;
            num = getNumInTxBuffer(&master->tx.txManager, nodes[i]->nodeId);
            if (num > 0) {
                MB_PRINTF("Master -> Node:%u, ttl:%u, numInBuffer:%u\n", nodes[i]->nodeId, nodes[i]->timeToLive, num);
                return false;
            }
            num = getNumInTxBuffer(&nodes[i]->txManager, MASTER_NODE_ID);
            if (num > 0) {
                MB_PRINTF("Node:%u -> Master, ttl:%u, numInBuffer:%u\n", nodes[i]->nodeId, nodes[i]->timeToLive, num);
                return false;
            }
        }
    }
    return true;
}

// =================================================== //


void getMasterRxData(tMaster * master, uint8_t * rxData, uint8_t numBytes) {
    uint16_t size;
    tNodeIndex srcNodeId;
    uint8_t * data = masterPeekNextRxDataPacket(master, &size, &srcNodeId);
    assert(data);
    memcpy(rxData, data, numBytes);
    masterPopNextDataPacket(master);
}

void getNodeRxData(tNode * node, uint8_t * rxData, uint8_t numBytes) {
    uint16_t size;
    tNodeIndex srcNodeId;
    uint8_t * data = nodePeekNextRxDataPacket(node, &size, &srcNodeId);
    memcpy(rxData, data, numBytes);
    nodePopNextDataPacket(node);
}

uint32_t processAllRxData(tPacketChecker * checker, tMaster * master, tNode * nodes[MAX_NODES], uint32_t numNodes) {
    uint32_t numReceived = 0;
    // Process any Master Rx packets
    while (true) {
        uint16_t size;
        tNodeIndex srcNodeId;
        uint8_t * masterRxData = masterPeekNextRxDataPacket(master, &size, &srcNodeId);
        if (masterRxData) {
            processRxPacket(checker, masterRxData, size);
            masterPopNextDataPacket(master);
            numReceived++;
        } else {
            break;
        }
    }
    // Process any Nodes Rx packets
    for (uint32_t i=1; i<numNodes+1; i++) {
        while (true) {
            uint16_t size;
            tNodeIndex srcNodeId;
            uint8_t * nodeRxData = nodePeekNextRxDataPacket(nodes[i], &size, &srcNodeId);
            if (nodeRxData) {
                processRxPacket(checker, nodeRxData, size);
                nodePopNextDataPacket(nodes[i]);
                numReceived++;
            } else {
                break;
            }
        }
    }
    return numReceived;
}