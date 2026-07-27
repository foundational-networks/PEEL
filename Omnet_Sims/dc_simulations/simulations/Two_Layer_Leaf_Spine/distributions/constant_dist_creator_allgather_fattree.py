from http import server
from itertools import count
import sqlite3
import numpy as np
import csv
import random
import multiprocessing as mp
from Variables import *
import math


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

class Route:
    def __init__(self, ID):
        self.ID = ID
        self.ports_to_dsts_list_of_lists = [[]]
        self.jump_to_idx_list_of_lists = [[]]
        self.last_idx = 1

def sort_tree_by_port_indices(node):
    # Base case: do nothing if no children
    if not node.children_list or not node.children_port_idx_list:
        return

    # Create list of (index, child) tuples and sort by index
    combined = list(zip(node.children_port_idx_list, node.children_list))
    combined.sort(key=lambda x: x[0])

    # Unzip back into separate lists
    node.children_port_idx_list, node.children_list = zip(*combined)
    
    # Convert back to lists
    node.children_port_idx_list = list(node.children_port_idx_list)
    node.children_list = list(node.children_list)

    # Recurse on children
    for child in node.children_list:
        sort_tree_by_port_indices(child)

def print_tree(node, level=0):
    # return
    if not node:
        return
    # Print the current node with indentation based on its depth
    print(" " * (level * 4) + f"Node --> ports: {node.children_port_idx_list}")
    # Recursively print each child
    for child in node.children_list:
        print_tree(child, level + 1)

def read_file_from_memory(dist_file_path):
    xs = []
    ys = []
    with open(dist_file_path, 'r') as csvfile:
        csv_file = csv.reader(csvfile, delimiter=',')
        for row in csv_file:
            try:
                x = float(row[0])
                cdf = float(row[1])
                xs.append(x)
                ys.append(cdf)
            except:
                continue
    return xs, ys


def get_random_point_based_on_cdf(xs, ys):
    random_y = random.random()
    minY = 5  # Distance in cdf is certainly less than 1
    minX = None
    for i in range(len(xs)):
        x = xs[i]
        cdf = ys[i]
        distance = abs(random_y - cdf)
        if (distance < minY or (distance == minY and minX < x)):
            minY = distance
            minX = x
    if minX is None:
        raise Exception('Something is wrong!')
    return minX


def get_inter_arrival_time(xs, ys, ML_INTER_ARRIVAL_MULTIPLIER):
    # dist is in us
    dist_data_microsecond = None
    if ML_INTER_ARRIVAL_DIST_TYPE == 'file':
        dist_data_microsecond = get_random_point_based_on_cdf(xs, ys)
    elif ML_INTER_ARRIVAL_DIST_TYPE == 'poisson':
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

    num_servers_per_tor = NUM_SERVERS_UNDER_EACH_RACK
    num_tors_per_pod = int(K/2)
    num_servers_per_pod = num_tors_per_pod * num_servers_per_tor
    num_aggs_per_pod = int(K/2)
    num_cores_per_agg = int(K/2)

    dst_tor_idx = int(dst_idx/num_servers_per_tor)
    dst_pod_idx = int(dst_idx/num_servers_per_pod)
    src_tor_idx = int(src_idx/num_servers_per_tor)
    src_pod_idx = int(src_idx/num_servers_per_pod)
    if (src_tor_idx == dst_tor_idx):
        # src and dst are under the same ToR
        highest_level = 1
    elif (src_pod_idx == dst_pod_idx):
        # src and dst are reachable through aggregate switches
        highest_level = 2
    else:
        # src and dst are reachable through core switches
        highest_level = 3

    # going up
    if highest_level not in level_info.level_map:
        highest_achieved_node = level_info.level_map[level_info.highest_level_achieved]
        for level in range(level_info.highest_level_achieved, highest_level):
            new_node = Node()
            level_info.level_map[level+1] = new_node
            if (level == 0):
                # port to get from server to leaf ToR can go from any of the gpus
                # choose the source gpu at random
                new_node_port_idx = random.randint(0, NUM_GPUS-1)
            elif (level == 1):
                # port to get from ToR to Agg
                new_node_port_idx = num_servers_per_tor * NUM_GPUS + \
                    random.randint(0, num_aggs_per_pod-1)
            elif (level == 2):
                # port to get from Agg to Core
                new_node_port_idx = num_tors_per_pod + \
                    random.randint(0, num_cores_per_agg-1)
            else:
                raise Exception("No other levels should be available")
            
            highest_achieved_node.children_list.append(new_node)
            highest_achieved_node.children_port_idx_list.append(new_node_port_idx)
            highest_achieved_node = new_node
        level_info.highest_level_achieved = highest_level

    # going down
    curr_node = level_info.level_map[highest_level]
    for level in range(highest_level, 0, -1):
        if (level == 1):
            # in ToR level area code is dst server idx
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
            port_idx = (dst_idx % num_servers_per_tor)*NUM_GPUS + addition_port_idx
            # print(f'({src_idx},{dst_idx}) --> {port_idx}')
        elif (level == 2):
            # in aggregate level area code is dst tor idx
            area_code = dst_tor_idx
            port_idx = (dst_tor_idx % num_tors_per_pod)
        elif (level == 3):
            # in the core level, area code is dst pod idx
            area_code = dst_pod_idx
            port_idx = dst_pod_idx
        else:
            print("Unknown level...")
        if ((level,area_code) not in level_info.going_down_areas_dict):
            new_node = Node()
            curr_node.children_list.append(new_node)
            curr_node.children_port_idx_list.append(port_idx)
            level_info.going_down_areas_dict[(level,area_code)] = new_node
        elif (level == 1):
            raise Exception("How can area code exists when we are in level 1 (ToR)")
        curr_node = level_info.going_down_areas_dict[(level,area_code)]

