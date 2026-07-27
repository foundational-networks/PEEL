//
// Copyright (C) 2006 Levente Meszaros
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

#ifndef __INET_AUGMENTEDETHERMACFULLDUPLEX_H
#define __INET_AUGMENTEDETHERMACFULLDUPLEX_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ethernet/EtherMacBase.h"
#include "../CollectiveApps/UDPLongBroadcastPasserApp.h"
#include "../CollectiveApps/UDPLongAllgatherInitiatorApp.h"
#include "../BouncingSwitchRelay/BouncingIeee8021dRelay.h"

using namespace inet;

/**
 * A simplified version of EtherMac. Since modern Ethernets typically
 * operate over duplex links where's no contention, the original CSMA/CD
 * algorithm is no longer needed. This simplified implementation doesn't
 * contain CSMA/CD, frames are just simply queued up and sent out one by one.
 */
class INET_API AugmentedEtherMacFullDuplex : public EtherMacBase
{
  public:
    AugmentedEtherMacFullDuplex();
    ~AugmentedEtherMacFullDuplex();

  protected:

    enum BatchingStatus { BATCH_SEND = 1, BATCH_SLEEP, BATCH_TERMINATE };
    enum CollectiveAlgTypes { BCAST_SINGLE_SENDER = 1, BCAST_BINOMIAL_TREE, BCAST_BINARY_TREE, BCAST_RING, BCAST_NETWORK_ASSISTED, BCAST_INA, BCAST_OPTIREDUCE, BCAST_PARTIAL_MULTICAST};
    enum CollectiveTypes { ALL_TO_ALL_V = 1, ALL_REDUCE, REDUCE, ALL_GATHER, BROADCAST, REDUCE_SCATTER};
    enum TreeTraversalType { TREE_UP = 1, TREE_DOWN };

    unsigned long long light_throughput_bits_sent = 0;
    unsigned long long light_throughput_bits_rcvd = 0;

    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void initializeStatistics() override;
    virtual void initializeFlags() override;
    virtual void handleMessageWhenUp(cMessage *msg) override;

    // finish
    virtual void finish() override;

    // event handlers
    virtual void handleEndIFGPeriod();
    virtual void handleEndTxPeriod();
    virtual void handleEndPausePeriod();
    virtual void handleSelfMessage(cMessage *msg) override;

    // helpers
    virtual void startFrameTransmission();
    virtual void handleUpperPacket(Packet *pk) override;
    void fill_mcast_tree_info(EthernetMacHeader* eth_header, bool drop_notif=false);
    bool is_tree_idx_down(unsigned long flow_id, int src_gpu, int tree_idx, simtime_t recovery_time=0);
    virtual void processMsgFromNetwork(EthernetSignal *signal);
    virtual void processReceivedDataFrame(Packet *packet, const Ptr<const EthernetMacHeader>& frame);
    virtual void processPauseCommand(int pauseUnits);
    virtual void scheduleEndIFGPeriod();
    virtual void scheduleEndPausePeriod(int pauseUnits);
    virtual void beginSendFrames();
    bool is_allowed_to_send_by_PFC();
    int global_collective_alg = -1;
    int global_collective = -1;
    double get_rate_dividier(int collective, int algorithm, bool optireduce_in_reduction_phase);
    void pushPacketToQeueue(Packet *packet);
  public:

    // statistics
    simtime_t totalSuccessfulRxTime;    // total duration of successful transmissions on channel

    // dcqcn
    void dcqcn_generate_congestion_notification(Packet* packet);
    void cc_cancel_all_timers(CCFlowInfo* flow_info);
    void cc_cancel_and_delete_all_timers(CCFlowInfo* flow_info);
    void cc_schedule_timer(cMessage *timeoutMsg, double gap);
    void cc_schedule_send_timer(CCFlowInfo* flow_info);
    void handle_cc_send_timer(unsigned long flow_id, int gpu_idx);
    void dcqcn_schedule_increase_timer(CCFlowInfo* dcqcn_flow_info);
    void handle_dcqcn_increase_timer(unsigned long flow_id, int gpu_idx);
    void dcqcn_schedule_alpha_timer(CCFlowInfo* dcqcn_flow_info);
    void handle_dcqcn_alpha_timer(unsigned long flow_id, int gpu_idx);
    void dcqcn_activate_appropriate_rate_increase(CCFlowInfo* dcqcn_flow_info);
    void apply_FR(CCFlowInfo* dcqcn_flow_info);
    void apply_HI(CCFlowInfo* dcqcn_flow_info);
    void apply_AI(CCFlowInfo* dcqcn_flow_info);
    void handle_cnp_packet(unsigned long cnp_flow_id, int cnp_src_gpu_id);
    int packet_creation_batch_size = -1;
    int queue_insert_thresh = -1;

