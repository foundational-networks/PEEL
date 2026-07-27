################### GENERAL SETTING ###################
NUM_SPINES = 16
NUM_AGGS = 32
NUM_SERVERS_UNDER_EACH_RACK = 2
K = 8
REP_NUM = 1
EDGE_LINK_CAPACITY = 100 * (10 ** 9)
AGG_LINK_CAPACITY = 100 * (10 ** 9)
CORE_LINK_CAPACITY = 100 * (10 ** 9)
################### GENERAL SETTING ###################


# ################### BG SETTING ###################
NUM_BACKGROUND_CONNECTIONS_PER_SERVER = 0
# ################### BG SETTING ###################


# ################### BURSTY SETTING ###################
NUM_BURSTY_APP_PER_SERVER = 0
# ################### BURSTY SETTING ###################


################### ML SETTING ###################
ML_BASE_TIME = 0.2
NUM_ML_APP_PER_SERVER = 1
ML_CATEGORY = 'web'
# file, poisson
ML_INTER_ARRIVAL_DIST_TYPE = 'poisson'
# lambda is in us, we set lambda to the average points generated from 'web' category.
ML_INTER_ARRIVAL_DIST_POISSON_LAMBDA = 1000 # in microseconds so 1000 is one millisecond. Never set this to 1 or something near zero
ML_INTER_ARRIVAL_DIST_FILE_NAME = 'inter_arrival_{}_two_tier.csv'.format(ML_CATEGORY)
ML_FLOW_SIZE_DIST_FILE_NAME = 'flow_size_{}_two_tier.csv'.format(ML_CATEGORY)
ML_FLOW_SIZE_MULTIPLIER = 1

# For all-to-allv
FLOW_SIZE_GENERATION_TYPE = None
# normal
FLOW_DIST_NORMAL_MEAN = -1
FLOW_DIST_NORMAL_STD = -1


# for broadcast and reduce
is_broadcast = False

# if we are generating constant load
constant_load = False

# enable maximum locality between leafs (just for leaf-spine)
maximize_locality = False

# whether to use one job per GPU logic
one_job_per_gpu = True

FRAGMENT_RATIOS = [None]

ML_FLOW_ID_BASE = '12'
MAX_THREAD_NUM = 1
ERROR_TOLLERANCE = 0
NUM_GPUS = 8

FAIL_PERCENTAGE_LIST = [1, 2, 4, 8, 10] # SETTING THIS TO 1 MEANS 1% NOT 100%
LOOK_AHEAD_LIST = [0, 2, 4]

# LB: having multiple trees for LB
LB_TREE_PER_COLLECTIVE_LIST = [None]

## SMALL SCALE -- DFSIZE
SIM_TIME = 1.2
NUM_FLOWS_PER_ML_QUERY_LIST = [64]
ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2]]
ML_INTER_ARRIVAL_MULTIPLIER_LIST = [1000.0]
TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [1]]

# # For all-to-allv
# FLOW_SIZE_GENERATION_TYPE = "normal"
# # normal
# FLOW_DIST_NORMAL_MEAN = 64000000
# FLOW_DIST_NORMAL_STD = 32000000

########################################################## Broadcast FATTREE CONSTANT LOAD GENERATION (5s) ####################################

# # ## 30% load, dfsize
# SIM_TIME = 2.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.24414, 0.48828125, 0.9765625, 1.953125, 3.90625, 7.8125, 15.625, 31.25, 62.5, 125.0, 250.0]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [4096, 2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4]]
# # #-----------------------------
# ## 50% load, dfsize
# SIM_TIME = 2.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# # FRAGMENT_RATIOS = [1.0/16, 1.0/8, 1.0/4, 1.0/2]
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.14654125, 0.29342625, 0.58685375, 1.179245, 2.35849, 4.80769125, 9.61538375, 20.8333325, 41.66666625, 83.3333325, 166.666665]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [6824, 3408, 1704, 848, 424, 208, 104, 48, 24, 12, 6]]
# #-----------------------------
# # ## 70% load, dfsize
# SIM_TIME = 2.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.10469, 0.20938, 0.4194625, 0.838925, 1.68918875, 3.3783775, 6.94444375, 13.88888875, 31.25, 62.5, 125.0]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [9552, 4776, 2384, 1192, 592, 296, 144, 72, 32, 16, 8]]
# # #-----------------------------