def fill_route_lists(curr_node:Node, curr_idx:int, route:Route):

    for idx, child in enumerate(curr_node.children_list):
        route.ports_to_dsts_list_of_lists.append([])
        route.jump_to_idx_list_of_lists.append([])
        route.ports_to_dsts_list_of_lists[curr_idx].append(curr_node.children_port_idx_list[idx])
        route.jump_to_idx_list_of_lists[curr_idx].append(route.last_idx)
        route.last_idx += 1
        fill_route_lists(child, route.last_idx-1, route)

# returns True if things are consequitive with at most one gap
def is_almost_consecutive(sorted_list):
    gaps = 0
    for i in range(len(sorted_list) - 1):
        if sorted_list[i + 1] - sorted_list[i] != 1:
            gaps += 1
            if gaps > 1:
                return False
    return True

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
    
def get_allgather_participants_fragment(NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, query_time, server_gpu_to_available_time_dict, fragment_ratio):

    # when we pass 1/8 it means 1/8 is not selectable so 7/8 can be selected
    fragment_ratio = (1.0 - fragment_ratio)

    num_pods = K
    num_servers_per_tor = NUM_SERVERS_UNDER_EACH_RACK
    num_tors_per_pod = int(K/2)
    num_gpus_under_tor = num_servers_per_tor * NUM_GPUS

    #  TODO: THIS MIGHT END IN AN INFINITE LOOP IF NOT ENOUGH NODES ARE AVAILABLE AFTER FRAGMENTATION!

    # initialize server_gpu_to_available_time_dict
    if (len(server_gpu_to_available_time_dict) == 0):
        for server_idx in range(num_pods * num_tors_per_pod * num_servers_per_tor):
            for gpu_idx in range(NUM_GPUS):
                if (server_idx, gpu_idx) not in server_gpu_to_available_time_dict:
                    server_gpu_to_available_time_dict[(server_idx, gpu_idx)] = -1

    if (NUM_FLOWS_PER_ML_QUERY > (num_gpus_under_tor * num_tors_per_pod * num_pods * fragment_ratio)):
        print(NUM_FLOWS_PER_ML_QUERY)
        print(num_gpus_under_tor * num_tors_per_pod * num_pods * fragment_ratio)
        raise Exception("Not enough destinatins even available!")

    scale = NUM_FLOWS_PER_ML_QUERY
    current_server_idx = random.randint(0, NUM_AGGS * num_servers_per_tor - 1)
    curr_gpu_idx = 0
    chosen_node = []
    fragments_remaining = int(scale * (1-fragment_ratio))
    while len(chosen_node) < scale:
        if (query_time >= server_gpu_to_available_time_dict[(current_server_idx, curr_gpu_idx)]):
            # node is avaiable
            chosen_node.append((current_server_idx, curr_gpu_idx))
        else:
            # node is already busy causing fragments
            fragments_remaining -= 1
        curr_gpu_idx += 1
        if (curr_gpu_idx == NUM_GPUS):
            current_server_idx += 1
            current_server_idx %= (NUM_AGGS * num_servers_per_tor)
            curr_gpu_idx = 0
    fragments_remaining = max(0, fragments_remaining)
    if (fragments_remaining > 0):
        # new nodes must be chosen and then be fragmented
        # have not yet reached the target!
        tmp_fragments_remaining = fragments_remaining
        while (tmp_fragments_remaining > 0):
            # if node is busy, we count it as target fragment, if not we add it for now and remove it later
            if (query_time >= server_gpu_to_available_time_dict[(current_server_idx, curr_gpu_idx)]):
                # node is avaiable
                chosen_node.append((current_server_idx, curr_gpu_idx))
            else:
                fragments_remaining -= 1    # no need to remove this in the future, busy node already causing fragmentation
            tmp_fragments_remaining -= 1

            if (fragments_remaining < 0):
                raise Exception("Fragments remaining should never reach less than zero in this scenario!")

            curr_gpu_idx += 1
            if (curr_gpu_idx == NUM_GPUS):
                current_server_idx += 1
                current_server_idx %= (NUM_AGGS * num_servers_per_tor)
                curr_gpu_idx = 0
    
    # randomly remove remaining fragmentations!
    # removes Y random elements from X in-place
    random_indices = set(random.sample(range(len(chosen_node)), fragments_remaining))
    chosen_node = [x for i, x in enumerate(chosen_node) if i not in random_indices]

    if (len(chosen_node) != scale):
        raise Exception("At this point, len(chosen_nodes) must be equal to scale!!")
    
    index_of_dest_servers, index_of_dest_gpus = zip(*chosen_node)
    index_of_dest_servers = list(index_of_dest_servers)
    index_of_dest_gpus = list(index_of_dest_gpus)
        
        # if (scale > 0):
        #     if (while_counter > 0):
        #         raise Exception("Not enough nodes found!")
        #     print("Not enough nodes are available, let's try another time...")
        #     while_counter += 1
        #     scale = NUM_FLOWS_PER_ML_QUERY
        #     index_of_dest_servers = []
        #     index_of_dest_gpus = []
        #     query_time = earliest_time_for_X_tors(server_gpu_to_available_time_dict, scale)
    
    if (len(index_of_dest_servers) != len(index_of_dest_gpus)):
        raise Exception("len(index_of_dest_servers) != len(index_of_dest_gpus)")
    if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY):
        raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY")

    # add processing overhead
    random_processing_overhead =  1 + random.uniform(0, 0.25)
    processing_time = random_processing_overhead * ML_FLOW_SIZE * 8 / (100 * 10**9)
    for idx in range(len(index_of_dest_servers)):
        server_gpu_to_available_time_dict[(index_of_dest_servers[idx], index_of_dest_gpus[idx])] = query_time + processing_time

    # print(index_of_dest_servers)
    # print(index_of_dest_gpus)
    return index_of_dest_servers, index_of_dest_gpus, query_time
    
