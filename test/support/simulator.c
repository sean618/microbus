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
// Keep events sorted by time
// All this memcpying and shifting everything is very inefficient if there are lost of events in the buffer
// Could improve by using a linked list or something else if we need to

static void addEvent(tSimulation * sim, tEvent * event) {
    if (sim->numEvents == MAX_EVENTS) {
        myAssert(0, "Event buffer full\n");
        return;
    }
    
    // Find the position to insert it
    // Insert at the end as default
    uint32_t index = sim->numEvents; 
    // Now search to see if there is an earlier time to insert it
    for (uint32_t i=0; i<sim->numEvents; i++) {
        if (event->timeNs < sim->events[i].timeNs) {
            index = i;
            break;
        }
    }
    // Shift all events up
    for (int32_t j=sim->numEvents; j>index; j--) {
        memcpy(&sim->events[j], &sim->events[j-1], sizeof(tEvent));
    }
    // Insert new event
    memcpy(&sim->events[index], event, sizeof(tEvent));
    sim->numEvents++;
}

static tEvent * peekNextEvent(tSimulation * sim) {
    if (sim->numEvents == 0) {
        return NULL;
    }
    return &sim->events[0];
}

static bool popEvent(tSimulation * sim) {
    if (sim->numEvents == 0) {
        return false;
    }
    
    // Shift all events down
    for (uint32_t i=1; i<sim->numEvents; i++) {
        memcpy(&sim->events[i-1], &sim->events[i], sizeof(tEvent));
    }
    sim->numEvents--;
    return true;
}

// ======================================== //
// HAL actions when event is actioned

static void psSet(tSimulation * sim, bool val) {
    myAssert(val != sim->psWire, "Setting PS wire but value not changing\n");
    if (val != sim->psWire) {
        sim->psWire = val;
        // This will fire the interrupts on all nodes
        for (uint32_t i=0; i<sim->numNodes; i++) {
            if (i != MASTER_NODE_ID) {
                myAssert(sim->nodes[i].simId == sim->nodes[i].nodeId, "");
                psLineInterrupt(&sim->nodes[i], sim->psWire);
            }
        }
    }
}

// Slaves get ready for the transfer
static void nodeTransferInitiated(tSimulation * sim, tEvent * event, bool tx) {
    if (sim->transferOccuring) {
        // Master is already transmitting 
        //event->node->partialRx = true; // TODO: work out how much is transferred
        myAssert(0, "Node started receiving after master started the transfer");
        if (tx) {
            myAssert(0, "Node started transmitting after master started the transfer");
        }
    }
    sim->nodeInfo[event->nodeSimId].rx = true;
    sim->nodeInfo[event->nodeSimId].tx = tx;
}

// At the start of the master transfer copy all master tx bytes into the MOSI buffer
// and OR in all the nodes tx bytes into the MISO buffer
static void masterTransferStarted(tSimulation * sim, tEvent * event) {
    sim->transferOccuring = true;
    
    // Set the bus to all high - as there will be pull-ups
    memset(&sim->misoData[0], 0xFF, sizeof(sim->misoData));
    memset(&sim->mosiData[0], 0xFF, sizeof(sim->mosiData));
    
    //sim->mosiDataBytes = event->data.numTransferBytes;
    
    uint32_t numTransferBytes = sim->nodeInfo[MASTER_NODE_ID].numTransferBytes;
    myAssert(numTransferBytes < MAX_TX_DATA_BYTES, "Master Tx data exceeded limit");
    
    bool slaveTx = false;
    for (uint32_t i=0; i<sim->numNodes; i++) {
        if (i == MASTER_NODE_ID) {
            // Put data on bus - Master to mosi
            memcpy(sim->mosiData, sim->nodeInfo[i].txData, numTransferBytes);
        } else {
            // Put data on bus - Nodes to miso
            if (sim->nodeInfo[i].tx) {
                myAssert(!slaveTx, "Corruption\n");
                slaveTx = true;
                myAssert(sim->nodeInfo[i].numTransferBytes < MAX_TX_DATA_BYTES, "Node Tx data exceeded limit");
                uint32_t numBytes = MIN(numTransferBytes, sim->nodeInfo[i].numTransferBytes);
                for (uint32_t j=0; j<numBytes; j++) {
                    sim->misoData[j] &= sim->nodeInfo[i].txData[j];
                }
                //sim->misoDataBytes = MAX(sim->misoDataBytes, sim->nodes[i].numTransferBytes);
                // TODO: assert if not enum packets
            }
        }
    }
    
    // Add an event for when the transfer will be finished
    tEvent finishedEvent = {0};
    finishedEvent.type = MASTER_TRANSFER_FINISHED;
    finishedEvent.timeNs = sim->timeNs + SPI_SLOT_TIME_US * 1000;
    addEvent(sim, &finishedEvent);
}

static void masterTransferFinished(tSimulation * sim, tEvent * event) {
    for (uint32_t i=0; i<sim->numNodes; i++) {
        if (i == MASTER_NODE_ID) {
            // miso to master
            memcpy(sim->nodeInfo[i].rxData, sim->misoData, sim->nodeInfo[i].numTransferBytes);
            sim->misoDataBytes = 0;
        } else {
            // mosi to nodes
            if (sim->nodeInfo[i].rx) {
                memcpy(sim->nodeInfo[i].rxData, sim->mosiData, sim->nodeInfo[i].numTransferBytes);
            }
        }
    }
    sim->transferOccuring = false;
    transferCompleteCb(&sim->nodes[MASTER_NODE_ID]);
}

