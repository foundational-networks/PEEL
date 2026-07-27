import sqlite3
import threading
from Variables import *
import numpy as np
import random


NUM_EDGES = None
NUM_AGGS = None
NUM_CORES = None
NUM_SERVERS_UNDER_EACH_EDGE = None

def get_edges_deflection_identifiers():
    print("Enabling edges for deflection")
    deflection_identifiers = []
    for i in range(NUM_EDGES):
        deflection_identifiers.append(1)
    for i in range(NUM_AGGS):
        deflection_identifiers.append(0)
    for i in range(NUM_CORES):
        deflection_identifiers.append(0)
    return deflection_identifiers

def get_aggs_deflection_identifiers():
    print("Enabling aggs for deflection")
    deflection_identifiers = []
    for i in range(NUM_EDGES):
        deflection_identifiers.append(0)
    for i in range(NUM_AGGS):
        deflection_identifiers.append(1)
    for i in range(NUM_CORES):
        deflection_identifiers.append(0)
    return deflection_identifiers

def get_cores_deflection_identifiers():
    print("Enabling cores for deflection")
    deflection_identifiers = []
    for i in range(NUM_EDGES):
        deflection_identifiers.append(0)
    for i in range(NUM_AGGS):
        deflection_identifiers.append(0)
    for i in range(NUM_CORES):
        deflection_identifiers.append(1)
    return deflection_identifiers



def get_partitioned_graph_deflection_identifiers():
    # The PARTITION_EDGE_NUM and PARTITION_AGG_NUM are per pod
    if NUM_EDGES % K != 0 or NUM_AGGS % K != 0:
        raise Exception("NUM_EDGES % K != 0 or NUM_AGGS % K != 0")
    if PARTITION_EDGE_NUM + PARTITION_AGG_NUM + PARTITION_CORE_NUM > int(NUM_EDGES/K) + int(NUM_AGGS)/K + NUM_CORES:
        raise Exception("PARTITION_EDGE_NUM + PARTITION_AGG_NUM + PARTITION_CORE_NUM > NUM_EDGES/K + NUM_AGGS/K + NUM_CORES")
    print("Partitioning graph. Randomly enabling {} edges, {} aggs, and {} cores for deflection".format(PARTITION_EDGE_NUM, PARTITION_AGG_NUM, PARTITION_CORE_NUM))
    deflection_indexes_edges = []
    for i in range(K):
        arr = [0] * int(NUM_EDGES/K)
        indices = random.sample(range(int(NUM_EDGES/K)), PARTITION_EDGE_NUM)

        for index in indices:
            arr[index] = 1
        deflection_indexes_edges += arr
    print(deflection_indexes_edges)
    deflection_indexes_aggs = []
    for i in range(K):
        arr = [0] * int(NUM_AGGS/K)
        indices = random.sample(range(int(NUM_AGGS/K)), PARTITION_AGG_NUM)

        for index in indices:
            arr[index] = 1
        deflection_indexes_aggs += arr
    print(deflection_indexes_aggs)
    deflection_indexes_cores = [0] * NUM_CORES
    indices = random.sample(range(NUM_CORES), PARTITION_CORE_NUM)

    for index in indices:
        deflection_indexes_cores[index] = 1
    print(deflection_indexes_cores)
    deflection_identifiers = deflection_indexes_edges + deflection_indexes_aggs + deflection_indexes_cores
    return deflection_identifiers




def get_deflection_identifiers():
    deflection_identifiers = None
    if FT_DEFLECT_POINTS == 'edges':
        deflection_identifiers = get_edges_deflection_identifiers()
    elif FT_DEFLECT_POINTS == 'aggs':
        deflection_identifiers = get_aggs_deflection_identifiers()
    elif FT_DEFLECT_POINTS == 'cores':
        deflection_identifiers = get_cores_deflection_identifiers()
    elif FT_DEFLECT_POINTS == 'partition_graph':
        deflection_identifiers = get_partitioned_graph_deflection_identifiers()
    print(deflection_identifiers)
    return deflection_identifiers


def fill_tables(rep_num):
    column_names = ['edge{}'.format(i) for i in range(NUM_EDGES)] + \
                   ['agg{}'.format(i) for i in range(NUM_AGGS)] + \
                   ['core{}'.format(j) for j in range(NUM_CORES)]
    deflection_identifiers = []
    all_inserted_row_deflection_identifiers = []
    deflection_identifiers = get_deflection_identifiers()

    table_headers = ''
    for title in column_names:
        table_headers += title + ' text, '
    if table_headers != '':
        table_headers = table_headers[:-2]

    if table_headers != '':

        deflection_identifiers_table_name = "deflection_identifiers"
        if 'partition_graph' in FT_DEFLECT_POINTS:
            deflection_identifiers_db_name = '{}_cores_{}_aggs_{}_edges_{}_servers_{}_{}_{}_{}_{}_rep.db' \
                .format(NUM_CORES, NUM_AGGS, NUM_EDGES, NUM_SERVERS_UNDER_EACH_EDGE, FT_DEFLECT_POINTS, PARTITION_EDGE_NUM, PARTITION_AGG_NUM, PARTITION_CORE_NUM, rep_num)
        else:
            deflection_identifiers_db_name = '{}_cores_{}_aggs_{}_edges_{}_servers_{}_{}_rep.db' \
                .format(NUM_CORES, NUM_AGGS, NUM_EDGES, NUM_SERVERS_UNDER_EACH_EDGE, FT_DEFLECT_POINTS, rep_num)

        deflection_identifiers_db = sqlite3.connect(deflection_identifiers_db_name)

        deflection_identifiers_cursor = deflection_identifiers_db.cursor()

        deflection_identifiers_cursor.execute("DROP TABLE IF EXISTS {}".format(deflection_identifiers_table_name))
        deflection_identifiers_cursor.execute(
            "CREATE TABLE {} ({})".format(deflection_identifiers_table_name, table_headers))

        all_inserted_row_deflection_identifiers.append(tuple(deflection_identifiers))

        command_deflection_identifiers = "INSERT INTO {} VALUES (".format(deflection_identifiers_table_name)
        for i in range(len(deflection_identifiers)):
            command_deflection_identifiers += '?,'
        command_deflection_identifiers = command_deflection_identifiers[:-1] + ')'

        deflection_identifiers_cursor.executemany(command_deflection_identifiers,
                                                  all_inserted_row_deflection_identifiers)

        deflection_identifiers_db.commit()
        deflection_identifiers_db.close()


if FT_DEFLECT_POINTS not in FT_DEFLECT_POINT_OPTIONS:
    raise Exception("Unknown deflection point...")


NUM_EDGES = int((K**2)/2)
NUM_AGGS = NUM_EDGES
NUM_CORES = int((K**2)/4)
NUM_SERVERS_UNDER_EACH_EDGE = int(K/2)

class myThread (threading.Thread):
    def __init__(self, rep_num):
        threading.Thread.__init__(self)
        self.rep_num = rep_num
    def run(self):
        print("Starting thread for repnum: {}".format(self.rep_num))
        fill_tables(self.rep_num)
        print("Exiting thread for repnum: {}".format(self.rep_num))

threads = []
for rep_num in range(REP_NUM):
    try:
        m_thread = myThread(rep_num)
        m_thread.start()
        threads.append(m_thread)
    except:
        print("Error: unable to start thread")

for m_thread in threads:
   m_thread.join()


print("All threads are over!")