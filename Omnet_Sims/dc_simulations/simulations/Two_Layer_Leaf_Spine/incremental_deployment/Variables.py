################### GENERAL ###################
REP_NUM = 1
RANDOM_SWITCH_NUM = 2

################### LEAF-SPINE ###################
NUM_SPINES = 4
NUM_AGGS = 8
NUM_SERVERS_UNDER_EACH_RACK = 40
LS_DEFLECT_POINTS = 'partition_graph'
LS_DEFLECT_POINT_OPTIONS = ['none', 'all', 'leafs', 'spines', 'leafs_random', 'spines_random', 'random', 'partition_graph']
PARTITION_LEAF_NUM = 8
PARTITION_SPINE_NUM = 1
################### LEAF-SPINE ###################

################### FAT-TREE ###################
K = 8
FT_DEFLECT_POINTS = 'partition_graph'
FT_DEFLECT_POINT_OPTIONS = ['edges', 'aggs', 'cores', 'partition_graph']
PARTITION_EDGE_NUM = 1
PARTITION_AGG_NUM = 3
PARTITION_CORE_NUM = 0
################### FAT-TREE ###################
