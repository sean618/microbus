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
#include "testSupport.h"

bool disableAllocationLogging = false;

// 52 in 2000 - master tx empty (2.75)
// 32 in 2000 - master allocation (1.6)
// 55 in 2000 - restarting window (2.75)
void test_fully_loaded_system(
        uint32_t numNodes,
        uint32_t numPackets,
        uint32_t numFramesToRun,
        uint32_t expMasterBw,
        uint32_t expTotalNodeBw,
        bool singleChannel, 
        bool onlyMaster,
        bool onlyFirstNode) {

    MB_PRINTF("test_fully_loaded_system, numNodes: %u, numPackets: %u, numFramesToRun: %u\n", numNodes, numPackets, numFramesToRun);
    tPacketChecker checker = {0};
    tMaster * master;
    tNode * nodes[MAX_NODES];
    initSystem(&checker, &master, nodes, numNodes, singleChannel);

    runUntilAllNodesOnNetwork(&master, nodes, numNodes, disableAllocationLogging, singleChannel);

    // Run for a while to ensure allocation BW is small
    loggingEnabled = false;
    run(master, &nodes[1], NULL, numNodes, NUM_SLOTS_BEFORE_ALLOCATION_CHANGED * 64 * 2, false, singleChannel);
    loggingEnabled = true;

    uint32_t packetsSent[MAX_NODES] = {0};
    uint32_t numPacketsSent = 0;
    uint32_t numPacketsReceived = 0;
    uint32_t framesRun = 0;

    for (uint32_t i=0; i<numFramesToRun; i++) {
        // MB_PRINTF("Frame: %u\n", i);
        fillTxBuffersWithRandomPackets(numPackets, &checker, master, nodes, numNodes, packetsSent, &numPacketsSent, onlyMaster, onlyFirstNode, false);

        // Run simulation for 1 frame
        framesRun++;
        run(master, &nodes[1], NULL, numNodes, 1, false, singleChannel);

        if (i == 500) {
            MB_PRINTF("Test point\n");
        }

        numPacketsReceived += processAllRxData(&checker, master, nodes, numNodes);

        if ((numPacketsReceived >= numPackets) && (numPacketsReceived == numPacketsSent)) {
            if (areAllTxBuffersEmpty(master, nodes, numNodes, false)) {
                break;
            }
        }
    }
    // assert(areAllTxBuffersEmpty(master, nodes, numNodes, false));
    checkAllPacketsReceived(&checker);

    // for (uint32_t i=1; i<numNodes+1; i++) {
    //     printf("Node %u, BW: %u%%\n", i, (100*packetsSent[i]) / framesRun);
    // }
    uint32_t masterBw = (100*packetsSent[0]) / framesRun;
    uint32_t totalNodeBw = (100*(numPackets - packetsSent[0])) / framesRun;
    printf("Master BW: %u%%\n", masterBw);
    printf("Total Node BW: %u%%\n", totalNodeBw);

    assert(masterBw >= expMasterBw);
    assert(totalNodeBw >= expTotalNodeBw);
}

void test_full_system_with_rx_buffer_overflows(uint32_t numNodes, uint32_t numPackets, uint32_t numFramesToRun, bool singleChannel) {
    printf("test_full_system_with_rx_buffer_overflows, numNodes: %u, numPackets: %u, numFramesToRun: %u\n", numNodes, numPackets, numFramesToRun);
    tPacketChecker checker = {0};
    tMaster * master;
    tNode * nodes[MAX_NODES];
    initSystem(&checker, &master, nodes, numNodes, singleChannel);
    runUntilAllNodesOnNetwork(&master, nodes, numNodes, disableAllocationLogging, singleChannel);

    uint32_t packetsSent[MAX_NODES] = {0};
    uint32_t numPacketsSent = 0;
    uint32_t numPacketsReceived = 0;
    uint32_t framesRun = 0;

    for (uint32_t i=0; i<numFramesToRun; i++) {
        fillTxBuffersWithRandomPackets(numPackets, &checker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);

        // Run simulation for 1 frame
        framesRun += 10;
        run(master, &nodes[1], NULL, numNodes, 10, false, singleChannel);

        numPacketsReceived += processAllRxData(&checker, master, nodes, numNodes);

        if ((numPacketsReceived >= numPackets) && (numPacketsReceived == numPacketsSent)) {
            if (areAllTxBuffersEmpty(master, nodes, numNodes, false)) {
                break;
            }
        }
    }
    assert(areAllTxBuffersEmpty(master, nodes, numNodes, true));
    checkAllPacketsReceived(&checker);
}

