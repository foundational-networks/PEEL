from itertools import count
import sqlite3
from tkinter.messagebox import NO
import numpy as np
import csv
import random
import multiprocessing as mp
from Variables import *
from collections import Counter



class Node:
    def __init__(self):
        self.children_list = []
        self.children_port_idx_list = []

class LevelInfo:
    def __init__(self, srcNode):
        self.highest_level_achieved = 0
        self.level_map = dict()
        self.level_map[self.highest_level_achieved] = srcNode
        self.going_down_areas_dict = dict()

def print_tree(node, level=0):
    if not node:
        return
    # Print the current node with indentation based on its depth
    print(" " * (level * 4) + f"Node --> ports: {node.children_port_idx_list}")
    # Recursively print each child
    for child in node.children_list:
        print_tree(child, level + 1)

class Route:
    def __init__(self, ID):
        self.ID = ID
        self.ports_to_dsts_list_of_lists = [[]]
        self.jump_to_idx_list_of_lists = [[]]
        self.last_idx = 1

def is_multicast_based_collective(collective):
    if (collective == "broadcast"):
        return True
    if (collective == "allreduce"):
        return True
    if (collective == "allgather"):
        return True
    return False

def get_collective(m_txt):
    if "SendRecv" in m_txt:
        return "alltoallv"
    if "AllReduce" in m_txt:
        return "allreduce"
    if "AllGather" in m_txt:
        return "allgather"
    if "ReduceScatter" in m_txt:
        return "reducescatter"
    if "Broadcast" in m_txt:
        return "broadcast"
    if "Reduce" in m_txt:
        return "reduce"
    return None

def get_alg(m_txt):
    if "RING" in m_txt:
        return "ring"
    if "TREE" in m_txt:
        return "binarytree"
    return None

def read_collective_file(dist_file_path):
    collective_alg_count_dict = dict()
    with open(dist_file_path, 'r') as csvfile:
        csv_file = csv.reader(csvfile, delimiter=',')
        is_first_line = True
        for row in csv_file:
            if (is_first_line):
                is_first_line = False
                continue
            if (row[0] == ''):
                continue
            collective = get_collective(row[0])
            if (collective is None):
                print(f'row[0]: {row[0]}')
                raise Exception("Collective cannot be none!")
            alg = get_alg(row[0])
            if (alg is None):
                if collective == "alltoallv" or collective == "alltoall":
                    alg = "singlesender"
                else:
                    raise Exception("Alg cannot be none!")

            # For now to handle send/receive ring
            if collective == "alltoallv" and alg != "singlesender":
                collective = "allreduce"
                print(f'{collective}-{alg}')

            # collective and alg must not have a hyphen in their name!
            hyphen_count = collective.count('-')
            if (hyphen_count != 0):
                raise Exception("We must have no hyphen in collective name!")
            if (alg is not None):
                hyphen_count = alg.count('-')
                if (hyphen_count != 0):
                    raise Exception("We must have no hyphen in algorithm name!")

            key = f'{collective}-{alg}'
            if (key not in collective_alg_count_dict):
                collective_alg_count_dict[key] = 0
            collective_alg_count_dict[key] += int(row[1])
    total_count = sum(collective_alg_count_dict.values())
    print(f'Total of {total_count} instances: {collective_alg_count_dict}')
    percentages_dict = {key: (value / total_count) * 100 for key, value in collective_alg_count_dict.items()}
    print(f'Percentages: {percentages_dict}')
    if (not (99.9 <= sum(percentages_dict.values()) <= 100.1)):
        raise Exception(f"Invalid percentage calc: {sum(percentages_dict.values())}")
    return percentages_dict

def get_random_query_type(collective_alg_percentage_list):
    # print(f'possible queries: {collective_alg_percentage_list}')
    random_double = random.uniform(0, 100)
    m_sum = 0
    for i in collective_alg_percentage_list:
        m_sum += i[1]
        if (m_sum >= random_double):
            # print(f'Choosing {i[0]}')
            return i[0]
    raise Exception("No query type was chosen!")


def get_inter_arrival_time(ML_INTER_ARRIVAL_MULTIPLIER):
    # dist is in us
    dist_data_microsecond = None
    if ML_INTER_ARRIVAL_DIST_TYPE == 'poisson':
        dist_data_microsecond = np.random.poisson(ML_INTER_ARRIVAL_DIST_POISSON_LAMBDA, 1)[0]
    else:
        raise Exception("Unknown inter-arrival distribution type")
    return ML_INTER_ARRIVAL_MULTIPLIER * dist_data_microsecond * 1.0 / 1000000


def get_flow_size():
    if (FLOW_SIZE_GENERATION_TYPE == "normal"):
        if (FLOW_DIST_NORMAL_MEAN < 0 or FLOW_DIST_NORMAL_STD < 0):
            raise Exception("Invalid normal mean or std")
        fsize = -1
        while (fsize <= 0):
            fsize = np.random.normal(FLOW_DIST_NORMAL_MEAN, FLOW_DIST_NORMAL_STD)
        fsize = int(fsize)
        # print(fsize)
        return fsize
    raise Exception("Unknown flow size generation type!")
    # # dist is in KB
    # return int(ML_FLOW_SIZE_MULTIPLIER * get_random_point_based_on_cdf(xs, ys) * 1000)