# # ## 30% load, dscale
# SIM_TIME = 2.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [32, 64, 128, 256, 512]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [64]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [4.46428, 8.92857, 17.85714, 35.71428, 71.42857]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [224, 112, 56, 28, 14]]
# # #-----------------------------
# # ## 50% load, dscale
# SIM_TIME = 2.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [32, 64, 128, 256, 512]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [64]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [2.67856, 5.35714, 10.71428, 21.73913, 43.47826]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [373, 186, 93, 46, 23]]
# # #-----------------------------
# # ## 70% load, dscale
# SIM_TIME = 2.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [32, 64, 128, 256, 512]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [64]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [1.9157, 3.84615, 7.6923, 15.625, 31.25]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [522, 260, 130, 64, 32]]
# # #-----------------------------




########################################################## Broadcast LEAF-SPINE CONSTANT LOAD GENERATION (5s) ####################################

# ## 30% load, dfsize
# SIM_TIME = 1.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.06666, 0.13332, 0.26664, 0.53328, 1.06656, 2.13312, 4.26624, 8.53248, 17.06496, 34.12992, 68.25984]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [15000, 7500, 3750, 1875, 938, 469, 234, 117, 58, 29, 15]]
# #-----------------------------
# ## 50% load, dfsize
# SIM_TIME = 1.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# # FRAGMENT_RATIOS = [1.0/16, 1.0/8, 1.0/4, 1.0/2]
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.039996, 0.079992, 0.159984, 0.319968, 0.639936, 1.279872, 2.559744, 5.119488, 10.238976, 20.477952, 40.955904]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [25000, 12500, 6250, 3125, 1563, 781, 390, 195, 98, 49, 25]]
# # #-----------------------------
# # ## 70% load, dfsize
# SIM_TIME = 1.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.02857, 0.05713, 0.11427, 0.22854, 0.45709, 0.91419, 1.82838, 3.6567, 7.31355, 14.6271, 29.25421]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [35000, 17500, 8750, 4375, 2188, 1093, 546, 273, 137, 68, 35]]
# # #-----------------------------


# ## 50% load, dscale
# SIM_TIME = 1.2
# is_broadcast = True
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [32, 64, 128, 256, 512]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [64]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.639936, 1.279872, 2.559744, 5.119488, 10.238976]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [1562, 781, 390, 195, 97]]
# # #-----------------------------


########################################################## All-Gather LEAF-SPINE/FAT-TREE CONSTANT LOAD GENERATION (5s) ####################################

# ## 50% load, dfsize
# SIM_TIME = 2.2
# is_broadcast = False
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# # FRAGMENT_RATIOS = [1.0/16, 1.0/8, 1.0/4, 1.0/2]
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [2, 8, 32, 128, 512, 2048]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [2.51889, 10.10101, 40.0, 166.66666, 500.0, 2000.0]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [397, 99, 25, 6, 2, 0.5]]
# # #-----------------------------

# ## 50% load, dscale
# SIM_TIME = 2.2
# is_broadcast = False
# constant_load = True
# maximize_locality = False
# one_job_per_gpu = True
# # FRAGMENT_RATIOS = [1.0/16, 1.0/8, 1.0/4, 1.0/2]
# NUM_FLOWS_PER_ML_QUERY_LIST = [32, 64, 128, 256, 512]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [64]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [19.23076, 76.92307, 250.0, 1000.0, 2000.0]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [52, 13, 4, 1, 0.5]]
# # #-----------------------------


########################################################## Combinations LEAF-SPINE CONSTANT LOAD GENERATION (5s) ####################################

# ## 50% load, dfsize
# SIM_TIME = 1.2
# is_broadcast = False
# constant_load = False
# maximize_locality = False
# one_job_per_gpu = True
# NUM_FLOWS_PER_ML_QUERY_LIST = [64]
# ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [8]]
# ML_INTER_ARRIVAL_MULTIPLIER_LIST = [10.0]
# TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [100]]
# FLOW_DIST_NORMAL_MEAN = 8000000
# FLOW_DIST_NORMAL_STD = 4000000
# FLOW_SIZE_GENERATION_TYPE = "normal"

# # # #-----------------------------


########################################################## Broadcast FATTREE CONSTANT LOAD GENERATION -- FRAGMENTATION (5s) ####################################


## 50% load, dfsize
SIM_TIME = 2.2
is_broadcast = True
constant_load = True
maximize_locality = False
one_job_per_gpu = True
FRAGMENT_RATIOS = [0.1, 0.25, 0.40, 0.55, 0.7, 0.85]
NUM_FLOWS_PER_ML_QUERY_LIST = [64]
ML_FLOW_SIZE_LIST = [int(i * (10 ** 6)) for i in [8, 32]]
ML_INTER_ARRIVAL_MULTIPLIER_LIST = [0.58685375, 2.35849]
TARGET_NUM_ML_QUERIES_LIST = [int(i * (SIM_TIME - ML_BASE_TIME)) for i in [1704, 424]]
#-----------------------------