from http import server
import math
import hashlib
import random

Ks = [6, 8, 10, 12, 14]
Ks = [4, 8, 16, 32, 64]
NUM_SWITCH_PORTS = 64
receiver_percentages = [0.2]
hash_funciton_nums = [4]

def get_server_num(k):
    return int(k**3 / 4.0)

def get_core_num(k):
    return int(k**2 / 4.0)

def get_servers_per_pod(k):
    return int(k**2 / 4.0)

def get_pod_idx_for_server(k, server_idx):
    return int(server_idx / get_servers_per_pod(k))

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

# RSBF
def initialize_bitstream(length):
    length = int(length)
    return ''.join('0' for _ in range(length))

def random_bitstream(length):
    return ''.join(random.choice('01') for _ in range(length))

# Define hash functions using those seeds
def hash_with_seed(seed, value):
    combined = f"{seed}_{value}"
    hash_object = hashlib.sha256(combined.encode())
    return int(hash_object.hexdigest(), 16)

def check_s(s, M):
    s_lengths = [len(i) for i in s]
    if (sum(s_lengths) != M):
        print(M)
        print(sum(s_lengths))
        raise Exception("sum s != M")

def replace_char(s, i, new_char):
    if i < 0 or i >= len(s):
        raise IndexError("Index out of range")
    return s[:i] + new_char + s[i+1:]

def apply_hash_functions(s, seeds, receiver_host_list):
    receiver_pod_number = get_pod_number_for_servers(K, receiver_host_list)
    receiver_edge_number = get_edge_number_for_servers(K, receiver_host_list)
    receiver_host_num = len(receiver_host_list)
    port_num_list = [2, receiver_pod_number, receiver_edge_number + receiver_host_num]
    for i in range(len(port_num_list)):
        for j in range(port_num_list[i]):
            for seed in seeds:
                m_hash = hash_with_seed(seed, j)
                bit_idx = m_hash % len(s[i])
                s[i] = replace_char(s[i], bit_idx, '1')

def get_f_num(s, seeds, receiver_host_list):
    receiver_pod_number = get_pod_number_for_servers(K, receiver_host_list)
    receiver_edge_number = get_edge_number_for_servers(K, receiver_host_list)
    total_switch_num_at_each_layer = [2, 1, receiver_pod_number+receiver_edge_number]
    total_port_num_at_each_layer = [i * NUM_SWITCH_PORTS if i is not None else None for i in total_switch_num_at_each_layer]
    final_f_num = 0
    for layer_idx in range(len(total_port_num_at_each_layer)):
        f_num = 0
        for port_idx in range(total_port_num_at_each_layer[layer_idx]):
            for seed_idx, seed in enumerate(seeds):
                m_hash = hash_with_seed(seed, port_idx)
                bit_idx = m_hash % len(s[layer_idx])
                if (s[layer_idx][bit_idx] != '1'):
                    break
                if (seed_idx == len(seeds) - 1):
                    # all bits are 1
                    f_num += 1
        # if (layer_idx < len(total_port_num_at_each_layer)-1):
        #     if (f_num < total_switch_num_at_each_layer[layer_idx+1]):
        #         print(s)
        #         print(total_port_num_at_each_layer[layer_idx+1])
        #         print(f_num)
        #         print(total_switch_num_at_each_layer[layer_idx+1])
        #         raise Exception("f_num should always end up being >= total_switch_num_at_each_layer[layer_idx+1]")
        #     total_switch_num_at_each_layer[layer_idx+1] = f_num
        #     total_port_num_at_each_layer = [i * NUM_SWITCH_PORTS if i is not None else None for i in total_switch_num_at_each_layer]
        final_f_num += f_num
    return final_f_num


