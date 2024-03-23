#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "unity.h"
#include "microbus.h"
#include "txManager.h"
#include "networkManager.h"
#include "usefulLib.h"



// void recordNodeTxChance(uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex rxNodeId, tTxManager * txManager, uint8_t * rxPacketsStart, uint8_t * rxPacketsEnd);
// void recordPacketRecieved(tNetworkManager * nwManager, uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex rxNodeId);


void test_packets(void) {
    // Node
    tPacket nodePacket = {0};
    uint64_t uniqueId = 7;
    tNodeIndex nodeId = INVALID_NODE_ID;
    txNewNodeRequest(&nodePacket, uniqueId);
    
    // Master
    tPacket masterPacket = {0};
    tNetworkManager nwManager = {0};
    uint8_t nodeTTL[10] = {0};
    uint8_t maxRxNodes = 10;
    rxNewNodePacketRequest(&nwManager, nodeTTL, maxRxNodes, &nodePacket);
    txNewNodeResponse(&nwManager, &masterPacket);
    
    // Node
    rxNewNodePacketResponse(&masterPacket, uniqueId, &nodeId);
    
    TEST_ASSERT_EQUAL_UINT(FIRST_AGENT_NODE_ID, nodeId);
}
