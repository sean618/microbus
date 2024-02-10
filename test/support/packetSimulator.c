// =============================================== //
// Packet based simulation that just passes packets
// without worrying about corruption or exact timings.
// This is a lot easier to debug than the event based
// simulation as the call stack show where the packet
// came from (the event based uses callbacks)
// =============================================== //

#include "stdlib.h"
#include "unity.h"
#include "simulator.h"
#include "packetChecker.h"

// ======================================== //
// HAL actions when event is actioned

static void psSet(tSimulation * sim, bool val) {
    myAssert(val != sim->psWire, "Setting PS wire but value not changing\n");
    if (val != sim->psWire) {
        sim->psWire = val;
        // This will fire the interrupts on all nodes
        for (uint32_t i=0; i<sim->numNodes; i++) {
            psLineInterrupt(&sim->nodes[i], sim->psWire);
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
    
    uint32_t numTransferBytes = sim->masterInfo.numTransferBytes;
    bool test = numTransferBytes < MAX_TX_DATA_BYTES;
    myAssert(test, "Master Tx data exceeded limit");
    
    // Put data on bus - Master to mosi
    memcpy(sim->mosiData, sim->masterInfo.txData, numTransferBytes);
    
    bool slaveTx = false;
    for (uint32_t i=0; i<sim->numNodes; i++) {
        // Put data on bus - Nodes to miso
        if (sim->nodeInfo[i].tx) {
            myAssert(!slaveTx, "Corruption\n");
            slaveTx = true;
            myAssert(sim->nodeInfo[i].numTransferBytes < MAX_TX_DATA_BYTES, "Node Tx data exceeded limit");
            uint32_t numBytes = MIN(numTransferBytes, sim->nodeInfo[i].numTransferBytes);
            for (uint32_t j=0; j<numBytes; j++) {
                // Bitwise and because it's pulled-up so any 0 will cause it to be zero
                sim->misoData[j] &= sim->nodeInfo[i].txData[j];
            }
            //sim->misoDataBytes = MAX(sim->misoDataBytes, sim->nodes[i].numTransferBytes);
            // TODO: assert if not enum packets
        }
    }
    
    // Add an event for when the transfer will be finished
    tEvent finishedEvent = {0};
    finishedEvent.type = MASTER_TRANSFER_FINISHED;
    finishedEvent.timeNs = sim->timeNs + SPI_SLOT_TIME_US * 1000;
    addEvent(sim, &finishedEvent);
}

static void masterTransferFinished(tSimulation * sim, tEvent * event) {
    // miso to master
    myAssert(sim->masterInfo.rxData != NULL, "Master rx data ptr not set");
    memcpy(sim->masterInfo.rxData, sim->misoData, sim->masterInfo.numTransferBytes);
    sim->misoDataBytes = 0;
    
    // mosi to nodes
    for (uint32_t i=0; i<sim->numNodes; i++) {
        if (sim->nodeInfo[i].rx) {
            myAssert(sim->nodeInfo[i].rxData != NULL, "Slave rx data ptr not set");
            memcpy(sim->nodeInfo[i].rxData, sim->mosiData, sim->nodeInfo[i].numTransferBytes);
        }
    }
    sim->transferOccuring = false;
    masterTransferCompleteCb(&sim->master, false);
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
void setPs(tSimulation * sim, tMasterNode * master, bool val) {
    psSet(sim, val);
    // tEvent event = {0};
    // event.type = val == 0 ? SET_PS_LOW : SET_PS_HIGH;
    // // Pause for 100ns
    // event.timeNs = sim->timeNs + 1;
    // addEvent(sim, &event);
}

void startMasterTxRxDMA(tSimulation * sim, tMasterNode * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    // Create an event for when this will happen
    tEvent event = {0};
    event.type = MASTER_TRANSFER_STARTED;
    event.timeNs = sim->timeNs + DMA_WAIT_TIME_US * 1000 - 100;
    event.nodeSimId = master->simId;
    addEvent(sim, &event);
    // Store the HAL data for when the event occurs
    sim->masterInfo.numTransferBytes = numBytes;
    myAssert(rxData != NULL, "Master invalid rx data ptr");
    myAssert(txData != NULL, "Master invalid tx data ptr");
    sim->masterInfo.txData = txData;
    sim->masterInfo.rxData = rxData;
    // if (tx) {
    //     tNodeIndex dstSimNodeId = getSimNodeId(sim, txData[0]);
    //     printf("Time: %9ld: Node %3d -> %d, sending packet data:%d\n", event.timeNs, node->simId, dstSimNodeId, txData[2]);
    // }
}
void startSlaveTxRxDMA(tSimulation * sim, tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    // Create an event for when this will happen
    tEvent event = {0};
    if (tx) {
        event.type = NODE_TRANSFER_INITIATED_TX_RX;
    } else {
        event.type = NODE_TRANSFER_INITIATED_RX;
    }
    event.timeNs = sim->timeNs + DMA_WAIT_TIME_US * 1000 - 100;
    event.nodeSimId = node->simId;
    addEvent(sim, &event);
    // Store the HAL data for when the event occurs
    sim->nodeInfo[node->simId].numTransferBytes = numBytes;
    myAssert(rxData != NULL, "Master invalid rx data ptr");
    myAssert(txData != NULL, "Master invalid tx data ptr");
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

void simulate(tSimulation * sim, uint64_t runTimeNs, bool start) {
    // TODO
    if (start) {
        startMicrobus(&sim->master);
    }

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
        sim->master.simTimeNs = sim->timeNs;
        for (uint32_t i=0; i<sim->numNodes; i++) {
            sim->nodes[i].simTimeNs = sim->timeNs;
        }
        actionEvent(sim, event);
        sim->eventCounter++;
        popEvent(sim);
    }
    printf("Simulated %d events over %ldms\n", sim->eventCounter, sim->timeNs/1000000);
    
    // Process any Master received packets
    while (sim->master.rxPacketsReceived > 0) {
        sim->master.rxPacketsReceived--;
        tPacket * packet = &sim->master.rxPacket[sim->master.rxPacketsReceived];
        processRxPacket(sim, sim->master.nodeId, packet);
    }
    
    // Process any slave received packets
    for (uint32_t i=0; i<sim->numNodes; i++) {
        while (sim->master.rxPacketsReceived > 0) {
            sim->nodes[i].rxPacketsReceived--;
            tPacket * packet = &sim->nodes[i].rxPacket[sim->nodes[i].rxPacketsReceived];
            processRxPacket(sim, sim->nodes[i].nodeId, packet);
        }
    }
}