import csv
import gzip

from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np

directory1 = BASE_DIR + 'DUP_ACKS_FAST_RETRANSMISSION/'

for category in CATEGORIES:
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
                                                fast_ret_num_array = []
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

                                                    fast_ret_num = 0
                                                    with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                                        first_line = True
                                                        row1 = None
                                                        for row in csv_reader:
                                                            if first_line:
                                                                first_line = False
                                                                row1 = row
                                                                continue
                                                        fast_ret_num += int(row[-1])

                                                    fast_ret_num_array.append(fast_ret_num)

                                                print("Info for filename: {}".format(file_name))
                                                print('average number of fast re-transmissions is {}\n'.format(np.mean(fast_ret_num_array)))
                                                print('--------------------------------------------')