def get_allgather_participants_one_job_per_gpu(NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, query_time, tor_to_available_time_dict):

    num_pods = K
    num_servers_per_tor = NUM_SERVERS_UNDER_EACH_RACK
    num_tors_per_pod = int(K/2)
    num_gpus_under_tor = num_servers_per_tor * NUM_GPUS

    if (NUM_FLOWS_PER_ML_QUERY % num_gpus_under_tor != 0):
        raise Exception("We assumed that we always require at least all nodes below the chosen tors")

    start_tor_idx = random.randint(0, num_pods*num_tors_per_pod - 1)
    # start_tor_idx = 0

    # first check if we have enough tors available
    enough_tors_available = False
    available_tors_num = 0
    required_tors = int(NUM_FLOWS_PER_ML_QUERY / num_gpus_under_tor)
    i = 0
    tor_idx = start_tor_idx
    while (available_tors_num < required_tors and i < num_pods * num_tors_per_pod):
        if tor_idx not in tor_to_available_time_dict:
            tor_to_available_time_dict[tor_idx] = 0
        if tor_to_available_time_dict[tor_idx] <= query_time:
            available_tors_num += 1
        if (available_tors_num == required_tors):
            enough_tors_available = True
        tor_idx += 1
        tor_idx %= num_pods*num_tors_per_pod
        i += 1

    if (not enough_tors_available):
        earliest_time = earliest_time_for_X_tors(tor_to_available_time_dict, required_tors)
        # print(earliest_time)
        if (earliest_time > query_time):
            # print(f'query time updated from {query_time} to {earliest_time}')
            query_time = earliest_time

    chosen_tors = []
    tor_idx = start_tor_idx
    random_processing_overhead =  1 + random.uniform(0, 0.25)
    processing_time = random_processing_overhead * ML_FLOW_SIZE * 8 / (100 * 10**9)
    # print(f'random overhead: {random_processing_overhead}, "processing time: {processing_time}')
    i = 0
    while (len(chosen_tors) < required_tors):
        if tor_to_available_time_dict[tor_idx] <= query_time:
            chosen_tors.append(tor_idx)
        tor_idx += 1
        tor_idx %= num_pods*num_tors_per_pod
        i += 1
        if (i == num_pods * num_tors_per_pod and len(chosen_tors) < required_tors):
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
    
    if (len(index_of_dest_servers) != len(index_of_dest_gpus)):
        raise Exception("len(index_of_dest_servers) != len(index_of_dest_gpus)")
    if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY):
        raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY")

    return index_of_dest_servers, index_of_dest_gpus, query_time

