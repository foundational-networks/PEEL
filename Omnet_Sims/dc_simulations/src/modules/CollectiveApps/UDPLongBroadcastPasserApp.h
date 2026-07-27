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

#ifndef __INET_UDPLongBroadcastPasserApp_H
#define __INET_UDPLongBroadcastPasserApp_H

#include <vector>

#include "inet/common/INETDefs.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "map"
#include "unordered_set"
#include <limits>
#include "unordered_map"
#include "common/BCastCustomCollectiveMsgPasserInfo.h"
#include <tuple>

using namespace inet;

/**
 * UDP application. See NED for more info.
 */
class INET_API UDPLongBroadcastPasserApp : public ApplicationBase, public UdpSocket::ICallback
{
  public:
    virtual int generate_next_batch(unsigned long flow_id, int gpu_idx=-1) override;
    virtual void dataArrived(Packet *msg, bool just_complete_message=false);
    virtual void passed_from_initiator(Packet *msg);
    double get_processing_delay(int gpu_idx,
            int collective_type,
            int collective_alg,
            int tree_traversal_dir=-1,
            bool optireduce_in_reduction_phase = true);
    simtime_t get_intra_host_pass_delay(unsigned long msg_size_bytes);

  protected:

    enum CollectiveAlgTypes { BCAST_SINGLE_SENDER = 1, BCAST_BINOMIAL_TREE, BCAST_BINARY_TREE, BCAST_RING, BCAST_NETWORK_ASSISTED, BCAST_INA, BCAST_OPTIREDUCE, BCAST_PARTIAL_MULTICAST, BCAST_MULTIWRITE};
    enum CollectiveTypes { ALL_TO_ALL_V = 1, ALL_REDUCE, REDUCE, ALL_GATHER, BROADCAST, REDUCE_SCATTER};
    enum MsgKinds { PASS = 1, FINISH};
    enum BatchingStatus { BATCH_SEND = 1, BATCH_SLEEP, BATCH_TERMINATE };
    enum TreeSearchType { TREE_LOW = 1, TREE_HIGH };
    enum TreeTraversalType { TREE_UP = 1, TREE_DOWN };

    int app_index, parent_index;
//    uint32 collective_alg_type;
    uint32 repetition_num;
    UdpSocket socket;
    bool is_message_splitted = false;
    int splitted_chunk_size_bytes = -1;
    int splitted_subflow_num = -1;
    int mtu = -1;

//    std::unordered_map<unsigned long, b> flowid_bitsrcvd_mapper;
    // gpu_idx, flow_id --> b
    std::map<std::tuple<int, unsigned long>, b> flowid_bitsrcvd_mapper;

    // this maps a gpu_idx, flow_id to msg info that are being passed
//    std::map<unsigned long, BCastCustomCollectiveMsgPasserInfo*> flowid_passingmsg_mapper;
    // for messages that are being passed
    std::map<std::tuple<int, unsigned long>, BCastCustomCollectiveMsgPasserInfo*> flowid_passingmsg_mapper;
    // for messages that are received
    std::map<std::tuple<int, unsigned long>, BCastCustomCollectiveMsgPasserInfo*> flowid_receivedmsg_mapper;

    // Extra info
//    std::map<UdpSocket*, int> socket_to_destination_mapper; // maps a socket to a server_idx
    std::map<std::tuple<int,int>, UdpSocket*> destination_to_socket_mapper; // maps a server_idx to a socket
    int mss = -1;
    int packet_creation_batch_size = -1;

    int nvlink_speed_gbps = -1;
    bool mcast_in_host;
    int num_gpus;
    int num_servers_under_each_leaf = -1;
    int fattree_k = -1;
    int num_spines = -1;
    int num_leafs = -1;
    // we put things here till the time they are popped
    std::unordered_map<unsigned long, BCastCustomCollectiveMsgPasserInfo*> storage;
    unsigned long storage_id = 1;
    unsigned long storage_cap = std::numeric_limits<long>::max() - 100;

//    simtime_t processing_delay;
    double processing_delay_mean, processing_delay_stddev;
    unsigned int seed;

    // for binary tree
    // maps (gpu_idx, query_id) to the flow id that is sent up for it
    std::map<std::tuple<int, unsigned long>, unsigned long> query_sent_up;
    // maps (gpu_idx, query_id) to the flow ids that have been observed for it so far
    std::map<std::tuple<int, unsigned long>, std::unordered_set<unsigned long>> observed_flow_ids_sets;
//    bool is_allreduce = false;