void test_full_system_with_master_restart(uint32_t numNodes, bool singleChannel) {
    printf("test_full_system_with_master_restart, numNodes: %u\n", numNodes);

    tPacketChecker checker = {0};
    tMaster * master;
    tNode * nodes[MAX_NODES];
    initSystem(&checker, &master, nodes, numNodes, singleChannel);
    runUntilAllNodesOnNetwork(&master, nodes, numNodes, disableAllocationLogging, singleChannel);

    uint32_t packetsSent[MAX_NODES] = {0};
    uint32_t numPacketsSent = 0;
    uint32_t numPacketsReceived = 0;
    uint32_t framesRun = 0;

    // Frames will be dropped - so use a temporary packet checker
    tPacketChecker notUsedChecker = {0};
    initPacketChecker(&notUsedChecker);
    
    // Run for a bit 
    for (uint32_t i=0; i<100; i++) {
        fillTxBuffersWithRandomPackets(200, &notUsedChecker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);
        framesRun += 1;
        run(master, &nodes[1], NULL, numNodes, 1, false, singleChannel);
        numPacketsReceived += processAllRxData(&notUsedChecker, master, nodes, numNodes);
    }

    // Reset the master
    freeMaster(master);
    master = createMaster(10, 10, singleChannel); // 20*300 = 6KB

    // Run until master has registered all nodes
    bool disconnected[MAX_NODES] = {0};
    for (uint32_t i=0; i<2000; i++) {
        run(master, &nodes[1], NULL, numNodes, 1, true, false);

        for (tNodeIndex node=1; node<numNodes+1; node++) {
            if (nodes[node]->nodeId == UNALLOCATED_NODE_ID) {
                disconnected[node] = true;
            }
        }
        
        uint32_t numConnected = 0;
        uint8_t connectedNodesBitfield[MAX_NODES/8] = {0};
        getConnectedNodesBitField(master, connectedNodesBitfield);
        for (uint32_t byte=0; byte<MAX_NODES/8; byte++) {
            for (uint32_t bit=0; bit<8; bit++) {
                bool connected = 0x1 & (connectedNodesBitfield[byte] >> bit);
                if (connected) {
                    numConnected++;
                }
            }
        }
        if (numConnected == numNodes+1) {
            break;
        }
    }

    // Check all nodes were disconnected
    for (tNodeIndex node=1; node<numNodes+1; node++) {
        assert(disconnected[node]);
    }

    // Run for a bit
    {
        uint32_t numPacketsSent = 0;
        uint32_t numPacketsReceived = 0;
        uint32_t numPackets = 400;
        uint32_t packetsSent[MAX_NODES] = {0};
        for (uint32_t i=0; i<1500; i++) {
            fillTxBuffersWithRandomPackets(numPackets, &checker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);
            framesRun += 1;
            run(master, &nodes[1], NULL, numNodes, 1, false, false);
            numPacketsReceived += processAllRxData(&checker, master, nodes, numNodes);
            if ((numPacketsReceived >= numPackets) && (numPacketsReceived == numPacketsSent)) {
                if (areAllTxBuffersEmpty(master, nodes, numNodes, false)) {
                    break;
                }
            }
        }
    }

    assert(areAllTxBuffersEmpty(master, nodes, numNodes, true));
    checkAllPacketsReceived(&checker);
}

