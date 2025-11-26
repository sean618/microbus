


import re
from collections import defaultdict
from tabulate import tabulate

log_file_path = 'log.txt'


mbNetwork = defaultdict(dict)
mbIndexToNodeId = {}

nodes = defaultdict(dict)
nodeIdToMbNodeIndex = {}



# # Wireless

# def agent_new_node_request(line, hex_pattern=re.compile(r"Agent: sent new node request (\d+)")):
#     match = hex_pattern.search(line)
#     if match:
#         mbIndex = int(match.group(1)) & 0xFF
#         mbNetwork[mbIndex]["newNodeTx"] = mbNetwork[mbIndex].get("newNodeTx", 0) + 1

# def agent_joined(line, hex_pattern=re.compile(r"Agent:(\d+) - joined - uniqueId:0x(.+)")):
#     match = hex_pattern.search(line)
#     if match:
#         nodeId = int(match.group(1))
#         mbIndex = int(match.group(2), 16) & 0xFF
#         mbIndexToNodeId[mbIndex] = nodeId
#         mbNetwork[mbIndex]["Joined"] = mbNetwork[mbIndex].get("Joined", 0) + 1
#         mbNetwork[mbIndex]["agentId"] = nodeId


# Microbus

def microbus_new_node_request(line, hex_pattern=re.compile(r"Node, Tx new node request: 0x(.+)")):
    match = hex_pattern.search(line)
    if match:
        nIndex = int(match.group(1), 16) & 0xFF
        mbIndex = (int(match.group(1), 16) >> 8) & 0xFF
        idx = (mbIndex, nIndex)
        nodes[idx]["newNodeTx"] = nodes[idx].get("newNodeTx", 0) + 1

def microbus_partial_join(line, hex_pattern=re.compile(r"Master - Node:(\d+) partial join - uniqueId:0x(.+)")):
    match = hex_pattern.search(line)
    if match:
        nodeId = int(match.group(1))
        nIndex = int(match.group(2), 16) & 0xFF
        mbIndex = (int(match.group(2), 16) >> 8) & 0xFF
        nodeOrAgent = (int(match.group(2), 16) >> 16) & 0xFF
        idx = (mbIndex, nIndex)
        nodeIdToMbNodeIndex[nodeId] = idx
        if nodeOrAgent == 2:
            nodes[idx]["nodeId"] = nodeId

def microbus_node_join(line, hex_pattern=re.compile(r"Node:(\d+) - joined - uniqueId:0x(.+)")):
    match = hex_pattern.search(line)
    if match:
        nodeId = int(match.group(1))
        nIndex = int(match.group(2), 16) & 0xFF
        mbIndex = (int(match.group(2), 16) >> 8) & 0xFF
        idx = (mbIndex, nIndex)
        nodeIdToMbNodeIndex[nodeId] = idx
        nodes[idx]["Joined"] = nodes[idx].get("Joined", 0) + 1

def microbus_node_fully_joined(line, hex_pattern=re.compile(r"Master Node:(\d+) - fully joined")):
    match = hex_pattern.search(line)
    if match:
        nodeId = int(match.group(1))
        idx = nodeIdToMbNodeIndex[nodeId]
        nodes[idx]["OnNetwork"] = 1
        nodes[idx]["MasterJoined"] = nodes[idx].get("MasterJoined", 0) + 1

def microbus_node_removed(line, hex_pattern=re.compile(r"Master - Node:(\d+), removed from network")):
    match = hex_pattern.search(line)
    if match:
        nodeId = int(match.group(1))
        idx = nodeIdToMbNodeIndex[nodeId]
        nodes[idx]["OnNetwork"] = 0
        nodes[idx]["Removed"] = nodes[idx].get("Removed", 0) + 1


def wireless_tx_buffer_full(line, hex_pattern=re.compile(r"(\d+) - wirelessTxBufferFullMbPacketDropped: (\d+)")):
    match = hex_pattern.search(line)
    if match:
        nodeId = int(match.group(1))
        packet_type = int(match.group(2))
        if nodeId == 0:
            idx = 0xFF, 0xFF
        else:
            idx = nodeIdToMbNodeIndex[nodeId]
        if packet_type == 3 or packet_type == 4:
            # nodes[idx]["w_tx_dropped__empty"] = nodes[idx].get("w_tx_dropped__empty", 0) + 1
            pass
        elif packet_type == 5 or packet_type == 6:
            nodes[idx]["w_tx_dropped__new_node"] = nodes[idx].get("w_tx_dropped__new_node", 0) + 1
        else:
            nodes[idx]["w_tx_dropped__other"] = nodes[idx].get("w_tx_dropped__other", 0) + 1



with open(log_file_path, 'r') as log_file:
    for line in log_file:
        try:
            agent_new_node_request(line)
            agent_joined(line)
            
            microbus_new_node_request(line)
            microbus_partial_join(line)
            microbus_node_join(line)
            microbus_node_fully_joined(line)
            microbus_node_removed(line)
            wireless_tx_buffer_full(line)
        except Exception as e:
            print(line, flush=True)
            raise





# print("\n\nWireless\n")

# for entry, val in mbNetwork.items():
#     print(entry, val)


# sorted_nodes = dict(sorted(nodes.items()))
# # for entry, val in sorted_nodes.items():
# #     print(entry, val)

# print("\n\nMicrobus\n")

# headers = sorted({key for inner in sorted_nodes.values() for key in inner})
# table = []

# for key, inner in sorted_nodes.items():
#     row = [str(key)] + [inner.get(h, '') for h in headers]
#     table.append(row)

# # Add the ID column to headers
# print(tabulate(table, headers=['ID'] + headers, tablefmt='grid'))



