from distutils import core
from math import ceil
import math

num_nodes_under_each_leaf = 16

def peel_hdr_overhead(k):
    switch_port = k
    prefix_value = ceil(math.log2(switch_port))
    prefix_len = int(math.log2(prefix_value)) + 1

    hop_layers = 5

    total_bits = hop_layers * (prefix_value + prefix_len)

    print(f'Peel -- k: {k}')
    print(f'Naive overhead: {total_bits} b --- {ceil(total_bits / 8.0)} B')
    print('\n--------------------------------------\n')

def hdr_overhead_naive(k, scale):
    num_dst_leafs = ceil(scale/num_nodes_under_each_leaf)
    dst_pod_number = ceil(num_dst_leafs * 1.0 / (k/2))
    num_dst_spines = ceil(dst_pod_number * (k/2))
    if (dst_pod_number <= 1):
        num_cores = 0
        core_id_bits = 0
    else:
        num_cores = int(k/2 * k/2)
        core_id_bits = ceil(math.log2(num_cores))
    
    # every switch is assigned a unique id in our network
    leaf_id_bits = ceil(math.log2(k*(k/2)))
    spine_id_bits = leaf_id_bits

    # for every switch, we add id plus number of ports bits
    leaf_bits = num_dst_leafs * (leaf_id_bits + k)
    spine_bits = num_dst_spines * (spine_id_bits + k)
    core_bits = num_cores * (core_id_bits + k)

    total_bits = leaf_bits + spine_bits + core_bits

    print(f'Naive -- k: {k} -- scale: {scale}')
    print(f'leaf overhead: {leaf_bits} b -- spine overhead: {spine_bits} b -- core overhead: {core_bits} b')
    print(f'Naive overhead: {total_bits} b --- {ceil(total_bits / 8.0)} B')
    print('\n--------------------------------------\n')


def hdr_overhead_d2(k, scale):
    kmax = int(k/2)
    num_dst_leafs = ceil(scale/num_nodes_under_each_leaf)
    dst_pod_number = ceil(num_dst_leafs * 1.0 / (k/2))
    num_dst_spines = ceil(dst_pod_number * (k/2))
    if (dst_pod_number <= 1):
        num_cores = 0
        core_id_bits = 0
    else:
        num_cores = int(k/2 * k/2)
        core_id_bits = ceil(math.log2(num_cores))
    
    # every switch is assigned a unique id in our network
    leaf_id_bits = ceil(math.log2(k*(k/2)))
    spine_id_bits = ceil(math.log2(k))  # all spines in a pod shrink into one logical switch

    leafu_bits = leaf_id_bits + num_nodes_under_each_leaf - 1 + 1   # sending down + one bit for upstream multipath
    spineu_bits = spine_id_bits + (min(k/2, num_dst_leafs) - 1) + 1 # sending down to destination leafs + one bit for upstream multipath
    cored_bits = min(num_cores, 1) * (core_id_bits + k)  # core downlink bits
    spined_bits = (dst_pod_number-1) * (spine_id_bits + k/2) # all downstream spines in other pods are considered one per pod
    leafd_bits = (num_dst_leafs - 1) * (leaf_id_bits + num_nodes_under_each_leaf) # all leafs except for the source leaf apply the same 

    total_bits = leafu_bits + spineu_bits + cored_bits + spined_bits + leafd_bits

    print(f'D2 -- k: {k} -- scale: {scale}')
    print(f'leaf upside overhead: {leafu_bits} b -- spine upside overhead: {spineu_bits} b --\
        core downside overhead: {cored_bits} b -- spine downside overhead: {spined_bits} b --\
        leaf downside overhead: {leafd_bits} b')
    print(f'Naive overhead: {total_bits} b --- {ceil(total_bits / 8.0)} B')
    print('\n--------------------------------------\n')


def hdr_overhead_d2_d3(k, scale):
    kmax = int(k/2)
    num_dst_leafs = ceil(scale/num_nodes_under_each_leaf)
    dst_pod_number = ceil(num_dst_leafs * 1.0 / (k/2))
    num_dst_spines = ceil(dst_pod_number * (k/2))
    if (dst_pod_number <= 1):
        num_cores = 0
        core_id_bits = 0
    else:
        num_cores = int(k/2 * k/2)
        core_id_bits = ceil(math.log2(num_cores))
    
    # every switch is assigned a unique id in our network
    leaf_id_bits = ceil(math.log2(k*(k/2)))
    spine_id_bits = ceil(math.log2(k))  # all spines in a pod shrink into one logical switch

    leafu_bits = leaf_id_bits + num_nodes_under_each_leaf - 1 + 1   # sending down + one bit for upstream multipath
    spineu_bits = spine_id_bits + (min(k/2, num_dst_leafs) - 1) + 1 # sending down to destination leafs + one bit for upstream multipath
    cored_bits = min(num_cores, 1) * (core_id_bits + k)  # core downlink bits
    spined_bits = ceil((dst_pod_number-1) / kmax) * (k/2) + (dst_pod_number-1) * (spine_id_bits)    # we group logical spines into groups of kmax and presend them with one p-rule and a list of ids 
    leafd_bits = ceil((num_dst_leafs-1) / kmax) * num_nodes_under_each_leaf + (num_dst_leafs - 1) * (leaf_id_bits) # similar to spine, downstream leafs are grouped together 

    total_bits = leafu_bits + spineu_bits + cored_bits + spined_bits + leafd_bits

    print(f'D2+D3 -- k: {k} -- scale: {scale}')
    print(f'leaf upside overhead: {leafu_bits} b -- spine upside overhead: {spineu_bits} b --\
        core downside overhead: {cored_bits} b -- spine downside overhead: {spined_bits} b --\
        leaf downside overhead: {leafd_bits} b')
    print(f'Naive overhead: {total_bits} b --- {ceil(total_bits / 8.0)} B')
    print('\n--------------------------------------\n')


