'''
------------------------------------------------------------------------
Iterated lists:
'''

ML_QUERY_SCALE_LIST = [64]

ML_QUERY_INTER_MULT_LIST = [0.14654125, 0.58685375, 2.35849, 9.61538375, 41.66666625]

ML_FLOW_SIZE_LIST = [i * (10 ** 6) for i in [2, 8, 32, 128, 512]]

ML_NUM_GPUS = [8]
ML_NVLINK_Gbps = [7200]
CHUNK_SIZES = [1000]
SUB_FLOW_NUMS = [4, 8, 16, 32, 64, 128]
SUB_FLOW_NUMS = [8]

IS_FRAGMENTED = False

NUM_REQUESTS_PER_BURST = [100]      # Incast scales
LOAD_LIST = [25]   # BG inter-arrival multipliers' translation into load
BG_FLOW_SIZE_MULT = [1]     # BG flow-size multipliers -- Keep 1
BG_INTER_ARRIVAL_MULT = ['0.2428']
INCAST_INTER_ARRIVAL_MULT = [0.125]
INCAST_FLOW_SIZE = [i * (10 ** 3) for i in [40]]     # Incast flow size

FAIL_PERCENTAGE_LIST = [1, 2, 4, 8, 10]
LOOK_AHEAD_LIST = [0]

VERBOSE = False

CATEGORIES = ["ring_bcast", "tree_bcast", "mcast_optimal_bcast", "mcast_bcast_orca", "mcast_bcast_elmo", "mcast_bcast_peel"]


'''
------------------------------------------------------------------------
Constants
'''
BASE_DIR = './'
LEAF_SPINE = 0
FAT_TREE = 1
TOPOLOGY = FAT_TREE
REP_NUM = 1
MSS = 1440
K = 8
BASE_SPINE_NUM = 16
BASE_AGG_NUM = 32
SERVERS_UNDER_EACH_RACK = 2
NUM_BURSTY_APPS = 0
BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS = 0
NUM_ML_APPS = 1
TOTAL_DC_SERVER_NUM = BASE_AGG_NUM * SERVERS_UNDER_EACH_RACK
SIM_TIME = 1.2
mice_flow_size = 100 * (10 ** 3)
elephant_flow_size = 10 * (10 ** 6)
DENOMINATOR = 1     # keep 1 unless you want normalization
TARGET_NUM_ITERATIONS = 5
IS_ALLREDUCE = None
APPLY_IN_SRC_REDUCE = None
IS_BROADCAST = None
IS_REDUCE = None