static void actionEvent(tSimulation * sim, tEvent * event) {
    switch (event->type) {
        // case SET_PS_HIGH:
        // case SET_PS_LOW:
        //     psSet(sim, event->type == SET_PS_HIGH);
        //     break;
        case NODE_TRANSFER_INITIATED_RX:
        case NODE_TRANSFER_INITIATED_TX_RX:
            nodeTransferInitiated(sim, event, event->type == NODE_TRANSFER_INITIATED_TX_RX);
            break;
        case MASTER_TRANSFER_STARTED:
            masterTransferStarted(sim, event);
            break;
        case MASTER_TRANSFER_FINISHED:
            masterTransferFinished(sim, event);
            break;
        default:
            myAssert(0, "Unhandled event type");
            return;
    }
}

// ======================================== //
// HAL replacements

// void pauseMaster(uint64_t pauseNs) {
//     sim->master.timeNs += pauseNs;
// }

// void pauseNode(tNode * node, uint64_t pauseNs) {
//     sim->nodes[node->simId].timeNs += pauseNs;
// }

void psSet(tSimulation * sim, bool val);
void setPs(tSimulation * sim, tNode * master, bool val) {
    psSet(sim, val);
    // tEvent event = {0};
    // event.type = val == 0 ? SET_PS_LOW : SET_PS_HIGH;
    // // Pause for 100ns
    // event.timeNs = sim->timeNs + 1;
    // addEvent(sim, &event);
}

void startTxRxDMA(tSimulation * sim, tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    // Create an event for when this will happen
    tEvent event = {0};
    if (node->simId == MASTER_NODE_ID) {
        event.type = MASTER_TRANSFER_STARTED;
    } else if (tx) {
        event.type = NODE_TRANSFER_INITIATED_TX_RX;
    } else {
        event.type = NODE_TRANSFER_INITIATED_RX;
    }
    event.timeNs = sim->timeNs + DMA_WAIT_TIME_US * 1000 - 100;
    event.nodeSimId = node->simId;
    addEvent(sim, &event);
    // Store the HAL data for when the event occurs
    sim->nodeInfo[node->simId].numTransferBytes = numBytes;
    sim->nodeInfo[node->simId].txData = txData;
    sim->nodeInfo[node->simId].rxData = rxData;
    // if (tx) {
    //     tNodeIndex dstSimNodeId = getSimNodeId(sim, txData[0]);
    //     printf("Time: %9ld: Node %3d -> %d, sending packet data:%d\n", event.timeNs, node->simId, dstSimNodeId, txData[2]);
    // }
}

void stopTxRxDMA(tSimulation * sim, tNode * node) {
    tNodeInfo * simNode = &sim->nodeInfo[node->simId];
    simNode->tx = 0;
    simNode->rx = 0;
}


// ======================================== //

void simInit(tSimulation * sim) {
    memset(sim, 0, sizeof(tSimulation));
    initPacketChecker(&sim->packetChecker);
}

tNode * simCreateNode(tSimulation * sim, bool master) {
    tNodeIndex nodeSimIndex = sim->numNodes;
    if (master) {
        myAssert(sim->nodeInfo[MASTER_NODE_ID].valid == false, "Can only create one master");
        nodeSimIndex = MASTER_NODE_ID;
    } else {
        if (!sim->nodeInfo[MASTER_NODE_ID].valid) {
            nodeSimIndex++;
        }
    }
    sim->nodeInfo[nodeSimIndex].valid = true;
    sim->nodes[nodeSimIndex].simId = nodeSimIndex;
    sim->numNodes++;
    return &sim->nodes[nodeSimIndex];
}

tPacket * simCreatePacket(tSimulation * sim, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId) {
    tPacket * packet = createPacket(&sim->packetChecker, srcNodeSimId, dstNodeSimId);
    addTxPacket(&sim->nodes[srcNodeSimId], sim->nodes[dstNodeSimId].nodeId, packet);
    return packet;
}

void simulate(tSimulation * sim, uint64_t runTimeNs) {
    start(&sim->nodes[MASTER_NODE_ID]);
    
    uint64_t prevTimeNs = 0;
    while (sim->timeNs < runTimeNs) {
        tEvent * event = peekNextEvent(sim);
        if (event == NULL) {
            // Finished - although shouldn't ever happen
            myAssert(0, "Events finished");
            break;
        }
        
        //printf("%d, Event %d, time: %ld\n", i, event.type, event.timeNs);
        myAssert(event->timeNs >= sim->timeNs, "Event time earlier than sim time!");
        sim->timeNs = event->timeNs;
        actionEvent(sim, event);
        sim->eventCounter++;
        popEvent(sim);
    }
    printf("Simulated %d events over %ldms\n", sim->eventCounter, sim->timeNs/1000000);
    
    // Process any received packets
    // TODO: tidy
    for (uint32_t i=0; i<sim->numNodes; i++) {
        while (sim->nodes[i].rxPacketsReceived > 0) {
            sim->nodes[i].rxPacketsReceived--;
            bool masterToNode = (i != MASTER_NODE_ID);
            tPacket * packet = &sim->nodes[i].rxPacket[sim->nodes[i].rxPacketsReceived];
            tNodeIndex dstNodeId = packet->data[0];
            tNodeIndex srcNodeId = packet->data[1];
            myAssert(dstNodeId == sim->nodes[i].nodeId, "Node destination doesn't match\n");
            // Search for matching nodeId
            tNodeIndex srcSimNodeId = getSimNodeId(sim, srcNodeId);
            processPacket(&sim->packetChecker, srcSimNodeId, i, packet);
        }
    }
}

void simCheckAllPacketsReceived(tSimulation * sim) {
    checkAllPacketsReceived(&sim->packetChecker);
}