void test_full_system_with_node_restart(uint32_t numNodes, bool singleChannel) {
    printf("test_full_system_with_node_restart, numNodes: %u\n", numNodes);

    tPacketChecker checker = {0};
    tMaster * master;
    tNode * nodes[MAX_NODES];
    initSystem(&checker, &master, nodes, numNodes, singleChannel);
    runUntilAllNodesOnNetwork(&master, nodes, numNodes, disableAllocationLogging, singleChannel);

    uint32_t packetsSent[MAX_NODES] = {0};
    uint32_t numPacketsSent = 0;
    uint32_t numPacketsReceived = 0;
    uint32_t framesRun = 0;

    // Frames will be dropped - so use a temporary packet checker
    tPacketChecker notUsedChecker = {0};
    initPacketChecker(&notUsedChecker);
    
    // Run for a bit 
    for (uint32_t i=0; i<200; i++) {
        fillTxBuffersWithRandomPackets(200, &notUsedChecker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, true); // These packets might be lost during the restart
        framesRun += 1;
        run(master, &nodes[1], NULL, numNodes, 1, false, singleChannel);
        numPacketsReceived += processAllRxData(&notUsedChecker, master, nodes, numNodes);
    }

    MB_PRINTF("Restarting Nodes\n");

    // Restart/re-create the nodes
    for (uint32_t i=1; i<numNodes+1; i++) {
        freeNode(nodes[i]);
        nodes[i] = createNode(4, 4, 0); // 8*300 = 2.4KB
    }

    // Run until master has registered all nodes
    runUntilAllNodesOnNetwork(&master, nodes, numNodes, false, singleChannel);
    processAllRxData(&notUsedChecker, master, nodes, numNodes);

    MB_PRINTF("Nodes should all be on the network\n");

    // Run for a bit
    {
        uint32_t numPacketsSent = 0;
        uint32_t numPacketsReceived = 0;
        uint32_t numPackets = 400;
        uint32_t packetsSent[MAX_NODES] = {0};
        for (uint32_t i=0; i<1500; i++) {
            fillTxBuffersWithRandomPackets(numPackets, &checker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);
            framesRun += 1;
            run(master, &nodes[1], NULL, numNodes, 1, false, singleChannel);
            numPacketsReceived += processAllRxData(&checker, master, nodes, numNodes);
            if ((numPacketsReceived >= numPackets) && (numPacketsReceived == numPacketsSent)) {
                if (areAllTxBuffersEmpty(master, nodes, numNodes, false)) {
                    break;
                }
            }
        }
    }

    assert(areAllTxBuffersEmpty(master, nodes, numNodes, true));
    checkAllPacketsReceived(&checker);
}

