import csv
import gzip
import os.path

from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np
from Common import *
# from pymsgbox import *

directory1 = BASE_DIR + 'HALF_RTT/'
directory2 = BASE_DIR + 'HALF_RTT_SRC_IDX/'
directory3 = BASE_DIR + 'PKT_SENT_SRC/'


if (len(CATEGORIES) != len(failiure_probs)):
    raise Exception("len(CATEGORIES) != len(failiure_probs)")

for latency_thresh in latency_threshs:
    for ml_query_inter_arrival_mult in ML_QUERY_INTER_MULT_LIST:
        print("Half-RTT from server[{}] to server[{}]".format(src_idx, dst_idx))
        print('Latency thresh is {}s'.format(latency_thresh))
        print('Load multiplier is {}'.format(ml_query_inter_arrival_mult))
        all_probs_rcvd = []
        all_probs_sent = []
        i = 0
        for category in CATEGORIES:
            # print ('Category is {}'.format(category))
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
                                                column_idx = None
                                                half_rtts_list = []
                                                print(file_name)
                                                other_than_source_to_dst_found = False
                                                with gzip.open(directory1 + file_name + '.gz', 'rt') as file1, gzip.open(directory2 + file_name + '.gz', 'rt') as file2:
                                                    file_reader1 = csv.reader(file1, delimiter=',')
                                                    file_reader2 = csv.reader(file2, delimiter=',')
                                                    for line1, line2 in zip(file_reader1, file_reader2):
                                                        if first_line:
                                                            for idx, col in enumerate(line1):
                                                                if 'server[{}]'.format(dst_idx) in col:
                                                                    column_idx = idx
                                                                    # print('Column idx is {}'.format(column_idx))
                                                                    if 'server[{}]'.format(dst_idx) not in line2[column_idx]:
                                                                        raise Exception("No one-to-one mapping")
                                                                    # print(line1[column_idx])
                                                                    # print(line2[column_idx])
                                                                    # print('-----------------------')
                                                                    break
                                                            first_line = False
                                                            continue
                                                        if (column_idx is None):
                                                            raise Exception("olumn_idx is None")
                                                        if (line1[column_idx] != line2[column_idx]):
                                                            raise Exception("How is the record time not the same!")
                                                        if (line1[column_idx] == ''):
                                                            break
                                                        # if (queueing_time_start > float(line1[column_idx])):
                                                        #     continue
                                                        if (int(line2[column_idx+1]) == src_idx):
                                                            half_rtts_list.append(float(line1[column_idx+1]))
                                                        else:
                                                            other_than_source_to_dst_found = True
                                                            # print(line1[column_idx+1])
                                                            # print(line2[column_idx+1])
                                                            # print('---------------------------')

                                                first_line = True
                                                column_idx = None
                                                packet_sent_cnt = 0
                                                with gzip.open(directory3 + file_name + '.gz', 'rt') as file1:
                                                    file_reader1 = csv.reader(file1, delimiter=',')
                                                    for line1 in file_reader1:
                                                        if first_line:
                                                            for idx, col in enumerate(line1):
                                                                if 'server[{}]'.format(src_idx) in col:
                                                                    column_idx = idx
                                                                    break
                                                            first_line = False
                                                            continue
                                                        if (column_idx is None):
                                                            print('Didnt find server[{}]'.format(src_idx))
                                                            raise Exception("column_idx is None")
                                                        if (line1[column_idx] == ''):
                                                            break
                                                        # if (queueing_time_start > float(line1[column_idx])):
                                                        #     continue
                                                        if (int(line1[column_idx+1]) != src_idx):
                                                            raise Exception("int(line1[column_idx+1]) != src_idx")
                                                        
                                                        packet_sent_cnt += 1

                                                # if (not other_than_source_to_dst_found):
                                                #     raise Exception("The destination has no senders other than the source!")
                                                
                                                if (len(half_rtts_list) > packet_sent_cnt):
                                                    raise Exception("len(half_rtts_list) > packet_sent_cnt")
                                                
                                                print('Server[{}] sent {} pkts and server[{}] received {} of them ({}%)'.format(src_idx, packet_sent_cnt, dst_idx, len(half_rtts_list), len(half_rtts_list)/packet_sent_cnt*100))
                                                print("Mean: {}".format(np.mean(half_rtts_list)))
                                                print("p10: {}".format(np.percentile(half_rtts_list, 10)))
                                                num_rtts_less_than_threshold = len([i for i in half_rtts_list if i <= latency_thresh])
                                                print("Occurance prob: {}".format(failiure_probs[i]))
                                                print("SENT: Percentage meeting the threshold ({}s): {}%".format(latency_thresh, num_rtts_less_than_threshold/packet_sent_cnt*100))
                                                print("RCVD: Percentage meeting the threshold ({}s): {}%".format(latency_thresh, num_rtts_less_than_threshold/len(half_rtts_list)*100))
                                                all_probs_sent.append(num_rtts_less_than_threshold/packet_sent_cnt)
                                                all_probs_rcvd.append(num_rtts_less_than_threshold/len(half_rtts_list))
                                                print('---------------------------------------')
                                                print()
                                                i += 1
    
        if (len(all_probs_rcvd) != len(failiure_probs) or len(all_probs_sent) != len(failiure_probs)):
            raise Exception("len(all_probs) != len(failiure_probs)")

        final_prob_sent = 0
        for i in range(len(all_probs_sent)):
            final_prob_sent += (all_probs_sent[i] * failiure_probs[i])
        print('SENT: Final prob of meeting {}s is {}%'.format(latency_thresh, final_prob_sent*100))

        final_prob_rcvd = 0
        for i in range(len(all_probs_rcvd)):
            final_prob_rcvd += (all_probs_rcvd[i] * failiure_probs[i])
        print('RCVD: Final prob of meeting {}s is {}%'.format(latency_thresh, final_prob_rcvd*100))

        print("\n#########################\n")