def hdr_overhead_asym(k, scale):
    kmax = int(k/2)
    num_dst_leafs = ceil(scale/num_nodes_under_each_leaf)
    dst_pod_number = ceil(num_dst_leafs * 1.0 / (k/2))
    num_dst_spines = ceil(dst_pod_number * (k/2))
    if (dst_pod_number <= 1):
        num_cores = 0
        core_id_bits = 0
    else:
        num_cores = int(k/2 * k/2)
        core_id_bits = ceil(math.log2(num_cores))
    
    # every switch is assigned a unique id in our network
    leaf_id_bits = ceil(math.log2(k*(k/2)))
    spine_id_bits = leaf_id_bits    # d2 is disabled so we don't shrink the spines into one logical switch

    leafu_bits = leaf_id_bits + num_nodes_under_each_leaf + (k/2)   # since spines are not grouped together, we need all ports bits
    # we assume that the upstream is divided by two in every level
    spineu_bits = 2 * (spine_id_bits + k) # cores are not grouped together so it's all port bits
    cored_bits = 4 * (min(num_cores, 1) * (core_id_bits + k))  # core downlink bits
    spined_bits = ceil((dst_pod_number-1) / kmax) * (k/2) + (dst_pod_number-1) * (spine_id_bits)    # we group logical spines into groups of kmax and presend them with one p-rule and a list of ids 
    leafd_bits = ceil((num_dst_leafs-1) / kmax) * num_nodes_under_each_leaf + (num_dst_leafs - 1) * (leaf_id_bits) # similar to spine, downstream leafs are grouped together 

    total_bits = leafu_bits + spineu_bits + cored_bits + spined_bits + leafd_bits

    print(f'Asym (no D2) -- k: {k} -- scale: {scale}')
    print(f'leaf upside overhead: {leafu_bits} b -- spine upside overhead: {spineu_bits} b --\
        core downside overhead: {cored_bits} b -- spine downside overhead: {spined_bits} b --\
        leaf downside overhead: {leafd_bits} b')
    print(f'Naive overhead: {total_bits} b --- {ceil(total_bits / 8.0)} B')
    print('\n--------------------------------------\n')

# k = 8
# scale = 32
# while (scale <= 256):
#     print('#################################\n')
#     hdr_overhead_naive(k, scale)
#     hdr_overhead_d2(k, scale)
#     hdr_overhead_d2_d3(k, scale)
#     hdr_overhead_asym(k, scale)
#     scale *= 2

# peel_hdr_overhead(k)
# print('\n\n\n')

# k = 16
# scale = 64
# while (scale <= 1024):
#     print('#################################\n')
#     hdr_overhead_naive(k, scale)
#     hdr_overhead_d2(k, scale)
#     hdr_overhead_d2_d3(k, scale)
#     hdr_overhead_asym(k, scale)
#     scale *= 2

# peel_hdr_overhead(k)
# print('\n\n\n')

# k = 32
# scale = 64
# while (scale <= 4096):
#     print('#################################')
#     # hdr_overhead_naive(k, scale)
#     # hdr_overhead_d2(k, scale)
#     hdr_overhead_d2_d3(k, scale)
#     # hdr_overhead_asym(k, scale)
#     scale *= 2

# peel_hdr_overhead(k)
# # print('\n\n\n')

# k = 64
# scale = 64
# while (scale <= 16384):
#     # print('#################################')
#     # hdr_overhead_naive(k, scale)
#     # hdr_overhead_d2(k, scale)
#     hdr_overhead_d2_d3(k, scale)
#     # hdr_overhead_asym(k, scale)
#     scale *= 2


# peel_hdr_overhead(k)
# print('\n\n\n')


# k = 8
# scale = 128
# while (k <= 256):
#     hdr_overhead_d2_d3(k, scale)
#     peel_hdr_overhead(k)
#     k *= 2

# print('\n\n\n')


k = 8
receiver_percentages = 0.2
while (k <= 256):
    total_nodes = int(k * k/2.0 * num_nodes_under_each_leaf)
    scale = receiver_percentages * total_nodes
    hdr_overhead_d2_d3(k, scale)
    peel_hdr_overhead(k)
    k *= 2

print('\n\n\n')
