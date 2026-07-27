'''
Unfortunately, the mac address tables is not something that can be preloaded automatically.
You should always lookout for parameters to tune. You should change this based on your topology and every component
that's SAD :(
'''

import re


class Rack:
    def __init__(self, rack_id):
        self.rack_id = rack_id
        self.gpus = []


class Gpu:
    def __init__(self, server_id):
        self.server_id = server_id
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

NUM_SPINES = 16
NUM_AGGS = 32
NUM_SERVERS_UNDER_EACH_AGG = 2
NUM_GPUS_PER_SERVER = 8

BASE_MAC_ADDR = "0A:AA:00:00:00:01"
BASE_MAC_INT = mac_to_int(BASE_MAC_ADDR)

TOTAL_SPINE_PORT_NUM = NUM_SPINES * NUM_AGGS
TOTAL_AGG_PORT_NUM = NUM_AGGS * (NUM_SPINES + (NUM_SERVERS_UNDER_EACH_AGG*NUM_GPUS_PER_SERVER))
BASE_GPU_MAC_INT = BASE_MAC_INT + TOTAL_SPINE_PORT_NUM + TOTAL_AGG_PORT_NUM
print('Base gpu mac address is {}'.format(int_to_mac(BASE_GPU_MAC_INT)))
print('Base port id is {}'.format(BASE_PORT_ID))


racks = []
for agg_id in range(NUM_AGGS):
    rack = Rack(rack_id=agg_id)
    base_gpu_id = agg_id * NUM_SERVERS_UNDER_EACH_AGG * NUM_GPUS_PER_SERVER
    for gpu_idx in range(NUM_SERVERS_UNDER_EACH_AGG * NUM_GPUS_PER_SERVER):
        gpu = Gpu(base_gpu_id + gpu_idx)
        gpu.mac_address = BASE_GPU_MAC_INT + base_gpu_id + gpu_idx
        rack.gpus.append(gpu)
    racks.append(rack)

for spine_idx in range(NUM_SPINES):
    file_name = 'spine[{}].txt'.format(spine_idx)
    file = open(file_name, 'w')
    for spine_port_num in range(NUM_AGGS):
        rack = racks[spine_port_num]
        if rack.rack_id != spine_port_num:
            raise Exception('rack.rack_id != spine_port_num')
        for gpu in rack.gpus:
            file.write('0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + spine_port_num))
    file.close()

for src_agg_idx in range(NUM_AGGS):
    file_name = 'agg[{}].txt'.format(src_agg_idx)
    file = open(file_name, 'w')
    for dest_agg_idx in range(NUM_AGGS):
        src_rack = racks[src_agg_idx]
        dst_rack = racks[dest_agg_idx]
        for gpu_idx in range(len(dst_rack.gpus)):
            gpu = dst_rack.gpus[gpu_idx]
            if src_rack.rack_id == dst_rack.rack_id:
                file.write('0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + gpu_idx))
            else:
                for spine_num in range(NUM_SPINES):
                    file.write('0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + (NUM_SERVERS_UNDER_EACH_AGG*NUM_GPUS_PER_SERVER) + spine_num))
    file.close()