def find_non_sequential_tors(lst):
    if not lst:
        raise Exception("List is empty")
    
    tor_set = set()
    for server_idx in lst:
        tor_idx = int(server_idx / NUM_SERVERS_UNDER_EACH_RACK)
        tor_set.add(tor_idx)
    tor_list = sorted(tor_set)

    tor_list_sequential = all(tor_list[i] + 1 == tor_list[i + 1] for i in range(len(tor_list) - 1))
    tor_list_probably_sequencial = (tor_list[0] == 0 and tor_list[-1] == NUM_AGGS-1)
    if (not tor_list_sequential and not tor_list_probably_sequencial):
        print(f'tor list is not sequential: {tor_list}')


def get_allgather_participants(NUM_FLOWS_PER_ML_QUERY):

    num_pods = K
    num_servers_per_tor = NUM_SERVERS_UNDER_EACH_RACK
    num_tors_per_pod = int(K/2)
    num_servers_per_pod = num_tors_per_pod * NUM_SERVERS_UNDER_EACH_RACK

    total_dest_num = NUM_FLOWS_PER_ML_QUERY
    index_of_dest_servers = []
    index_of_dest_gpus = []
    possible_pod_list = [j for j in range(num_pods)]
    # possible_TOR_list = [j for j in range(num_tors)]
    chosen_pod = None
    while total_dest_num != 0:
        if (chosen_pod is None or not maximize_locality):
            chosen_pod = random.sample(possible_pod_list, 1)[0]
        else:
            chosen_pod += 1
            chosen_pod %= num_pods
        if (chosen_pod not in possible_pod_list):
            raise Exception("chosen_pod not in possible_pod_list")
        possible_pod_list.remove(chosen_pod)
        possible_ToR_list = [j for j in range(num_tors_per_pod)]
        max_nodes_per_pod = num_servers_per_pod * NUM_GPUS
        tors_to_select = math.ceil(min(total_dest_num, max_nodes_per_pod) / (num_servers_per_tor * NUM_GPUS))
        assert(tors_to_select <= len(possible_ToR_list))
        chosen_tors = []
        chosen_tor = None
        while len(chosen_tors) < tors_to_select:
            if (chosen_tor is None or not maximize_locality):
                chosen_tor = random.sample(possible_ToR_list, 1)[0]
            else:
                chosen_tor += 1
                chosen_tor %= num_tors_per_pod
            if (chosen_tor not in possible_ToR_list):
                raise Exception("chosen_tor not in possible_ToR_list")
            possible_ToR_list.remove(chosen_tor)
            chosen_tors.append(chosen_tor)

        random.shuffle(chosen_tors)
        for chosen_tor in chosen_tors:
            possible_servers = [t for t in range(chosen_pod * num_servers_per_pod + \
                                                chosen_tor * num_servers_per_tor, \
                                                chosen_pod * num_servers_per_pod + \
                                                (chosen_tor+1) * num_servers_per_tor)]
            random.shuffle(possible_servers)
            possible_dsts = []
            possible_dst_gpus = []
            for j in possible_servers:
                possible_dsts += [j for _ in range(NUM_GPUS)]
                added_gpus = [m for m in range(NUM_GPUS)]
                random.shuffle(added_gpus)
                possible_dst_gpus += added_gpus
            if len(possible_dsts) <= total_dest_num:
                # we need all the servers under this pod
                index_of_dest_servers += possible_dsts
                index_of_dest_gpus += possible_dst_gpus
                total_dest_num -= len(possible_dsts)
            else:
                index_of_dest_servers += possible_dsts[0:total_dest_num]
                index_of_dest_gpus += possible_dst_gpus[0:total_dest_num]
                total_dest_num = 0
                break
    
    if (len(index_of_dest_servers) != len(index_of_dest_gpus)):
        raise Exception("len(index_of_dest_servers) != len(index_of_dest_gpus)")
    if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY):
        raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY")
    return index_of_dest_servers, index_of_dest_gpus