def create_tree(src_idx:int, dst_idx:int, level_info:LevelInfo, counter_per_dst_dict, src_dst_to_port_dict):
    dst_tor_idx = int(dst_idx/NUM_SERVERS_UNDER_EACH_RACK)
    src_tor_idx = int(src_idx/NUM_SERVERS_UNDER_EACH_RACK)
    if (src_tor_idx == dst_tor_idx):
        # src and dst are under the same ToR
        highest_level = 1
    else:
        # src and dst are reachable through spine
        highest_level = 2

    if highest_level not in level_info.level_map:
        highest_achieved_node = level_info.level_map[level_info.highest_level_achieved]
        for level in range(level_info.highest_level_achieved, highest_level):
            new_node = Node()
            level_info.level_map[level+1] = new_node
            if (level == 0):
                # port to get from server to leaf that can go from any of the gpus
                # choose the source gpu at random
                new_node_port_idx = random.randint(0, NUM_GPUS-1)
            elif (level == 1):
                # port to get from leaf to spine
                new_node_port_idx = NUM_SERVERS_UNDER_EACH_RACK * NUM_GPUS + \
                    random.randint(0, NUM_SPINES-1)
            else:
                raise Exception("No other levels should be available")
            
            highest_achieved_node.children_list.append(new_node)
            highest_achieved_node.children_port_idx_list.append(new_node_port_idx)
            highest_achieved_node = new_node
        level_info.highest_level_achieved = highest_level

    curr_node = level_info.level_map[highest_level]
    for level in range(highest_level, 0, -1):
        if (level == 1):
            # in leaf level area code is dst server idx
            area_code = dst_idx
            # which dst port to use
            addition_port_idx = 0
            if ((src_idx,dst_idx) in src_dst_to_port_dict):
                addition_port_idx = src_dst_to_port_dict[(src_idx,dst_idx)]
            else:
                if dst_idx in counter_per_dst_dict:
                    counter_per_dst_dict[dst_idx] += 1
                    counter_per_dst_dict[dst_idx] %= NUM_GPUS
                    addition_port_idx = counter_per_dst_dict[dst_idx]
                else:
                    addition_port_idx = 0
                    counter_per_dst_dict[dst_idx] = 0
                src_dst_to_port_dict[(src_idx,dst_idx)] = addition_port_idx
            # choose the dst gpu at random
            # port_idx = (dst_idx % NUM_SERVERS_UNDER_EACH_RACK)*NUM_GPUS + random.randint(0, NUM_GPUS-1)
            port_idx = (dst_idx % NUM_SERVERS_UNDER_EACH_RACK)*NUM_GPUS + addition_port_idx
            # print(f'({src_idx},{dst_idx}) --> {port_idx}')
        elif (level == 2):
            # in spine level area code is dst tor idx
            area_code = dst_tor_idx
            port_idx = dst_tor_idx
        else:
            print("Unknown level...")
        if ((level,area_code) not in level_info.going_down_areas_dict):
            new_node = Node()
            curr_node.children_list.append(new_node)
            curr_node.children_port_idx_list.append(port_idx)
            level_info.going_down_areas_dict[(level,area_code)] = new_node
        elif (level == 1):
            raise Exception("How can area code exists when we are in level 1 (leaf)")
        curr_node = level_info.going_down_areas_dict[(level,area_code)]

def fill_route_lists(curr_node:Node, curr_idx:int, route:Route):

    for idx, child in enumerate(curr_node.children_list):
        route.ports_to_dsts_list_of_lists.append([])
        route.jump_to_idx_list_of_lists.append([])
        route.ports_to_dsts_list_of_lists[curr_idx].append(curr_node.children_port_idx_list[idx])
        route.jump_to_idx_list_of_lists[curr_idx].append(route.last_idx)
        route.last_idx += 1
        fill_route_lists(child, route.last_idx-1, route)

def earliest_time_for_X_tors(tor_to_available_time_dict, X):
    # Step 1: Extract all availability times
    all_times = list(tor_to_available_time_dict.values())
    
    # Step 2: Sort times in ascending order
    all_times.sort()
    
    # Step 3: Check if we have at least X tors
    if X > len(all_times):
        raise Exception("No such time exists...")
    
    # Step 4: Return the X-th earliest available time
    return all_times[X - 1]
    
