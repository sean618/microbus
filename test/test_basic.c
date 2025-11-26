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

void test_max_nodes_new_node_allocation(void) {
    #define NUM_NODES (MAX_NODES-1)

    for (uint32_t z=0; z<10; z++) {
        tMaster * master = createMaster(2, 3, false);
        tNode * nodes[NUM_NODES];

        for (uint32_t i=0; i<NUM_NODES; i++) {
            nodes[i] = createNode(4, 3, 0);
        }

        uint32_t numFrames = 4000;
        
        // Run until nodes are on the network
        run(master, nodes, NULL, NUM_NODES, numFrames, true, false);

        for (uint32_t i=0; i<NUM_NODES; i++) {
            assert(nodes[i]->nodeId != UNALLOCATED_NODE_ID);
            freeNode(nodes[i]);
        }

        freeMaster(master);
    }
}

void test_packets_to_from_each_node(uint32_t numNodes, uint32_t numPacketsPerNode) {
    tMaster * master = createMaster(1+(numNodes*numPacketsPerNode), 2+(numNodes*numPacketsPerNode), false);
    tNode * nodes[MAX_NODES] = {};

    for (uint32_t i=0; i<numNodes; i++) {
        nodes[i] = createNode(2+numPacketsPerNode, 3+numPacketsPerNode, 0);
    }
    
    // Run until nodes are on the network
    run(master, nodes, NULL, numNodes, 4000, true, false);

    for (uint32_t i=0; i<numNodes; i++) {
        assert(nodes[i]->nodeId != UNALLOCATED_NODE_ID);
    }
    
    for (uint32_t i=0; i<numNodes; i++) {
        for (uint32_t z=0; z<numPacketsPerNode; z++) {
            // Add a packet on each side
            uint8_t * masterTxData = masterAllocateTxPacket(master);
            assert(masterTxData);
            masterTxData[0] = 0xAB;
            masterTxData[1] = i;
            masterTxData[2] = z;
            masterSubmitAllocatedTxPacket(master, nodes[i]->nodeId, 3);

            uint8_t * nodeTxData = nodeAllocateTxPacket(nodes[i]);
            assert(nodeTxData);
            nodeTxData[0] = 0xCD;
            nodeTxData[1] = i;
            nodeTxData[2] = z;
            nodeSubmitAllocatedTxPacket(nodes[i], 0, 3);
        }
    }
    
    run(master, nodes, NULL, numNodes, 1000, false, false);
    
    // Read the rx data and check it matches
    bool masterPacketReceived[MAX_NODES] = {0};
    for (uint32_t i=0; i<numNodes*numPacketsPerNode; i++) {
        uint8_t masterRxData[3];
        getMasterRxData(master, masterRxData, 3);
        assert(masterRxData[0] == 0xCD);
        masterPacketReceived[masterRxData[1]] = true;
    }
    for (uint32_t i=0; i<numNodes; i++) {
        for (uint32_t z=0; z<numPacketsPerNode; z++) {
            uint8_t nodeRxData[3];
            getNodeRxData(nodes[i], nodeRxData, 3);
            assert(nodeRxData[0] == 0xAB);
            assert(nodeRxData[1] == i);
            assert(nodeRxData[2] == z);

            assert(masterPacketReceived[i] == true);
        }

        // Check all tx buffers are now empty
        assert(0 == getNumInTxBuffer(&nodes[i]->txManager, MASTER_NODE_ID));
        assert(0 == getNumInTxBuffer(&master->tx.txManager, nodes[i]->nodeId));
    }
}