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
        self.tors = []


class Segment:
    def __init__(self, seg_id):
        self.seg_id = seg_id
        self.gpus = []


class Tor:
    def __init__(self, tor_id):
        self.tor_id = tor_id
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
NUM_PODS = 2
NUM_SERVERS_PER_SEGMENT = 2
NUM_SEGMENTS_PER_POD = 16
NUM_TORS_PER_SEGMENT = 16
NUM_GPUS_PER_SERVER = 16
NUM_AGGS_PER_PLANE = 15
NUM_CORES_PER_AGG = 8

BASE_MAC_ADDR = "0A:AA:00:00:00:01"
BASE_MAC_INT = mac_to_int(BASE_MAC_ADDR)

NUM_CORES_PER_PLANE = int(NUM_AGGS_PER_PLANE / 15) * NUM_CORES_PER_AGG
NUM_CORES = NUM_CORES_PER_PLANE * 2
NUM_AGGS_UNDER_EACH_CORE = 15 * NUM_PODS
NUM_AGGS_PER_POD = (NUM_AGGS_PER_PLANE * 2)
NUM_AGGS = NUM_AGGS_PER_POD * NUM_PODS
NUM_TORS_UNDER_EACH_AGG = int(NUM_TORS_PER_SEGMENT / 2) * NUM_SEGMENTS_PER_POD
NUM_TORS_PER_POD = NUM_TORS_PER_SEGMENT * NUM_SEGMENTS_PER_POD
NUM_TORS = NUM_TORS_PER_POD * NUM_PODS
NUM_SERVERS_PER_POD = NUM_SERVERS_PER_SEGMENT * NUM_SEGMENTS_PER_POD
NUM_SERVERS = NUM_SERVERS_PER_POD * NUM_PODS

TOTAL_CORE_PORT_NUM = NUM_CORES * NUM_AGGS_UNDER_EACH_CORE
TOTAL_AGG_PORT_NUM = NUM_AGGS * (NUM_TORS_UNDER_EACH_AGG + NUM_CORES_PER_AGG)        # K/2 aggs per pod, K pods, K ports per agg
TOTAL_TOR_PORT_NUM = NUM_TORS * (NUM_SERVERS_PER_SEGMENT + NUM_AGGS_PER_PLANE) # K/2 edges per pod, K pods, NUM_SERVERS_UNDER_EACH_EDGE * num_gpus ports toward servers and K/2 ports toward aggs
BASE_SERVER_MAC_INT = BASE_MAC_INT + TOTAL_CORE_PORT_NUM + TOTAL_AGG_PORT_NUM + TOTAL_TOR_PORT_NUM
print('Base server mac address is {}'.format(int_to_mac(BASE_SERVER_MAC_INT)))
print('Base port id is {}'.format(BASE_PORT_ID))

gpus = []
tors = []
segments = []

for segment_id in range(NUM_SEGMENTS_PER_POD * NUM_PODS):
    start_tor_id = segment_id * NUM_TORS_PER_SEGMENT
    end_tor_id = start_tor_id + NUM_TORS_PER_SEGMENT
    base_gpu_id = segment_id * NUM_SERVERS_PER_SEGMENT * NUM_GPUS_PER_SERVER
    seg = Segment(seg_id=segment_id)

    for tor_id in range(start_tor_id, end_tor_id):
        tor = Tor(tor_id=tor_id)
        tors.append(tor)
    for server_idx in range(NUM_SERVERS_PER_SEGMENT):
        for gpu_idx in range(NUM_GPUS_PER_SERVER):
            gpu_id = base_gpu_id + (server_idx * NUM_GPUS_PER_SERVER) + gpu_idx
            gpu = Gpu(gpu_id=gpu_id)
            gpu.mac_address = BASE_SERVER_MAC_INT+gpu_id
            print('GPU[{}] mac address: {}'.format(gpu_id, int_to_mac(gpu.mac_address)))
            gpus.append(gpu)
            connected_tor = tors[start_tor_id+gpu_idx]
            connected_tor.gpus.append(gpu)
            seg.gpus.append(gpu)
    segments.append(seg)

print('------------------------------------------')

aggs = []
for agg_id in range(NUM_AGGS):
    agg = Agg(agg_id=agg_id)
    agg_pod = int(agg_id / NUM_AGGS_PER_POD)
    base_agg_in_pod = agg_pod*NUM_AGGS_PER_POD
    agg_plane = 0
    if agg_id >= (base_agg_in_pod+NUM_AGGS_PER_PLANE):
        agg_plane = 1

    start_tor_id = (agg_pod*NUM_TORS_PER_POD) + agg_plane
    for tor_id in range(start_tor_id, NUM_TORS_PER_POD, 2):
        agg.tors.append(tors[tor_id])
    aggs.append(agg)

