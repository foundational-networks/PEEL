//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_UDPLongAllgatherInitiatorApp_H
#define __INET_UDPLongAllgatherInitiatorApp_H

#include <vector>

#include "inet/common/INETDefs.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "map"
#include "unordered_set"
#include "common/AllgatherCustomCollectiveMsgInitiatorInfo.h"
#include "./UDPLongBroadcastPasserApp.h"
#include "tuple"
#include <functional>
#include <limits>
#include <cmath>

using namespace inet;

// auto ccl

struct AutoCCLCCTInfo {
    simtime_t collective_start = 0;
    simtime_t collective_end = 0;
    unsigned int flows_completed = 0;
    unsigned int max_flows_per_collective = 0;
    int collective_type = -1;
    int collective_alg_type = -1;
    unsigned long query_id;
};



struct AutoCCLPlacementSig {
    int numHosts = 0;
    int numLeafsTouched = 0;
    int maxGPUsPerHost = 0;
    int maxHostsPerLeaf = 0;

    bool operator<(const AutoCCLPlacementSig& other) const {
        return std::tie(numHosts, numLeafsTouched, maxGPUsPerHost, maxHostsPerLeaf) <
               std::tie(other.numHosts, other.numLeafsTouched, other.maxGPUsPerHost, other.maxHostsPerLeaf);
    }
};

struct AutoCCLTaskKey {
    int opType = 0; // broadcast, all-gather, all-reduce
    int sizeBin = 0;
    int groupSizeBin = 0;
    AutoCCLPlacementSig placement;
    int loadBin = 0;

    bool operator<(const AutoCCLTaskKey& other) const {
        return std::tie(opType, sizeBin, groupSizeBin, placement, loadBin) <
               std::tie(other.opType, other.sizeBin, other.groupSizeBin, other.placement, other.loadBin);
    }
};

struct AutoCCLAlgoStats {
    int trials = 0;
    double meanCCT = 0.0;
    double meanBytes = 0.0;
    double bestCCT = std::numeric_limits<double>::infinity();
};

struct AutoCCLSelectorState {
    AutoCCLAlgoStats ring;
    AutoCCLAlgoStats tree;
    int bestAlgo = -1;
    int occurrences = 0;
};


/**
 * UDP application. See NED for more info.
 */
class INET_API UDPLongAllgatherInitiatorApp : public ApplicationBase, public UdpSocket::ICallback
{
  public:

    std::list <simtime_t> inter_arrival_times;
    std::list <unsigned long> flow_sizes, dst_server_nums, flow_ids, query_ids, collective_types, collective_algs;
    std::list <int> dst_server_idx;
    std::list <int> dst_gpu_idx;
    typedef std::list<unsigned int> jump_idx_list; // one jump_idx_list is for one switch
    std::list<std::list<jump_idx_list>> jump_to_idx_list_of_lists; // a list of lists of jump_idx_list is for all the switches in the path and for various queries
    typedef std::list<unsigned int> port_list;  // one port_list is for one switch
    std::list<std::list<port_list>> ports_to_dst_list_of_lists; // a list of port_list is for all the switches in the path and for various queries
    virtual int generate_next_batch(unsigned long flow_id, int gpu_idx=-1) override;
    int collective_str_to_type(std::string collective_type_str);
    int collective_alg_str_to_type(std::string collective_alg_type_str);
    void fill_out_query_type_info(std::string query_type_info_str);
    unsigned int getLbTreeIdx(unsigned long flow_id);

    // for becca optireduce
    unsigned int getLbTreeIdx(unsigned long query_id, int src_gpu_idx);
    std::list<std::list<port_list>> optireduce_get_port_list(unsigned long query_id, int src_gpu_idx);
    void optireduce_remove_port_list(unsigned long query_id, int src_gpu_idx);
    std::list<std::list<jump_idx_list>> optireduce_get_jump_list(unsigned long query_id, int src_gpu_idx);
    void optireduce_remove_jump_list(unsigned long query_id, int src_gpu_idx);

    virtual int get_tor_idx_for_server(int server_idx);
    virtual int get_pod_idx_for_tor(int tor_idx);
    unsigned long get_relative_tor_idx(unsigned long tor_idx);
    int cidr_type = -1;
    bool partial_mcast_use_cidr_agg = false;
    bool partial_mcast_use_cidr_core = false;
    unsigned int agg_cidr_max_bit_size = 0;
    unsigned int core_cidr_max_bit_size = 0;
    // maps tor idx to id
    std::unordered_map<unsigned long, std::string>
        CIRD_minimal_prefix_cover(
            std::unordered_set<unsigned long> node_idx_list,
            int max_bit_size,
            int trie_type);
    enum TrieType { CIDR_AGG = 1, CIDR_CORE};

