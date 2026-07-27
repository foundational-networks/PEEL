import sqlite3
from Variables import *
from os import path

read_idx = 0

for rep_num in range(REP_NUM):
    file_name = '{}_spines_{}_leafs_{}_servers_{}_rep_{}.db' \
            .format(NUM_SPINES, NUM_AGGS, NUM_SERVERS_UNDER_EACH_RACK, rep_num, LS_DEFLECT_POINTS)
    if path.exists(file_name):
        print('Checking background deflection identifier DB:')
        deflection_identifier_db = sqlite3.connect(file_name)
        cursor = deflection_identifier_db.execute('select * from deflection_identifiers')
        names = [description[0] for description in cursor.description]
        print(names)
        deflection_identifier_cursor = deflection_identifier_db.cursor()
        for row in deflection_identifier_cursor.execute('SELECT * FROM deflection_identifiers'):
            print(row)
        print('----------------------------------------------------')
        deflection_identifier_db.close()
