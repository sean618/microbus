// #include "string.h"
// #include "stdbool.h"
// #include "stdint.h"
// #include "stdio.h"
// #include "stdlib.h"
// //#include "scheduler.h"
// #include "newNode.h"
// #include "microbus.h"
// #include "node.h"

// // ========================================= //

// static bool isNodeRxPacketCorrupt(tPacket * packet) {
//     bool crcValid = true;
//     uint8_t protocolVersion = (packet->protocolVersionAndPacketType >> 4) & 0xF;
//     tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
//     if (!crcValid) {
//         myAssert(0, "Invalid CRC");
//         // TODO: increment counter
//         return true;
//     } else if (protocolVersion != MICROBUS_VERSION) {
//         myAssert(0, "Invalid version");
//         // TODO: increment counter
//         return true;
//     } else if (packetType != DATA_PACKET && packetType != NEW_NODE_RESPONSE_PACKET) {
//         myAssert(0, "Invalid packet type");
//         return true;
//     }
//     return false;
// }

// // Return pointer to the buffer entry for the next rx packet
// // NOTE: this can't take too long. It must complete before the next packet is transmitted
// static tPacket * nodeProcessRxHeaders(tNode * node, tPacket * packet) {
//     // By default re-use the rx packet for the next rx as well
//     tPacket * nextRxPacket = packet;
//     tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
//     if (packet->dstNodeId == node->nodeId) {
//         if (packetType == DATA_PACKET) {
//             if(rxPacketCheckAndUpdateSeqNum(&node->txManager, 0, packet->txSeqNum)) {
//                 if (CIRCULAR_BUFFER_FULL(node->rxPacketsStart, node->rxPacketsEnd, node->maxRxPackets)) {
//                     // If full re-use the previous rx packet so that we can still update our meta-data from the rx headers
//                     nextRxPacket = packet;
//                     myAssert(0, "Rx packet buffer full!");
//                 } else {
//                     // Leave in the buffer to be processed outside of this driver
//                     // Update the tail and use the next free entry
//                     node->rxPacketsEnd = INCR_AND_WRAP(node->rxPacketsEnd, 1, node->maxRxPackets);
//                     nextRxPacket = &node->rxPackets[node->rxPacketsEnd];
//                 }
//             }
//         } else if (packetType == NEW_NODE_RESPONSE_PACKET) {
//             if (node->nodeId == UNALLOCATED_NODE_ID) {
//                 rxNewNodePacketResponse(node, packet);
//             }
//         }
//     }
//     return nextRxPacket;
// }

// static tPacket * nodeProcessTx(tNode * node, tPacket * rxPacket) {
//     if (rxPacket->nextTxNodeId == node->nodeId) {
//         // If we haven't got a node ID then we can send a request when the next slot is unused
//         if (node->nodeId == UNALLOCATED_NODE_ID) {
//             return txNewNodeRequest(node);
//         } else {
//             // Optimisation - this ensures every node gets an ack back from the master before it transmits
//             // so the acks always match - even if the master is always transmitting to node 1 and node 2 is
//             // always transmitting to master
//             rxAckSeqNum(&node->txManager, 0, rxPacket->ackSeqNum);
//             // If we've got a node ID and it's our turn we can transmit
//             return getNextTxPacket(&node->txManager);
//         }
//     }
//     return NULL;
// }

// // ========================================= //
// // Called by interupt

// void nodeProcessRxAndGetTx(tNode * node, tPacket ** txPacket, tPacket ** nextRxPacket) {
//     tPacket * rxPacket = &node->rxPackets[node->rxPacketsEnd];
//     // By default reuse the rx packet entry
//     *nextRxPacket = rxPacket;
//     *txPacket = NULL;
    
//     // We can ignore the packet if it's not for use or telling us we're the next node to Tx
//     if (rxPacket->dstNodeId == node->nodeId || rxPacket->nextTxNodeId == node->nodeId) {
//         if (!isNodeRxPacketCorrupt(rxPacket)) {
//             *nextRxPacket = nodeProcessRxHeaders(node, rxPacket);
//             *txPacket = nodeProcessTx(node, rxPacket);
//         }
//     }
//     (*nextRxPacket)->protocolVersionAndPacketType = 0;
// }

// // ========================================= //
// // Called by main thread

// void nodeInit(tNode * node, uint8_t maxTxPacketEntries, tTxPacketEntry txPacketEntries[], uint8_t maxRxPackets, tPacket rxPackets[]) {
//     memset(node, 0, sizeof(tNode));
//     initTxManager(&node->txManager, 1, &node->txSeqNumStart, &node->txSeqNumEnd, &node->txSeqNumNext, &node->rxSeqNum, maxTxPacketEntries, txPacketEntries);
//     node->nodeId = UNALLOCATED_NODE_ID;
//     node->maxRxPackets = maxRxPackets;
//     node->rxPackets = rxPackets;
// }

// uint8_t * nodeAllocateTxPacket(tNode * node, tNodeIndex dstNodeId, uint16_t numBytes) {
//     if (node->nodeId == UNALLOCATED_NODE_ID || node->nodeId > MAX_NODES) {
//         myAssert(0, "Node not initialised yet");
//         return NULL;
//     }
//     if (numBytes > PACKET_DATA_SIZE) {
//         myAssert(0, "Tx packet exceeds max size");
//         return NULL;
//     }
//     tPacket * packet = allocateTxPacket(&node->txManager, node->nodeId, dstNodeId);
//     if (packet != NULL) {
//         packet->dataSize1 = numBytes >> 8;
//         packet->dataSize2 = numBytes & 0xFF;
//     }
//     return packet;
// }

// tPacket * nodePeekNextRxDataPacket(tNode * node) {
//     tPacket * entry;
//     CIRCULAR_BUFFER_PEEK(entry, node->rxPackets, node->rxPacketsStart, node->rxPacketsEnd, node->maxRxPackets);
//     return entry;
// }

// void nodePopNextDataPacket(tNode * node) {
//     CIRCULAR_BUFFER_POP(node->rxPacketsStart, node->rxPacketsEnd, node->maxRxPackets);
// }


