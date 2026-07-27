from itertools import count
import sqlite3
import numpy as np
import csv
import random
import multiprocessing as mp
from Variables import *


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

def create_tree(src_idx:int, dst_idx:int, level_info:LevelInfo, query_id, query_id_map_counter_per_dst_dict, query_id_map_src_dst_to_port_dict, tree_idx, tree_port_to_leaf_indexes):
    if (query_id not in query_id_map_counter_per_dst_dict):
        raise Exception("How is query_id not in query_id_map_counter_per_dst_dict")
    # get the dicts for a specific query id
    counter_per_dst_dict = query_id_map_counter_per_dst_dict[query_id]
    src_dst_to_port_dict = query_id_map_src_dst_to_port_dict[query_id]
    
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
                # new_node_port_idx = NUM_SERVERS_UNDER_EACH_RACK * NUM_GPUS + \
                #     random.randint(0, NUM_SPINES-1)
                new_node_port_idx = NUM_SERVERS_UNDER_EACH_RACK * NUM_GPUS + tree_port_to_leaf_indexes[tree_idx]
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

def is_multicast_based_collective(query, always_generate_tree):
    if (always_generate_tree):
        return True
    if ("broadcast" in query):
        return True
    if ("allreduce" in query):
        return True
    if ("allgather" in query):
        return True
    return False

