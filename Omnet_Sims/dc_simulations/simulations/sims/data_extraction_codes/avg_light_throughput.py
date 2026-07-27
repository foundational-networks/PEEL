import csv
import gzip

from VARIABLES import *
import numpy as np

directory1 = BASE_DIR + 'LIGHT_THROUGHPUT_BITS_SENT/'
directory2 = BASE_DIR + 'LIGHT_THROUGHPUT_BITS_RCVD/'
directory3 = BASE_DIR + 'LIGHT_GOODPUT_NUM_BITS_SENT_TO_APP/'

SIM_TIME = 5.2

server_name = 'server[0]'

print("Light Through-/Good-put")

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

                                                        throughput_sent_array = []
                                                        specific_throughput_sent = None
                                                        throughput_rcvd_array = []
                                                        specific_throughput_rcvd = None
                                                        goodput_array = []
                                                        specific_goodput = None

                                                        with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                bits_sent = int(row[-1])
                                                                throughput_sent_array.append(bits_sent)
                                                                if server_name in row[-3]:
                                                                    specific_throughput_sent = bits_sent

                                                        with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                bits_rcvd = int(row[-1])
                                                                throughput_rcvd_array.append(bits_rcvd)
                                                                if server_name in row[-3]:
                                                                    specific_throughput_rcvd = bits_rcvd

                                                        with gzip.open(directory3 + file_name + '.gz', 'rt') as csv_file:
                                                            csv_reader = csv.reader(csv_file, delimiter=',')
                                                            first_line = True
                                                            row1 = None
                                                            for row in csv_reader:
                                                                if first_line:
                                                                    first_line = False
                                                                    row1 = row
                                                                    continue
                                                                goodput = int(row[-1])
                                                                goodput_array.append(goodput)
                                                                if server_name in row[-3]:
                                                                    specific_goodput = goodput

                                                        print(file_name.replace("_",","))
                                                        print('throughput sent of {}, throughput rcvd of {}, goodput of {}, '
                                                              'average throughput sent of all, average throughput rcvd of all, average goodput of all\n'
                                                              '{},{},{},{},{},{}'.format(
                                                            server_name,
                                                            server_name,
                                                            server_name,
                                                            specific_throughput_sent / SIM_TIME,
                                                            specific_throughput_rcvd / SIM_TIME,
                                                            specific_goodput / SIM_TIME,
                                                            np.mean(throughput_sent_array) / SIM_TIME,
                                                            np.mean(throughput_rcvd_array) / SIM_TIME,
                                                            np.mean(goodput_array) / SIM_TIME))
                                                        print()
                                                except:
                                                    print(file_name.replace("_", ","))
                                                    print(
                                                        'throughput sent of {}, throughput rcvd of {}, goodput of {}, '
                                                        'average throughput sent of all, average throughput rcvd of all, average goodput of all\n'
                                                        '{},{},{},{},{},{}'.format(
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
            'throughput sent of {}, throughput rcvd of {}, goodput of {}, '
            'average throughput sent of all, average throughput rcvd of all, average goodput of all\n'
            '{},{},{},{},{},{}'.format(
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
