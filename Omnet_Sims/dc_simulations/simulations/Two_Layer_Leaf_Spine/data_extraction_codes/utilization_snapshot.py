import csv
from fileinput import filename
import gzip
import os.path
import math
from platform import node
import statistics
from tracemalloc import start


from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np
from Common import *
# from pymsgbox import *

MAX_TIME = 1.2  # 1.2S

class UtilizationInfo:
    def __init__(self, name, record_start, record_end, popped_start, popped_end) -> None:
        self.name = name
        self.record_start = record_start
        self.record_end = record_end
        self.popped_start = popped_start
        self.popped_end = popped_end

    def print_info(self):
        print(f'{self.name}: {self.record_start} s to {self.record_end} s -- from {self.popped_start} B to {self.popped_end} B')

    def get_util(self):
        # self.print_info()
        popped_bytes = self.popped_end - self.popped_start
        if (popped_bytes == 0):
            return 0
        if (self.record_end <= self.record_start):
            raise Exception("Invalid start end time!")
        util = (popped_bytes * 8.0) / (self.record_end - self.record_start)
        # print(f'{self.name}: {util / 10**6}')
        # raise Exception(f"{util}")
        return util

directory1 = BASE_DIR + 'AGG_POPPED_BYTES/'

bg_inter_arrival_mult = BG_INTER_ARRIVAL_MULT[0]
incast_inter_arrival_mult = INCAST_INTER_ARRIVAL_MULT[0]
incast_flow_size = INCAST_FLOW_SIZE[0]
incast_scale = NUM_REQUESTS_PER_BURST[0]

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
                                    spine_dict = dict()
                                    utilization_list = []
                                    max_record_time = 0
                                    min_record_time = 10**6
                                    # AGG_POPPED_BYTES/ --> flow id and flow start time
                                    with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                continue

                                            for idx in range(0, len(row), 2):
                                                node_name = row1[idx]
                                                if row[idx] == '':
                                                    continue
                                                if (float(row[idx]) >= MAX_TIME):
                                                    continue
                                                if ('spine[' in node_name):
                                                    spine_name = node_name.split('spine[')[1]
                                                    spine_name = spine_name.split(']')[0]
                                                    spine_idx = int(spine_name)
                                                    eth_name = node_name.split('eth[')[1]
                                                    eth_name = eth_name.split(']')[0]
                                                    eth_idx = int(eth_name)
                                                    node_name = f'spine[{spine_idx}].eth[{eth_idx}]'
                                                    if node_name not in spine_dict:
                                                        spine_dict[node_name] = UtilizationInfo(node_name, float(row[idx]), \
                                                            float(row[idx]), int(row[idx+1]), int(row[idx+1]))
                                                    spine_dict[node_name].record_end = float(row[idx])
                                                    spine_dict[node_name].popped_end = int(row[idx+1])
                                                    max_record_time = max(max_record_time, float(row[idx]))
                                                    min_record_time = min(min_record_time, float(row[idx]))

                                    for spine in spine_dict:
                                        spine_dict[spine].record_start = min_record_time
                                        spine_dict[spine].record_end = max_record_time
                                        utilization_list.append(spine_dict[spine].get_util())

                                    print(f'Max time: {MAX_TIME} s')
                                    print(f'min_record_time: {min_record_time} s, max_record_time: {max_record_time} s')
                                    print(f'mean: {np.mean(utilization_list) / (10**6)} Mbps')
                                    print(f'std dev: {statistics.stdev(utilization_list) / (10**6)} Mbps')
                                    print(f'std_dev/mean (perc): {statistics.stdev(utilization_list) / np.mean(utilization_list) * 100} %')
                                    print(f'Max: {max(utilization_list) / (10**6)} Mbps')
                                    print('-----------------\n\n')



    if num_iterations > TARGET_NUM_ITERATIONS:
        raise Exception("num_iterations > TARGET_NUM_ITERATIONS")
    for iteration in range(num_iterations, TARGET_NUM_ITERATIONS):
        print()
