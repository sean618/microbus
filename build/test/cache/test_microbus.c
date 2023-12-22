#include "src/microbus.h"
#include "/var/lib/gems/3.0.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"




void simulate(tMaster * master, tSlave slave[], uint32_t numSlaves, uint64_t numSimulationSlots) {

    for (uint64_t i=0; i<numSimulationSlots; i++) {



        tPacket * txPacket = getTxPacket(&master->common);

        if (txPacket) {

            for (uint32_t s=0; s<numSlaves; s++) {

                printf("Adding packet %d to slave %d\n", txPacket->val, s);

                addRxPacket(&slave[s].common, txPacket);

                removeTxPacket(&master->common);

            }

        }

        for (uint32_t s=0; s<numSlaves; s++) {



            txPacket = getTxPacket(&slave[s].common);

            if (txPacket) {

                printf("Adding packet %d from slave %d to master\n", txPacket->val, s);

                addRxPacket(&master->common, txPacket);

                removeTxPacket(&slave[s].common);

            }

        }

    }

}



void test_simple(void) {

    tMaster master = {0};

    tSlave slave[2] = {0};

    uint32_t numSlaves = 2;



    tPacket packet[3] = {{1},{2},{3}};

    addTxPacket(&master.common, &packet[0]);

    addTxPacket(&slave[0].common, &packet[1]);

    addTxPacket(&slave[1].common, &packet[2]);



    simulate(&master, slave, numSlaves, 100);





    UnityAssertEqualNumber((UNITY_INT)((packet[1].val)), (UNITY_INT)((master.common.rxPacket[0].val)), (

   ((void *)0)

   ), (UNITY_UINT)(41), UNITY_DISPLAY_STYLE_UINT);

    UnityAssertEqualNumber((UNITY_INT)((packet[2].val)), (UNITY_INT)((master.common.rxPacket[1].val)), (

   ((void *)0)

   ), (UNITY_UINT)(42), UNITY_DISPLAY_STYLE_UINT);





    UnityAssertEqualNumber((UNITY_INT)((packet[0].val)), (UNITY_INT)((slave[0].common.rxPacket[0].val)), (

   ((void *)0)

   ), (UNITY_UINT)(45), UNITY_DISPLAY_STYLE_UINT);

    UnityAssertEqualNumber((UNITY_INT)((packet[0].val)), (UNITY_INT)((slave[1].common.rxPacket[0].val)), (

   ((void *)0)

   ), (UNITY_UINT)(46), UNITY_DISPLAY_STYLE_UINT);

}
