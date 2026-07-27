import sqlite3
from Variables import *
import os.path
from os import path

read_idx = 0

background_flow_size_mult_str = '{:.6f}'.format(BG_FLOW_SIZE_MULTIPLIER)
background_inter_arrival_mult_str = '{:.6f}'.format(BG_INTER_ARRIVAL_MULTIPLIER)
bursty_flow_size_mult_str = '{:.6f}'.format(BURSTY_FLOW_SIZE_MULTIPLIER)
bursty_inter_arrival_mult_str = '{:.6f}'.format(BURSTY_INTER_ARRIVAL_MULTIPLIER_LIST[read_idx])


for rep_num in range(1):
    file_name = '{}_background_inter_arrival_time_db_{}_flowmult_{}_intermult_{}.db' \
        .format(BG_CATEGORY, background_flow_size_mult_str, background_inter_arrival_mult_str, rep_num)
    if path.exists(file_name):
        print('Checking background inter arrival time DB:')
        inter_arrival_time_db = sqlite3.connect(file_name)
        inter_arrival_time_cursor = inter_arrival_time_db.execute('SELECT * FROM inter_arrival LIMIT 10')
        names = list(map(lambda x: x[0], inter_arrival_time_cursor.description))
        print (names)
        for row in inter_arrival_time_cursor:
            print(row)
        print('----------------------------------------------------')
        inter_arrival_time_db.close()

    file_name = '{}_background_flow_size_db_{}_flowmult_{}_intermult_{}.db' \
        .format(BG_CATEGORY, background_flow_size_mult_str, background_inter_arrival_mult_str, rep_num)
    if path.exists(file_name):
        print('Checking background flow size DB:')
        flow_size_db = sqlite3.connect(file_name)
        flow_size_cursor = flow_size_db.cursor()
        for row in flow_size_cursor.execute('SELECT * FROM flow_size LIMIT 10'):
            print(row)
        print('----------------------------------------------------')
        flow_size_db.close()

    file_name = '{}_background_server_idx_db_{}_flowmult_{}_intermult_{}.db'\
        .format(BG_CATEGORY, background_flow_size_mult_str, background_inter_arrival_mult_str, rep_num)
    if path.exists(file_name):
        print('Checking background server index DB:')
        background_server_idx_db = sqlite3.connect(file_name)
        background_server_idx_cursor = background_server_idx_db.cursor()
        for row in background_server_idx_cursor.execute('SELECT * FROM server_idx LIMIT 10'):
            print(row)
        print('----------------------------------------------------')
        background_server_idx_db.close()

    file_name = '{}_background_flow_ids_db_{}_flowmult_{}_intermult_{}.db' \
        .format(BG_CATEGORY, background_flow_size_mult_str, background_inter_arrival_mult_str, rep_num)
    if path.exists(file_name):
        print('Checking background flow ids DB:')
        background_flow_ids_db = sqlite3.connect(file_name)
        background_flow_ids_cursor = background_flow_ids_db.cursor()
        for row in background_flow_ids_cursor.execute('SELECT * FROM flow_ids LIMIT 10'):
            print(row)
        print('----------------------------------------------------')
        background_flow_ids_db.close()

    file_name = '{}_bursty_inter_arrival_time_db_{}_flowmult_{}_intermult_{}_flows_per_incast_{}_incast_flow_size_{}.db'\
        .format(BURSTY_CATEGORY, bursty_flow_size_mult_str, bursty_inter_arrival_mult_str, NUM_REQUEST_PER_BURST_LIST[read_idx], INCAST_FLOW_SIZE_LIST[read_idx],
                rep_num)

    print(file_name)
    if path.exists(file_name):
        print('Checking bursty inter arrival time DB:')
        inter_arrival_time_db = sqlite3.connect(file_name)
        inter_arrival_time_cursor = inter_arrival_time_db.execute('SELECT server0app2 from inter_arrival')
        names = list(map(lambda x: x[0], inter_arrival_time_cursor.description))
        print (names)
        for row in inter_arrival_time_cursor:
            print(row)
        print('----------------------------------------------------')
        inter_arrival_time_db.close()

    file_name = '{}_bursty_server_idx_db_{}_flowmult_{}_intermult_{}_flows_per_incast_{}_incast_flow_size_{}.db'\
        .format(BURSTY_CATEGORY, bursty_flow_size_mult_str, bursty_inter_arrival_mult_str, TARGET_NUM_INCAST_EVENTS_LIST[read_idx],
                INCAST_FLOW_SIZE_LIST[read_idx], rep_num)
    if path.exists(file_name):
        print('Checking bursty server index DB:')
        bursty_server_idx_db = sqlite3.connect(file_name)
        bursty_server_idx_cursor = bursty_server_idx_db.cursor()
        for row in bursty_server_idx_cursor.execute('SELECT * FROM server_idx LIMIT 10'):
            print(row)
        print('----------------------------------------------------')
        bursty_server_idx_db.close()

    file_name = '{}_bursty_flow_ids_db_{}_flowmult_{}_intermult_{}_flows_per_incast_{}_incast_flow_size_{}.db' \
        .format(BURSTY_CATEGORY, bursty_flow_size_mult_str, bursty_inter_arrival_mult_str, TARGET_NUM_INCAST_EVENTS_LIST[read_idx],
                INCAST_FLOW_SIZE_LIST[read_idx], rep_num)
    if path.exists(file_name):
        print('Checking bursty flow ids DB:')
        bursty_flow_ids_db = sqlite3.connect(file_name)
        bursty_flow_ids_cursor = bursty_flow_ids_db.cursor()
        for row in bursty_flow_ids_cursor.execute('SELECT * FROM flow_ids LIMIT 10'):
            print(row)
        print('----------------------------------------------------')
        bursty_flow_ids_db.close()

    file_name = '{}_bursty_query_ids_db_{}_flowmult_{}_intermult_{}_flows_per_incast_{}_incast_flow_size_{}.db' \
        .format(BURSTY_CATEGORY, bursty_flow_size_mult_str, bursty_inter_arrival_mult_str, TARGET_NUM_INCAST_EVENTS_LIST[read_idx],
                INCAST_FLOW_SIZE_LIST[read_idx], rep_num)
    if path.exists(file_name):
        print('Checking bursty query ids DB:')
        bursty_query_ids_db = sqlite3.connect(file_name)
        bursty_query_ids_cursor = bursty_query_ids_db.cursor()
        for row in bursty_query_ids_cursor.execute('SELECT * FROM query_ids LIMIT 10'):
            print(row)
        print('----------------------------------------------------')
        bursty_query_ids_db.close()