def get_allgather_participants_one_job_per_gpu(NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, query_time, tor_to_available_time_dict):

    num_servers_per_tor = NUM_SERVERS_UNDER_EACH_RACK
    num_gpus_under_tor = num_servers_per_tor * NUM_GPUS

    if (NUM_FLOWS_PER_ML_QUERY % num_gpus_under_tor != 0):
        raise Exception("We assumed that we always require at least all nodes below the chosen tors")

    start_tor_idx = random.randint(0, NUM_AGGS - 1)

    # first check if we have enough tors available
    enough_tors_available = False
    available_tors_num = 0
    required_tors = int(NUM_FLOWS_PER_ML_QUERY / num_gpus_under_tor)
    i = 0
    tor_idx = start_tor_idx
    while (available_tors_num < required_tors and i < NUM_AGGS):
        if tor_idx not in tor_to_available_time_dict:
            tor_to_available_time_dict[tor_idx] = 0
        if tor_to_available_time_dict[tor_idx] <= query_time:
            available_tors_num += 1
        if (available_tors_num == required_tors):
            enough_tors_available = True
        tor_idx += 1
        tor_idx %= NUM_AGGS
        i += 1

    if (not enough_tors_available):
        query_time = earliest_time_for_X_tors(tor_to_available_time_dict, required_tors)

    chosen_tors = []
    tor_idx = start_tor_idx
    random_processing_overhead =  1 + random.uniform(0, 0.25)
    processing_time = random_processing_overhead * ML_FLOW_SIZE * 8 / (100 * 10**9)
    i = 0
    while (len(chosen_tors) < required_tors):
        if tor_to_available_time_dict[tor_idx] <= query_time:
            chosen_tors.append(tor_idx)
        else:
            print(f"Skipping tor {tor_idx}")
        tor_idx += 1
        tor_idx %= NUM_AGGS
        i += 1
        if (i == NUM_AGGS and len(chosen_tors) < required_tors):
            raise Exception("All tors have been visited!")

    index_of_dest_servers = []
    index_of_dest_gpus = []
    for chosen_tor in chosen_tors:
            tor_to_available_time_dict[chosen_tor] = query_time + processing_time
            possible_dsts = []
            possible_dst_gpus = []
            possible_servers = [t for t in range(chosen_tor * num_servers_per_tor, \
                                                (chosen_tor+1) * num_servers_per_tor)]
            for j in possible_servers:
                possible_dsts += [j for _ in range(NUM_GPUS)]
                added_gpus = [m for m in range(NUM_GPUS)]
                possible_dst_gpus += added_gpus
            index_of_dest_servers += possible_dsts
            index_of_dest_gpus += possible_dst_gpus
    # print(tor_to_available_time_dict)
    
    if (len(index_of_dest_servers) != len(index_of_dest_gpus)):
        raise Exception("len(index_of_dest_servers) != len(index_of_dest_gpus)")
    if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY):
        raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY")

    return index_of_dest_servers, index_of_dest_gpus, query_time

def get_allgather_participants(NUM_FLOWS_PER_ML_QUERY):
    total_dest_num = NUM_FLOWS_PER_ML_QUERY
    index_of_dest_servers = []
    index_of_dest_gpus = []
    possible_TOR_list = [j for j in range(NUM_AGGS)]
    while total_dest_num != 0:
        chosen_TOR = random.sample(possible_TOR_list, 1)[0]
        possible_TOR_list.remove(chosen_TOR)
        possible_servers = [t for t in range(chosen_TOR * NUM_SERVERS_UNDER_EACH_RACK, (chosen_TOR + 1) * NUM_SERVERS_UNDER_EACH_RACK)]
        random.shuffle(possible_servers)
        possible_dsts = []
        possible_dst_gpus = []
        for j in possible_servers:
            possible_dsts += [j for _ in range(NUM_GPUS)]
            possible_dst_gpus += [m for m in range(NUM_GPUS)]
        if len(possible_dsts) <= total_dest_num:
            index_of_dest_servers += possible_dsts
            index_of_dest_gpus += possible_dst_gpus
            total_dest_num -= len(possible_dsts)
        else:
            index_of_dest_servers += possible_dsts[0:total_dest_num]
            index_of_dest_gpus += possible_dst_gpus[0:total_dest_num]
            total_dest_num = 0
    if (len(index_of_dest_servers) != len(index_of_dest_gpus)):
        raise Exception("len(index_of_dest_servers) != len(index_of_dest_gpus)")
    if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY):
        raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY")
    return index_of_dest_servers, index_of_dest_gpus

def get_stats_dict(chosen_queries):
    counts = Counter(chosen_queries)
    total_count = len(chosen_queries)
    percentages = {key: (value / total_count) * 100 for key, value in counts.items()}
    return percentages

def report_diff(base, created):
    # Compare the dictionaries and compute differences
    print('\nReporting diff:')
    diff_report = {}
    for key in set(base.keys()).union(created.keys()):
        percentage_value = base.get(key, 0)
        stats_value = created.get(key, 0)
        diff_report[key] = {
            "expected value": percentage_value,
            "generated value": stats_value,
            "absolute_diff": abs(percentage_value - stats_value)
        }

    # Print the comparison report
    for key, value in diff_report.items():
        print(f"{key}: {value}")
    print('\n')