  protected:

    enum BatchingStatus { BATCH_SEND = 1, BATCH_SLEEP, BATCH_TERMINATE };
    enum SelfMsgKinds { SEND = 1, STOP};
    enum TreeSearchType { TREE_LOW = 1, TREE_HIGH };
    enum TreeTraversalType { TREE_UP = 1, TREE_DOWN };
    enum CollectiveAlgTypes { BCAST_SINGLE_SENDER = 1, BCAST_BINOMIAL_TREE, BCAST_BINARY_TREE, BCAST_RING, BCAST_NETWORK_ASSISTED, BCAST_INA, BCAST_OPTIREDUCE, BCAST_PARTIAL_MULTICAST, BCAST_MULTIWRITE};
    enum CollectiveTypes { ALL_TO_ALL_V = 1, ALL_REDUCE, REDUCE, ALL_GATHER, BROADCAST, REDUCE_SCATTER};
    enum LBGranularity {LB_FLOW = 1, LB_PKT};
    // random or round-robin
    enum LBTreeSelect {LB_Random = 1, LB_RR};
    int app_index, parent_index;
    uint32 repetition_num;
    double flow_size_multiplier, inter_arrival_time_multiplier, frag_ratio;
    int num_flows_per_ml_query, flow_size;

    // tells us what the collective is from Collectives enum
    int collective_type = -1;
    int collective_alg_type = -1;

    // Extra info
//    std::map<UdpSocket*, int> socket_to_destination_mapper; // maps a socket to a server_idx
    std::map<std::tuple<int,int>, UdpSocket*> destination_to_socket_mapper; // maps a server_idx to a socket
    std::map<unsigned long, AllgatherCustomCollectiveMsgInitiatorInfo*> flowid_to_msginfo_mapper; // maps a server_idx to a socket

    // parameters
    cMessage *selfMsg = nullptr;
    int mss = -1;
    int packet_creation_batch_size = -1;

    int num_gpus;

    double initiation_delay_mean, initiation_delay_stddev;
    unsigned int seed;
    std::list<simtime_t> initiation_delay_per_gpu;

    // for all reduce with double binary tree
    int source_gpu_round_trip_idx = 0;
    UDPLongBroadcastPasserApp* passer_app;

    int ignore = 0;

    // failure for ace
    int failure_perc = -1;
    int lookahead = -1;

    // should we reduce in source before sending with ace
    bool ace_apply_src_reduce = false;
    std::unordered_map<unsigned long, int> query_id_counter;


    // for the case of combinational collectives
    bool using_ace = false;

    // LB
    int lb_granularity;    // flow, packet
    int lb_tree_num;
    int lb_tree_select_type;
    std::unordered_map<unsigned long, unsigned int> flow_id_map_to_round_robin_idx;
    std::map<std::pair<unsigned long, int>, unsigned int> query_gpu_map_to_round_robin_idx; // for becca optireduce

    // mapping each flow id to list of trees each being a list of ports and jumps
    std::unordered_map<unsigned long, std::list<std::list<port_list>>> flow_id_map_to_lists_of_list_of_ports_to_dsts;
    std::unordered_map<unsigned long, std::list<std::list<jump_idx_list>>> flow_id_map_to_lists_of_list_of_jumps_to_idxs;

    // this is for reduce and ring
    std::unordered_set<unsigned long> ring_reduce_query_id;

    int splitted_subflow_num = -1;

    // this is for INA
//    std::unordered_map<unsigned long, unsigned int> flow_id_to_bytes_sent_map;
//    std::unordered_map<unsigned long, unsigned int> flow_id_to_subflow_idx_map;
    int num_servers_under_each_leaf = -1;

    std::unordered_map<unsigned long, unsigned long> flow_id_to_seq_num; // sequence number starts from 1 and is per-pkt

    // optireduce
    int optireduce_incast_factor = -1;
    bool use_becca_optireduce = false;
    // <query, src gpu> --> msg list record
    std::map<std::pair<unsigned long, int>, std::list<AllgatherCustomCollectiveMsgInitiatorInfo*>> optireduce_query_gpu_msg_record;
    // for becca optireduce
    // mapping each flow id to list of trees each being a list of ports and jumps
    std::map<std::pair<unsigned long, int>, std::list<std::list<port_list>>> query_gpu_map_to_lists_of_list_of_ports_to_dsts;
    std::map<std::pair<unsigned long, int>, std::list<std::list<jump_idx_list>>> query_gpu_map_to_lists_of_list_of_jumps_to_idxs;

