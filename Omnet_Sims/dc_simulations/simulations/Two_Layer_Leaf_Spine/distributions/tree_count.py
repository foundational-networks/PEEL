from itertools import count
import sqlite3
import numpy as np
import csv
import random
import multiprocessing as mp
from Variables import *



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

def fill_tables(rep_num, ML_INTER_ARRIVAL_MULTIPLIER,
                NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective=1):
    print(
        "IA = {}, SCALE = {}, FS = {}, num_trees_per_collective={}".format(ML_INTER_ARRIVAL_MULTIPLIER, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective))
    TARGET_NUM_ML_QUERIES = TARGET_NUM_ML_QUERIES_LIST[
        ML_INTER_ARRIVAL_MULTIPLIER_LIST.index(ML_INTER_ARRIVAL_MULTIPLIER)]

    titles = []

    flow_size_mult_str = '{:.6f}'.format(ML_FLOW_SIZE_MULTIPLIER)
    inter_arrival_mult_str = '{:.6f}'.format(ML_INTER_ARRIVAL_MULTIPLIER)
    server_idx_db_name = '{}_allgather_dst_server_idx_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    query_ids_db_name = '{}_allgather_query_ids_{}_flowmult_{}_intermult_{}_flows_per_ml_query_{}_flow_size_{}_gpus_{}.db' \
        .format(ML_CATEGORY, flow_size_mult_str, inter_arrival_mult_str, NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE,
                NUM_GPUS, rep_num)
    
    server_idx_table_name = "dst_server_idx"
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
    tree_count = 0
    query_set = set()

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

    for agg_idx in range(NUM_AGGS):
        for server_idx in range(NUM_SERVERS_UNDER_EACH_RACK):
            # src is server_idx
            server_idx_among_all = agg_idx * NUM_SERVERS_UNDER_EACH_RACK + server_idx
            index_of_dest_servers = []
            for m_index, dst_idx in enumerate(all_dst_server_idx_from_db[server_idx_among_all]):

                # which index to read to find the query id
                query_index = int(m_index/NUM_FLOWS_PER_ML_QUERY)
                query_id = all_query_ids_from_db[server_idx_among_all][query_index]

                # which index to read to find the query id
                query_index = int(m_index/NUM_FLOWS_PER_ML_QUERY)
                query_id = all_query_ids_from_db[server_idx_among_all][query_index]
                query_set.add(query_id)

                index_of_dest_servers.append(dst_idx)
                if (len(index_of_dest_servers) == NUM_FLOWS_PER_ML_QUERY):
                    tree_count += 1
                    
                    index_of_dest_servers = []
            
            if (len(index_of_dest_servers) != 0):
                raise Exception("len(index_of_dest_servers) should be 0 at this point!")
    print(f'{len(query_set)} queries -- total of {tree_count} trees')
    print('------------------------------------')

    server_idx_connection.close()
    query_ids_connection.close()


inter_arrival_x, inter_arrival_y = read_file_from_memory(ML_INTER_ARRIVAL_DIST_FILE_NAME)

with mp.Pool(processes=MAX_THREAD_NUM) as p:
    inputs = []
    for rep_num in range(REP_NUM):
        for num_trees_per_collective in LB_TREE_PER_COLLECTIVE_LIST:
            for ML_INTER_ARRIVAL_MULTIPLIER in ML_INTER_ARRIVAL_MULTIPLIER_LIST:
                for NUM_FLOWS_PER_ML_QUERY in NUM_FLOWS_PER_ML_QUERY_LIST:
                    for ML_FLOW_SIZE in ML_FLOW_SIZE_LIST:
                        if (NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0):
                            raise Exception("NUM_FLOWS_PER_ML_QUERY % NUM_GPUS != 0")
                        inputs.append((rep_num,
                                            ML_INTER_ARRIVAL_MULTIPLIER,
                                            NUM_FLOWS_PER_ML_QUERY, ML_FLOW_SIZE, num_trees_per_collective))
    p.starmap(fill_tables, inputs)
print("All threads are over!")