    std::map<std::tuple<int, unsigned long>, unsigned long> gpu_idx_flow_id_to_seq_num; // sequence number starts from 1 and is per-pkt

    std::unordered_map<unsigned long, unsigned long> ina_query_to_flow_id;
    std::unordered_map<unsigned long, unsigned long> ina_query_to_gpu_idx;

    // for opti reduce
    int optireduce_incast_factor = -1;
    bool use_becca_optireduce = false;
    bool use_cidr_optireduce = false;
    // maps (gpu_idx, query_id) to the completed flows' ids
    std::map<std::tuple<int, unsigned long>, std::set<unsigned long>> gpu_query_to_completed_flows;
    // <query, source gpu> --> list of iterators to flowid_passingmsg_mapper
    std::map<std::pair<unsigned long, int>, std::list<BCastCustomCollectiveMsgPasserInfo*>> optireduce_query_gpu_msg_record;

    // orca
    bool use_orca = false;

    //rsbf
    bool use_rsbf = false;

    bool use_fattree = false;

    // autoccl
    bool use_autoccl = false;
    bool autoccl_allreduce_only = false;
    int num_flows_per_ml_query = 0; // for retrieving the proper flow size!
    int retrieve_original_flow_size(int m_collective_type,
            int m_collective_alg, double flow_size);

    // maps (src gpu_idx, dst gpu_idx, flow_id) to set of arrived seq nums
    std::map<std::tuple<int, int, unsigned long>, std::set<unsigned long>> arrived_seq_nums_set;
    // set of seq num history (src gpu_idx, dst gpu_idx, flow_id)
    std::set<std::tuple<int, int, unsigned long>> seq_num_complete_flow_history;



  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;


    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;

    virtual void pass_msg(int server_idx, BCastCustomCollectiveMsgPasserInfo* msg_info);
    virtual int get_local_port();
    virtual void init_socket(int server_idx, int gpu_idx);
    virtual L3Address get_destination(int server_idx, int gpu_idx);
    virtual void pass_bcast_binomial_tree(BCastCustomCollectiveMsgPasserInfo* record_msg_info);
    virtual int find_parent(int size, int search_type, int element_idx_in_list);
    virtual int* find_children(int size, int search_type, int element_idx_in_list);

    virtual void pass_bcast_binary_tree(BCastCustomCollectiveMsgPasserInfo* record_msg_info);
    int get_num_children(BCastCustomCollectiveMsgPasserInfo* record_msg_info);
    virtual void pass_bcast_ring(BCastCustomCollectiveMsgPasserInfo* record_msg_info);
    virtual void pass_bcast_multiwrite(Packet* pkt, GenericAppMsg* payload);    // when node is relay
    virtual void pass_bcast_multiwrite(BCastCustomCollectiveMsgPasserInfo* record_msg_info); // when node is a dst
    virtual void pass_bcast_optireduce(BCastCustomCollectiveMsgPasserInfo* record_msg_info);
    virtual void optireduce_send_next(unsigned long query_id, int src_gpu_idx);

    virtual unsigned long insert_msg_storage(BCastCustomCollectiveMsgPasserInfo* msg_info);
    BCastCustomCollectiveMsgPasserInfo* pop_from_storage(unsigned long storage_loc);
    std::list<double> processing_delays;
    void schedule_finish_if_required(BCastCustomCollectiveMsgPasserInfo* msg_info, simtime_t delay);
    void schedule_pass(BCastCustomCollectiveMsgPasserInfo* msg_info, simtime_t delay);

    // <query id, sub-flow id> --> packets received count
    std::map<std::pair<unsigned long, unsigned long>, unsigned long> query_subf_to_pkt_count;
    void optireduce_update_rcvd_subflows();
    bool optireduce_should_pass_subflows();
  public:
    UDPLongBroadcastPasserApp() {}
    ~UDPLongBroadcastPasserApp();

    static simsignal_t flowEndedSignal;
    static simsignal_t flowEndedQueryIDSignal;
    static simsignal_t flowPassedSignal;
};


#endif // ifndef __INET_UDPLongBroadcastPasserApp_H

