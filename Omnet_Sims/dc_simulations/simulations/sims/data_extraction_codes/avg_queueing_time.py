import csv
import gzip

from VARIABLES import *
import numpy as np

directory1 = BASE_DIR + 'QUEUEING_TIME/'
directory2 = BASE_DIR + 'QUEUEING_TIME_INCAST/'

switch_name = 'agg'

print("Queueing Time")

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
                                                num_iterations += 1
                                                try:
                                                    queueing_time_array = []
                                                    queueing_time_incast_array = []
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

                                                        with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                if switch_name in row[-3]:
                                                                    try:
                                                                        if 'ndp' in category:
                                                                            if 'mainQueue' in row[-3] and 'mean' in row[-2]:
                                                                                queueing_time_array.append(float(row[-1]))
                                                                        else:
                                                                            if 'mean' in row[-2]:
                                                                                queueing_time_array.append(float(row[-1]))
                                                                    except:
                                                                        continue

                                                        with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                if switch_name in row[-3]:
                                                                    try:
                                                                        if 'ndp' in category:
                                                                            if 'mainQueue' in row[-3] and 'mean' in row[-2]:
                                                                                queueing_time_incast_array.append(float(row[-1]))
                                                                        else:
                                                                            if 'mean' in row[-2]:
                                                                                queueing_time_incast_array.append(float(row[-1]))
                                                                    except:
                                                                        continue

                                                    print(file_name.replace("_",","))
                                                    print('average queueing times of {}, queueing time, incast queueing time\n {},{}'.format(
                                                        switch_name,
                                                        np.mean(queueing_time_array),
                                                        np.mean(queueing_time_incast_array)))
                                                    print()
                                                except:
                                                    print(file_name.replace("_", ","))
                                                    print('average queueing times of {}, queueing time, incast queueing time\n {},{}'.format(
                                                        switch_name,
                                                        0,
                                                        0))
                                                    print()

    if num_iterations > TARGET_NUM_ITERATIONS:
        raise Exception("num_iterations > TARGET_NUM_ITERATIONS")
    for iteration in range(num_iterations, TARGET_NUM_ITERATIONS):
        print(file_name.replace("_", ","))
        print('average queueing times of {}, queueing time, incast queueing time\n {},{}'.format(
            switch_name,
            0,
            0))
        print()
