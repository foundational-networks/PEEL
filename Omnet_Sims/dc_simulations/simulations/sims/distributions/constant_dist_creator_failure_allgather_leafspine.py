import sqlite3
import numpy as np
import csv
import random
import multiprocessing as mp
from Variables import *


class Node:
    def __init__(self, id):
        self.id = id
        self.children_list = []
        self.children_port_idx_list = []

def print_tree(node, failed_link_set, level=0):
    if not node:
        return
    # Print the current node with indentation based on its depth
    print(" " * (level * 4) + f"Node({node.id}) --> ports: {node.children_port_idx_list}")
    # Recursively print each child
    for child in node.children_list:
        if (f'{node.id}{child.id}' in failed_link_set or \
            f'{child.id}{node.id}' in failed_link_set):
            raise Exception(f"{node.id} should not be connected to {child.id}")
        print_tree(child, failed_link_set, level + 1)

class Route:
    def __init__(self, ID):
        self.ID = ID
        self.ports_to_dsts_list_of_lists = [[]]
        self.jump_to_idx_list_of_lists = [[]]
        self.last_idx = 1

def create_tree(src_idx:int, dst_idx_list, failed_link_set, \
    query_id, query_id_map_counter_per_dst_dict, query_id_map_src_dst_to_port_dict, \
        LOOK_AHEAD, counter_list):

    if (query_id not in query_id_map_counter_per_dst_dict):
        raise Exception("How is query_id not in query_id_map_counter_per_dst_dict")
    # get the dicts for a specific query id
    counter_per_dst_dict = query_id_map_counter_per_dst_dict[query_id]
    src_dst_to_port_dict = query_id_map_src_dst_to_port_dict[query_id]

    # first create the destination leaf nodes and the destination nodes under them
    universe_leaf_dict = dict()   # mapping leaf idx to the leaf node
    universe_leaf_num = 0
    # list of unique dst server idxes that act as our universe for greedy algo
    universe_servers = list(set(dst_idx_list))
    for dst_idx in universe_servers:
        dst_server_node = Node(f'se{dst_idx}')
        dst_leaf_idx = int(dst_idx / NUM_SERVERS_UNDER_EACH_RACK)
        if (dst_leaf_idx not in universe_leaf_dict):
            universe_leaf_dict[dst_leaf_idx] = Node(f'l{dst_leaf_idx}')
            universe_leaf_num += 1
        # port idx for servers under a rack
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
        # port_idx = (dst_idx % NUM_SERVERS_UNDER_EACH_RACK)*NUM_GPUS + random.randint(0, NUM_GPUS-1)
        port_idx = (dst_idx % NUM_SERVERS_UNDER_EACH_RACK)*NUM_GPUS + addition_port_idx
        universe_leaf_dict[dst_leaf_idx].children_list.append(dst_server_node)
        universe_leaf_dict[dst_leaf_idx].children_port_idx_list.append(port_idx)
        

    # calculate the number of dst leafs accessible from each spine
    src_leaf_idx = int(src_idx / NUM_SERVERS_UNDER_EACH_RACK)
    dst_leaf_coverage_list_per_spine = []   # list of sets which indicates what set of dst leafs each spine covers

    for spine_idx in range(NUM_SPINES):
        coverage = set()
        if (f'l{src_leaf_idx}sp{spine_idx}' not in failed_link_set):
            for dst_leaf_idx in universe_leaf_dict:
                if (src_leaf_idx != dst_leaf_idx):
                    # if there is a link from source leaf to this core and from this core to dst leaf
                    if (f'l{dst_leaf_idx}sp{spine_idx}' not in failed_link_set):
                        coverage.add(dst_leaf_idx)
        dst_leaf_coverage_list_per_spine.append(coverage)

    remaining_dst_leafs = set(universe_leaf_dict.keys())
    # remove src leaf
    remaining_dst_leafs -= {src_leaf_idx}

    final_chosen_spines = []
    print(f'src_leaf_idx: {src_leaf_idx}')
    while (len(remaining_dst_leafs) > 0):
        best_spines_to_choose = []
        best_coverage_len = 0
        # we want to achieve something with lower lookahead nums
        best_lookahead_num = LOOK_AHEAD + 1

        print(f'remaining_dst_leafs: {remaining_dst_leafs}')
        print(f'dst_leaf_coverage_list_per_spine: {dst_leaf_coverage_list_per_spine}')

        for spine_idx, coverred_leafs in enumerate(dst_leaf_coverage_list_per_spine):
            tmp_remaining_dst_leafs = set(remaining_dst_leafs)
            
            coverage = tmp_remaining_dst_leafs & coverred_leafs
            coverage_len = len(coverage)

            # how many lookaheads are used for this coverage
            num_look_ahead = 0

            if (coverage_len == 0):
                continue

            tmp_remaining_dst_leafs -= coverage

            if (LOOK_AHEAD > 0):
                for _ in range(LOOK_AHEAD):
                    if (len(tmp_remaining_dst_leafs) == 0):
                        break
                    coverage = max(dst_leaf_coverage_list_per_spine, key=lambda x: len(tmp_remaining_dst_leafs & x))
                    coverage_len += len(tmp_remaining_dst_leafs & coverage)
                    tmp_remaining_dst_leafs -= coverage
                    num_look_ahead += 1

            if (coverage_len > best_coverage_len):
                best_coverage_len = coverage_len
                best_spines_to_choose = [spine_idx]
                best_lookahead_num = num_look_ahead
            elif (coverage_len == best_coverage_len):
                # if best_lookahead_num < num_look_ahead, we already achieved better results without addidng any extra sets
                if (best_lookahead_num == num_look_ahead):
                    best_spines_to_choose.append(spine_idx)
                if (num_look_ahead < best_lookahead_num):
                    # we achieved similar results with lower lookaheads (lower added subsets)
                    best_spines_to_choose = [spine_idx]
                    best_lookahead_num = num_look_ahead
        
        if (len(best_spines_to_choose) == 0):
            raise Exception("How is len(best_spines_to_choose) == 0")

        # random_idx = random.randint(0, len(best_spines_to_choose)-1)
        print(f'Options for spine choices: {best_spines_to_choose}')
        print(f'counter_list[0]: {counter_list[0]}')
        random_idx = 0
        if (len(best_spines_to_choose) > 1):
            random_idx = counter_list[0] % len(best_spines_to_choose)
            counter_list[0] += 1
        chosen_spine_idx = best_spines_to_choose[random_idx]
        final_chosen_spines.append(chosen_spine_idx)
        print(f'choosing spine {chosen_spine_idx}')

        final_coverage = remaining_dst_leafs & dst_leaf_coverage_list_per_spine[chosen_spine_idx]
        remaining_dst_leafs -= final_coverage
        print('--------------------------')

    print(f'final_chosen_spines are {final_chosen_spines}')
    added_spines_list_mapper = dict()
    added_leaf_set = set()
    for spine_idx in final_chosen_spines:
        spine_node = Node(f'sp{spine_idx}')
        for leaf_idx in dst_leaf_coverage_list_per_spine[spine_idx]:
            if (leaf_idx not in added_leaf_set):
                spine_node.children_list.append(universe_leaf_dict[leaf_idx])
                spine_node.children_port_idx_list.append(leaf_idx)
                added_leaf_set.add(leaf_idx)
        added_spines_list_mapper[spine_idx] = spine_node
    
    # complete the tree
    src_node = Node(f'se{src_idx}')
    if (src_leaf_idx in universe_leaf_dict):
        src_leaf_node = universe_leaf_dict[src_leaf_idx]
    else:
        src_leaf_node = Node(f'l{src_leaf_idx}')
    # port to get from server to leaf that can go from any of the gpus
    # choose the source gpu at random
    src_node.children_list.append(src_leaf_node)
    src_node.children_port_idx_list.append(random.randint(0, NUM_GPUS-1))

    for spine_idx in added_spines_list_mapper:
        new_node_port_idx = NUM_SERVERS_UNDER_EACH_RACK * NUM_GPUS + spine_idx
        src_leaf_node.children_list.append(added_spines_list_mapper[spine_idx])
        src_leaf_node.children_port_idx_list.append(new_node_port_idx)

    return src_node

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