def calc_rsbf_bit_size_per_BF(receiver_host_list, M, hash_funciton_num, I=10):
    seeds = [f"seed{i}" for i in range(hash_funciton_num)]
    receiver_pod_number = get_pod_number_for_servers(K, receiver_host_list)
    receiver_edge_number = get_edge_number_for_servers(K, receiver_host_list)
    receiver_host_num = len(receiver_host_list)
    port_num_list = [2, receiver_pod_number, receiver_edge_number + receiver_host_num]
    # we assume we have 3 BFs
    # layers 1, 2, 3
    s_curr = [initialize_bitstream(M // 3), initialize_bitstream(M // 3), initialize_bitstream(M - M // 3 - M // 3)]
    check_s(s_curr, M)
    apply_hash_functions(s_curr, seeds, receiver_host_list)
    f_curr = get_f_num(s_curr, seeds, receiver_host_list)
    for _ in range(I):
        f_try = f_curr
        s_try = [i for i in s_curr]
        for layer_idx in range(len(s_try)-2, -1,-1):
            while (port_num_list[layer_idx] / len(s_try[layer_idx]) > port_num_list[layer_idx+1] / len(s_try[layer_idx+1])):
                extended_bit_num = random.randint(1, M)
                s_try[layer_idx] = s_curr[layer_idx] + random_bitstream(extended_bit_num)
        last_element_size = M - sum(len(s_try[i]) for i in range(len(s_try)-1))
        if last_element_size <= 0:
            continue
        new_bit_num_list = [len(i) for i in s_try]
        s_try = [initialize_bitstream(new_bit_num_list[0]), \
            initialize_bitstream(new_bit_num_list[1]), \
                initialize_bitstream(last_element_size)]
        check_s(s_try, M)
        apply_hash_functions(s_try, seeds, receiver_host_list)
        f_try = get_f_num(s_try, seeds, receiver_host_list)
        if (f_curr > f_try):
            f_curr = f_try
            s_curr = [i for i in s_try]
    check_s(s_curr, M)
    result = [len(i) for i in s_curr]
    result_bytes = [int(i/8.0) for i in result]
    print(f'number of Bytes in each layer: {result_bytes}')
    return result


def calc_avg_header_size(receiver_host_list, M, hash_funciton_num):
    receiver_pod_number = get_pod_number_for_servers(K, receiver_host_list)
    receiver_edge_number = get_edge_number_for_servers(K, receiver_host_list)
    total_switches_traversed = 1 + 1 + 1 + receiver_pod_number + receiver_edge_number
    bit_allocation_list = calc_rsbf_bit_size_per_BF(receiver_host_list, M, hash_funciton_num)
    over_head_per_layer = [M, M-bit_allocation_list[0], M-bit_allocation_list[0]-bit_allocation_list[1]]
    average_header_size = (1 * over_head_per_layer[0] + 1 * over_head_per_layer[0] + 1 * over_head_per_layer[1] + \
        receiver_pod_number * over_head_per_layer[2] + receiver_edge_number * over_head_per_layer[2]) / total_switches_traversed
    return average_header_size

def calc_rsbf_bit_num(hash_funciton_num, receiver_host_list, K, target_false_positive_ratio):
    '''
        hash_funciton_num --> number of hash functions applied to each port
        total_forwarding_port_num --> a^1+a^2+a^3
        target_false_positive_ratio --> fp
    '''
    receiver_pod_number = get_pod_number_for_servers(K, receiver_host_list)
    receiver_edge_number = get_edge_number_for_servers(K, receiver_host_list)
    receiver_host_num = len(receiver_host_list)
    total_forwarding_port_num = 1 + 1 + receiver_pod_number + receiver_edge_number + receiver_host_num
    assert(0 < target_false_positive_ratio <= 1)
    denominator = (target_false_positive_ratio) ** (1.0 / hash_funciton_num)
    denominator = (1.0 - denominator)
    denominator = (denominator) ** (1.0 / (hash_funciton_num * total_forwarding_port_num))
    denominator = (1.0 - denominator)
    M = 1.0 / denominator
    avg_header_size = calc_avg_header_size(receiver_host_list, int(M), hash_funciton_num)
    print(f'K={K}, hash_funciton_num={hash_funciton_num}, a^1+a^2+a^3={total_forwarding_port_num}, \
        fp={target_false_positive_ratio*100}%\n\
        --> M={int(M)} bits\t{int(M/8.0)} bytes\n\
        --> avg header size = {int(avg_header_size)} bits\t{int(avg_header_size/8.0)} bytes')
    return M

for hash_funciton_num in hash_funciton_nums:
    for K in Ks:
        if (K > NUM_SWITCH_PORTS):
            raise Exception("number of pods cannot excess NUM_SWITCH_PORTS")
        for receiver_percentage in receiver_percentages:
            print(f'K: {K}, receiver_percentage: {receiver_percentage*100}%')
            receiver_list = random.sample(range(0, get_server_num(K)), int(receiver_percentage * get_server_num(K)))
            calc_rsbf_bit_num(hash_funciton_num, receiver_list, K, 0.01)
            calc_rsbf_bit_num(hash_funciton_num, receiver_list, K, 0.05)
            calc_rsbf_bit_num(hash_funciton_num, receiver_list, K, 0.1)
            calc_rsbf_bit_num(hash_funciton_num, receiver_list, K, 0.15)
            calc_rsbf_bit_num(hash_funciton_num, receiver_list, K, 0.2)
            print('-----------------------\n\n')