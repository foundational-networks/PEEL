import csv
import gzip
import os.path

from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np
from Common import *
# from pymsgbox import *
import math

directory1 = BASE_DIR + 'QUEUEING_TIME/'

CATEGORIES = ["none"]
for category in CATEGORIES:
    print('\n')
    print ('Category is {}'.format(category))
    for ml_query_inter_arrival_mult in ML_QUERY_INTER_MULT_LIST:
        for ml_query_scale in ML_QUERY_SCALE_LIST:
            for ml_query_fsize in ML_FLOW_SIZE_LIST:
                for marking_timer in MARKING_TIMER:
                    for ordering_timer in ORDERING_TIMER:
                        for bg_inter_arrival_mult in BG_INTER_ARRIVAL_MULT:
                            for incast_inter_arrival_mult in INCAST_INTER_ARRIVAL_MULT:
                                for incast_flow_size in INCAST_FLOW_SIZE:
                                    for incast_scale in NUM_REQUESTS_PER_BURST:
                                        if category == "dctcp_vertigo1_mem1":
                                            random_power_factor = 1
                                            random_power_bounce_factor = 1
                                        for rep_num in range(REP_NUM):
                                            if TOPOLOGY == FAT_TREE:
                                                file_name = '{}_k_{}_burstyapps_{}_mice' \
                                                            '_{}_reqPerBurst_{}_bgintermult_{}_burstyintermult' \
                                                            '_{}_rndfwfactor_{}_rndbouncefactor_{}_incastfsize_' \
                                                            '{}_mrktimer_{}_ordtimer_{}_rep_{}.csv'.format(
                                                    K, NUM_BURSTY_APPS,
                                                    BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                                    incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                                    random_power_factor, random_power_bounce_factor, incast_flow_size,
                                                    marking_timer, ordering_timer, rep_num, category)

                                            else:
                                                file_name = '{}_{}_{}_ls_{}_mlapp_{}_bursty_{}_bg' \
                                                            '_{}_reqpburst_{}_bgintermult_{}_burstyintermult' \
                                                            '_{}_mlqueryintermult' \
                                                            '_{}_incastfsize_{}_mlscale_{}_mlfsize' \
                                                            '_{}_mark_{}_ord_{}_rep_{}.csv'.format(
                                                    BASE_SPINE_NUM, BASE_AGG_NUM,
                                                    SERVERS_UNDER_EACH_RACK,
                                                    NUM_ML_APPS, NUM_BURSTY_APPS,
                                                    BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                                    incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                                    ml_query_inter_arrival_mult, incast_flow_size, ml_query_scale,
                                                    ml_query_fsize, marking_timer, ordering_timer, rep_num, category)
                                            first_line = True
                                            row1 = None
                                            agg_queueing_time_mapper = [[] for i in range(int(queueing_time_start * MULTIPLIER), int(queueing_time_end * MULTIPLIER), int(queueing_time_gap * MULTIPLIER))]
                                            spine_queueing_time_mapper = [[] for i in range(int(queueing_time_start * MULTIPLIER), int(queueing_time_end * MULTIPLIER), int(queueing_time_gap * MULTIPLIER))]
                                            
                                            print(file_name)

                                            with gzip.open(directory1 + file_name + '.gz', 'rt') as file1:
                                                file_reader1 = csv.reader(file1, delimiter=',')
                                                for line1 in file_reader1:
                                                    if first_line:
                                                        row1 = line1
                                                        first_line = False
                                                        continue
                                                    for idx in range(0, len(line1), 2):
                                                        # print(row1[idx])
                                                        if line1[idx] != '' and queueing_time_start <= float(line1[idx]) < queueing_time_end:
                                                            # print(line1)
                                                            time = float(line1[idx])
                                                            m_idx = int((time - queueing_time_start) / queueing_time_gap)

                                                            # if 'agg[' in row1[idx] and ('eth[{}'.format(SERVERS_UNDER_EACH_RACK) in row1[idx] or 'eth[{}'.format(SERVERS_UNDER_EACH_RACK+1) in row1[idx]):
                                                            #     agg_queueing_time_mapper[m_idx].append(float(line1[idx+1]))
                                                            # elif 'spine[' in row1[idx]:
                                                            #     spine_queueing_time_mapper[m_idx].append(float(line1[idx+1]))

                                                            # packets from s0 to s15 go through a0.eth[11]-->sp1.eth[1]-->a1
                                                            if 'agg[0]' in row1[idx] and 'eth[11]' in row1[idx]:
                                                                agg_queueing_time_mapper[m_idx].append(float(line1[idx+1]))
                                                            elif 'spine[1]' in row1[idx] and 'eth[1]' in row1[idx]:
                                                                spine_queueing_time_mapper[m_idx].append(float(line1[idx+1]))
                                            
                                            # print(agg_queueing_time_mapper)
                                            # print(spine_queueing_time_mapper)
                                            if (sum([len(i) for i in agg_queueing_time_mapper]) == 0 or sum([len(i) for i in spine_queueing_time_mapper]) == 0):
                                                raise Exception("Some lists are empty!")
                                            
                                            with open("archive/sim_le_1G_{}.csv".format(ml_query_inter_arrival_mult), "w", newline='') as file:
                                                writer = csv.DictWriter(file, fieldnames = ["sample", "unit"])
                                                writer.writeheader()
                                                for i in agg_queueing_time_mapper:
                                                    Q_delay = np.mean(i)
                                                    if (math.isnan(Q_delay)):
                                                        continue
                                                    print(Q_delay)
                                                    Q_delay *= (10**9)
                                                    writer.writerow({"sample": Q_delay, "unit": "ns"})
                                            
                                            print('-----------------------------------')

                                            with open("archive/sim_sp_1G_{}.csv".format(ml_query_inter_arrival_mult), "w", newline='') as file:
                                                writer = csv.DictWriter(file, fieldnames = ["sample", "unit"])
                                                writer.writeheader()
                                                for i in spine_queueing_time_mapper:
                                                    Q_delay = np.mean(i)
                                                    if (math.isnan(Q_delay)):
                                                        continue
                                                    print(Q_delay)
                                                    Q_delay *= (10**9)
                                                    writer.writerow({"sample": Q_delay, "unit": "ns"})

                                            