def fill_tables(rep_num, ML_INTER_ARRIVAL_MULTIPLIER,
                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective=1, fragment_ratio=None):
    
    if (num_trees_per_collective is None):
        num_trees_per_collective = NUM_SPINES
    
    print(
        "IA = {}, SCALE = {}, FS = {}, num_trees_per_collective={}".format(ML_INTER_ARRIVAL_MULTIPLIER, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective))

    ports_to_dsts_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)] # list of lists of ports to which the packet should be mcasted
    jump_to_idx_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]   # lists of lists of indexes that the next switch should consider

    titles = []

    flow_size_mult_str = '{:.6f}'.format(ML_FLOW_SIZE_MULTIPLIER)
    inter_arrival_mult_str = '{:.6f}'.format(ML_INTER_ARRIVAL_MULTIPLIER)
    server_idx_db_name = '{}_allgather_dst_server_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    ports_to_dest_db_name = '{}_allgather_ports_to_dst_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}_lb' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, num_trees_per_collective)
    jump_to_idx_db_name = '{}_allgather_jump_to_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}_lb' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, num_trees_per_collective)
    query_ids_db_name = '{}_allgather_query_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)
    query_types_db_name = '{}_allgather_query_types_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS)

    if fragment_ratio is not None:
        fragment_ratio_str = '{:.6f}'.format(fragment_ratio)
        server_idx_db_name += f'_{fragment_ratio_str}_frag'
        query_ids_db_name += f'_{fragment_ratio_str}_frag'
        ports_to_dest_db_name += f'_{fragment_ratio_str}_frag'
        jump_to_idx_db_name += f'_{fragment_ratio_str}_frag'
        query_types_db_name += f'_{fragment_ratio_str}_frag'

    server_idx_db_name += f'_{rep_num}.db'
    query_ids_db_name += f'_{rep_num}.db'
    ports_to_dest_db_name += f'_{rep_num}.db'
    jump_to_idx_db_name += f'_{rep_num}.db'
    query_types_db_name += f'_{rep_num}.db'
    
    server_idx_table_name = "dst_server_idx"
    ports_to_dest_table_name = "ports_to_dst"
    jump_to_idx_table_name = "jump_to_idx"
    query_ids_table_name = "query_ids"
    query_types_table_name = "query_types"

    try:
        # Connect to the SQLite database
        server_idx_connection = sqlite3.connect(server_idx_db_name)
        server_idx_cursor = server_idx_connection.cursor()
        
        # Get the column names of the specified table
        server_idx_cursor.execute(f"PRAGMA table_info({server_idx_table_name});")
        columns = server_idx_cursor.fetchall()
        
        if not columns:
            raise Exception(f"Table '{server_idx_table_name}' does not exist or has no columns.")

    except sqlite3.Error as e:
        raise Exception(f"Error: {e}")

    try:
        # Connect to the SQLite database
        query_ids_connection = sqlite3.connect(query_ids_db_name)
        query_ids_cursor = query_ids_connection.cursor()
        
        # Get the column names of the specified table
        query_ids_cursor.execute(f"PRAGMA table_info({query_ids_table_name});")
        columns = query_ids_cursor.fetchall()
        
        if not columns:
            raise Exception(f"Table '{query_ids_table_name}' does not exist or has no columns.")

    except sqlite3.Error as e:
        raise Exception(f"Error: {e}")

    always_generate_tree = True
    try:
        # Connect to the SQLite database
        query_types_connection = sqlite3.connect(query_types_db_name)
        query_types_cursor = query_types_connection.cursor()
        
        # Get the column names of the specified table
        query_types_cursor.execute(f"PRAGMA table_info({query_types_table_name});")
        columns = query_types_cursor.fetchall()

        always_generate_tree = False
        
        if not columns:
            print(f"Table '{query_types_table_name}' does not exist or has no columns.")
            # exit(0)
            raise Exception(f"Table '{query_types_table_name}' does not exist or has no columns.")
    except:
        if (always_generate_tree):
            print('Always generating tree')

    all_dst_server_idx_from_db = []
    all_query_ids_from_db = []
    all_query_types_from_db = []

    for agg_idx in range(NUM_AGGS):
        for server_idx in range(NUM_SERVERS_UNDER_EACH_RACK):
            server_idx_among_all = agg_idx * NUM_SERVERS_UNDER_EACH_RACK + server_idx
            for app_idx in range(NUM_ML_APP_PER_SERVER):
                m_title = 'server{}app{}'.format(server_idx_among_all,
                                                            1 + NUM_BACKGROUND_CONNECTIONS_PER_SERVER + NUM_BURSTY_APP_PER_SERVER 
                                                             + app_idx)
                # print(f"Reading column: {m_title}")
                
                server_idx_cursor.execute(f"SELECT {m_title} FROM {server_idx_table_name};")
                query_ids_cursor.execute(f"SELECT {m_title} FROM {query_ids_table_name};")
                query_types_cursor.execute(f"SELECT {m_title} FROM {query_types_table_name};")
                rows = server_idx_cursor.fetchall()
                tmp_server_idx_from_db = [int(item[0]) for item in rows if item[0].isdigit()]
                rows = query_ids_cursor.fetchall()
                tmp_query_idx_from_db = [int(item[0]) for item in rows if item[0].isdigit()]
                rows = query_types_cursor.fetchall()
                tmp_query_types_from_db = [item[0] for item in rows if len(item[0]) > 0]

                if (len(tmp_server_idx_from_db) != 0):
                    if (len(tmp_server_idx_from_db) % len(tmp_query_idx_from_db) != 0):
                        raise Exception("len(tmp_server_idx_from_db) % len(tmp_query_idx_from_db) must be zero")
                    if (int(len(tmp_server_idx_from_db) / len(tmp_query_idx_from_db)) != NUM_FLOWS_PER_ML_QUERY):
                        raise Exception("len(tmp_server_idx_from_db) / len(tmp_query_idx_from_db) must be equal to NUM_FLOWS_PER_ML_QUERY")
                
                all_dst_server_idx_from_db.append(tmp_server_idx_from_db)
                all_query_ids_from_db.append(tmp_query_idx_from_db)
                all_query_types_from_db.append(tmp_query_types_from_db)
                titles.append(m_title)
    
    if (len(all_dst_server_idx_from_db) != NUM_AGGS*NUM_SERVERS_UNDER_EACH_RACK):
        raise Exception("len(all_dst_server_idx_from_db) != NUM_AGGS*NUM_SERVERS_UNDER_EACH_RACK")

    for _mlist in all_dst_server_idx_from_db:
        if (len(_mlist) % (NUM_FLOWS_PER_ML_QUERY) != 0):
            raise Exception(f'{len(_mlist)} not dividable by {NUM_FLOWS_PER_ML_QUERY}')

    print(all_query_ids_from_db)
    print(all_query_types_from_db)

    # These two map each query id to a dictionary and each dictionary presents
    # either the counter per dst or (src,dst) to port 
    query_id_map_counter_per_dst_dict = dict()
    query_id_map_src_dst_to_port_dict = dict()

    for agg_idx in range(NUM_AGGS):
        for server_idx in range(NUM_SERVERS_UNDER_EACH_RACK):
            # src is server_idx
            server_idx_among_all = agg_idx * NUM_SERVERS_UNDER_EACH_RACK + server_idx
            index_of_dest_servers = []
            for m_index, dst_idx in enumerate(all_dst_server_idx_from_db[server_idx_among_all]):

                # which index to read to find the query id
                query_index = int(m_index/NUM_FLOWS_PER_ML_QUERY)
                query_id = all_query_ids_from_db[server_idx_among_all][query_index]
                if (not always_generate_tree):
                    query_type = all_query_types_from_db[server_idx_among_all][query_index]
                else:
                    query_type = None
                should_generate_tree = is_multicast_based_collective(query_type, always_generate_tree)
                # print(f'should_generate_tree for {query_type} is {should_generate_tree}')
                if (not should_generate_tree):
                    continue

                if (query_id not in query_id_map_counter_per_dst_dict):
                    query_id_map_counter_per_dst_dict[query_id] = dict()
                    query_id_map_src_dst_to_port_dict[query_id] = dict()

                index_of_dest_servers.append(dst_idx)
                if (len(index_of_dest_servers) == NUM_FLOWS_PER_ML_QUERY):
                    if (server_idx_among_all not in index_of_dest_servers):
                        raise Exception(f"Server{server_idx_among_all} shold be part of the query itself!")

                    print(f'index_of_dest_servers: {index_of_dest_servers}')
                    # Filter out the source from dst server list
                    index_of_dest_servers = [x for x in index_of_dest_servers if x!=server_idx_among_all]
                    if (fragment_ratio is None and len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-NUM_GPUS):
                        raise Exception(f"len(index_of_dest_servers): {len(index_of_dest_servers)} != {NUM_FLOWS_PER_ML_QUERY-NUM_GPUS}")
                    print(f'index_of_dest_servers after filtering: {index_of_dest_servers}\n')
                    print(f'query_id for these servers: {query_id}')

                    if (num_trees_per_collective > NUM_SPINES):
                        print(f'{num_trees_per_collective} > {NUM_SPINES}')
                        raise Exception("Number of possible trees per collective can be at most the number of spines")

                    tree_port_to_leaf_indexes = random.sample(range(NUM_SPINES), num_trees_per_collective)
                    for tree_idx in range(num_trees_per_collective):
                        src_node = Node()
                        level_info = LevelInfo(src_node)
                        dst_node_achieved_set = set()
                        # these are re-written later
                        if (fragment_ratio is None and len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-NUM_GPUS):
                            print(index_of_dest_servers)
                            raise Exception("len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-1")

                        for dst_idx in index_of_dest_servers:
                            if (dst_idx not in dst_node_achieved_set):
                                create_tree(server_idx_among_all, dst_idx, level_info, query_id, query_id_map_counter_per_dst_dict, query_id_map_src_dst_to_port_dict, tree_idx, tree_port_to_leaf_indexes)
                                dst_node_achieved_set.add(dst_idx)
                        print(f'tree for query {query_id}')
                        print_tree(src_node)
                        print()
                        
                        if (len(src_node.children_list) != 1):
                            raise Exception("len(src_node.children_list) != 1")
                        route = Route(server_idx_among_all)
                        fill_route_lists(src_node, 0, route)

                        ports_to_dsts_all[server_idx_among_all] += [route.ports_to_dsts_list_of_lists]
                        jump_to_idx_all[server_idx_among_all] += [route.jump_to_idx_list_of_lists]
                    
                    index_of_dest_servers = []
            
            if (len(index_of_dest_servers) != 0):
                raise Exception("len(index_of_dest_servers) should be 0 at this point!")

    table_headers = ''
    for title in titles:
        table_headers += title + ' text, '
    if table_headers != '':
        table_headers = table_headers[:-2]

    all_inserted_row_ports_to_dest = []
    all_inserted_row_jump_to_idx = []

    if table_headers != '':

        ports_to_dest_db = sqlite3.connect(ports_to_dest_db_name)
        ports_to_dest_cursor = ports_to_dest_db.cursor()

        jump_to_idx_db = sqlite3.connect(jump_to_idx_db_name)
        jump_to_idx_cursor = jump_to_idx_db.cursor()

        ports_to_dest_cursor.execute("DROP TABLE IF EXISTS {}".format(ports_to_dest_table_name))
        ports_to_dest_cursor.execute(
            "CREATE TABLE {} ({})".format(ports_to_dest_table_name, table_headers))

        jump_to_idx_cursor.execute("DROP TABLE IF EXISTS {}".format(jump_to_idx_table_name))
        jump_to_idx_cursor.execute(
            "CREATE TABLE {} ({})".format(jump_to_idx_table_name, table_headers))

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

        command_ports_to_dest = "INSERT INTO {} VALUES (".format(ports_to_dest_table_name)
        for i in range(len(inserted_row_ports_to_dest)):
            command_ports_to_dest += '?,'
        command_ports_to_dest = command_ports_to_dest[:-1] + ')'

        command_jump_to_idx = "INSERT INTO {} VALUES (".format(jump_to_idx_table_name)
        for i in range(len(inserted_row_jump_to_idx)):
            command_jump_to_idx += '?,'
        command_jump_to_idx = command_jump_to_idx[:-1] + ')'

        if (len(all_inserted_row_ports_to_dest) > 0):
            ports_to_dest_cursor.executemany(command_ports_to_dest, all_inserted_row_ports_to_dest)
        ports_to_dest_db.commit()
        ports_to_dest_db.close()

        if (len(all_inserted_row_jump_to_idx) > 0):
            jump_to_idx_cursor.executemany(command_jump_to_idx, all_inserted_row_jump_to_idx)
        jump_to_idx_db.commit()
        jump_to_idx_db.close()
        
    print('------------------------------------')

    server_idx_connection.close()
    query_ids_connection.close()


