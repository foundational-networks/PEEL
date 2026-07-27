'''
Unfortunately, the mac address tables is not something that can be preloaded automatically.
You should always lookout for parameters to tune. You should change this based on your topology and every component
that's SAD :(
'''

import re

class Core:
    def __init__(self, core_id):
        self.core_id = core_id
        self.aggs = []


class Agg:
    def __init__(self, agg_id):
        self.agg_id = agg_id
        self.edges = []


class Edge:
    def __init__(self, edge_id):
        self.rack_id = edge_id
        self.gpus = []

class Gpu:
    def __init__(self, gpu_id):
        self.gpu_id = gpu_id
        self.mac_address = None


def mac_to_int(mac):
    res = re.match('^((?:(?:[0-9a-f]{2}):){5}[0-9a-f]{2})$', mac.lower())
    if res is None:
        raise ValueError('Invalid mac address. Make sure you used \":\" instead of \"-\"')
    return int(res.group(0).replace(':', ''), 16)


def int_to_mac(macint):
    if type(macint) != int:
        raise ValueError('invalid integer')
    return (':'.join(['{}{}'.format(a, b)
                     for a, b
                     in zip(*[iter('{:012x}'.format(macint))]*2)])).upper()

BASE_PORT_ID = 100
K = 8
NUM_SERVERS_UNDER_EACH_EDGE = 2

BASE_MAC_ADDR = "0A:AA:00:00:00:01"
BASE_MAC_INT = mac_to_int(BASE_MAC_ADDR)

NUM_CORES = int(K/2 * K/2)
NUM_AGGS_UNDER_EACH_CORE = K
NUM_AGGS = int(K/2 * K)
NUM_EDGES = NUM_AGGS
NUM_EDGES_UNDER_EACH_AGG = int(K/2)
NUM_SERVERS = int(K/2 * K) * NUM_SERVERS_UNDER_EACH_EDGE
NUM_GPUS_PER_SERVER = 8

TOTAL_CORE_PORT_NUM = int(K/2 * K/2 * K)     # K/2 aggs per pod, K/2 ports per agg --> K^2/4 cores --> K ports per core
TOTAL_AGG_PORT_NUM = int(K/2 * K * K)        # K/2 aggs per pod, K pods, K ports per agg
TOTAL_EDGE_PORT_NUM = int(NUM_SERVERS * NUM_GPUS_PER_SERVER) + int(K/2 * K * K/2) # K/2 edges per pod, K pods, NUM_SERVERS_UNDER_EACH_EDGE * num_gpus ports toward servers and K/2 ports toward aggs
BASE_SERVER_MAC_INT = BASE_MAC_INT + TOTAL_CORE_PORT_NUM + TOTAL_AGG_PORT_NUM + TOTAL_EDGE_PORT_NUM
print('Base server mac address is {}'.format(int_to_mac(BASE_SERVER_MAC_INT)))
print('Base port id is {}'.format(BASE_PORT_ID))

gpus = []
edges = []
for edge_id in range(NUM_EDGES):
    edge = Edge(edge_id=edge_id)
    base_gpu_id = edge_id * NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER
    for gpu_idx in range(NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER):
        gpu = Gpu(gpu_id=base_gpu_id + gpu_idx)
        gpu.mac_address = BASE_SERVER_MAC_INT + base_gpu_id + gpu_idx
        print('GPU[{}] mac address: {}'.format(base_gpu_id + gpu_idx, int_to_mac(gpu.mac_address)))
        gpus.append(gpu)
        edge.gpus.append(gpu)
    edges.append(edge)

print('------------------------------------------')

aggs = []
for agg_id in range(NUM_AGGS):
    agg = Agg(agg_id=agg_id)
    base_edge_id = int(agg_id/(K/2)) * int(K/2)
    for edge_idx in range(NUM_EDGES_UNDER_EACH_AGG):
        agg.edges.append(edges[base_edge_id + edge_idx])
    aggs.append(agg)

cores = []
for core_id in range(NUM_CORES):
    core = Core(core_id=core_id)
    agg_id = int(core_id/(K/2))
    for i in range(NUM_AGGS_UNDER_EACH_CORE):
        print('core[{}] connected to agg[{}]'.format(core_id, agg_id))
        core.aggs.append(aggs[agg_id])
        agg_id += int(K/2)
    print('---------------------------------------')
    cores.append(core)


for core_idx in range(NUM_CORES):
    file_name = 'fat_tree_core[{}].txt'.format(core_idx)
    file = open(file_name, 'w')
    # every core has K ports
    for core_port_num in range(K):
        start_gpu_idx = core_port_num * int(K/2 * NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER)
        end_gpu_idx = start_gpu_idx + int(K/2 * NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER)
        for gpu_idx in range(start_gpu_idx, end_gpu_idx):
            file.write('0\t{}\t{}\n'.format(int_to_mac(gpus[gpu_idx].mac_address), BASE_PORT_ID + core_port_num))
    file.close()

for agg_idx in range(NUM_AGGS):
    file_name = 'fat_tree_agg[{}].txt'.format(agg_idx)
    file = open(file_name, 'w')
    # every core has K ports
    start_edge_idx = int(agg_idx/(K/2)) * int(K/2)
    end_edge_idx = start_edge_idx + int(K/2)
    for edge_idx in range(NUM_EDGES):
        edge = edges[edge_idx]
        if start_edge_idx <= edge_idx < end_edge_idx:
            # go through edge
            for gpu in edge.gpus:
                file.write(
                    '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + edge_idx - start_edge_idx))
        else:
            # go through core
            for gpu in edge.gpus:
                for core_idx in range(int(K/2)):
                    # Every agg is connected to K/2 cores
                    # There are K/2 ports connected to edges
                    file.write(
                        '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + int(K/2) + core_idx))

    file.close()

for edge_idx in range(NUM_EDGES):
    file_name = 'fat_tree_edge[{}].txt'.format(edge_idx)
    file = open(file_name, 'w')
    # every core has K ports
    start_gpu_idx = edge_idx * NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER
    end_gpu_idx = start_gpu_idx + NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER
    for gpu_idx in range(NUM_SERVERS*NUM_GPUS_PER_SERVER):
        gpu = gpus[gpu_idx]
        if start_gpu_idx <= gpu_idx < end_gpu_idx:
            # go to server
            file.write(
                '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + gpu_idx - start_gpu_idx))
        else:
            # go through aggs
            for agg_idx in range(int(K/2)):
                # There are NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS ports connected to servers
                file.write(
                    '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS_PER_SERVER + agg_idx))

    file.close()