def fill_tables(rep_num, inter_arrival_x, inter_arrival_y, ML_INTER_ARRIVAL_MULTIPLIER,
                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, fragment_ratio=None):

    # fragmentation ratio: what percentage of hosts are considered un available during job assignment
    if fragment_ratio is not None and not (0 <= fragment_ratio < 1):
        raise Exception("Invalid fragmentation ratio...")

    print(
        "IA = {}, SCALE = {}, FS = {}, Fragmentation %= {}".format(ML_INTER_ARRIVAL_MULTIPLIER, \
            NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, fragment_ratio))
    
    TARGET_NUM_ML_QUERIES = TARGET_NUM_ML_QUERIES_LIST[
        ML_INTER_ARRIVAL_MULTIPLIER_LIST.index(ML_INTER_ARRIVAL_MULTIPLIER)]

    num_servers = int(K**2 / 2) * NUM_SERVERS_UNDER_EACH_RACK

    server_idx_all = [[] for i in range(num_servers)]
    dst_gpu_idx_all = [[] for i in range(num_servers)]
    ports_to_dsts_all = [[] for i in range(num_servers)] # list of lists of ports to which the packet should be mcasted
    jump_to_idx_all = [[] for i in range(num_servers)]   # lists of lists of indexes that the next switch should consider
    flow_ids_all = [[] for i in range(num_servers)]
    flow_size_all = [[] for i in range(num_servers)]
    query_ids_all = [[] for i in range(num_servers)]
    inter_arrival_times_all = [[] for i in range(num_servers)]

    flow_ID = 1
    titles = []
    servers_offered_load = [0 for i in range(num_servers)]

    if one_job_per_gpu and maximize_locality:
        raise Exception("one_job_per_gpu and maximize_locality cannot be True together!")

    
    if (NUM_FLOWS_PER_ML_QUERY > (num_servers * NUM_GPUS)):
        print(NUM_FLOWS_PER_ML_QUERY)
        print(num_servers * NUM_GPUS)
        raise Exception("The number of destinations surpasses the possible destinations. Max is K^3 / 4 * NUM_GPUS")

    NUM_ML_QUERIES = None
    query_times = []
    while NUM_ML_QUERIES is None:
        query_ids = []
        query_id_counter = 1
        query_times.clear()
        last_time_query_sent = ML_BASE_TIME
        while last_time_query_sent < SIM_TIME:
            query_ids.append(query_id_counter)
            query_id_counter += 1
            query_inter_arrival_time = get_inter_arrival_time(inter_arrival_x, inter_arrival_y,
                                                               ML_INTER_ARRIVAL_MULTIPLIER)
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
    inter_arrival_db_name = '{}_allgather_inter_arrival_time_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    server_idx_db_name = '{}_allgather_dst_server_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    gpu_idx_db_name = '{}_allgather_dst_gpu_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    ports_to_dest_db_name = '{}_allgather_ports_to_dst_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    jump_to_idx_db_name = '{}_allgather_jump_to_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    flow_ids_db_name = '{}_allgather_flow_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    flow_size_db_name = '{}_allgather_flow_size_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    query_ids_db_name = '{}_allgather_query_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)

    if fragment_ratio is not None:
        fragment_ratio_str = '{:.6f}'.format(fragment_ratio)
        inter_arrival_db_name += f'_{fragment_ratio_str}_frag'
        server_idx_db_name += f'_{fragment_ratio_str}_frag'
        gpu_idx_db_name += f'_{fragment_ratio_str}_frag'
        ports_to_dest_db_name += f'_{fragment_ratio_str}_frag'
        jump_to_idx_db_name += f'_{fragment_ratio_str}_frag'
        flow_ids_db_name += f'_{fragment_ratio_str}_frag'
        flow_size_db_name += f'_{fragment_ratio_str}_frag'
        query_ids_db_name += f'_{fragment_ratio_str}_frag'

    
    inter_arrival_db_name += f'_{rep_num}.db'
    server_idx_db_name += f'_{rep_num}.db'
    gpu_idx_db_name += f'_{rep_num}.db'
    ports_to_dest_db_name += f'_{rep_num}.db'
    jump_to_idx_db_name += f'_{rep_num}.db'
    flow_ids_db_name += f'_{rep_num}.db'
    flow_size_db_name += f'_{rep_num}.db'
    query_ids_db_name += f'_{rep_num}.db'

    inter_arrival_table_name = "inter_arrival"
    server_idx_table_name = "dst_server_idx"
    gpu_idx_table_name = "dst_gpu_idx"
    ports_to_dest_table_name = "ports_to_dst"
    jump_to_idx_table_name = "jump_to_idx"
    flow_ids_table_name = "flow_ids"
    flow_size_table_name = "flow_size"
    query_ids_table_name = "query_ids"

    for server_idx in range(num_servers):
        for app_idx in range(NUM_ML_APP_PER_SERVER):
            titles.append('server{}app{}'.format(server_idx,
                                                        1 + NUM_BACKGROUND_CONNECTIONS_PER_SERVER + NUM_BURSTY_APP_PER_SERVER 
                                                            + app_idx))

    total_num_flows = 0
    tor_to_available_time_dict = dict()
    server_gpu_to_available_time_dict = dict()
    for ml_query_idx in range(NUM_ML_QUERIES):
        if fragment_ratio is not None:
            participants, participant_gpus, first_available_query_time = \
                get_allgather_participants_fragment(NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, \
                    query_times[ml_query_idx], server_gpu_to_available_time_dict, fragment_ratio)
            query_times[ml_query_idx] = first_available_query_time
        elif one_job_per_gpu:
            participants, participant_gpus, first_available_query_time = \
                get_allgather_participants_one_job_per_gpu(NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, \
                    query_times[ml_query_idx], tor_to_available_time_dict)
            query_times[ml_query_idx] = first_available_query_time
        else:
            if (fragment_ratio is not None):
                raise Exception("get_allgather_participants does not work with fragment_ratio")
            participants, participant_gpus = get_allgather_participants(NUM_FLOWS_PER_ML_QUERY)
        query_id = query_ids[ml_query_idx]

        # print(f'participant servers: {participants}')
        # print(f'participant gpus: {participant_gpus}')

        if (maximize_locality and not is_almost_consecutive(sorted(list(set(participants))))):
            print(f'participant servers: {sorted(list(set(participants)))}')
            raise Exception("We have maximized locality, why aren't the dsts consecuitive!")
        
        print(f'fragmentation ratio: {fragment_ratio}')
        print(f'{len(sorted(list(set(participants))))} participant servers: {sorted(list(set(participants)))}')
        find_non_sequential_tors(sorted(list(set(participants))))

        counter_per_dst_dict = dict()
        src_dst_to_port_dict = dict()
        
        chosen_participant_element_idx = None
        if (is_broadcast):
            chosen_participant_element_idx = random.randint(0, len(participants)-1)
            # chosen_participant_element_idx = 0
            print(f'chosen source server idx: {participants[chosen_participant_element_idx]}')
        print('------------------')
        
        for participant_idx, participant_gpu_idx in zip(participants, participant_gpus):

            if (chosen_participant_element_idx is not None):
                if (participant_idx != participants[chosen_participant_element_idx] or 
                    participant_gpu_idx != participant_gpus[chosen_participant_element_idx]):
                    # not the participant we want as the source
                    continue

            query_ids_all[participant_idx].append(query_id)
            if participant_idx > len(server_idx_all):
                raise Exception("participant_idx > len(server_idx_all)")
            inter_arrival_times_all[participant_idx].append(query_times[ml_query_idx])
        
            src_node = Node()
            level_info = LevelInfo(src_node)
            dst_node_achieved_set = set()
            # these are re-written later
            index_of_dest_servers = [i for i in participants if i != participant_idx]
            index_of_dest_gpus = [j for i,j in zip(participants, participant_gpus) if i != participant_idx]
            if (fragment_ratio is None and len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-NUM_GPUS):
                print(len(index_of_dest_servers))
                print(NUM_FLOWS_PER_ML_QUERY-NUM_GPUS)
                raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-NUM_GPUS")

            for dst_idx in index_of_dest_servers:
                if (dst_idx not in dst_node_achieved_set):
                    create_tree(participant_idx, dst_idx, level_info, counter_per_dst_dict, src_dst_to_port_dict)
                    dst_node_achieved_set.add(dst_idx)
                    
            sort_tree_by_port_indices(src_node)
            # print_tree(src_node)

            if (len(src_node.children_list) != 1):
                raise Exception("len(src_node.children_list) != 1")
            route = Route(participant_idx)
            fill_route_lists(src_node, 0, route)
            # print(route.ports_to_dsts_list_of_lists)
            # print(route.jump_to_idx_list_of_lists)

            # Here, I'm not excluding the participant_idx because I need it in tree allreduce, 
            # I will exclude it in Omnet++ code
            index_of_dest_servers = [i for i in participants]
            index_of_dest_gpus = [j for i,j in zip(participants, participant_gpus)]

            server_idx_all[participant_idx] += index_of_dest_servers
            dst_gpu_idx_all[participant_idx] += index_of_dest_gpus
            ports_to_dsts_all[participant_idx] += [route.ports_to_dsts_list_of_lists]
            jump_to_idx_all[participant_idx] += [route.jump_to_idx_list_of_lists]

            # print(f'Source: server[{participant_idx}]')
            # print(f'Dsts: {server_idx_all[participant_idx]}')
            # print(f'Gpus: {dst_gpu_idx_all[participant_idx]}')
            # print(f'Route:\n\tports_to_dsts_list_of_lists: {route.ports_to_dsts_list_of_lists}\n\tjump_to_idx_list_of_lists: {route.jump_to_idx_list_of_lists}')
            # print('-----------------------------\n')

            if (len(server_idx_all[participant_idx]) != len(dst_gpu_idx_all[participant_idx])):
                raise Exception("len(server_idx_all[participant_idx]) != len(dst_gpu_idx_all[participant_idx])")

            # NUM_FLOWS_PER_ML_QUERY-NUM_GPUS is because the paritcipant itself should be excluded
            # however, add more flow ids to deal with allreduce as well
            for i in range(NUM_FLOWS_PER_ML_QUERY):
                # server_idx_all[query_source].append(index_of_dest_servers[i])
                transferred_flow_id = int(ML_FLOW_ID_BASE + str(flow_ID))
                flow_ids_all[participant_idx].append(transferred_flow_id)
                # fsize should be in bytes
                if (FLOW_SIZE_GENERATION_TYPE is None):
                    fsize = ML_FLOW_SIZE
                else:
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
    offered_Bps_per_pod = offered_Bps / K
    load_at_core = (offered_Bps_per_pod * 8 * 100)/(K**2/4 * CORE_LINK_CAPACITY)
    print(f'Oferred load at core: {load_at_core}%')
    print('last_time_query_sent is : {}'.format(last_time_query_sent))
    print('Total of {} ML queries and total of {} flows were sent.'.
          format(NUM_ML_QUERIES,
                 total_num_flows))

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
            inserted_row_inter_arrival_times = []
            for app_idx in range(len(server_idx_all)):
                if length >= len(query_ids_all[app_idx]):
                    inserted_row_query_ids.append('')
                    inserted_row_inter_arrival_times.append('')
                else:
                    inserted_row_query_ids.append(
                        '{}'.format(query_ids_all[app_idx][length]))
                    inserted_row_inter_arrival_times.append(
                        '{}'.format(inter_arrival_times_all[app_idx][length]))
            all_inserted_row_query_ids.append(tuple(inserted_row_query_ids))
            all_inserted_row_inter_arrival.append(tuple(inserted_row_inter_arrival_times))

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

        server_idx_cursor.executemany(command_server_idx, all_inserted_row_server_idx)
        server_idx_db.commit()
        server_idx_db.close()

        gpu_idx_cursor.executemany(command_gpu_idx, all_inserted_row_gpu_idx)
        gpu_idx_db.commit()
        gpu_idx_db.close()

        inter_arrival_time_cursor.executemany(command_inter_arrival, all_inserted_row_inter_arrival)
        inter_arrival_time_db.commit()
        inter_arrival_time_db.close()

        ports_to_dest_cursor.executemany(command_ports_to_dest, all_inserted_row_ports_to_dest)
        ports_to_dest_db.commit()
        ports_to_dest_db.close()

        jump_to_idx_cursor.executemany(command_jump_to_idx, all_inserted_row_jump_to_idx)
        jump_to_idx_db.commit()
        jump_to_idx_db.close()

        flow_ids_cursor.executemany(command_flow_ids, all_inserted_row_flow_ids)
        flow_ids_db.commit()
        flow_ids_db.close()

        flow_size_cursor.executemany(command_flow_size, all_inserted_row_flow_size)
        flow_size_db.commit()
        flow_size_db.close()

        query_ids_cursor.executemany(command_query_ids, all_inserted_row_query_ids)
        query_ids_db.commit()
        query_ids_db.close()
        
    print('------------------------------------')


