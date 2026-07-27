import csv
import gzip

from VARIABLES import *
import numpy as np
from Common import *


directory1 = BASE_DIR + 'LIGHT_DROPS/'
directory2 = BASE_DIR + 'FB_PACKET_DROPPED/'
directory3 = BASE_DIR + 'FB_BOUNCING_PASSED/'
directory4 = BASE_DIR + 'DROPS_RET_TOTAL_PAYLOAD_LEN/'
directory5 = BASE_DIR + 'DROPS_SEQS/'
directory6 = BASE_DIR + 'DROPS_RET_COUNTS/'
directory7 = BASE_DIR + 'LIGHT_IN_QUEUE_DROP_COUNTER/'
directory8 = BASE_DIR + 'LIGHT_IN_RELAY_DROP_COUNTER/'

print("Drops")

for category in CATEGORIES:
    num_iterations = 0
    for q_size in QUEUE_SIZES:
        for boosting_factor in BOOSTING_FACTORS:
            for ttl in TTLS:
                for random_power_factor in RANDOM_POWER_FACTOR:
                    for random_power_bounce_factor in RANDOM_POWER_BOUNCE_FACTOR:
                        for marking_timer in MARKING_TIMER:
                            for ordering_timer in ORDERING_TIMER:
                                for bg_inter_arrival_mult in BG_INTER_ARRIVAL_MULT:
                                    for incast_inter_arrival_mult in INCAST_INTER_ARRIVAL_MULT:
                                        for incast_flow_size in INCAST_FLOW_SIZE:
                                            for incast_scale in NUM_REQUESTS_PER_BURST:
                                                if category == "dctcp_vertigo1_mem1":
                                                    random_power_factor = 1
                                                    random_power_bounce_factor = 1
                                                num_iterations += 1
                                                num_in_queue_drops_array = []
                                                num_intra_switch_drops_array = []
                                                num_bouncing_passed_drops_array = []
                                                num_bursty_packets_dropped_array = []
                                                num_background_packets_dropped_array = []
                                                light_in_queue_drop_num_array = []
                                                light_in_relay_drop_num_array = []
                                                v2pifo_num_all_packets_dropped_array = []
                                                v2pifo_seq_all_packets_dropped_array = []
                                                v2pifo_num_mice_packets_dropped_array = []
                                                v2pifo_num_cat_packets_dropped_array = []
                                                v2pifo_num_elephant_packets_dropped_array = []
                                                v2pifo_num_bursty_packets_dropped_array = []
                                                v2pifo_num_background_packets_dropped_array = []
                                                # try:
                                                for rep_num in range(REP_NUM):
                                                    if TOPOLOGY == FAT_TREE:
                                                        if IS_UPDATED_NAME:
                                                            file_name = '{}_k_{}_burstyapps_{}_mice' \
                                                                        '_{}_reqPerBurst_{}_bgintermult_{}_burstyintermult_{}_ttl' \
                                                                        '_{}_rndfwfactor_{}_rndbouncefactor_{}_incastfsize_' \
                                                                        '{}_mrktimer_{}_ordtimer_{}_qs_{}_bst_{}_rep_{}.csv'.format(
                                                                K, NUM_BURSTY_APPS,
                                                                BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                                                incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                                                ttl, random_power_factor, random_power_bounce_factor, incast_flow_size,
                                                                marking_timer, ordering_timer,
                                                                q_size, boosting_factor, rep_num, category)
                                                        else:
                                                            file_name = '{}_k_{}_burstyapps_{}_mice' \
                                                                        '_{}_reqPerBurst_{}_bgintermult_{}_burstyintermult_{}_ttl' \
                                                                        '_{}_rndfwfactor_{}_rndbouncefactor_{}_incastfsize_' \
                                                                        '{}_mrktimer_{}_ordtimer_{}_rep_{}.csv'.format(
                                                                K, NUM_BURSTY_APPS,
                                                                BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                                                incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                                                ttl, random_power_factor, random_power_bounce_factor, incast_flow_size,
                                                                marking_timer, ordering_timer, rep_num, category)

                                                    elif IS_UPDATED_NAME:
                                                        file_name = '{}_spines_{}_aggs_{}_servers_{}_burstyapps_{}_mice' \
                                                                    '_{}_reqPerBurst_{}_bgintermult_{}_burstyintermult_{}_ttl' \
                                                                    '_{}_rndfwfactor_{}_rndbouncefactor_{}_incastfsize_' \
                                                                    '{}_mrktimer_{}_ordtimer_{}_qs_{}_bst_{}_rep_{}.csv'.format(
                                                            BASE_SPINE_NUM, BASE_AGG_NUM,
                                                            SERVERS_UNDER_EACH_RACK,
                                                            NUM_BURSTY_APPS,
                                                            BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                                            incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                                            ttl, random_power_factor, random_power_bounce_factor, incast_flow_size,
                                                            marking_timer, ordering_timer,
                                                            q_size, boosting_factor, rep_num, category)

                                                    else:
                                                        file_name = '{}_spines_{}_aggs_{}_servers_{}_burstyapps_{}_mice' \
                                                                    '_{}_reqPerBurst_{}_bgintermult_{}_burstyintermult_{}_ttl' \
                                                                    '_{}_rndfwfactor_{}_rndbouncefactor_{}_incastfsize_' \
                                                                    '{}_mrktimer_{}_ordtimer_{}_rep_{}.csv'.format(
                                                            BASE_SPINE_NUM, BASE_AGG_NUM,
                                                            SERVERS_UNDER_EACH_RACK,
                                                            NUM_BURSTY_APPS,
                                                            BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                                            incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                                            ttl, random_power_factor, random_power_bounce_factor, incast_flow_size,
                                                            marking_timer, ordering_timer, rep_num, category)
                                                    num_drop = 0
                                                    num_in_queue_drop = 0
                                                    num_intra_swtich_drops = 0
                                                    num_bouncing_passed_drop = 0
                                                    num_bursty_packet_drop = 0
                                                    num_background_packet_drop = 0
                                                    light_in_queue_drop_num = 0
                                                    light_in_relay_drop_num = 0
                                                    try:
                                                        with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                num_in_queue_drop += int(row[-1])
                                                    except:
                                                        pass

                                                    try:
                                                        with gzip.open(directory7 + file_name + '.gz',
                                                                       'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                light_in_queue_drop_num += int(row[-1])
                                                    except:
                                                        pass

                                                    try:
                                                        with gzip.open(directory8 + file_name + '.gz',
                                                                       'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                light_in_relay_drop_num += int(row[-1])
                                                    except:
                                                        pass

                                                    try:
                                                        with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                for j in range(0, len(row), 2):
                                                                    try:
                                                                        time = float(row[j])
                                                                        is_bursty = int(row[j+1])
                                                                    except:
                                                                        continue
                                                                    num_intra_swtich_drops += 1
                                                                    if (is_bursty == 1):
                                                                        num_bursty_packet_drop += 1
                                                                    else:
                                                                        num_background_packet_drop += 1
                                                    except:
                                                        pass

                                                    try:
                                                        with gzip.open(directory3 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                num_bouncing_passed_drop += int(row[-1])
                                                    except:
                                                        pass

                                                    num_in_queue_drops_array.append(num_in_queue_drop)
                                                    num_intra_switch_drops_array.append(num_intra_swtich_drops)
                                                    num_bouncing_passed_drops_array.append(num_bouncing_passed_drop)
                                                    num_bursty_packets_dropped_array.append(num_bursty_packet_drop)
                                                    num_background_packets_dropped_array.append(num_background_packet_drop)
                                                    light_in_queue_drop_num_array.append(light_in_queue_drop_num)
                                                    light_in_relay_drop_num_array.append(light_in_relay_drop_num)

                                                    v2pifo_dropped_packets = []
                                                    try:
                                                        with gzip.open(directory4 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                for j in range(0, len(row), 2):
                                                                    try:
                                                                        time = float(row[j])
                                                                        total_payload_len = int(row[j+1])
                                                                        v2pifo_dropped_packets.append(V2PIFOPacketDropped(recording_time=time, total_payload_len=total_payload_len))
                                                                    except:
                                                                        continue
                                                    except:
                                                        pass

                                                    v2pifo_packet_dropped_idx = 0
                                                    try:
                                                        with gzip.open(directory5 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                for j in range(0, len(row), 2):
                                                                    try:
                                                                        time = float(row[j])
                                                                        if(v2pifo_dropped_packets[v2pifo_packet_dropped_idx].recording_time != time):
                                                                            raise Exception("Something wrong with seqs results!")
                                                                        seq = int(row[j+1])
                                                                        v2pifo_dropped_packets[v2pifo_packet_dropped_idx].seq = seq
                                                                        v2pifo_packet_dropped_idx += 1
                                                                    except:
                                                                        continue
                                                    except:
                                                        pass

                                                    v2pifo_packet_dropped_idx = 0
                                                    try:
                                                        with gzip.open(directory6 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                for j in range(0, len(row), 2):
                                                                    try:
                                                                        time = float(row[j])
                                                                        if(v2pifo_dropped_packets[v2pifo_packet_dropped_idx].recording_time != time):
                                                                            raise Exception("Something wrong with ret_count results!")
                                                                        ret_count = int(row[j+1])
                                                                        v2pifo_dropped_packets[v2pifo_packet_dropped_idx].ret_count = ret_count
                                                                        v2pifo_packet_dropped_idx += 1
                                                                    except:
                                                                        continue
                                                    except:
                                                        pass

                                                    v2pifo_num_all_packets_dropped_array.append(len(v2pifo_dropped_packets))
                                                    if len(v2pifo_dropped_packets) == 0:
                                                        v2pifo_seq_all_packets_dropped_array.append(0)
                                                        v2pifo_num_bursty_packets_dropped_array.append(0)
                                                        v2pifo_num_background_packets_dropped_array.append(0)
                                                        v2pifo_num_mice_packets_dropped_array.append(0)
                                                        v2pifo_num_cat_packets_dropped_array.append(0)
                                                        v2pifo_num_elephant_packets_dropped_array.append(0)
                                                    else:
                                                        v2pifo_seq_all_packets_dropped_array.append(np.mean([i.seq for i in v2pifo_dropped_packets]))
                                                        v2pifo_num_bursty_packets_dropped_array.append(len([i for i in v2pifo_dropped_packets if i.total_payload_len == (8 * incast_flow_size)]))
                                                        v2pifo_num_background_packets_dropped_array.append(len([i for i in v2pifo_dropped_packets if i.total_payload_len != (8 * incast_flow_size)]))
                                                        v2pifo_num_mice_packets_dropped_array.append(len([i for i in v2pifo_dropped_packets if i.total_payload_len <= (8 * mice_flow_size)]))
                                                        v2pifo_num_cat_packets_dropped_array.append(len([i for i in v2pifo_dropped_packets if (8 * mice_flow_size) < i.total_payload_len < (8 * elephant_flow_size)]))
                                                        v2pifo_num_elephant_packets_dropped_array.append(len([i for i in v2pifo_dropped_packets if i.total_payload_len >= (8 * elephant_flow_size)]))


                                                print(file_name.replace("_",","))
                                                print("In-queue drops, intra-switch, bouncing passed, bursty packets, bg packets, V2PIFO all drops, V2PIFO average seq_num, V2PIFO bursty drops,V2PIFO bg drops, V2PIFO mice drops, V2PIFO cat drops, V2PIFO elephant drops, "
                                                      "Total in relay drop, Total in queue drop")

                                                print('{},{},{},{},{},{},{},{},{},{},{},{},{},{}'.format(
                                                        np.mean(num_in_queue_drops_array),
                                                        np.mean(num_intra_switch_drops_array),
                                                        np.mean(num_bouncing_passed_drops_array),
                                                        np.mean(num_bursty_packets_dropped_array),
                                                        np.mean(num_background_packets_dropped_array),
                                                        np.mean(v2pifo_num_all_packets_dropped_array),
                                                        np.mean(v2pifo_seq_all_packets_dropped_array),
                                                        np.mean(v2pifo_num_bursty_packets_dropped_array),
                                                        np.mean(v2pifo_num_background_packets_dropped_array),
                                                        np.mean(v2pifo_num_mice_packets_dropped_array),
                                                        np.mean(v2pifo_num_cat_packets_dropped_array),
                                                        np.mean(v2pifo_num_elephant_packets_dropped_array),
                                                        np.mean(light_in_relay_drop_num_array),
                                                        np.mean(light_in_queue_drop_num_array)))

                                                print()

    if num_iterations > TARGET_NUM_ITERATIONS:
        raise Exception("num_iterations > TARGET_NUM_ITERATIONS")
    for iteration in range(num_iterations, TARGET_NUM_ITERATIONS):
        print(0)
        print(0)

        print('{},{},{},{},{},{},{},{},{},{},{},{},{},{}'.format(
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0))

        print()