def get_failed_link_set(FAIL_PERCENTAGE):
    total_num_links = NUM_AGGS * NUM_SPINES
    failed_links = 0
    failed_link_set = set()
    for leaf_idx in range(NUM_AGGS):
        for spine_idx in range(NUM_SPINES):
            fail_prob = random.randint(0, 100)
            if (fail_prob < FAIL_PERCENTAGE):
                # failed
                failed_link_set.add(f'l{leaf_idx}sp{spine_idx}')
                failed_link_set.add(f'sp{spine_idx}l{leaf_idx}')
                failed_links += 1
    print(f'Expected failure prob: {FAIL_PERCENTAGE}, actual failure prob: {failed_links/total_num_links}')
    return failed_link_set

def fill_tables(rep_num, ML_INTER_ARRIVAL_MULTIPLIER,
                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, failed_link_set, LOOK_AHEAD, FAIL_PERCENTAGE):
    print(
        "IA = {}, SCALE = {}, FS = {}, failed_links_percentage = {}%, LOOK_AHEAD = {}".format(ML_INTER_ARRIVAL_MULTIPLIER, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, len(failed_link_set)/2*100/(NUM_AGGS*NUM_SPINES), LOOK_AHEAD))
    TARGET_NUM_ML_QUERIES = TARGET_NUM_ML_QUERIES_LIST[
        ML_INTER_ARRIVAL_MULTIPLIER_LIST.index(ML_INTER_ARRIVAL_MULTIPLIER)]

    ports_to_dsts_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)] # list of lists of ports to which the packet should be mcasted
    jump_to_idx_all = [[] for i in range(NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK)]   # lists of lists of indexes that the next switch should consider

    titles = []
    counter_list = [0]
    
    # if (NUM_FLOWS_PER_ML_QUERY >= (NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK * NUM_GPUS)):
    #     raise Exception("The number of destinations surpasses the possible destinations. Max is NUM_AGGS * NUM_SERVERS_UNDER_EACH_RACK - 1")

    flow_size_mult_str = '{:.6f}'.format(ML_FLOW_SIZE_MULTIPLIER)
    inter_arrival_mult_str = '{:.6f}'.format(ML_INTER_ARRIVAL_MULTIPLIER)
    server_idx_db_name = '{}_allgather_dst_server_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    ports_to_dest_db_name = '{}_allgather_ports_to_dst_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}_failperc_{}_lookahead_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, FAIL_PERCENTAGE, LOOK_AHEAD, rep_num)
    jump_to_idx_db_name = '{}_allgather_jump_to_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}_failperc_{}_lookahead_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, FAIL_PERCENTAGE, LOOK_AHEAD, rep_num)
    query_ids_db_name = '{}_allgather_query_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    failed_links_file_name = '{}_allgather_failed_links_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}_failperc_{}.txt' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, FAIL_PERCENTAGE, rep_num)
                
    server_idx_table_name = "dst_server_idx"
    ports_to_dest_table_name = "ports_to_dst"
    jump_to_idx_table_name = "jump_to_idx"
    query_ids_table_name = "query_ids"

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

    all_dst_server_idx_from_db = []
    all_query_ids_from_db = []

    if (NUM_ML_APP_PER_SERVER > 1):
        raise Exception("This code is created for NUM_ML_APP_PER_SERVER == 1")

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
                rows = server_idx_cursor.fetchall()
                tmp_server_idx_from_db = [int(item[0]) for item in rows if item[0].isdigit()]
                rows = query_ids_cursor.fetchall()
                tmp_query_idx_from_db = [int(item[0]) for item in rows if item[0].isdigit()]

                if (len(tmp_server_idx_from_db) != 0):
                    if (len(tmp_server_idx_from_db) % len(tmp_query_idx_from_db) != 0):
                        raise Exception("len(tmp_server_idx_from_db) % len(tmp_query_idx_from_db) must be zero")
                    if (int(len(tmp_server_idx_from_db) / len(tmp_query_idx_from_db)) != NUM_FLOWS_PER_ML_QUERY):
                        raise Exception("len(tmp_server_idx_from_db) / len(tmp_query_idx_from_db) must be equal to NUM_FLOWS_PER_ML_QUERY")
                
                all_dst_server_idx_from_db.append(tmp_server_idx_from_db)
                all_query_ids_from_db.append(tmp_query_idx_from_db)
                titles.append(m_title)
    
    if (len(all_dst_server_idx_from_db) != NUM_AGGS*NUM_SERVERS_UNDER_EACH_RACK):
        raise Exception("len(all_dst_server_idx_from_db) != NUM_AGGS*NUM_SERVERS_UNDER_EACH_RACK")

    for _mlist in all_dst_server_idx_from_db:
        if (len(_mlist) % (NUM_FLOWS_PER_ML_QUERY) != 0):
            raise Exception(f'{len(_mlist)} not dividable by {NUM_FLOWS_PER_ML_QUERY}')
    
    # Open the file in write mode ('w') to overwrite if it exists
    with open(failed_links_file_name, "w") as file:
        for link in failed_link_set:
            file.write(link + "\n")  # Write each link followed by a newline


    # These two map each query id to a dictionary and each dictionary presents
    # either the counter per dst or (src,dst) to port 
    query_id_map_counter_per_dst_dict = dict()
    query_id_map_src_dst_to_port_dict = dict()

    for agg_idx in range(NUM_AGGS):
        for server_idx in range(NUM_SERVERS_UNDER_EACH_RACK):
            # src is server_idx
            server_idx_among_all = agg_idx * NUM_SERVERS_UNDER_EACH_RACK + server_idx
            # if (server_idx_among_all != 0):
            #     continue
            index_of_dest_servers = []
            for m_index, dst_idx in enumerate(all_dst_server_idx_from_db[server_idx_among_all]):

                # which index to read to find the query id
                query_index = int(m_index/NUM_FLOWS_PER_ML_QUERY)
                query_id = all_query_ids_from_db[server_idx_among_all][query_index]
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
                    if (len(index_of_dest_servers) != NUM_FLOWS_PER_ML_QUERY-NUM_GPUS):
                        raise Exception(f"len(index_of_dest_servers): {len(index_of_dest_servers)} != {NUM_FLOWS_PER_ML_QUERY-NUM_GPUS}")
                    print(f'index_of_dest_servers after filtering: {index_of_dest_servers}\n')
                    print(f'query_id for these servers: {query_id}')

                    # one query is done
                    src_node = create_tree(server_idx_among_all, index_of_dest_servers, failed_link_set, \
                        query_id, query_id_map_counter_per_dst_dict, query_id_map_src_dst_to_port_dict, LOOK_AHEAD, \
                        counter_list)
                    print_tree(src_node, failed_link_set)

                    if (len(src_node.children_list) != 1):
                        raise Exception("len(src_node.children_list) != 1")
                    route = Route(server_idx_among_all)
                    fill_route_lists(src_node, 0, route)

                    # Here, I'm not excluding the participant_idx because I need it in tree allreduce, 
                    # I will exclude it in Omnet++ code
                    ports_to_dsts_all[server_idx_among_all] += [route.ports_to_dsts_list_of_lists]
                    jump_to_idx_all[server_idx_among_all] += [route.jump_to_idx_list_of_lists]
                    index_of_dest_servers = []

                    # print(f'Source: server[{server_idx_among_all}]')
                    # print(f'Dsts: {index_of_dest_servers}')
                    # print(f'Route:\n\tports_to_dsts_list_of_lists: {route.ports_to_dsts_list_of_lists}\n\tjump_to_idx_list_of_lists: {route.jump_to_idx_list_of_lists}')
                    # print('-----------------------------\n')
                    # raise Exception("oh")

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


        command_ports_to_dest = "INSERT INTO {} VALUES (".format(ports_to_dest_table_name)
        for i in range(len(inserted_row_ports_to_dest)):
            command_ports_to_dest += '?,'
        command_ports_to_dest = command_ports_to_dest[:-1] + ')'

        command_jump_to_idx = "INSERT INTO {} VALUES (".format(jump_to_idx_table_name)
        for i in range(len(inserted_row_jump_to_idx)):
            command_jump_to_idx += '?,'
        command_jump_to_idx = command_jump_to_idx[:-1] + ')'

        ports_to_dest_cursor.executemany(command_ports_to_dest, all_inserted_row_ports_to_dest)
        ports_to_dest_db.commit()
        ports_to_dest_db.close()

        jump_to_idx_cursor.executemany(command_jump_to_idx, all_inserted_row_jump_to_idx)
        jump_to_idx_db.commit()
        jump_to_idx_db.close()
        
    print('------------------------------------')

    server_idx_connection.close()
    query_ids_connection.close()