inter_arrival_x, inter_arrival_y = read_file_from_memory(ML_INTER_ARRIVAL_DIST_FILE_NAME)

if constant_load:
    print('Constant load')
    with mp.Pool(processes=MAX_THREAD_NUM) as p:
        inputs = []
        for rep_num in range(REP_NUM):
            for fragment_ratio in FRAGMENT_RATIOS:
                multiplier_idx = -1
                for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                    for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                        if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                            raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                        multiplier_idx += 1
                        ML_INTER_ARRIVAL_MULTIPLIER = ML_INTER_ARRIVAL_MULTIPLIER_LIST[multiplier_idx]
                        inputs.append((rep_num, inter_arrival_x, inter_arrival_y,
                                            ML_INTER_ARRIVAL_MULTIPLIER,
                                            NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                                            fragment_ratio))
        p.starmap(fill_tables, inputs)
else:
    print('Changing load')
    with mp.Pool(processes=MAX_THREAD_NUM) as p:
        inputs = []
        for rep_num in range(REP_NUM):
            for fragment_ratio in FRAGMENT_RATIOS:
                for ML_INTER_ARRIVAL_MULTIPLIER in ML_INTER_ARRIVAL_MULTIPLIER_LIST:
                    for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                        for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                            if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                                raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                            inputs.append((rep_num, inter_arrival_x, inter_arrival_y,
                                                ML_INTER_ARRIVAL_MULTIPLIER,
                                                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                                                fragment_ratio))
        p.starmap(fill_tables, inputs)
print("All threads are over!")