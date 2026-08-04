import csv
from fileinput import filename
import gzip
import os.path
import math

from VARIABLES import *
import matplotlib.pyplot as plt
import numpy as np
from Common import *
# from pymsgbox import *

def is_power_of_two(n):
  if n <= 0:
    return False

  return (n & (n - 1)) == 0

directory1 = BASE_DIR + 'REQUEST_SENT/'
directory2 = BASE_DIR + 'REPLY_LENGTH_ASKED/'
directory3 = BASE_DIR + 'FLOW_ENDED/'
directory7 = BASE_DIR + 'FLOW_PASSED/'
directory4 = BASE_DIR + 'FLOW_STARTED_QUERY_ID/'
directory5 = BASE_DIR + 'FLOW_NUM_PER_QUERY/'
directory6 = BASE_DIR + 'FLOW_ENDED_QUERY_ID/'
directory8 = BASE_DIR + 'CONTROLLER_DELAY/'

out_directory = BASE_DIR + 'EXTRA/'

bg_inter_arrival_mult = BG_INTER_ARRIVAL_MULT[0]
incast_inter_arrival_mult = INCAST_INTER_ARRIVAL_MULT[0]
incast_flow_size = INCAST_FLOW_SIZE[0]
incast_scale = NUM_REQUESTS_PER_BURST[0]

