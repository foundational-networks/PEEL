import csv
from fileinput import filename
import gzip
import os.path
import math
import statistics


from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np
from Common import *
# from pymsgbox import *

def is_power_of_two(n):
  if n <= 0:
    return False

  return (n & (n - 1)) == 0

directory1 = BASE_DIR + 'UTILIZATION/'
directory2 = BASE_DIR + 'LIGHT_TREE_COUNTER/'

bg_inter_arrival_mult = BG_INTER_ARRIVAL_MULT[0]
incast_inter_arrival_mult = INCAST_INTER_ARRIVAL_MULT[0]
incast_flow_size = INCAST_FLOW_SIZE[0]
incast_scale = NUM_REQUESTS_PER_BURST[0]

LOAD_GEN_TIME = 0.1

for category in CATEGORIES:
    num_iterations = 0

    if ('chunk' not in category):
        CHUNK_SIZES = [0]
    if ('subf' not in category):
        SUB_FLOW_NUMS = [0]
        
    for ml_query_inter_arrival_mult in ML_QUERY_INTER_MULT_LIST:
        for ml_query_scale in ML_QUERY_SCALE_LIST:
            for ml_query_fsize in ML_FLOW_SIZE_LIST:
                for ml_num_gpus in ML_NUM_GPUS:
                    for nvlink_speed_gbps in ML_NVLINK_Gbps:
                        for chunksize in CHUNK_SIZES:
                            for subf_num in SUB_FLOW_NUMS:
                                num_iterations += 1
                                all_flows = []
                                all_queries = []
                                for rep_num in range(REP_NUM):
                                    
                                    file_name = '{}_{}_{}_ls_{}_mlapp_{}_bursty_{}_bg' \
                                                '_{}_reqpburst_{}_bgintermult_{}_burstyintermult' \
                                                '_{}_mlqueryintermult' \
                                                '_{}_incastfsize_{}_mlscale_{}_mlfsize' \
                                                '_{}_gpus_{}_nv'.format(
                                        BASE_SPINE_NUM, BASE_AGG_NUM,
                                        SERVERS_UNDER_EACH_RACK,
                                        NUM_ML_APPS, NUM_BURSTY_APPS,
                                        BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                        incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                        ml_query_inter_arrival_mult, incast_flow_size, ml_query_scale,
                                        ml_query_fsize, ml_num_gpus,
                                        nvlink_speed_gbps)

                                    if ('chunk' in category):
                                        file_name += f'_{chunksize}_chunk'
                                    if ('subf' in category):
                                        file_name += f'_{subf_num}_subf'

                                    file_name += f'_{rep_num}_rep_{category}.csv'

                                    print(file_name)

                                    active_leaf_name_set = set()
                                    with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                continue
                                            bps = float(row[-1])
                                            node_name = row[-3]
                                            port_name = node_name.split('eth[')[1]
                                            port_name = port_name.split(']')[0]
                                            port_idx = int(port_name)
                                            # if (bps > 0):
                                            if (row[-2] == 'bits/sec sent'):
                                                # sent
                                                if (bps > 0 and 'agg[' in node_name and port_idx >= (ml_num_gpus * SERVERS_UNDER_EACH_RACK)):
                                                    node_idx = node_name.split('agg[')[1]
                                                    node_idx = node_idx.split(']')[0]
                                                    node_idx = int(node_idx)
                                                    active_leaf_name_set.add(f'agg[{node_idx}]')



                                    utilization_list = []
                                    active_leaf_utilization_list = []   # only non-zero values
                                    spine_dict = dict()
                                    spine_to_leaf_utilization = []
                                    # UTILIZATION/ --> flow id and flow start time
                                    with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                continue
                                            bps = float(row[-1])
                                            node_name = row[-3]
                                            port_name = node_name.split('eth[')[1]
                                            port_name = port_name.split(']')[0]
                                            port_idx = int(port_name)
                                            # if (bps > 0):
                                            if (row[-2] == 'bits/sec sent'):
                                                # sent
                                                if ('agg[' in node_name and port_idx >= (ml_num_gpus * SERVERS_UNDER_EACH_RACK)):
                                                    utilization_list.append(bps)
                                                    node_idx = node_name.split('agg[')[1]
                                                    node_idx = node_idx.split(']')[0]
                                                    node_idx = int(node_idx)
                                                    if (f'agg[{node_idx}]' in active_leaf_name_set):
                                                        active_leaf_utilization_list.append(bps)
                                                if ('spine[' in node_name):
                                                    spine_to_leaf_utilization.append(bps)
                                            else:
                                                if (bps > 0):
                                                    # recieved
                                                    if ('spine[' in node_name):
                                                        spine_name = node_name.split('spine[')[1]
                                                        spine_name = spine_name.split(']')[0]
                                                        spine_idx = int(spine_name)
                                                        spine_name = f'spine[{spine_idx}]'
                                                        if spine_name not in spine_dict:
                                                            spine_dict[spine_name] = 0
                                                        spine_dict[spine_name] += 1
                                    
                                    tree_counter_dict = dict()
                                    # LIGHT_TREE_COUNTER/
                                    with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                continue
                                            tree_num = int(row[-1])
                                            node_name = row[-3]
                                            if ('spine[' in node_name):
                                                spine_name = node_name.split('spine[')[1]
                                                spine_name = spine_name.split(']')[0]
                                                spine_idx = int(spine_name)
                                                spine_name = f'spine[{spine_idx}]'
                                                if spine_name in tree_counter_dict:
                                                    raise Exception("Repititive spine?")
                                                tree_counter_dict[spine_name] = tree_num

                                                
                                    
                                    # scale based on end time / sim time also change it to mbps
                                    utilization_list_mbps = [u*(10.2/LOAD_GEN_TIME)/(10**6) for u in utilization_list]
                                    active_leaf_utilization_list_mbps = [u*(10.2/LOAD_GEN_TIME)/(10**6) for u in active_leaf_utilization_list]
                                    spine_to_leaf_utilization_mbps =  [u*(10.2/LOAD_GEN_TIME)/(10**6) for u in spine_to_leaf_utilization]
                                    trees_per_spines_list = list(tree_counter_dict.values())
                                    print(f'active_leaf_utilization_list_mbps: {active_leaf_utilization_list_mbps}')
                                    print(f'{len(active_leaf_name_set)} active leafs: {active_leaf_name_set}')
                                    print(f'mean: {np.mean(utilization_list_mbps)} Mbps')
                                    print(f'std dev: {statistics.stdev(utilization_list_mbps)} Mbps')
                                    print(f'active leaf -- mean: {np.mean(active_leaf_utilization_list_mbps)} Mbps')
                                    print(f'active leaf -- std dev: {statistics.stdev(active_leaf_utilization_list_mbps)} Mbps')
                                    print(f'Mean non-zero links per spine: {np.mean(list(spine_dict.values()))}')
                                    print(f'std dev non-zero links per spine: {statistics.stdev(list(spine_dict.values()))}')
                                    print(f'Number of non-zero links per spine: {spine_dict}')
                                    print(f'Mean spine to leaf utilization: {np.mean(spine_to_leaf_utilization_mbps)} Mbps')
                                    print(f'stddev spine to leaf utilization: {statistics.stdev(spine_to_leaf_utilization_mbps)} Mbps')
                                    print(f'p50 spine to leaf utilization: {np.percentile(spine_to_leaf_utilization_mbps, 50)} Mbps')
                                    print(f'p90 spine to leaf utilization: {np.percentile(spine_to_leaf_utilization_mbps, 90)} Mbps')
                                    print(f'load generation time: {LOAD_GEN_TIME}')
                                    print(f'trees per spines list: {list(tree_counter_dict.values())}')
                                    print(f'Mean #trees per spine: {np.mean(trees_per_spines_list)}')
                                    print(f'Stddev #trees per spine: {statistics.stdev(trees_per_spines_list)}')
                                    print('-----------------\n\n')



    if num_iterations > TARGET_NUM_ITERATIONS:
        raise Exception("num_iterations > TARGET_NUM_ITERATIONS")
    for iteration in range(num_iterations, TARGET_NUM_ITERATIONS):
        print()
