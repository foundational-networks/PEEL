import csv
from fileinput import filename
import gzip
import os.path
import math
import statistics
from tracemalloc import start


from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np
from Common import *
# from pymsgbox import *

class OOOInfo:
    def __init__(self, name) -> None:
        self.name = name
        self.oooNum = None
        self.pktRcvdNum = None
        
directory1 = BASE_DIR + 'LIGHT_OOO_DATA_COUNT/'
directory2 = BASE_DIR + 'LIGHT_DATA_PKT_RCVD_NUM/'

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
                                    ooo_dict = dict()

                                    # LIGHT_OOO_DATA_COUNT/ --> server name and ooo count
                                    with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                continue

                                            ooo_count = int(row[-1])
                                            node_name = row[-3]
                                            ooo_dict[node_name] = OOOInfo(node_name)
                                            ooo_dict[node_name].oooNum = ooo_count
                                    
                                    # LIGHT_DATA_PKT_RCVD_NUM/ --> server name and rcvd packet num count
                                    with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                continue

                                            pkt_rcvd_count = int(row[-1])
                                            node_name = row[-3]
                                            if node_name not in ooo_dict:
                                                raise Exception(f"{node_name} not in ooo_dict")
                                            ooo_dict[node_name].pktRcvdNum = pkt_rcvd_count

                                    sum_pkts_rcvd = 0
                                    sum_ooo = 0
                                    for ooo_info in ooo_dict:
                                        sum_pkts_rcvd += ooo_dict[ooo_info].pktRcvdNum
                                        sum_ooo += ooo_dict[ooo_info].oooNum
                                    
                                    print(f'pkt num: {sum_pkts_rcvd}, ooo num: {sum_ooo}, perc: {sum_ooo/sum_pkts_rcvd*100}%')
                                    print('-----------------\n')
        print('\n\n')



    if num_iterations > TARGET_NUM_ITERATIONS:
        raise Exception("num_iterations > TARGET_NUM_ITERATIONS")
    for iteration in range(num_iterations, TARGET_NUM_ITERATIONS):
        print()
