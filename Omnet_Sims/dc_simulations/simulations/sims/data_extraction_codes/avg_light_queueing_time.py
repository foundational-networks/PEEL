import csv
import gzip

from VARIABLES import *
import numpy as np

directory1 = BASE_DIR + 'LIGHT_ALL_QUEUEING_TIME/'
directory2 = BASE_DIR + 'LIGHT_MICE_QUEUEING_TIME/'

port_name = 'agg[0].eth[0]'
switch_name = 'agg[0]'

if '.eth[' not in port_name:
    raise Exception("Your port_name is wrong :)")

print("Light Queueing Time")

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

                                                        port_queueing_time_array = []
                                                        switch_queueing_time_array = []
                                                        all_queueing_time_array = []

                                                        with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                queueing_time = float(row[-1])
                                                                all_queueing_time_array.append(queueing_time)
                                                                if switch_name in row[-3]:
                                                                    switch_queueing_time_array.append(queueing_time)
                                                                if port_name in row[-3]:
                                                                    port_queueing_time_array.append(queueing_time)

                                                        mice_port_queueing_time_array = []
                                                        mice_switch_queueing_time_array = []
                                                        mice_all_queueing_time_array = []

                                                        with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                mice_queueing_time = float(row[-1])
                                                                mice_all_queueing_time_array.append(mice_queueing_time)
                                                                if switch_name in row[-3]:
                                                                    mice_switch_queueing_time_array.append(mice_queueing_time)
                                                                if port_name in row[-3]:
                                                                    mice_port_queueing_time_array.append(mice_queueing_time)

                                                        print(file_name.replace("_",","))
                                                        print('average queueing times of {}, average queueing times of {}, average queueing times of all, '
                                                              'average mice queueing times of {}, average mice queueing times of {}, average mice queueing times of all\n '
                                                              '{},{},{},{},{},{}'.format(
                                                            switch_name,
                                                            port_name,
                                                            switch_name,
                                                            port_name,
                                                            np.mean(switch_queueing_time_array),
                                                            np.mean(port_queueing_time_array),
                                                            np.mean(all_queueing_time_array),
                                                            np.mean(mice_switch_queueing_time_array),
                                                            np.mean(mice_port_queueing_time_array),
                                                            np.mean(mice_all_queueing_time_array)))
                                                        print()
                                                except:
                                                    print(file_name.replace("_", ","))
                                                    print(
                                                        'average queueing times of {}, average queueing times of {}, average queueing times of all, '
                                                        'average mice queueing times of {}, average mice queueing times of {}, average mice queueing times of all\n '
                                                        '{},{},{},{},{},{}'.format(
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

    if num_iterations > TARGET_NUM_ITERATIONS:
        raise Exception("num_iterations > TARGET_NUM_ITERATIONS")
    for iteration in range(num_iterations, TARGET_NUM_ITERATIONS):
        print(file_name.replace("_", ","))
        print(
            'average queueing times of {}, average queueing times of {}, average queueing times of all, '
            'average mice queueing times of {}, average mice queueing times of {}, average mice queueing times of all\n '
            '{},{},{},{},{},{}'.format(
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