    // for rate calculation
    int num_gpus = -1;
    int num_flows_per_ml_query = -1;
    int num_servers_under_each_leaf = -1;
    bool use_fattree = false;

    int server_module_index;

    // INA
    std::unordered_map<unsigned long, std::unordered_set<unsigned long>> queryid_to_seqnum_map;
    unsigned long aggregator_num = 0; // number of aggregators in the switch

    unsigned long long lightSentDataPktNum = 0;
    unsigned long long lightRcvdDataPktNum = 0;

    unsigned long long lightOOOdatacount = 0;
    bool measureOOO = false;
    // query id --> (expected seq, set of received seqs that are not in order)
    std::map<unsigned long long, std::pair<unsigned long, std::set<unsigned long>>> query_seq_map;

    // for opti reduce
    bool use_becca_optireduce = false;
    bool use_cidr_optireduce = false;

    // using tree initiation packet
    bool initiate_tree = false;

    bool partial_mcast_programmable_cores = false;
    std::map<std::tuple<unsigned long, int, unsigned long>, std::map<unsigned long, Packet*>> query_torgroup_seqnum_to_flowid_packets;
    std::map<std::tuple<unsigned long, int>, unsigned long> query_torgroup_to_candidate_flowid;
    std::map<std::tuple<unsigned long, int>, unsigned long> query_torgroup_to_highest_seqnum_before_controller_setup;
    std::map<std::tuple<unsigned long, int, int>, simtime_t> flowid_srcgpu_treeidx_to_recoverytime;

    bool isPacketAppropriateForMulticast(std::string pkt_name);
    bool should_packet_be_grouped(unsigned long query_id,
            int tor_group_idx,
            unsigned long seq_num,
            simtime_t controller_setup_finish_time);

    // CIDR
    bool partial_mcast_use_cidr_agg = false;
    bool partial_mcast_use_cidr_core = false;
    std::map<std::tuple<unsigned long, std::string, unsigned long>, std::map<unsigned long, Packet*>> query_cidrid_seqnum_to_flowid_packets;
    std::map<std::tuple<unsigned long, std::string>, unsigned long> query_cidrid_to_candidate_flowid;
    std::map<std::tuple<unsigned long, std::string, unsigned long>, std::map<unsigned long, Packet*>> query_corecidrid_seqnum_to_flowid_packets;
    std::map<std::tuple<unsigned long, std::string>, unsigned long> query_corecidrid_to_candidate_flowid;

    // swift
    void create_ack_header(Packet *ack_pkt);
    void swift_update_send_rates(simtime_t send_time,
            unsigned long flow_id, int src_gpu_idx, int hop_count);
    simtime_t swift_calculate_fabric_target_delay(int hop_counts, double cwnd);
    double swift_cwnd_to_rate(double cwnd_pkt_num, simtime_t rtt);
    double swift_rate_to_cwnd_pkt_num(double rate, simtime_t rtt);
    // cwnds are per packet and are transmitted to per byte at last
    double swift_ai;     // fabric additive increament
    double swift_beta;       // fabric multiplicative decrease constant
    double swift_max_mdf;        // fabric maximum multiplicative decrease factor
    simtime_t swift_base_target_delay;    // base target delay
    double swift_per_hop_scaling_factor;  // h: per hop scaling factor
    double swift_fs_max_cwnd;     // max cwnd for target scaling
    double swift_fs_min_cwnd;     // min cwnd for target scaling
    double swift_fs_range;        // max scaling range

    bool limit_all_to_all_rate = false;

};

#endif // ifndef __INET_AUGMENTEDETHERMACFULLDUPLEX_H