for category in CATEGORIES:
    print('---------------------------------------------')
    num_iterations = 0

    if ('chunk' not in category):
        CHUNK_SIZES = [0]
    if ('subf' not in category):
        SUB_FLOW_NUMS = [0]

    if ('allreduce' in category):
        IS_ALLREDUCE = True
    else:
        IS_ALLREDUCE = False
        if ('reducescatter' not in category and 'reduce' in category):
            IS_REDUCE = True

    if ('srcreduce' in category):
        APPLY_IN_SRC_REDUCE = True
    else:
        APPLY_IN_SRC_REDUCE = False
    
    inter_arrival_mult_idx = -1
    for ml_query_scale in ML_QUERY_SCALE_LIST:
        for ml_query_fsize in ML_FLOW_SIZE_LIST:
            inter_arrival_mult_idx += 1
            ml_query_inter_arrival_mult = ML_QUERY_INTER_MULT_LIST[inter_arrival_mult_idx]
            for ml_num_gpus in ML_NUM_GPUS:
                for nvlink_speed_gbps in ML_NVLINK_Gbps:
                    for chunksize in CHUNK_SIZES:
                        for subf_num in SUB_FLOW_NUMS:
                            num_iterations += 1
                            all_flows = []
                            all_queries = []
                            for rep_num in range(REP_NUM):
                                    
                                if (TOPOLOGY == LEAF_SPINE):
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
                                elif (TOPOLOGY == FAT_TREE):
                                    file_name = '{}_ft_{}_mlapp_{}_bursty_{}_bg' \
                                                '_{}_reqpburst_{}_bgintermult_{}_burstyintermult' \
                                                '_{}_mlqueryintermult' \
                                                '_{}_incastfsize_{}_mlscale_{}_mlfsize' \
                                                '_{}_gpus_{}_nv'.format(
                                        K,
                                        NUM_ML_APPS, NUM_BURSTY_APPS,
                                        BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS,
                                        incast_scale, bg_inter_arrival_mult, incast_inter_arrival_mult,
                                        ml_query_inter_arrival_mult, incast_flow_size, ml_query_scale,
                                        ml_query_fsize, ml_num_gpus,
                                        nvlink_speed_gbps)
                                else:
                                    raise Exception("Unknown topology....")

                                # if ('chunk' in category):
                                #     file_name += f'_{chunksize}_chunk'
                                # if ('subf' in category):
                                #     file_name += f'_{subf_num}_subf'

                                file_name += f'_{rep_num}_rep_{category}.csv'

                                if (os.path.exists(out_directory + file_name)):
                                    raise Exception(out_directory+file_name + " exists...!")

                                flows_temp = dict()
                                queries_temp = dict()
                                flow_app_mapper = dict()
                                query_app_mapper = dict()
                                # REQUEST_SENT/ --> flow id and flow start time
                                with gzip.open(directory1 + file_name + '.gz', 'rt') as csv_file:
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
                                                flow_id = int(row[k+1])
                                                is_bursty = False
                                                is_ml = False
                                                app_idx = int((row1[k].split('app[')[1]).split(']')[0])
                                                if app_idx >= 1 + BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS + NUM_BURSTY_APPS:
                                                    is_ml = True
                                                elif app_idx >= 1 + BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS:
                                                    is_bursty = True
                                                if (flow_id not in flows_temp or flows_temp[flow_id].send_time > time):
                                                    flows_temp.update({flow_id : Flow(rep_num=rep_num, ID=flow_id, send_time=time, is_bursty=is_bursty, is_ml=is_ml)})
                                                server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                unique_id = 'server[{}].app[{}]'.format(server_idx, app_idx)
                                                if unique_id not in flow_app_mapper:
                                                    flow_app_mapper.update({unique_id: []})
                                                flow_app_mapper[unique_id].append(flow_id)
                                            except:
                                                continue

                                # FLOW_PASSED
                                with gzip.open(directory7 + file_name + '.gz', 'rt') as csv_file:
                                    csv_reader = csv.reader(csv_file, delimiter=',')
                                    first_line = True
                                    row1 = None
                                    for row in csv_reader:
                                        # print(row)
                                        if first_line:
                                            row1 = row
                                            first_line = False
                                        for k in range(0, len(row), 2):
                                            try:
                                                time = float(row[k])
                                                flow_id = int(row[k+1])
                                                if flow_id in flows_temp:
                                                    if (flows_temp[flow_id].send_time > time):
                                                        flows_temp[flow_id].send_time = time
                                                else:
                                                    is_bursty = False
                                                    is_ml = False
                                                    app_idx = int((row1[k].split('app[')[1]).split(']')[0])
                                                    if app_idx >= 1 + BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS + NUM_BURSTY_APPS:
                                                        is_ml = True
                                                    elif app_idx >= 1 + BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS:
                                                        is_bursty = True
                                                    flows_temp.update({flow_id : Flow(rep_num=rep_num, ID=flow_id, send_time=time, is_bursty=is_bursty, is_ml=is_ml)})
                                                    server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                    unique_id = 'server[{}].app[{}]'.format(server_idx, app_idx)
                                                    if unique_id not in flow_app_mapper:
                                                        flow_app_mapper.update({unique_id: []})
                                                    flow_app_mapper[unique_id].append(flow_id)
                                            except:
                                                continue

                                # 'REPLY_LENGTH_ASKED/' --> flow size
                                with gzip.open(directory2 + file_name + '.gz', 'rt') as csv_file:
                                    csv_reader = csv.reader(csv_file, delimiter=',')
                                    first_line = True
                                    row1 = None
                                    row_counter = None
                                    for row in csv_reader:
                                        if first_line:
                                            row1 = row
                                            first_line = False
                                            row_counter = 0
                                            continue
                                        row_counter += 1
                                        for k in range(0, len(row), 2):
                                            try:
                                                server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                app_idx = int((row1[k].split('app[')[1]).split(']')[0])
                                                unique_id = 'server[{}].app[{}]'.format(server_idx, app_idx)
                                                flow_id_list = flow_app_mapper[unique_id]
                                                flow_id = flow_id_list[row_counter - 1]
                                                flow = flows_temp[flow_id]
                                                length = int(row[k+1])
                                                flow.length = length
                                            except:
                                                if row_counter <= len(flow_id_list):
                                                    raise Exception("Something isn't quite right! row_counter: {}, len(flow_id_list): {}"
                                                                    .format(row_counter, len(flow_id_list)))
                                                continue

                                # FLOW_ENDED
                                with gzip.open(directory3 + file_name + '.gz', 'rt') as csv_file:
                                    csv_reader = csv.reader(csv_file, delimiter=',')
                                    first_line = True
                                    row1 = None
                                    for row in csv_reader:
                                        if first_line:
                                            row1 = row
                                            first_line = False
                                        for k in range(0, len(row), 2):
                                            try:
                                                time = float(row[k])
                                                flow_id = int(row[k + 1])
                                                flow = flows_temp.get(flow_id)
                                                if (flow.end_time is None or time > flow.end_time):
                                                    flow.end_time = time
                                            except:
                                                continue

                                # FLOW_STARTED_QUERY_ID/
                                with gzip.open(directory4 + file_name + '.gz', 'rt') as csv_file:
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
                                                query_id = int(row[k+1])
                                                if query_id not in queries_temp:
                                                    queries_temp.update({query_id : Query(rep_num=rep_num, ID=query_id, start_time=time)})
                                                elif queries_temp[query_id].start_time > time:
                                                    queries_temp[query_id].start_time = time
                                                app_idx = int((row1[k].split('app[')[1]).split(']')[0])
                                                server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                unique_id = 'server[{}].app[{}]'.format(server_idx, app_idx)
                                                if unique_id not in query_app_mapper:
                                                    query_app_mapper.update({unique_id: []})
                                                query_app_mapper[unique_id].append(query_id)
                                            except:
                                                continue
                                
                                # FLOW_NUM_PER_QUERY
                                with gzip.open(directory5 + file_name + '.gz', 'rt') as csv_file:
                                    csv_reader = csv.reader(csv_file, delimiter=',')
                                    first_line = True
                                    row1 = None
                                    row_counter = None
                                    for row in csv_reader:
                                        if first_line:
                                            row1 = row
                                            first_line = False
                                            row_counter = 0
                                            continue
                                        row_counter += 1
                                        for k in range(0, len(row), 2):
                                            try:
                                                server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                app_idx = int((row1[k].split('app[')[1]).split(']')[0])
                                                unique_id = 'server[{}].app[{}]'.format(server_idx, app_idx)
                                                query_id_list = query_app_mapper[unique_id]
                                                query_id = query_id_list[row_counter - 1]
                                                query = queries_temp[query_id]
                                                num_flows_per_query = int(row[k+1])
                                                if ("optireduce" in category):
                                                    if ("mcast" in category):
                                                        # In reduce phase, every node records scale-1 and then broadcasts to all hosts/not nodes
                                                        target_num_flows = (num_flows_per_query - 1) + (int(num_flows_per_query/ml_num_gpus) - 1)  # reduce all nodes + broadcast to all hosts
                                                        num_flows_per_query = target_num_flows * ml_query_scale   # repeated by all nodes
                                                    else:
                                                        # In reduce phase, every node records scale-1 and then broadcasts scale-1 flows
                                                        num_flows_per_query -= 1
                                                        num_flows_per_query *= 2
                                                        num_flows_per_query *= ml_query_scale
                                                elif 'mcast' in category:
                                                    num_flows_per_query /= ml_num_gpus
                                                    num_flows_per_query = int(num_flows_per_query)
                                                    # print(num_flows_per_query)
                                                    if (APPLY_IN_SRC_REDUCE):
                                                        num_flows_per_query *= (ml_query_scale/ml_num_gpus)
                                                    elif ('bcast' not in category):
                                                        num_flows_per_query *= ml_query_scale
                                                elif 'ina' in category:
                                                    num_flows_per_query /= ml_num_gpus
                                                elif ("ring" in category and IS_ALLREDUCE):
                                                    # every flow records the query end two times
                                                    # for ring, num_flows_per_query is initially scale - 1 and in allreduce what we get is 2*scale - 1 = 2 * num_flows_per_query + 1
                                                    num_flows_per_query *= 2
                                                    num_flows_per_query += 1
                                                    num_flows_per_query *= ml_query_scale
                                                elif ("tree" in category):
                                                    if (not is_power_of_two(ml_query_scale)):
                                                        raise Exception(f"How is scale: {ml_query_scale} not a power of 2")
                                                    if (IS_ALLREDUCE):
                                                        num_flows_per_query -= 1
                                                        # up and down for the two trees
                                                        num_flows_per_query *= 4
                                                    else:
                                                        num_flows_per_query += int(math.log2(ml_query_scale))
                                                        num_flows_per_query -= 1
                                                        if ('bcast' not in category):
                                                            num_flows_per_query *= ml_query_scale
                                                else:
                                                    if (IS_REDUCE):
                                                        if ("ring" in category):
                                                            # it goes through a loop
                                                            num_flows_per_query += 1
                                                        if ("srcreduce" in category):
                                                            num_flows_per_query = math.ceil(num_flows_per_query / ml_num_gpus)
                                                    elif ("srcreduce" in category):
                                                        num_flows_per_query *= (ml_query_scale/ml_num_gpus)
                                                    elif ("bcast" not in category):
                                                        num_flows_per_query *= ml_query_scale

                                                if query.num_flows is not None and query.num_flows != num_flows_per_query:
                                                    print("query.num_flows is not None and query.num_flows != num_flows_per_query")
                                                    raise Exception()
                                                query.num_flows = num_flows_per_query
                                            except:
                                                if row_counter <= len(query_id_list):
                                                    raise Exception("Something isn't quite right with query id! row_counter: {}, len(query_id_list): {}"
                                                                    .format(row_counter, len(query_id_list)))
                                                continue

                                # FLOW_ENDTED_QUERY_ID/
                                with gzip.open(directory6 + file_name + '.gz', 'rt') as csv_file:
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
                                                query_id = int(row[k+1])
                                                if query_id not in queries_temp:
                                                    raise Exception("how is query ended but it's ID is no in queries_temp")
                                                if queries_temp[query_id].end_time is None or queries_temp[query_id].end_time < time:
                                                    queries_temp[query_id].end_time = time
                                                queries_temp[query_id].num_completed_flows += 1
                                            except:
                                                continue
                                                
                                # CONTROLLER DELAY
                                try:
                                    with gzip.open(directory8 + file_name + '.gz', 'rt') as csv_file:
                                        csv_reader = csv.reader(csv_file, delimiter=',')
                                        first_line = True
                                        row1 = None
                                        row_counter = None
                                        for row in csv_reader:
                                            if first_line:
                                                row1 = row
                                                first_line = False
                                                row_counter = 0
                                                continue
                                            row_counter += 1
                                            for k in range(0, len(row), 2):
                                                try:
                                                    server_idx = int((row1[k].split('server[')[1]).split(']')[0])
                                                    app_idx = int((row1[k].split('app[')[1]).split(']')[0])
                                                    unique_id = 'server[{}].app[{}]'.format(server_idx, app_idx)
                                                    query_id_list = query_app_mapper[unique_id]
                                                    query_id = query_id_list[row_counter - 1]
                                                    query = queries_temp[query_id]
                                                    controller_delay = float(row[k+1])
                                                    query.controller_delay.append(controller_delay)
                                                except:
                                                    if row_counter <= len(query_id_list):
                                                        raise Exception("CONTROLLER DELAY: Something isn't quite right with query id! row_counter: {}, len(query_id_list): {}"
                                                                        .format(row_counter, len(query_id_list)))
                                                    continue
                                except:
                                    pass

                                all_flows.append(flows_temp)
                                all_queries.append(queries_temp)

                            fct_cdf_info = []
                            # pass_time_fct_cdf_info = []
                            bursty_fct_cdf_info = []
                            ml_fct_cdf_info = []
                            background_fct_cdf_info = []

                            mice_fct_cdf_info = []
                            cat_fct_cdf_info = []
                            elephant_fct_cdf_info = []

                            num_all_flows_started = 0
                            num_all_flows_ended = 0

                            num_bursty_flows_started = 0
                            num_bursty_flows_ended = 0

                            num_background_flows_started = 0
                            num_background_flows_ended = 0

                            num_ml_flows_started = 0
                            num_ml_flows_ended = 0

                            num_mice_flows_started = 0
                            num_mice_flows_ended = 0

                            num_cat_flows_started = 0
                            num_cat_flows_ended = 0

                            num_elephant_flows_started = 0
                            num_elephant_flows_ended = 0

                            for flow_pack in all_flows:
                                for flow_id in flow_pack:
                                    if flow_pack[flow_id].rep_num != all_flows.index(flow_pack):
                                        raise Exception('Something is wrong!')
                                    if flow_pack[flow_id].send_time is not None:
                                        num_all_flows_started += 1
                                        if flow_pack[flow_id].is_bursty:
                                            num_bursty_flows_started += 1
                                        elif flow_pack[flow_id].is_ml:
                                            num_ml_flows_started += 1
                                        else:
                                            num_background_flows_started += 1
                                        if flow_pack[flow_id].length <= mice_flow_size:
                                            num_mice_flows_started += 1
                                        elif flow_pack[flow_id].length >= elephant_flow_size:
                                            num_elephant_flows_started += 1
                                        else:
                                            num_cat_flows_started += 1
                                        if flow_pack[flow_id].end_time is not None:
                                            num_all_flows_ended += 1
                                            if (flow_pack[flow_id].end_time - flow_pack[flow_id].send_time == 0):
                                                continue
                                            fct_cdf_info.append(flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            # if (flow_pack[flow_id].pass_time is None):
                                            #     pass_time_fct_cdf_info.append(flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            # else:
                                            #     if (flow_pack[flow_id].end_time - flow_pack[flow_id].send_time <= 0):
                                            #         raise Exception("send time is bigger than pass time!")
                                            #     pass_time_fct_cdf_info.append(flow_pack[flow_id].pass_time - flow_pack[flow_id].send_time)
                                            if flow_pack[flow_id].length <= mice_flow_size:
                                                num_mice_flows_ended += 1
                                                mice_fct_cdf_info.append(flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            elif flow_pack[flow_id].length >= elephant_flow_size:
                                                num_elephant_flows_ended += 1
                                                elephant_fct_cdf_info.append(
                                                    flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            else:
                                                num_cat_flows_ended += 1
                                                cat_fct_cdf_info.append(flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            if flow_pack[flow_id].is_bursty:
                                                num_bursty_flows_ended += 1
                                                bursty_fct_cdf_info.append(
                                                    flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            elif flow_pack[flow_id].is_ml:
                                                num_ml_flows_ended += 1
                                                ml_fct_cdf_info.append(
                                                    flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                            else:
                                                background_fct_cdf_info.append(
                                                    flow_pack[flow_id].end_time - flow_pack[flow_id].send_time)
                                                num_background_flows_ended += 1

                            if num_bursty_flows_started == 0:
                                percentage_bursty_flows_finished = 0
                            else:
                                percentage_bursty_flows_finished = num_bursty_flows_ended / num_bursty_flows_started * 100

                            if num_ml_flows_started == 0:
                                percentage_ml_flows_finished = 0
                            else:
                                percentage_ml_flows_finished = num_ml_flows_ended / num_ml_flows_started * 100

                            if num_background_flows_started == 0:
                                percentage_background_flows_finished = 0
                            else:
                                percentage_background_flows_finished = num_background_flows_ended / num_background_flows_started * 100

                            if num_mice_flows_started == 0:
                                percentage_mice_flows_finished = 0
                            else:
                                percentage_mice_flows_finished = num_mice_flows_ended / num_mice_flows_started * 100

                            if num_cat_flows_started == 0:
                                percentage_cat_flows_finished = 0
                            else:
                                percentage_cat_flows_finished = num_cat_flows_ended / num_cat_flows_started * 100

                            if num_elephant_flows_started == 0:
                                percentage_elephant_flows_finished = 0
                            else:
                                percentage_elephant_flows_finished = num_elephant_flows_ended / num_elephant_flows_started * 100

                            if len(fct_cdf_info) == 0:
                                fct_cdf_info.append(0)
                            # if len(pass_time_fct_cdf_info) == 0:
                            #     pass_time_fct_cdf_info.append(0)
                            if len(mice_fct_cdf_info) == 0:
                                mice_fct_cdf_info.append(0)
                            if len(cat_fct_cdf_info) == 0:
                                cat_fct_cdf_info.append(0)
                            if len(elephant_fct_cdf_info) == 0:
                                elephant_fct_cdf_info.append(0)
                            if len(bursty_fct_cdf_info) == 0:
                                bursty_fct_cdf_info.append(0)
                            if len(ml_fct_cdf_info) == 0:
                                ml_fct_cdf_info.append(0)
                            if len(background_fct_cdf_info) == 0:
                                background_fct_cdf_info.append(0)

                            # print(file_name.replace("_",","))
                            print(category)

                            # print("type, started, finished, %Flow Completion, mean, p10, p50, p90, p99, p99.9")

                            # try:
                            #     print('all, {}, {}, {}, {}, {}, {}, {}, {}, {}\n \
                            #         mice of size, {}, {}, {}, {}, {}, {}, {}, {}, {}\ncat of size, {}, {}, {}, {}, {}, {}, {}, {}, {}\nelephant of size, {}, {}, {}, {}, {}, {}, {}, {}, {}\n bursty,{}, {}, {}, {}, {}, {}, {}, {}, {}\n ml,{}, {}, {}, {}, {}, {}, {}, {}, {}\n background,{}, {}, {}, {}, {}, {}, {}, {}, {}'.format(
                            #             num_all_flows_started / REP_NUM,
                            #             num_all_flows_ended / REP_NUM,
                            #             num_all_flows_ended / num_all_flows_started * 100,
                            #             np.mean(fct_cdf_info),
                            #             np.percentile(fct_cdf_info, 10),
                            #             np.percentile(fct_cdf_info, 50),
                            #             np.percentile(fct_cdf_info, 90),
                            #             np.percentile(fct_cdf_info, 99),
                            #             np.percentile(fct_cdf_info, 99.9),
                            #             num_all_flows_started / REP_NUM,
                            #             num_all_flows_ended / REP_NUM,
                            #             num_all_flows_ended / num_all_flows_started * 100,
                            #             num_mice_flows_started / REP_NUM,
                            #             num_mice_flows_ended / REP_NUM,
                            #             percentage_mice_flows_finished / REP_NUM,
                            #             np.mean(mice_fct_cdf_info),
                            #             np.percentile(mice_fct_cdf_info, 10),
                            #             np.percentile(mice_fct_cdf_info, 50),
                            #             np.percentile(mice_fct_cdf_info, 90),
                            #             np.percentile(mice_fct_cdf_info, 99),
                            #             np.percentile(mice_fct_cdf_info, 99.9),
                            #             num_cat_flows_started / REP_NUM,
                            #             num_cat_flows_ended / REP_NUM,
                            #             percentage_cat_flows_finished / REP_NUM,
                            #             np.mean(cat_fct_cdf_info),
                            #             np.percentile(cat_fct_cdf_info, 10),
                            #             np.percentile(cat_fct_cdf_info, 50),
                            #             np.percentile(cat_fct_cdf_info, 90),
                            #             np.percentile(cat_fct_cdf_info, 99),
                            #             np.percentile(cat_fct_cdf_info, 99.9),
                            #             num_elephant_flows_started / REP_NUM,
                            #             num_elephant_flows_ended / REP_NUM,
                            #             percentage_elephant_flows_finished / REP_NUM,
                            #             np.mean(elephant_fct_cdf_info),
                            #             np.percentile(elephant_fct_cdf_info, 10),
                            #             np.percentile(elephant_fct_cdf_info, 50),
                            #             np.percentile(elephant_fct_cdf_info, 90),
                            #             np.percentile(elephant_fct_cdf_info, 99),
                            #             np.percentile(elephant_fct_cdf_info, 99.9),
                            #             num_bursty_flows_started / REP_NUM,
                            #             num_bursty_flows_ended / REP_NUM,
                            #             percentage_bursty_flows_finished,
                            #             np.mean(bursty_fct_cdf_info),
                            #             np.percentile(bursty_fct_cdf_info, 10),
                            #             np.percentile(bursty_fct_cdf_info, 50),
                            #             np.percentile(bursty_fct_cdf_info, 90),
                            #             np.percentile(bursty_fct_cdf_info, 99),
                            #             np.percentile(bursty_fct_cdf_info, 99.9),
                            #             num_ml_flows_started / REP_NUM,
                            #             num_ml_flows_ended / REP_NUM,
                            #             percentage_ml_flows_finished,
                            #             np.mean(ml_fct_cdf_info),
                            #             np.percentile(ml_fct_cdf_info, 10),
                            #             np.percentile(ml_fct_cdf_info, 50),
                            #             np.percentile(ml_fct_cdf_info, 90),
                            #             np.percentile(ml_fct_cdf_info, 99),
                            #             np.percentile(ml_fct_cdf_info, 99.9),
                            #             num_background_flows_started/REP_NUM,
                            #             num_background_flows_ended/REP_NUM,
                            #             percentage_background_flows_finished,
                            #             np.mean(background_fct_cdf_info),
                            #             np.percentile(background_fct_cdf_info, 10),
                            #             np.percentile(background_fct_cdf_info, 50),
                            #             np.percentile(background_fct_cdf_info, 90),
                            #             np.percentile(background_fct_cdf_info, 99),
                            #             np.percentile(background_fct_cdf_info, 99.9)))
                            # except:
                            #     print('Info not found')
                            #     continue

                            for query_pack in all_queries:
                                qct_info = []
                                n_flows = None
                                old_n_flows = None
                                if (IS_FRAGMENTED):
                                    print('We have fragmented job assignment')
                                    for query_id in query_pack:
                                        # note that different queries might have different fragmentations
                                        if (query_pack[query_id].num_completed_flows > query_pack[query_id].num_flows):
                                            query_pack[query_id].old_num_flows = query_pack[query_id].num_flows
                                            query_pack[query_id].num_flows = query_pack[query_id].num_completed_flows
                                for query_id in query_pack:
                                    if query_pack[query_id].is_completed():
                                        if query_pack[query_id].end_time - query_pack[query_id].start_time <= 0:
                                            raise Exception("QCT cannot be <= 0")
                                        # add controller delay
                                        if (len(query_pack[query_id].controller_delay) == 0):
                                            mean_controller_delay = 0
                                        else:
                                            mean_controller_delay = np.mean(query_pack[query_id].controller_delay)
                                        qct_info.append(query_pack[query_id].end_time - \
                                                        query_pack[query_id].start_time + \
                                                            mean_controller_delay)
                                        if (n_flows is None):
                                            n_flows = query_pack[query_id].num_flows
                                            old_n_flows = query_pack[query_id].old_num_flows
                                if (len(qct_info) == 0):
                                    qct_info.append(0)
                                print(f'Mean CCT (s): {np.mean(qct_info)}, p99 CCT (s): {np.percentile(qct_info, 99)}')
                                # print('Queries ({} flows -- {} previous flows), {}, {}, {}, {}, {}, {}, {}, {}, {}'
                                #         .format(n_flows,
                                #                 old_n_flows,
                                #                 len(query_pack), len(qct_info),
                                #                 (len(qct_info) / len(query_pack))*100,
                                #                 np.mean(qct_info),
                                #                 np.percentile(qct_info, 10),
                                #                 np.percentile(qct_info, 50),
                                #                 np.percentile(qct_info, 90),
                                #                 np.percentile(qct_info, 99),
                                #                 np.percentile(qct_info, 99.9)))

                                print('\n')

                                with open(out_directory + "qct_"+file_name, 'w', encoding='UTF8') as f:
                                    for i in qct_info:
                                        writer = csv.writer(f)
                                        writer.writerow([i])

        print()
