import sqlite3
import threading
from Variables import *
import numpy as np


def get_none_deflection_identifiers():
    deflection_identifiers = []
    for i in range(NUM_AGGS):
        deflection_identifiers.append(0)
    for i in range(NUM_SPINES):
        deflection_identifiers.append(0)
    return deflection_identifiers


def get_all_deflection_identifiers():
    deflection_identifiers = []
    for i in range(NUM_AGGS):
        deflection_identifiers.append(1)
    for i in range(NUM_SPINES):
        deflection_identifiers.append(1)
    return deflection_identifiers


def get_leafs_deflection_identifiers():
    deflection_identifiers = []
    for i in range(NUM_AGGS):
        deflection_identifiers.append(1)
    for i in range(NUM_SPINES):
        deflection_identifiers.append(0)
    return deflection_identifiers


def get_spines_deflection_identifiers():
    deflection_identifiers = []
    for i in range(NUM_AGGS):
        deflection_identifiers.append(0)
    for i in range(NUM_SPINES):
        deflection_identifiers.append(1)
    return deflection_identifiers


def get_leafs_random_deflection_identifiers():
    if RANDOM_SWITCH_NUM > NUM_AGGS:
        raise Exception("RANDOM_SWITCH_NUM > NUM_AGGS")
    print("Randomly enabling {} leafs for deflection".format(RANDOM_SWITCH_NUM))
    deflection_indexes = np.random.choice(NUM_AGGS,
                                 size=RANDOM_SWITCH_NUM,
                                 replace=False)
    deflection_identifiers = [0 for i in range(NUM_AGGS + NUM_SPINES)]
    for i in deflection_indexes:
        deflection_identifiers[i] = 1
    return deflection_identifiers


def get_spines_random_deflection_identifiers():
    if RANDOM_SWITCH_NUM > NUM_SPINES:
        raise Exception("RANDOM_SWITCH_NUM > NUM_SPINES")
    print("Randomly enabling {} spines for deflection".format(RANDOM_SWITCH_NUM))
    deflection_indexes = np.random.choice(NUM_SPINES,
                                 size=RANDOM_SWITCH_NUM,
                                 replace=False)
    # Increase the indexes by num aggs because aggs come first in the list and
    # we are only dealing with spines
    for i in range(len(deflection_indexes)):
        deflection_indexes[i] += NUM_AGGS
    deflection_identifiers = [0 for i in range(NUM_AGGS + NUM_SPINES)]
    for i in deflection_indexes:
        deflection_identifiers[i] = 1
    return deflection_identifiers


def get_random_deflection_identifiers():
    if RANDOM_SWITCH_NUM > NUM_AGGS + NUM_SPINES:
        raise Exception("RANDOM_SWITCH_NUM > NUM_AGGS + NUM_SPINES")
    print("Randomly enabling {} switches for deflection".format(RANDOM_SWITCH_NUM))
    deflection_indexes = np.random.choice(NUM_AGGS + NUM_SPINES,
                                 size=RANDOM_SWITCH_NUM,
                                 replace=False)
    deflection_identifiers = [0 for i in range(NUM_AGGS + NUM_SPINES)]
    for i in deflection_indexes:
        deflection_identifiers[i] = 1
    return deflection_identifiers


def get_partitioned_graph_deflection_identifiers():
    if PARTITION_LEAF_NUM + PARTITION_SPINE_NUM > NUM_AGGS + NUM_SPINES:
        raise Exception("RANDOM_SWITCH_NUM > NUM_AGGS + NUM_SPINES")
    print("Partitioning graph. Randomly enabling {} leafs and {} spines for deflection".format(PARTITION_LEAF_NUM, PARTITION_SPINE_NUM))
    deflection_indexes_leafs = np.random.choice(NUM_AGGS,
                                          size=PARTITION_LEAF_NUM,
                                          replace=False)
    deflection_indexes_spines = np.random.choice(NUM_SPINES,
                                                size=PARTITION_SPINE_NUM,
                                                replace=False)
    deflection_identifiers = [0 for i in range(NUM_AGGS + NUM_SPINES)]
    for i in deflection_indexes_leafs:
        deflection_identifiers[i] = 1
    for i in deflection_indexes_spines:
        deflection_identifiers[NUM_AGGS+i] = 1
    return deflection_identifiers




def get_deflection_identifiers():
    print("Enabling leafs for deflection")
    deflection_identifiers = None
    if LS_DEFLECT_POINTS == 'all':
        deflection_identifiers = get_all_deflection_identifiers()
    if LS_DEFLECT_POINTS == 'none':
        deflection_identifiers = get_none_deflection_identifiers()
    elif LS_DEFLECT_POINTS == 'leafs':
        deflection_identifiers = get_leafs_deflection_identifiers()
    elif LS_DEFLECT_POINTS == 'spines':
        deflection_identifiers = get_spines_deflection_identifiers()
    elif LS_DEFLECT_POINTS == 'random':
        deflection_identifiers = get_random_deflection_identifiers()
    elif LS_DEFLECT_POINTS == 'leafs_random':
        deflection_identifiers = get_leafs_random_deflection_identifiers()
    elif LS_DEFLECT_POINTS == 'spines_random':
        deflection_identifiers = get_spines_random_deflection_identifiers()
    elif LS_DEFLECT_POINTS == 'partition_graph':
        deflection_identifiers = get_partitioned_graph_deflection_identifiers()
    print(deflection_identifiers)
    return deflection_identifiers


def fill_tables(rep_num):
    column_names = ['agg{}'.format(i) for i in range(NUM_AGGS)] + \
                   ['spine{}'.format(j) for j in range(NUM_SPINES)]
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
        if 'random' in LS_DEFLECT_POINTS:
            deflection_identifiers_db_name = '{}_spines_{}_leafs_{}_servers_{}_{}_{}_rep.db' \
                .format(NUM_SPINES, NUM_AGGS, NUM_SERVERS_UNDER_EACH_RACK, LS_DEFLECT_POINTS, RANDOM_SWITCH_NUM, rep_num)
        if 'partition_graph' in LS_DEFLECT_POINTS:
            deflection_identifiers_db_name = '{}_spines_{}_leafs_{}_servers_{}_{}_{}_{}_rep.db' \
                .format(NUM_SPINES, NUM_AGGS, NUM_SERVERS_UNDER_EACH_RACK, LS_DEFLECT_POINTS, PARTITION_LEAF_NUM, PARTITION_SPINE_NUM, rep_num)
        else:
            deflection_identifiers_db_name = '{}_spines_{}_leafs_{}_servers_{}_{}_rep.db' \
                .format(NUM_SPINES, NUM_AGGS, NUM_SERVERS_UNDER_EACH_RACK, LS_DEFLECT_POINTS, rep_num)

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


if LS_DEFLECT_POINTS not in LS_DEFLECT_POINT_OPTIONS:
    raise Exception("Unknown deflection point...")

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