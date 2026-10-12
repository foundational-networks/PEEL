'''
------------------------------------------------------------------------
Iterated lists:
------------------------------------------------------------------------
'''

ML_QUERY_SCALE_LIST = [64]

ML_QUERY_INTER_MULT_LIST = ['2.688000', '10.752000', '43.008000', '86.016000', '172.032000', '688.128000']

ML_FLOW_SIZE_LIST = [i * (10 ** 6) for i in [2, 8, 32, 64, 128, 512]]

# HPN uses 16 GPUs in the INI/result filename. The HPN response-time
# script follows the validated sample convention and writes
# int(ml_num_gpus * 2) into the filename.
ML_NUM_GPUS = [8]
ML_NVLINK_Gbps = [7200]
CHUNK_SIZES = [1000]
SUB_FLOW_NUMS = [8]

IS_FRAGMENTED = False
NUM_REQUESTS_PER_BURST = [100]      # Incast scales
LOAD_LIST = [25]                    # BG inter-arrival multipliers' translation into load
BG_FLOW_SIZE_MULT = [1]             # BG flow-size multipliers -- Keep 1
BG_INTER_ARRIVAL_MULT = ['0.2428']
INCAST_INTER_ARRIVAL_MULT = [0.125]
INCAST_FLOW_SIZE = [i * (10 ** 3) for i in [40]]

FAIL_PERCENTAGE_LIST = [1, 2, 4, 8, 10]
LOOK_AHEAD_LIST = [0]

VERBOSE = False

CATEGORIES = ["ring", "tree", "mcast_optimal", "mcast_orca", "mcast_peel"]

'''
------------------------------------------------------------------------
Constants
------------------------------------------------------------------------
'''

BASE_DIR = './'

HPN = 2
TOPOLOGY = HPN

REP_NUM = 1
MSS = 1440

NUM_PODS = 2
NUM_AGGS_PER_PLANE = 15
NUM_SEGMENTS_PER_POD = 16
NUM_SERVERS_PER_SEGMENT = 2

NUM_BURSTY_APPS = 0
BASE_NUM_BACKGROUND_CONNECTIONS_TO_OTHER_SERVERS = 0
NUM_ML_APPS = 1

SIM_TIME = 2.2
mice_flow_size = 100 * (10 ** 3)
elephant_flow_size = 10 * (10 ** 6)
DENOMINATOR = 1
TARGET_NUM_ITERATIONS = 5

IS_ALLREDUCE = None
APPLY_IN_SRC_REDUCE = None
IS_BROADCAST = None
IS_REDUCE = None
