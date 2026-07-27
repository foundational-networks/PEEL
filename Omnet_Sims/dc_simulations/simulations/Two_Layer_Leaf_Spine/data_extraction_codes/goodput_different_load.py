import csv
import gzip

from VARIABLES import *
from Common import *

print("Goodput")

directory = BASE_DIR + 'CHUNKS_RCVD_LENGTH/'
directory2 = BASE_DIR + 'CHUNKS_RCVD_TOTAL_LENGTH/'
rep_num = 0

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
                                                try:
                                                    all_data = []
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

                                                    server_data_list = []
                                                    with gzip.open(directory + file_name + '.gz', 'rt') as csv_file:
                                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                                        first_line = True
                                                        row1 = None
                                                        for row in csv_reader:
                                                            if first_line:
                                                                row1 = row
                                                                first_line = False
                                                                continue
                                                            for k in range(0, len(row), 2):
                                                                try:
                                                                    time = float(row[k])
                                                                    data_length = int(row[k+1])
                                                                    server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                                    server_data_list.append(Data(rep_num=rep_num, length=data_length, record_time=time))
                                                                except:
                                                                    continue

                                                    data_idx = 0
                                                    with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                                        first_line = True
                                                        row1 = None
                                                        row_counter = None
                                                        for row in csv_reader:
                                                            if first_line:
                                                                row1 = row
                                                                first_line = False
                                                                row_counter = -1
                                                                is_jitter = True
                                                                continue
                                                            row_counter += 1
                                                            for k in range(0, len(row), 2):
                                                                try:
                                                                    server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                                    time = float(row[k])
                                                                    total_lenght = int(row[k+1])
                                                                    if time == server_data_list[data_idx].record_time:
                                                                        server_data_list[data_idx].total_length =total_lenght
                                                                    else:
                                                                        raise Exception("Record mismatch!")
                                                                    data_idx += 1
                                                                except:
                                                                    continue

                                                    data_temp = []
                                                    for data in server_data_list:
                                                        if data.total_length is None or data.length is None or data.record_time is None:
                                                            print("category: {}".format(category))
                                                            print("data.length: {}".format(data.length))
                                                            print("data.total_length: {}".format(data.total_length))
                                                            print("data.record_time: {}".format(data.record_time))
                                                            raise Exception("Something is wrong with data!")
                                                        data_temp.append(data)
                                                    all_data.append(data_temp)

                                                    mice_flow_size_bits = 8 * mice_flow_size
                                                    mice_bits_received = 0

                                                    elephant_flow_size_bits = 8 * elephant_flow_size
                                                    elephant_bits_received = 0

                                                    cat_bits_received = 0

                                                    total_bits_received = 0

                                                    for data_pack in all_data:
                                                        for data in data_pack:
                                                            if data.rep_num != all_data.index(data_pack):
                                                                raise Exception('Something is wrong!')
                                                            total_bits_received += data.length
                                                            if data.total_length <= mice_flow_size_bits:
                                                                mice_bits_received += data.length
                                                            elif data.total_length >= elephant_flow_size_bits:
                                                                elephant_bits_received += data.length
                                                            else:
                                                                cat_bits_received += data.length

                                                    print(file_name.replace("_",","))
                                                    print("category, all sent, all bps, mice sent, mice bps, cat sent, cat bps, elephant sent, elepahnt bps")
                                                    print("{},{},{},{},{},{},{},{}".format(category, SIM_TIME, total_bits_received/SIM_TIME,
                                                                  mice_flow_size_bits, mice_bits_received/SIM_TIME,
                                                                  cat_bits_received/SIM_TIME,
                                                                  elephant_flow_size_bits, elephant_bits_received/SIM_TIME))
                                                    print()

                                                except:
                                                    print(file_name.replace("_", ","))
                                                    print("category, all sent, all bps, mice sent, mice bps, cat sent, cat bps, elephant sent, elepahnt bps")
                                                    print("{},{},{},{},{},{},{},{}".format(0,
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
            print(0)
            print("category, all sent, all bps, mice sent, mice bps, cat sent, cat bps, elephant sent, elepahnt bps")
            print("{},{},{},{},{},{},{},{}".format(0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0))
            print()