inter_arrival_x, inter_arrival_y = read_file_from_memory(ML_INTER_ARRIVAL_DIST_FILE_NAME)

if constant_load:
    print('Constant load')
    with mp.Pool(processes=MAX_THREAD_NUM) as p:
        inputs = []
        for rep_num in range(REP_NUM):
            for num_trees_per_collective in LB_TREE_PER_COLLECTIVE_LIST:
                for fragment_ratio in FRAGMENT_RATIOS:
                    multiplier_idx = -1
                    for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                        for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                            if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                                raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                            multiplier_idx += 1
                            ML_INTER_ARRIVAL_MULTIPLIER = ML_INTER_ARRIVAL_MULTIPLIER_LIST[multiplier_idx]
                            inputs.append((rep_num,
                                                ML_INTER_ARRIVAL_MULTIPLIER,
                                                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective, fragment_ratio))
        p.starmap(fill_tables, inputs)
else:
    print('Changing load')
    with mp.Pool(processes=MAX_THREAD_NUM) as p:
        inputs = []
        for rep_num in range(REP_NUM):
            for num_trees_per_collective in LB_TREE_PER_COLLECTIVE_LIST:
                for fragment_ratio in FRAGMENT_RATIOS:
                    for ML_INTER_ARRIVAL_MULTIPLIER in ML_INTER_ARRIVAL_MULTIPLIER_LIST:
                        for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                            for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                                if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                                    raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                                inputs.append((rep_num,
                                                    ML_INTER_ARRIVAL_MULTIPLIER,
                                                    NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective, fragment_ratio))
        p.starmap(fill_tables, inputs)
print("All threads are over!")