    // orca
    bool use_orca = false;
    double orca_controller_delay_mean, orca_controller_delay_stddev;
    //rsbf
    bool use_rsbf = false;
    int rsbf_hash_num = 0;
    std::list<unsigned int> rsbf_bytes;
    std::unordered_map<int, std::list<unsigned int>> hash_seeds_map;    // maps src gpu idx to list of seeds
    std::map<std::pair<unsigned long, int>, std::list<unsigned int>> bit_string_map;    // maps <query, src gpu> to the bit strings
    unsigned int rsbf_overhead = 0;
    // elmo
    bool use_elmo = false;
    unsigned int elmo_per_pkt_overhead_bytes = 0;
    unsigned int elmo_overhead_pop_bytes = 0;
    void compute_elmo_perpkt_overhead();

    bool use_fattree = false;
    int fattree_k = -1;

    bool use_hpn = false;

    int num_spines = -1;
    int num_leafs = -1;

    // initiate tree using lead packets
    bool initiate_tree = false;
    B tree_id_overhead_bytes = B(8);

    // cepheus
    bool use_cepheus = false;

    int nvlink_speed_gbps = -1;

    double random_fail_prob = -1;

    bool partial_mcast_chunk_data = false;
    bool partial_mcast_chunk_tors = false;
    bool partial_mcast_programmable_cores = false;
    bool partial_mcast_divide_tors_among_cores = false;

    // using CIDR in aggs
    enum CIDRType { CIDR_FULL_TREE = 1, CIDR_BW_GAIN};
    struct TrieNode {
        TrieNode* child[2] = {nullptr, nullptr};
        bool isLeaf = false;
        std::string id = "";
        std::list<std::string> leaf_ids;
        std::list<unsigned long> leaf_actual_node_indexes;
        int height = 0;
        long actual_node_idx = -1;
        ~TrieNode() {
            delete child[0];
            delete child[1];
        }
    };
    void CIDR_update_leaf_info(TrieNode* node);
    void CIDR_trie_insert(TrieNode* root,
            unsigned long node_idx,
            int max_bit_size,
            int trie_type);
    void trie_CIRD_compress_full_tree(TrieNode* node,
                std::unordered_map<unsigned long, std::string>& tor_index_to_prefix_map);
    void trie_CIRD_compress_bw_gain(TrieNode* node,
                std::unordered_map<unsigned long, std::string>& tor_index_to_prefix_map);
    void set_cidr_max_bit_size();
    double get_processing_delay(int collective_type, int collective_alg);

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;
    void initialize_rsbf_info();


    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;

    void read_value_from_db(std::string inter_arrival_db_path,
                std::string flow_size_db_path, std::string dst_server_num_db_path,
                std::string dst_server_idx_db_path, std::string dst_gpu_idx_db_path,
                std::string flow_ids_db_path,
                std::string query_ids_db_path, std::string query_types_db_path,
                std::string ports_to_dest_db_path,
                std::string jump_to_idx_db_path, std::string inter_arrival_table_name,
                std::string flow_size_table_name, std::string dst_server_num_table_name,
                std::string dst_server_idx_table_name, std::string dst_gpu_idx_table_name,
                std::string flow_ids_table_name,
                std::string query_ids_table_name, std::string query_types_table_name,
                std::string ports_to_dest_table_name,
                std::string jump_to_idx_table_name);
    void add_initiation_delays_and_sort();

    virtual simtime_t get_inter_arrival_time();
    virtual int get_flow_size(int m_collective_type, int m_collective_alg);
    virtual int get_collective_type();
    int get_collective_alg(int collective_type, unsigned long query_id);
    virtual int get_dst_server_idx();
    int get_dst_gpu_idx();
    virtual int get_dst_server_num_per_query(int m_collective_alg);
    virtual unsigned long get_flow_id();
    virtual unsigned long get_query_id();
    virtual std::list<port_list> get_ports_to_dst_list();
    virtual std::list<port_list> get_jump_to_idx_list();



    // autoccl
    // -------------------------

    static std::map<AutoCCLTaskKey, AutoCCLSelectorState> autoccl_table;
    static std::map<unsigned long, AutoCCLCCTInfo> autoccl_cct_tracker;
    static std::map<unsigned long, AutoCCLTaskKey> autoccl_queryid_to_key_mapper;
    static std::map<unsigned long, int> autoccl_queryid_to_collective_alg_mapper;
    static std::set<unsigned long> query_alg_restored;
//    static int rr_idx;

