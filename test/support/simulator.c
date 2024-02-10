#include "stdlib.h"
#include "unity.h"
#include "simulator.h"
#include "packetChecker.h"

// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

tNodeIndex getSimNodeId(tSimulation * sim, tNodeIndex nodeId) {
    for (uint32_t n=0; n<sim->numNodes; n++) {
        if (sim->nodes[n].nodeId == nodeId) {
            return n;
        }
    }
    myAssert(0, "Failed to find nodeId from simNodeId\n");
}


// ======================================== //

void simInit(tSimulation * sim) {
    memset(sim, 0, sizeof(tSimulation));
    initPacketChecker(&sim->packetChecker);
}

tMasterNode * simCreateMaster(tSimulation * sim, uint64_t uniqueId) {
    myAssert(sim->masterInfo.valid == false, "Can only create one master");
    sim->masterInfo.valid = true;
    memset(&sim->master, 0, sizeof(sim->master));
    sim->master.simId = MASTER_NODE_SIM_ID;
    sim->master.uniqueId = uniqueId;
    return &sim->master;
}

tNode * simCreateSlave(tSimulation * sim, uint64_t uniqueId) {
    tNodeIndex nodeSimIndex = sim->numNodes;
    memset(&sim->nodes[nodeSimIndex], 0, sizeof(sim->nodes[nodeSimIndex]));
    sim->nodeInfo[nodeSimIndex].valid = true;
    sim->nodes[nodeSimIndex].simId = nodeSimIndex;
    sim->nodes[nodeSimIndex].uniqueId = uniqueId;
    sim->nodes[nodeSimIndex].nodeId = INVALID_NODE_ID;
    sim->numNodes++;
    return &sim->nodes[nodeSimIndex];
}

tPacket * simMasterCreateTxPacket(tSimulation * sim, tNodeIndex dstNodeSimId) {
    tPacket * packet = createPacket(&sim->packetChecker, MASTER_NODE_SIM_ID, dstNodeSimId);
    tNodeIndex dstNodeId = sim->nodes[dstNodeSimId].nodeId;
    myAssert(dstNodeId != INVALID_NODE_ID, "Trying to create packets to nodes not yet assigned a node ID");
    addMasterTxPacket(&sim->master, dstNodeId, packet);
    return packet;
}

tPacket * simSlaveCreateTxPacket(tSimulation * sim, tNodeIndex srcNodeSimId) {
    tPacket * packet = createPacket(&sim->packetChecker, srcNodeSimId, MASTER_NODE_SIM_ID);
    addSlaveTxPacket(&sim->nodes[srcNodeSimId], packet);
    return packet;
}

void processRxPacket(tSimulation * sim, tNodeIndex rxNodeId, tPacket * packet) {
    tNodeIndex dstNodeId = packet->data[0];
    tNodeIndex srcNodeId = packet->data[1];
    myAssert(dstNodeId == rxNodeId, "Node destination doesn't match\n");
    // Search for matching nodeId
    tNodeIndex srcSimNodeId = getSimNodeId(sim, srcNodeId);
    tNodeIndex dstSimNodeId = getSimNodeId(sim, dstNodeId);
    processPacket(&sim->packetChecker, srcSimNodeId, dstSimNodeId, packet);
}

void simulate() {
    
}

void simCheckAllPacketsReceived(tSimulation * sim) {
    checkAllPacketsReceived(&sim->packetChecker);
}