void test_full_system_with_nodes_disconnecting(uint32_t numNodes, bool singleChannel) {
    MB_PRINTF("test_full_system_with_nodes_disconnecting, numNodes: %u\n", numNodes);
    printf("test_full_system_with_nodes_disconnecting, numNodes: %u\n", numNodes);

    tPacketChecker checker = {0};
    tMaster * master;
    tNode * nodes[MAX_NODES];
    initSystem(&checker, &master, nodes, numNodes, singleChannel);
    runUntilAllNodesOnNetwork(&master, nodes, numNodes, disableAllocationLogging, singleChannel);

    uint32_t packetsSent[MAX_NODES] = {0};
    uint32_t numPacketsSent = 0;
    uint32_t numPacketsReceived = 0;
    uint32_t framesRun = 0;

    // Frames will be dropped - so use a temporary packet checker
    tPacketChecker notUsedChecker = {0};
    initPacketChecker(&notUsedChecker);
    
    // Run for a bit 
    for (uint32_t i=0; i<200; i++) {
        fillTxBuffersWithRandomPackets(200, &notUsedChecker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);
        framesRun += 1;
        run(master, &nodes[1], NULL, numNodes, 1, false, singleChannel);
        numPacketsReceived += processAllRxData(&notUsedChecker, master, nodes, numNodes);
    }


    for (uint32_t z=0; z<10; z++) {
        bool ignoreNodes[MAX_NODES] = {0};
        uint32_t numConnectedNodes = 0;
        for (uint32_t i=1; i<numNodes+1; i++) {
            if (rand() % 2) {
                ignoreNodes[i]= true;
            } else {
                numConnectedNodes++;
            }
        }

        // // Process any rx frames on ignored nodes before they are ignored
        // // To ensure that there are no historic new node responses that get applied later
        // for (uint32_t n=1; n<numNodes+1; n++) {
        //     if (ignoreNodes[n]) {
        //         for (uint32_t i=0; i<20; i++) {
        //             tPacket * txPacket;
        //             tPacket * rxPacket;
        //             nodeDualChannelPipelinedPostProcess(nodes[n]);
        //             nodeDualChannelPipelinedPreProcess(nodes[n], &txPacket, &rxPacket, false);
        //         }
        //     }
        // }
        
        
        uint32_t numFramesToRun = rand() % 20000;
        for (uint32_t i=0; i<numFramesToRun; i++) {
            fillTxBuffersWithRandomPackets(200, &notUsedChecker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);
            framesRun += 1;
            run(master, &nodes[1], ignoreNodes, numNodes, 1, true, singleChannel);
            numPacketsReceived += processAllRxData(&notUsedChecker, master, nodes, numNodes);
        }
    }
    
    // Run for long enough that any nodes will have left the network
    run(master, &nodes[1], NULL, numNodes, 3000, true, false);

    // Run until master has registered all nodes
    runUntilAllNodesOnNetwork(&master, nodes, numNodes, disableAllocationLogging, singleChannel);

    // Run for a bit
    {
        uint32_t numPacketsSent = 0;
        uint32_t numPacketsReceived = 0;
        uint32_t numPackets = 400;
        uint32_t packetsSent[MAX_NODES] = {0};
        for (uint32_t i=0; i<4000; i++) {
            fillTxBuffersWithRandomPackets(numPackets, &checker, master, nodes, numNodes, packetsSent, &numPacketsSent, false, false, false);
            framesRun += 1;
            run(master, &nodes[1], NULL, numNodes, 1, false, singleChannel);
            numPacketsReceived += processAllRxData(&checker, master, nodes, numNodes);
            if ((numPacketsReceived >= numPackets) && (numPacketsReceived == numPacketsSent)) {
                if (areAllTxBuffersEmpty(master, nodes, numNodes, false)) {
                    break;
                }
            }
        }
    }

    assert(areAllTxBuffersEmpty(master, nodes, numNodes, true));
    checkAllPacketsReceived(&checker);
}


// ============================================= //

void testMicrobus() {
    // single channel

    test_fully_loaded_system(1, 1000, 4000, 0, 70, true, false, true); // Only node - Master needs to run every 4 cycles - would expect to get around 70% for node

    test_fully_loaded_system(1, 1000, 4000, 40, 40, true, false, false);
    test_fully_loaded_system(1, 1000, 4000, 60, 0, true, true, false); // Only master - 65% is the best we can get with rxAcks every 4 and servicing every 6
    test_fully_loaded_system(4, 1000, 4000, 40, 0, true, true, false); // Only master - 40% is the best we can get with as acks are occuring every other turn

    // Dual channel

    test_fully_loaded_system(1, 1000, 2000, 95, 95, false, false, false);

    test_fully_loaded_system(10, 4000, 3000, 80, 95, false, false, false);

    test_full_system_with_master_restart(1, false);
    test_full_system_with_master_restart(10, false);

    test_full_system_with_node_restart(1, false);
    test_full_system_with_node_restart(10, false);

    test_full_system_with_nodes_disconnecting(10, false);
    test_full_system_with_nodes_disconnecting(62, false);

    test_full_system_with_rx_buffer_overflows(1, 1000, 2000, false);

}