    AutoCCLTaskKey autoccl_build_task_key(int collective_type);
    int autoccl_update_collective_alg(int collective_type, unsigned long query_id);
    int autoccl_size_bin(uint64_t bytes);
    int autoccl_group_size_bin(int groupSize);
    AutoCCLPlacementSig autoccl_build_placement_sig();
    int autoccl_pick_better_alg(AutoCCLSelectorState& st);
    int autoccl_other_alg(int alg);
    bool use_autoccl = false;
    bool autoccl_allreduce_only = false;

    void instantiate_autoccl_cct_info(bool autoccl_used,
            int collective_type,
            int collective_alg_type,
            unsigned long query_id);

    // -------------------------



    Packet* create_packet(std::string pkt_name,
            B pkt_size,
            unsigned long seq_num,
            AllgatherCustomCollectiveMsgInitiatorInfo* msg_info,
            int src_gpu_idx,
            int dst_gpu_idx,
            unsigned int useful_data,   // the actual data that is in the payload (not considering the overhead)
            int init_tree_pkt_idx = -1,
            unsigned long remaining_bytes = 0);
    virtual void send_msg(int server_idx,
            int gpu_idx,
            AllgatherCustomCollectiveMsgInitiatorInfo* msg_info,
            int src_gpu_idx,
            bool perform_tree_initiation = false);
    virtual void send_to_self(int src_gpu_idx, int dst_gpu_idx, AllgatherCustomCollectiveMsgInitiatorInfo* msg_info);
    virtual int get_local_port();
    virtual void init_socket(int server_idx, int gpu_idx);
    virtual L3Address get_destination(int server_idx, int gpu_idx);
    virtual void snd_allgather_single_sender(int collective_type, unsigned long query_id);
    virtual int find_parent(int size, int search_type, int element_idx_in_list, bool just_leafs = false);
    virtual void snd_allgather_binary_tree(int collective_type, unsigned long query_id);
    virtual void snd_allgather_binomial_tree(int collective_type, unsigned long query_id);
    virtual void snd_allgather_ring(int collective_type, unsigned long query_id);
    void snd_allgather_multiwrite(int collective_type, unsigned long query_id);
    virtual void optireduce_send_next(unsigned long query_id, int src_gpu_idx);
    virtual void snd_allgather_optireduce(int collective_type, unsigned long query_id);
    virtual void snd_allgather_network_assisted(int collective_type, unsigned long query_id);
    virtual void snd_allgather_partial_mcast(int collective_type, unsigned long query_id);
    virtual void snd_allgather_ina(int collective_type, unsigned long query_id);
    virtual int get_random_gpu_idx();
//    virtual void filter_out_self_data();
    simtime_t get_intra_host_pass_delay(unsigned long msg_size_bytes);
    std::list<std::string> generate_rsbf_bitstreams(std::list<jump_idx_list> port_info, int src_gpu_idx);

    std::size_t hashWithSeed(std::string value, unsigned int seed) {
        std::size_t h1 = std::hash<std::string>{}(value);
        return h1 ^ (static_cast<std::size_t>(seed) + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }

    double get_controller_delay(bool using_orca, bool using_elmo, bool is_onetime_generation = false);

    unsigned int get_orca_per_pkt_overhead_bytes();

    std::tuple<int, int, int> get_aggregation_msg_number(std::list<int> dst_servers_excluding_src);


  public:
    UDPLongAllgatherInitiatorApp() {}
    ~UDPLongAllgatherInitiatorApp();

    static simsignal_t flowNumSignal;
    static simsignal_t requestSentSignal;
    static simsignal_t replyLengthsSignal;
    static simsignal_t flowStartedQueryIDSignal;
    static simsignal_t flowStartedCollectiveTypeSignal;
    static simsignal_t flowStartedCollectiveAlgSignal;
    static simsignal_t controllerDelaySignal;
    static simsignal_t autocclChosenCollectiveAlgSignal;
    static simsignal_t autocclChosenCollectiveSignal;

    // autoccl
    static void autoccl_inject_flow_finish(bool autoccl_used,
            bool autoccl_is_for_allreduce_only,
            int collective_type,
            int collective_alg_type,
            unsigned long query_id,
            unsigned long m_flow_size);
};


#endif // ifndef __INET_UDPLongAllgatherInitiatorApp_H