def fill_tables(rep_num, collective_alg_percentage_dict, 
                collective_alg_percentage_list, ML_INTER_ARRIVAL_MULTIPLIER,
                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE):
    print(
        "IA = {}, SCALE = {}, FS = {}".format(ML_INTER_ARRIVAL_MULTIPLIER, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE))
    TARGET_NUM_ML_QUERIES = TARGET_NUM_ML_QUERIES_LIST[
        ML_INTER_ARRIVAL_MULTIPLIER_LIST.index(ML_INTER_ARRIVAL_MULTIPLIER)]

    server_idx_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]
    dst_gpu_idx_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]
    ports_to_dsts_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)] # list of lists of ports to which the packet should be mcasted
    jump_to_idx_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]   # lists of lists of indexes that the next switch should consider
    flow_ids_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]
    flow_size_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]
    query_ids_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]
    query_types_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]
    inter_arrival_times_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]

    flow_ID = 1
    titles = []
    servers_offered_load = [0 for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]

    
    if (NUM_FLOWS_PER_ML_QUERY >= (NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK * NUM_GPUS)):
        raise Exception("The number of destinations surpasses the possible destinations. Max is NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK - 1")

    NUM_ML_QUERIES = None
    while NUM_ML_QUERIES is None:
        query_ids = []
        query_types = []
        query_id_counter = 1
        query_times = []
        last_time_query_sent = ML_BASE_TIME
        while last_time_query_sent < SIM_TIME:
            query_ids.append(query_id_counter)
            query_types.append(get_random_query_type(collective_alg_percentage_list))
            query_id_counter += 1
            query_inter_arrival_time = get_inter_arrival_time(ML_INTER_ARRIVAL_MULTIPLIER)
            last_time_query_sent += query_inter_arrival_time
            # For ML the inter-arrival times presents the start time not the gap
            query_times.append(last_time_query_sent)
        if TARGET_NUM_ML_QUERIES is None or \
                (1 - ERROR_TOLLERANCE) * TARGET_NUM_ML_QUERIES <= len(query_times) <= (
                1 + ERROR_TOLLERANCE) * TARGET_NUM_ML_QUERIES:
            NUM_ML_QUERIES = len(query_times)
        else:
            print("Num incast events stays None because TARGET_NUM_ML_QUERIES isn't None and "
                  "len(query_times): {} and TARGET_NUM_ML_QUERIES:{}"
                  .format(len(query_times), TARGET_NUM_ML_QUERIES))
            print("---------------------------------------------------")

    flow_size_mult_str = '{:.6f}'.format(ML_FLOW_SIZE_MULTIPLIER)
    inter_arrival_mult_str = '{:.6f}'.format(ML_INTER_ARRIVAL_MULTIPLIER)
    inter_arrival_db_name = '{}_allgather_inter_arrival_time_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    server_idx_db_name = '{}_allgather_dst_server_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    gpu_idx_db_name = '{}_allgather_dst_gpu_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    ports_to_dest_db_name = '{}_allgather_ports_to_dst_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    jump_to_idx_db_name = '{}_allgather_jump_to_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    flow_ids_db_name = '{}_allgather_flow_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    flow_size_db_name = '{}_allgather_flow_size_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    query_ids_db_name = '{}_allgather_query_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    query_types_db_name = '{}_allgather_query_types_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    inter_arrival_table_name = "inter_arrival"
    server_idx_table_name = "dst_server_idx"
    gpu_idx_table_name = "dst_gpu_idx"
    ports_to_dest_table_name = "ports_to_dst"
    jump_to_idx_table_name = "jump_to_idx"
    flow_ids_table_name = "flow_ids"
    flow_size_table_name = "flow_size"
    query_ids_table_name = "query_ids"
    query_types_table_name = "query_types"

    for agg_idx in range(NUM_AGGS):
        for server_idx in range(NUM_SERVERS_UNDER_EACH_RACK):
            server_idx_among_all = agg_idx * NUM_SERVERS_UNDER_EACH_RACK + server_idx
            for app_idx in range(NUM_ML_APP_PER_SERVER):
                titles.append('server{}app{}'.format(server_idx_among_all,
                                                            1 + NUM_BACKGROUND_CONNECTIONS_PER_SERVER + NUM_BURSTY_APP_PER_SERVER 
                                                             + app_idx))


    total_num_flows = 0
    tor_to_available_time_dict = dict()
    for ml_query_idx in range(NUM_ML_QUERIES):
        if one_job_per_gpu:
            participants, participant_gpus, first_available_query_time = \
                get_allgather_participants_one_job_per_gpu(NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, query_times[ml_query_idx], tor_to_available_time_dict)
            query_times[ml_query_idx] = first_available_query_time
        else:
            participants, participant_gpus = get_allgather_participants(NUM_FLOWS_PER_ML_QUERY)
        query_id = query_ids[ml_query_idx]
        query_type = query_types[ml_query_idx]
        query_type_splitted = query_type.split('-')
        collective = query_type_splitted[0]
        alg = query_type_splitted[1]
        if (alg == "None"):
            alg = None

        # print(participants)
        # print(participant_gpus)
        # print(f'{len(sorted(list(set(participants))))} participant servers: {sorted(list(set(participants)))}')
        # print('------------------')
        counter_per_dst_dict = dict()
        src_dst_to_port_dict = dict()

        m_is_broadcast = (collective == "broadcast")
        
        chosen_participant_element_idx = None
        if (m_is_broadcast):
            chosen_participant_element_idx = random.randint(0, len(participants)-1)


        for participant_idx, participant_gpu_idx in zip(participants, participant_gpus):

            if (chosen_participant_element_idx is not None):
                if (participant_idx != participants[chosen_participant_element_idx] or 
                    participant_gpu_idx != participant_gpus[chosen_participant_element_idx]):
                    # not the participant we want as the source
                    continue


            query_ids_all[participant_idx].append(query_id)
            query_types_all[participant_idx].append(query_type)
            if participant_idx > len(server_idx_all):
                raise Exception("participant_idx > len(server_idx_all)")
            inter_arrival_times_all[participant_idx].append(query_times[ml_query_idx])
        
            src_node = Node()
            level_info = LevelInfo(src_node)
            dst_node_achieved_set = set()
            # these are re-written later
            index_of_dest_servers = [i for i in participants if i != participant_idx]
            index_of_dest_gpus = [j for i,j in zip(participants, participant_gpus) if i != participant_idx]
            if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-NUM_GPUS):
                raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-1")

            for dst_idx in index_of_dest_servers:
                if (dst_idx not in dst_node_achieved_set):
                    create_tree(participant_idx, dst_idx, level_info, counter_per_dst_dict, src_dst_to_port_dict)
                    dst_node_achieved_set.add(dst_idx)

            if (len(src_node.children_list) != 1):
                raise Exception("len(src_node.children_list) != 1")
            route = Route(participant_idx)
            fill_route_lists(src_node, 0, route)

            # Here, I'm not excluding the participant_idx because I need it in tree allreduce, 
            # I will exclude it in Omnet++ code
            index_of_dest_servers = [i for i in participants]
            index_of_dest_gpus = [j for i,j in zip(participants, participant_gpus)]

            server_idx_all[participant_idx] += index_of_dest_servers
            dst_gpu_idx_all[participant_idx] += index_of_dest_gpus

            # just add this for collectives that would rely on multicast not for others
            if (is_multicast_based_collective(collective)):
                ports_to_dsts_all[participant_idx] += [route.ports_to_dsts_list_of_lists]
                jump_to_idx_all[participant_idx] += [route.jump_to_idx_list_of_lists]
                # print(f'tree for query {query_id}')
                # print_tree(src_node)

            # print(f'Source: server[{participant_idx}]')
            # print(f'Dsts: {server_idx_all[participant_idx]}')
            # print(f'Gpus: {dst_gpu_idx_all[participant_idx]}')
            # print(f'Route:\n\tports_to_dsts_list_of_lists: {route.ports_to_dsts_list_of_lists}\n\tjump_to_idx_list_of_lists: {route.jump_to_idx_list_of_lists}')
            # print('-----------------------------\n')

            if (len(server_idx_all[participant_idx]) != len(dst_gpu_idx_all[participant_idx])):
                raise Exception("len(server_idx_all[participant_idx]) != len(dst_gpu_idx_all[participant_idx])")

            if (len(server_idx_all[participant_idx]) % NUM_FLOWS_PER_ML_QUERY != 0):
                raise Exception(f"len(server_idx_all[participant_idx]): {len(server_idx_all[participant_idx])}")

            # NUM_FLOWS_PER_ML_QUERY-NUM_GPUS is because the paritcipant itself should be excluded
            # however, add more flow ids to deal with allreduce as well
            for i in range(NUM_FLOWS_PER_ML_QUERY):
                # server_idx_all[query_source].append(index_of_dest_servers[i])
                transferred_flow_id = int(ML_FLOW_ID_BASE + str(flow_ID))
                flow_ids_all[participant_idx].append(transferred_flow_id)
                # fsize should be in bytes
                # just write flow size for all-to-allv cases
                if (collective == "alltoallv"):
                    if (FLOW_SIZE_GENERATION_TYPE is None):
                        raise Exception("Must indicate FLOW_SIZE_GENERATION_TYPE for alltoallv")
                    fsize = get_flow_size()
                    flow_size_all[participant_idx].append(fsize)
                flow_ID += 1

                # just a counter
                total_num_flows += 1
            
            # exclude the sender gpu
            servers_offered_load[participant_idx] += (NUM_FLOWS_PER_ML_QUERY-1)*(ML_FLOW_SIZE)

            if (chosen_participant_element_idx is not None):
                '''
                    We already created the collectiv efor the chosen source (broadcast/reduce)
                    no need to continue to other participant for this collective
                '''
                break

    # For all reduce we are sending the whole chunk of data so in offerred load we should multiply it by NUM_FLOWS_PER_ML_QUERY
    print(f'total offered bytes: {sum(servers_offered_load)}B')
    offered_Bps = sum(servers_offered_load)/(SIM_TIME-ML_BASE_TIME)
    print(f'total offered bytes per second: {offered_Bps}Bps')
    load_at_core = (offered_Bps * 8 * 100)/(NUM_AGGS * NUM_SPINES * CORE_LINK_CAPACITY)
    print(f'Oferred load at core: {load_at_core}%')
    print('last_time_query_sent is : {}'.format(last_time_query_sent))
    print('Total of {} ML queries and total of {} flows were sent.'.
          format(NUM_ML_QUERIES,
                 total_num_flows))
    # print(f'Chosen queries: {query_types}')
    m_stats = get_stats_dict(query_types)
    print(f'chose query types stats: {m_stats}')
    report_diff(collective_alg_percentage_dict, m_stats)

    table_headers = ''
    for title in titles:
        table_headers += title + ' text, '
    if table_headers != '':
        table_headers = table_headers[:-2]

    all_inserted_row_inter_arrival = []
    all_inserted_row_flow_size = []
    all_inserted_row_server_idx = []
    all_inserted_row_gpu_idx = []
    all_inserted_row_ports_to_dest = []
    all_inserted_row_jump_to_idx = []
    all_inserted_row_flow_ids = []
    all_inserted_row_query_ids = []
    all_inserted_row_query_types = []

    if table_headers != '':

        inter_arrival_time_db = sqlite3.connect(inter_arrival_db_name)
        inter_arrival_time_cursor = inter_arrival_time_db.cursor()

        server_idx_db = sqlite3.connect(server_idx_db_name)
        server_idx_cursor = server_idx_db.cursor()

        gpu_idx_db = sqlite3.connect(gpu_idx_db_name)
        gpu_idx_cursor = gpu_idx_db.cursor()

        ports_to_dest_db = sqlite3.connect(ports_to_dest_db_name)
        ports_to_dest_cursor = ports_to_dest_db.cursor()

        jump_to_idx_db = sqlite3.connect(jump_to_idx_db_name)
        jump_to_idx_cursor = jump_to_idx_db.cursor()

        flow_ids_db = sqlite3.connect(flow_ids_db_name)
        flow_ids_cursor = flow_ids_db.cursor()

        flow_size_db = sqlite3.connect(flow_size_db_name)
        flow_size_cursor = flow_size_db.cursor()

        query_ids_db = sqlite3.connect(query_ids_db_name)
        query_ids_cursor = query_ids_db.cursor()

        query_types_db = sqlite3.connect(query_types_db_name)
        query_types_cursor = query_types_db.cursor()

        inter_arrival_time_cursor.execute("DROP TABLE IF EXISTS {}".format(inter_arrival_table_name))
        inter_arrival_time_cursor.execute("CREATE TABLE {} ({})".format(inter_arrival_table_name, table_headers))

        server_idx_cursor.execute("DROP TABLE IF EXISTS {}".format(server_idx_table_name))
        server_idx_cursor.execute(
            "CREATE TABLE {} ({})".format(server_idx_table_name, table_headers))

        gpu_idx_cursor.execute("DROP TABLE IF EXISTS {}".format(gpu_idx_table_name))
        gpu_idx_cursor.execute(
            "CREATE TABLE {} ({})".format(gpu_idx_table_name, table_headers))

        ports_to_dest_cursor.execute("DROP TABLE IF EXISTS {}".format(ports_to_dest_table_name))
        ports_to_dest_cursor.execute(
            "CREATE TABLE {} ({})".format(ports_to_dest_table_name, table_headers))

        jump_to_idx_cursor.execute("DROP TABLE IF EXISTS {}".format(jump_to_idx_table_name))
        jump_to_idx_cursor.execute(
            "CREATE TABLE {} ({})".format(jump_to_idx_table_name, table_headers))

        flow_ids_cursor.execute("DROP TABLE IF EXISTS {}".format(flow_ids_table_name))
        flow_ids_cursor.execute("CREATE TABLE {} ({})".format(flow_ids_table_name, table_headers))

        flow_size_cursor.execute("DROP TABLE IF EXISTS {}".format(flow_size_table_name))
        flow_size_cursor.execute("CREATE TABLE {} ({})".format(flow_size_table_name, table_headers))

        query_ids_cursor.execute("DROP TABLE IF EXISTS {}".format(query_ids_table_name))
        query_ids_cursor.execute(
            "CREATE TABLE {} ({})".format(query_ids_table_name, table_headers))

        query_types_cursor.execute("DROP TABLE IF EXISTS {}".format(query_types_table_name))
        query_types_cursor.execute(
            "CREATE TABLE {} ({})".format(query_types_table_name, table_headers))

        max_server_idx_length = None
        for i in server_idx_all:
            if max_server_idx_length is None or len(i) > max_server_idx_length:
                max_server_idx_length = len(i)
        for length in range(max_server_idx_length):
            inserted_row_server_idx = []
            for app_idx in range(len(server_idx_all)):
                if length >= len(server_idx_all[app_idx]):
                    inserted_row_server_idx.append('')
                else:
                    inserted_row_server_idx.append(
                        '{}'.format(server_idx_all[app_idx][length]))
            all_inserted_row_server_idx.append(tuple(inserted_row_server_idx))

        # the length should be same as max_server_idx_length
        for length in range(max_server_idx_length):
            inserted_row_gpu_idx = []
            for app_idx in range(len(dst_gpu_idx_all)):
                if length >= len(dst_gpu_idx_all[app_idx]):
                    inserted_row_gpu_idx.append('')
                else:
                    inserted_row_gpu_idx.append(
                        '{}'.format(dst_gpu_idx_all[app_idx][length]))
            all_inserted_row_gpu_idx.append(tuple(inserted_row_gpu_idx))

        max_ports_to_dst_length = None
        for i in ports_to_dsts_all:
            if max_ports_to_dst_length is None or len(i) > max_ports_to_dst_length:
                max_ports_to_dst_length = len(i)
        inserted_row_ports_to_dest = []
        for length in range(max_ports_to_dst_length):
            inserted_row_ports_to_dest = []
            for app_idx in range(len(ports_to_dsts_all)):
                if length >= len(ports_to_dsts_all[app_idx]):
                    inserted_row_ports_to_dest.append('')
                else:
                    inserted_row_ports_to_dest.append(
                        '{}'.format(ports_to_dsts_all[app_idx][length]))
            all_inserted_row_ports_to_dest.append(tuple(inserted_row_ports_to_dest))

        max_jump_to_idx_length = None
        for i in jump_to_idx_all:
            if max_jump_to_idx_length is None or len(i) > max_jump_to_idx_length:
                max_jump_to_idx_length = len(i)
        inserted_row_jump_to_idx = []
        for length in range(max_jump_to_idx_length):
            inserted_row_jump_to_idx = []
            for app_idx in range(len(jump_to_idx_all)):
                if length >= len(jump_to_idx_all[app_idx]):
                    inserted_row_jump_to_idx.append('')
                else:
                    inserted_row_jump_to_idx.append(
                        '{}'.format(jump_to_idx_all[app_idx][length]))
            all_inserted_row_jump_to_idx.append(tuple(inserted_row_jump_to_idx))

        
        max_flow_ids_length = None
        for i in flow_ids_all:
            if max_flow_ids_length is None or len(i) > max_flow_ids_length:
                max_flow_ids_length = len(i)
        for length in range(max_flow_ids_length):
            inserted_row_flow_ids = []
            for app_idx in range(len(flow_ids_all)):
                if length >= len(flow_ids_all[app_idx]):
                    inserted_row_flow_ids.append('')
                else:
                    inserted_row_flow_ids.append(
                        '{}'.format(flow_ids_all[app_idx][length]))
            all_inserted_row_flow_ids.append(tuple(inserted_row_flow_ids))

        
        max_flow_size_length = None
        for i in flow_size_all:
            if max_flow_size_length is None or len(i) > max_flow_size_length:
                max_flow_size_length = len(i)
        inserted_row_flow_size = []
        for length in range(max_flow_size_length):
            inserted_row_flow_size = []
            for app_idx in range(len(flow_size_all)):
                if length >= len(flow_size_all[app_idx]):
                    inserted_row_flow_size.append('')
                else:
                    inserted_row_flow_size.append(
                        '{}'.format(flow_size_all[app_idx][length]))
            all_inserted_row_flow_size.append(tuple(inserted_row_flow_size))

        max_query_length = None
        for i in query_ids_all:
            if max_query_length is None or len(i) > max_query_length:
                max_query_length = len(i)
        for length in range(max_query_length):
            inserted_row_query_ids = []
            inserted_row_query_types = []
            inserted_row_inter_arrival_times = []
            for app_idx in range(len(server_idx_all)):
                if length >= len(query_ids_all[app_idx]):
                    inserted_row_query_ids.append('')
                    inserted_row_query_types.append('')
                    inserted_row_inter_arrival_times.append('')
                else:
                    inserted_row_query_ids.append(
                        '{}'.format(query_ids_all[app_idx][length]))
                    inserted_row_query_types.append(
                        '{}'.format(query_types_all[app_idx][length]))
                    inserted_row_inter_arrival_times.append(
                        '{}'.format(inter_arrival_times_all[app_idx][length]))
            all_inserted_row_query_ids.append(tuple(inserted_row_query_ids))
            all_inserted_row_query_types.append(tuple(inserted_row_query_types))
            all_inserted_row_inter_arrival.append(tuple(inserted_row_inter_arrival_times))

        # print(all_inserted_row_query_types)

        command_inter_arrival = "INSERT INTO {} VALUES (".format(inter_arrival_table_name)
        for i in range(len(inserted_row_inter_arrival_times)):
            command_inter_arrival += '?,'
        command_inter_arrival = command_inter_arrival[:-1] + ')'

        command_server_idx = "INSERT INTO {} VALUES (".format(server_idx_table_name)
        for i in range(len(inserted_row_server_idx)):
            command_server_idx += '?,'
        command_server_idx = command_server_idx[:-1] + ')'

        command_gpu_idx = "INSERT INTO {} VALUES (".format(gpu_idx_table_name)
        for i in range(len(inserted_row_gpu_idx)):
            command_gpu_idx += '?,'
        command_gpu_idx = command_gpu_idx[:-1] + ')'

        command_ports_to_dest = "INSERT INTO {} VALUES (".format(ports_to_dest_table_name)
        for i in range(len(inserted_row_ports_to_dest)):
            command_ports_to_dest += '?,'
        command_ports_to_dest = command_ports_to_dest[:-1] + ')'

        command_jump_to_idx = "INSERT INTO {} VALUES (".format(jump_to_idx_table_name)
        for i in range(len(inserted_row_jump_to_idx)):
            command_jump_to_idx += '?,'
        command_jump_to_idx = command_jump_to_idx[:-1] + ')'

        command_flow_ids = "INSERT INTO {} VALUES (".format(flow_ids_table_name)
        for i in range(len(inserted_row_flow_ids)):
            command_flow_ids += '?,'
        command_flow_ids = command_flow_ids[:-1] + ')'

        command_flow_size = "INSERT INTO {} VALUES (".format(flow_size_table_name)
        for i in range(len(inserted_row_flow_size)):
            command_flow_size += '?,'
        command_flow_size = command_flow_size[:-1] + ')'

        command_query_ids = "INSERT INTO {} VALUES (".format(query_ids_table_name)
        for i in range(len(inserted_row_query_ids)):
            command_query_ids += '?,'
        command_query_ids = command_query_ids[:-1] + ')'

        command_query_types = "INSERT INTO {} VALUES (".format(query_types_table_name)
        for i in range(len(inserted_row_query_types)):
            command_query_types += '?,'
        command_query_types = command_query_types[:-1] + ')'


        server_idx_cursor.executemany(command_server_idx, all_inserted_row_server_idx)
        server_idx_db.commit()
        server_idx_db.close()

        gpu_idx_cursor.executemany(command_gpu_idx, all_inserted_row_gpu_idx)
        gpu_idx_db.commit()
        gpu_idx_db.close()

        inter_arrival_time_cursor.executemany(command_inter_arrival, all_inserted_row_inter_arrival)
        inter_arrival_time_db.commit()
        inter_arrival_time_db.close()

        if (len(all_inserted_row_ports_to_dest) > 0):
            ports_to_dest_cursor.executemany(command_ports_to_dest, all_inserted_row_ports_to_dest)
        ports_to_dest_db.commit()
        ports_to_dest_db.close()

        if (len(all_inserted_row_jump_to_idx) > 0):
            jump_to_idx_cursor.executemany(command_jump_to_idx, all_inserted_row_jump_to_idx)
        jump_to_idx_db.commit()
        jump_to_idx_db.close()

        flow_ids_cursor.executemany(command_flow_ids, all_inserted_row_flow_ids)
        flow_ids_db.commit()
        flow_ids_db.close()

        if (len(inserted_row_flow_size) > 0):
            flow_size_cursor.executemany(command_flow_size, all_inserted_row_flow_size)
        flow_size_db.commit()
        flow_size_db.close()

        query_ids_cursor.executemany(command_query_ids, all_inserted_row_query_ids)
        query_ids_db.commit()
        query_ids_db.close()

        query_types_cursor.executemany(command_query_types, all_inserted_row_query_types)
        query_types_db.commit()
        query_types_db.close()
        
    print('------------------------------------')


TRACE_FILE_NAME = "collectives_algs_dist.csv"
# TRACE_FILE_NAME = "collectives_algs_dist_hoawei.csv"
collective_alg_percentage_dict = read_collective_file(TRACE_FILE_NAME)
collective_alg_percentage_list = list(collective_alg_percentage_dict.items())
sorted_collective_alg_percentage_list = sorted(collective_alg_percentage_list, key=lambda x: x[1])

# just to be safe :)
is_broadcast = None
is_reduce = None

with mp.Pool(processes=1) as p:
    inputs = []
    for rep_num in range(REP_NUM):
        for ML_INTER_ARRIVAL_MULTIPLIER in ML_INTER_ARRIVAL_MULTIPLIER_LIST:
            for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                    if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                        raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                    inputs.append((rep_num, collective_alg_percentage_dict, sorted_collective_alg_percentage_list,
                                        ML_INTER_ARRIVAL_MULTIPLIER,
                                        NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE))
    p.starmap(fill_tables, inputs)
print("All threads are over!")
