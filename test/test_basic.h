// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TEST_BASIC_H
#define TEST_BASIC_H

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

void test_max_nodes_new_node_allocation(void);
void test_packets_to_from_each_node(uint32_t numNodes, uint32_t numPacketsPerNode);

#endif