print("------------------------------------------")

cores = []
print(NUM_CORES)
for core_id in range(NUM_CORES):
    core = Core(core_id=core_id)
    agg_chunk = int(core_id/8) # 15 aggs per core
    agg_id = agg_chunk*15
    for i in range(NUM_AGGS_UNDER_EACH_CORE):
        print('core[{}] connected to agg[{}]'.format(core_id, agg_id))
        core.aggs.append(aggs[agg_id])
        agg_id += 1
        if (agg_id%15 == 0): agg_id+=(NUM_AGGS_PER_POD-15)
    cores.append(core)

print('------------------------------------------')

for core_idx in range(NUM_CORES):
    file_name = 'hpn_core[{}].txt'.format(core_idx)
    file = open(file_name, 'w')
    core_plane = 0
    if core_idx >= NUM_CORES_PER_PLANE:
        core_plane = 1
    # every core is connected to 15 aggs from each pod so has that many ports
    for pod in range(NUM_PODS):
        for i in range(15):
            core_port_num = (pod * 15) + i
            start_gpu_idx = pod * (NUM_SERVERS_PER_POD * NUM_GPUS_PER_SERVER) + core_plane
            end_gpu_idx = start_gpu_idx + (NUM_SERVERS_PER_POD * NUM_GPUS_PER_SERVER)
            for gpu_idx in range(start_gpu_idx, end_gpu_idx, 2):
                file.write('0\t{}\t{}\n'.format(int_to_mac(gpus[gpu_idx].mac_address), BASE_PORT_ID + core_port_num))
    file.close()

for agg_idx in range(NUM_AGGS):
    file_name = 'hpn_agg[{}].txt'.format(agg_idx)
    file = open(file_name, 'w')

    agg_pod = int(agg_idx / NUM_AGGS_PER_POD)
    base_agg_in_pod = agg_pod * NUM_AGGS_PER_POD
    agg_plane = 0
    if agg_idx >= (base_agg_in_pod + NUM_AGGS_PER_PLANE):
        agg_plane = 1

    start_tor_idx = agg_pod * NUM_TORS_PER_POD
    end_tor_idx = start_tor_idx + NUM_TORS_PER_POD

    for tor_idx in range(agg_plane, NUM_TORS, 2):
        tor = tors[tor_idx]
        if start_tor_idx <= tor_idx < end_tor_idx:
            # go through tors
            for gpu in tor.gpus:
                file.write(
                    '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + int((tor_idx - start_tor_idx) / 2)))
        else:
            # go through core
            for gpu in tor.gpus:
                for core_idx in range(NUM_CORES_PER_AGG):
                    file.write(
                        '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + NUM_TORS_UNDER_EACH_AGG + core_idx))

    file.close()

for tor_idx in range(NUM_TORS):
    file_name = 'hpn_tor[{}].txt'.format(tor_idx)
    file = open(file_name, 'w')

    tor_pod = int(tor_idx / NUM_TORS_PER_POD)
    tor_segment = int(tor_idx / NUM_TORS_PER_SEGMENT)
    base_tor_in_pod = tor_pod * NUM_TORS_PER_POD
    tor_plane = 0
    if tor_idx % 2 != 0:
        tor_plane = 1

    port = 0
    for gpu in tors[tor_idx].gpus:
        file.write(
            '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + port))
        port+=1

    for gpu_idx in range(NUM_SERVERS*NUM_GPUS_PER_SERVER):
        gpu_segment_id = int(gpu_idx / (NUM_GPUS_PER_SERVER * NUM_SERVERS_PER_SEGMENT))
        if (gpu_segment_id == tor_segment):
            # skip gpus witin the same segment, either they are taken care of in downstream code or are on a different rail
            continue
        gpu = gpus[gpu_idx]
        # go through aggs
        for agg_idx in range(NUM_AGGS_PER_PLANE):
            # There are NUM_SERVERS_UNDER_EACH_EDGE * NUM_GPUS ports connected to servers
            file.write(
                '0\t{}\t{}\n'.format(int_to_mac(gpu.mac_address), BASE_PORT_ID + len(tors[tor_idx].gpus) + agg_idx))

    file.close()
