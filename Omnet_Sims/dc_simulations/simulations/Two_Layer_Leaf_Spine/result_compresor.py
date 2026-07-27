import os
from os import listdir
from os.path import isfile, join
from subprocess import call

RESULT_FILES_DIR = './results/'

OUTPUT_FILE_DIRECTORY = './results/'

onlyfiles = [f for f in listdir(RESULT_FILES_DIR) if
             isfile(join(RESULT_FILES_DIR, f)) and
             (f[-4:] == '.vec' or f[-4:] == '.vci' or
              f[-4:] == '.sca' or f[-4:] == '.out')]
f = open(RESULT_FILES_DIR + 'compressor.sh', 'w')
for file_name in onlyfiles:
    num_spines = file_name.split('_spines')[0]
    num_aggs = (file_name.split('_aggs')[0]).split('_')[-1]
    num_servers_under_each_rack = (file_name.split('_servers_')[0]).split('_')[-1]
    num_bursty_apps = (file_name.split('_burstyapps')[0]).split('_')[-1]
    num_mice_flows_per_server = (file_name.split('_mice')[0]).split('_')[-1]
    inter_arrival_multiplier = (file_name.split('_intermult')[0]).split('_')[-1]
    flow_size_multiplier = (file_name.split('_fsizemult')[0]).split('_')[-1]
    ttl = (file_name.split('_ttl_')[0]).split('_')[-1]
    agg_q_size = (file_name.split('_aggqsize')[0]).split('_')[-1]
    spine_q_size = (file_name.split('_spineqsize')[0]).split('_')[-1]
    threshold = 0
    if 'threshold' in file_name:
        threshold = (file_name.split('_threshold')[0]).split('_')[-1]


    command = "7z a {}.7z {}\n".format(file_name, file_name)
    print(command)
    f.write(command)

    command = "rm -rf {}\n".format(file_name)
    print(command)
    f.write(command)

f.close()
