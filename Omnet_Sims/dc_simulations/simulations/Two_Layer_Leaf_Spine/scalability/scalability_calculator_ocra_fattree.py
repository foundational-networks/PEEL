from http import server
import math
import hashlib
import random

Ks = [4, 8, 16, 32, 64]
NUM_SWITCH_PORTS = 128
link_id_bit_overhead = math.ceil(math.log2(NUM_SWITCH_PORTS))
receiver_percentages = [0.4]
hash_funciton_nums = [4]
filter_sizes_bits = [16]

def get_server_num(k):
    return int(k**3 / 4.0)

def get_core_num(k):
    return int(k**2 / 4.0)

def get_servers_per_pod(k):
    return int(k**2 / 4.0)

def get_pod_idx_for_server(k, server_idx):
    return int(server_idx / get_servers_per_pod(k))

def get_agg_port_idx_for_server(k, server_idx):
    server_idx_in_pod = server_idx % get_servers_per_pod(k)
    edge_idx = int(server_idx_in_pod / (K/2))
    return edge_idx

def get_pod_number_for_servers(k, server_list):
    pod_set = set()
    for server in server_list:
        m_pod = get_pod_idx_for_server(k, server)
        pod_set.add(m_pod)
    return len(pod_set)

def get_servers_per_edge(k):
    return int(k / 2.0)

def get_edge_idx_for_server(k, server_idx):
    return int(server_idx / get_servers_per_edge(k))

def get_edge_number_for_servers(k, server_list):
    edge_set = set()
    for server in server_list:
        m_edge = get_edge_idx_for_server(k, server)
        edge_set.add(m_edge)
    return len(edge_set)

def initialize_bitstream(length):
    length = int(length)
    return ''.join('0' for _ in range(length))

# Define hash functions using those seeds
def hash_with_seed(seed, value):
    combined = f"{seed}_{value}"
    hash_object = hashlib.sha256(combined.encode())
    return int(hash_object.hexdigest(), 16)

def replace_char(s, i, new_char):
    if i < 0 or i >= len(s):
        raise IndexError("Index out of range")
    return s[:i] + new_char + s[i+1:]

def apply_hash_functions(s, seeds, value):
    for seed in seeds:
        m_hash = hash_with_seed(seed, value)
        bit_idx = m_hash % len(s)
        s = replace_char(s, bit_idx, '1')
    return s

def calc_orca_state_per_switch(K, F, receiver_list, hash_funciton_num):
    seeds = [f"seed{i}" for i in range(hash_funciton_num)]
    # here, we assume that we choose one aggregate per pod
    agg_to_port_dict = dict()
    for recever in receiver_list:
        agg_idx = get_pod_idx_for_server(K, recever)
        port_idx = get_agg_port_idx_for_server(K, recever)
        if (agg_idx not in agg_to_port_dict):
            agg_to_port_dict[agg_idx] = set()
        agg_to_port_dict[agg_idx].add(port_idx)
    s = initialize_bitstream(F)
    for agg_idx in agg_to_port_dict:
        for port_idx in agg_to_port_dict[agg_idx]:
            s = apply_hash_functions(s, seeds, f'agg[{agg_idx}].port{port_idx}')
    total_false_positives = 0
    for agg_idx in agg_to_port_dict:
        for port_idx in range(NUM_SWITCH_PORTS):
            if port_idx not in agg_to_port_dict[agg_idx]:
                for seed_idx, seed in enumerate(seeds):
                    m_hash = hash_with_seed(seed, f'agg[{agg_idx}].port{port_idx}')
                    bit_idx = m_hash % len(s)
                    if s[bit_idx] != '1':
                        break
                    if (seed_idx == len(seeds) - 1):
                        total_false_positives += 1
    total_bit_overhead = link_id_bit_overhead * total_false_positives
    average_bit_overhead = math.ceil(total_bit_overhead / len(agg_to_port_dict))
    print(f'\t{len(agg_to_port_dict)} aggregate switches, total false positives: {total_false_positives}, \
        link_id_bit_overhead: {link_id_bit_overhead} bits --> \n\
        \t\ttotal overhead: {total_bit_overhead} bits, {int(total_bit_overhead / 8)} bytes\n\
        \t\taverage overhead per switch: {average_bit_overhead} bits, {int(average_bit_overhead / 8)} bytes')



def calc_orca_bit_num(K, F):
    L_d = int(K/2)
    L_u = int(K/2)
    P_d = int(K/2)
    P_u = int(K/2)
    C_d = K

    bit_over_head = int(1 + max(L_d, math.ceil(math.log2(L_u)) + P_d + F + math.ceil(math.log2(P_u)) + C_d))
    print(f'\tOverhead per packet: {bit_over_head} bits, {math.ceil(bit_over_head/8)} bytes')

for hash_funciton_num in hash_funciton_nums:
    for K in Ks:
        if (K > NUM_SWITCH_PORTS):
            raise Exception("number of pods cannot excess NUM_SWITCH_PORTS")
        for receiver_percentage in receiver_percentages:
            for F in filter_sizes_bits:
                print(f'K: {K}, receiver_percentage: {receiver_percentage*100}%, F: {F}, hash_funciton_num: {hash_funciton_num}')
                receiver_list = random.sample(range(0, get_server_num(K)), int(receiver_percentage * get_server_num(K)))
                calc_orca_bit_num(K, F)
                calc_orca_state_per_switch(K, F, receiver_list, hash_funciton_num)
                print()
            print('-----------------------\n\n')