for FAIL_PERCENTAGE in FAIL_PERCENTAGE_LIST:
    failed_link_set = get_failed_link_set(FAIL_PERCENTAGE)

    # # this is for the toy example
    # failed_link_set = {'l1sp1', 'l1sp2', 'sp1l1', 'sp2l1', 'l2sp2', 'sp2l2', 
    #     'l3sp2', 'sp2l3', 'l4sp0', 'sp0l4', 'l5sp0', 'sp0l5', 
    #     'l6sp0', 'l6sp1', 'sp0l6', 'sp1l6'}


    print(f'Failed links are {failed_link_set}\n')

    with mp.Pool(processes=MAX_THREAD_NUM) as p:
        inputs = []
        for rep_num in range(REP_NUM):
            for ML_INTER_ARRIVAL_MULTIPLIER in ML_INTER_ARRIVAL_MULTIPLIER_LIST:
                for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                    for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                        if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                            raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                        for LOOK_AHEAD in LOOK_AHEAD_LIST:
                            inputs.append((rep_num,
                                                ML_INTER_ARRIVAL_MULTIPLIER,
                                                NUM_FLOWS_PER_ML_QUERY, 
                                                ML_FLOW_SIZE,
                                                set(failed_link_set),
                                                LOOK_AHEAD,
                                                FAIL_PERCENTAGE))
        p.starmap(fill_tables, inputs)
    print(f"All threads are over for failure percentage: {FAIL_PERCENTAGE}!")
