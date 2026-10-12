//
// Copyright (C) 2006 Levente Meszaros
// Copyright (C) 2011 Zoltan Bojthe
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

#include "inet/common/Simsignals.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/ethernet/EtherEncap.h"
#include "inet/linklayer/ethernet/EtherFrame_m.h"
#include "./AugmentedEtherMacFullDuplex.h"
#include "inet/linklayer/ethernet/EtherPhyFrame_m.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/transportlayer/tcp_common/TcpHeader_m.h"
#include "inet/transportlayer/udp/UdpHeader_m.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/transportlayer/common/CrcMode_m.h"

using namespace inet;

// TODO: refactor using a statemachine that is present in a single function
// TODO: this helps understanding what interactions are there and how they affect the state

Define_Module(AugmentedEtherMacFullDuplex);

AugmentedEtherMacFullDuplex::AugmentedEtherMacFullDuplex()
{
}

AugmentedEtherMacFullDuplex::~AugmentedEtherMacFullDuplex()
{
    recordScalar("lightThroughputBitsSent", light_throughput_bits_sent);
    recordScalar("lightThroughputBitsRcvd", light_throughput_bits_rcvd);
    recordScalar("lightSentDataPktNum", lightSentDataPktNum);
    recordScalar("lightRcvdDataPktNum", lightRcvdDataPktNum);
    recordScalar("lightOOOdatacount", lightOOOdatacount);

    for (auto it = flow_id_to_info_mapper.begin(); it != flow_id_to_info_mapper.end(); it++) {
        cc_cancel_and_delete_all_timers(it->second);
//        if (it->second->packet_list.size() != 0) {
//            std::cout << getFullPath() << ": Flow with id: " << it->second->flow_id << " didn't finish. Number of "
//                    "outstanding pkts: " << it->second->packet_list.size() << endl;
//            it->second->print_info();
//            std::cout << "------------------------" << endl;
//        }
        for (auto packet_it = it->second->packet_list.begin(); packet_it != it->second->packet_list.end(); packet_it++)
            delete (*packet_it);
        delete it->second;
    }
    flow_id_to_info_mapper.clear();

    last_pkt_arrived_record.clear();

    flow_id_to_receiver_cnp_generation_time_mapper.clear();
    flow_id_to_sender_cnp_generation_time_mapper.clear();

    unsigned long ooo_remaining = 0;
    for (auto it = query_seq_map.begin(); it != query_seq_map.end(); it++) {
        if (it->second.second.size() > 0)
            ooo_remaining += it->second.second.size();
    }
    if (ooo_remaining > 0)
        std::cout << getFullPath() << ": " << ooo_remaining << " records remaining when measuring ooo" << endl;

    if (stored_base_packets_for_recovery.size() > 0) {
        EV << getFullPath() << std::endl;
        for (auto store_pkt_info : stored_base_packets_for_recovery) {
            unsigned long flowid = std::get<0>(store_pkt_info.first);
            unsigned long seqnum = std::get<1>(store_pkt_info.first);
            std::cout << flowid << "," << seqnum << " remaining in stored_base_packets_for_recovery" << std::endl;
            delete store_pkt_info.second;
        }
    }
//    else
//        std::cout << getFullPath() << ": stored_base_packets_for_recovery is empty!" << std::endl;
}

void AugmentedEtherMacFullDuplex::initialize(int stage)
{
    EtherMacBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        if (!par("duplexMode"))
            throw cRuntimeError("Half duplex operation is not supported by AugmentedEtherMacFullDuplex, use the EtherMac module for that! (Please enable csmacdSupport on EthernetInterface)");
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        use_udp = par("use_udp");

        if (use_udp) {
            std::string rate_control_type_str = par("rate_control_type");
            if (rate_control_type_str.compare("dcqcn") == 0) {
                EV << "Using DCQCN" << endl;
                rate_control_type = dcqcn;
            } else if (rate_control_type_str.compare("swift") == 0) {
                EV << "Using swift" << endl;
                rate_control_type = swift;
            } else if (rate_control_type_str.compare("sharp") == 0) {
                EV << "Using sharp rate control" << endl;
                rate_control_type = sharp;
            } else
                throw cRuntimeError("unknown rate_control_type_str");
        } else
            rate_control_type = -1;

        initiate_tree = bool(par("initiate_tree"));

        num_gpus = int(par("num_gpus"));
        num_flows_per_ml_query = int(par("num_flows_per_ml_query"));
        num_servers_under_each_leaf = int(par("num_servers"));
        use_fattree = bool(par("use_fattree"));

        server_module_index = getParentModule()->getParentModule()->getIndex();
        mac_module_index = getParentModule()->getIndex();

        measureOOO = bool(par("measureOOO"));

        limit_all_to_all_rate = bool(par("limit_all_to_all_rate"));

        if (rate_control_type >= 0) {
            if (rate_control_type == dcqcn) {
                cnp_generation_gap = par("cnp_generation_gap");
                if (cnp_generation_gap <= 0)
                    throw cRuntimeError("cnp_generation_gap <= 0");
                sender_cnp_gap_applied = bool(par("sender_cnp_gap_applied"));
                dcqcn_F = par("dcqcn_F");
                dcqcn_HI_factor = par("dcqcn_HI_factor");
                dcqcn_AI_factor = par("dcqcn_AI_factor");
            } else if (rate_control_type == swift) {
                // initialize swift params
                swift_ai = par("additive_increase_constant");
                swift_beta = par("multiplicative_decrease_constant");
                swift_max_mdf = par("max_mdf");
                swift_base_target_delay = par("base_target_delay");
                swift_per_hop_scaling_factor = par("per_hop_scaling_factor");
                swift_fs_max_cwnd = par("max_cwnd_target_scaling");
                swift_fs_min_cwnd = par("min_cwnd_target_scaling");
                swift_fs_range = par("max_scaling_range");

                if (swift_ai <= 0 ||
                        swift_beta <= 0 ||
                        swift_max_mdf <= 0 ||
                        swift_base_target_delay <= 0 ||
                        swift_per_hop_scaling_factor <= 0 ||
                        swift_fs_max_cwnd <= 0 ||
                        swift_fs_min_cwnd <= 0 ||
                        swift_fs_range <= 0
                        )
                    throw cRuntimeError("Invalid swift params");
            } else if (rate_control_type == sharp) {
                //todo: set the params for ina transport
            }

            dcqcn_mtu = par("dcqcn_mtu");
            if (dcqcn_mtu <= 0)
                throw cRuntimeError("dcqcn_mtu <= 0");

            dcqcn_initial_rate_devider = par("dcqcn_initial_rate_devider");
            dcqcn_limit_max_rate = par("dcqcn_limit_max_rate");
            // if dcqcn_initial_rate_devider is zero, it is not set
            // it it's > 0, it is manually set
            // if it's 0 <, we are dealing with combinational queries
            if (dcqcn_initial_rate_devider < 0) {
                if (server_module_index == 0)
                    std::cout << "Dynamically setting the initial_rate_devider as the collective is combinational!" << endl;
            } else if (dcqcn_initial_rate_devider == 0) {
                if (server_module_index == 0)
                    std::cout << "initial_rate_devider not set!" << endl;
                dcqcn_limit_max_rate = false;
            } else {
                if (server_module_index == 0) {
                    std::cout << "rate divider: " << dcqcn_initial_rate_devider << ", initial_rate initialized to " << get_link_util() / dcqcn_initial_rate_devider << endl;
                    if (dcqcn_limit_max_rate)
                        std::cout << "max_rate capped to " << get_link_util() / dcqcn_initial_rate_devider << endl;
                }
            }

            packet_creation_batch_size = par("packet_creation_batch_size");
            queue_insert_thresh = par("queue_insert_thresh");
            if (queue_insert_thresh <= 0)
                std::cout << "Warning: queue_insert_thresh <= 0" << endl;
        }

        aggregator_num = std::stoul(par("aggregator_num"));

        use_becca_optireduce = bool(par("use_becca_optireduce"));
        use_cidr_optireduce = bool(par("use_cidr_optireduce"));

        partial_mcast_programmable_cores = bool(par("partial_mcast_programmable_cores"));

        partial_mcast_use_cidr_agg = bool(par("partial_mcast_use_cidr_agg"));
        partial_mcast_use_cidr_core = bool(par("partial_mcast_use_cidr_core"));

        if (partial_mcast_programmable_cores && partial_mcast_use_cidr_core)
            throw cRuntimeError("partial_mcast_programmable_cores & partial_mcast_use_cidr_core cannot be enabled together!");


        if (!partial_mcast_use_cidr_agg && partial_mcast_use_cidr_core)
            throw cRuntimeError("How is CIDR enabled on the core but not the aggregate!!!");

        // For SC 2024 Ring based loss recovery for multicast
        std::string recovery_type_str = par("recovery_type");
        recovery_wait_time = double(par("recovery_wait_time"));
        hierarchical_src_recovery_wait_time = double(par("hierarchical_src_recovery_wait_time"));
        hierarchical_src_broadcast_threshold = double(par("hierarchical_src_broadcast_threshold"));
        hierarchical_seqnum_gap_bdp_multiplier = double(par("hierarchical_seqnum_gap_bdp_multiplier"));

        if (recovery_type_str.compare("notif") == 0) {
            recovery_type = DROP_NOTIF;
        }
//        else if (recovery_type_str.compare("deflect") == 0) {
//            recovery_type = DEFLECT_ON_DROP;
//        }
        else if (recovery_type_str.compare("ring_based") == 0) {
            if (recovery_wait_time <= 0)
                throw cRuntimeError("Invalid recovery_wait_time");
            recovery_type = RING_BASED;
        } else if (recovery_type_str.compare("hierarchical") == 0) {
            if (recovery_wait_time <= 0)
                throw cRuntimeError("Invalid recovery_wait_time");
            recovery_type = HIERARCHICAL;
        }else
           recovery_type = -1;

        beginSendFrames();    //FIXME choose an another stage for it
    }
}

void AugmentedEtherMacFullDuplex::initializeStatistics()
{
    EtherMacBase::initializeStatistics();

    // initialize statistics
    totalSuccessfulRxTime = 0.0;
}

void AugmentedEtherMacFullDuplex::initializeFlags()
{
    EtherMacBase::initializeFlags();

    duplexMode = true;
    physInGate->setDeliverOnReceptionStart(false);
}

void AugmentedEtherMacFullDuplex::handleMessageWhenUp(cMessage *msg)
{
    if (channelsDiffer)
        readChannelParameters(true);

    if (msg->isSelfMessage())
        handleSelfMessage(msg);
    else if (msg->getArrivalGateId() == upperLayerInGateId)
        handleUpperPacket(check_and_cast<Packet *>(msg));
    else if (msg->getArrivalGate() == physInGate)
        processMsgFromNetwork(check_and_cast<EthernetSignal *>(msg));
    else
        throw cRuntimeError("Message received from unknown gate!");
    processAtHandleMessageFinished();
}

void AugmentedEtherMacFullDuplex::erase_from_waitedseqs(unsigned long query_id,
        int src_gpu_idx,
        unsigned long seq_num,
        bool is_first_caller) {

    Enter_Method("erase_from_waitedseqs");


    auto query_srcgpu_seqnum_tuple = std::make_tuple(query_id, src_gpu_idx, seq_num);

    if (query_srcgpu_to_waitedseqs.find(query_srcgpu_seqnum_tuple) != query_srcgpu_to_waitedseqs.end())
        query_srcgpu_to_waitedseqs.erase(query_srcgpu_seqnum_tuple);

    std::string self_mac_path = getFullPath();
    if (is_first_caller) {
        // call last_packet_arrived for other gpus
        for (int i = 0; i < num_gpus; i++) {

            // skip self
            if (i == mac_module_index)
                continue;

            std::size_t start = self_mac_path.find("eth[");
            if (start == std::string::npos)
                throw cRuntimeError("mac was not found in the name!");
            start += 4; // move to first char after "server["
            std::size_t end = self_mac_path.find(']', start);
            if (end == std::string::npos)
                throw cRuntimeError("2) End not found!");
            std::string gpu_mac_path = self_mac_path.substr(0, start) +
                    std::to_string(i) +
                    self_mac_path.substr(end);

//            std::cout << "updated from " << self_mac_path << " to " <<
//                    gpu_mac_path << std::endl;

            AugmentedEtherMacFullDuplex* mac_module =
                    check_and_cast<AugmentedEtherMacFullDuplex*>(getModuleByPath(gpu_mac_path.c_str()));
            mac_module->erase_from_waitedseqs(query_id, src_gpu_idx, seq_num, false);
        }
    }

}

void AugmentedEtherMacFullDuplex::fullfill_waiting_requests(unsigned long query_id,
        int src_gpu_idx,
        unsigned long seq_num,
        bool is_first_caller,
        bool hierarchical_resent_from_source) {

    Enter_Method("fullfill_waiting_requests");

    auto query_srcgpu_seqnum_tuple = std::make_tuple(query_id, src_gpu_idx, seq_num);

    if (recovery_request_waitlist.find(query_srcgpu_seqnum_tuple) !=
            recovery_request_waitlist.end()) {
        if (recovery_request_waitlist[query_srcgpu_seqnum_tuple].size() == 0)
            throw cRuntimeError("Why is the packet list empty!!");

        EV << "Fullfilling " <<
                recovery_request_waitlist[query_srcgpu_seqnum_tuple].size() << " waiting requests..." << std::endl;
        EV << "hierarchical_resent_from_source: " << hierarchical_resent_from_source << std::endl;

        for (auto pkt : recovery_request_waitlist[query_srcgpu_seqnum_tuple]) {
            // don't do anything since/if the source already multicastedto everyone under this ToR
            if (hierarchical_resent_from_source) {
                // just to be safe!
                perform_deep_delete(pkt);
            } else
                pushPacketToQeueue(pkt);
        }

        if (transmitState == TX_IDLE_STATE) {
            // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
            // it is nullptr before popping a packet
            EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
            if (!txQueue->isEmpty() && !currentTxFrame)
                popTxQueue();
            beginSendFrames();
        }

        recovery_request_waitlist.erase(query_srcgpu_seqnum_tuple);
    }

    std::string self_mac_path = getFullPath();
    if (is_first_caller) {
        // call last_packet_arrived for other gpus
        for (int i = 0; i < num_gpus; i++) {

            // skip self
            if (i == mac_module_index)
                continue;

            std::size_t start = self_mac_path.find("eth[");
            if (start == std::string::npos)
                throw cRuntimeError("mac was not found in the name!");
            start += 4; // move to first char after "server["
            std::size_t end = self_mac_path.find(']', start);
            if (end == std::string::npos)
                throw cRuntimeError("2) End not found!");
            std::string gpu_mac_path = self_mac_path.substr(0, start) +
                    std::to_string(i) +
                    self_mac_path.substr(end);

//            std::cout << "updated from " << self_mac_path << " to " <<
//                    gpu_mac_path << std::endl;

            AugmentedEtherMacFullDuplex* mac_module =
                    check_and_cast<AugmentedEtherMacFullDuplex*>(getModuleByPath(gpu_mac_path.c_str()));
            mac_module->fullfill_waiting_requests(query_id, src_gpu_idx, seq_num, false, hierarchical_resent_from_source);
        }
    }
}

bool AugmentedEtherMacFullDuplex::is_pkt_lost_for_this_server(unsigned long query_id,
        int src_gpu_idx,
        unsigned long seq_num,
        bool is_first_caller) {

    Enter_Method("is_pkt_lost_for_this_server");

    auto query_srcgpu_seqnum_tuple = std::make_tuple(query_id, src_gpu_idx, seq_num);
    if (query_srcgpu_to_waitedseqs.find(query_srcgpu_seqnum_tuple) !=
            query_srcgpu_to_waitedseqs.end())
        return true;

    std::string self_mac_path = getFullPath();
    if (is_first_caller) {
        // call last_packet_arrived for other gpus
        for (int i = 0; i < num_gpus; i++) {

            // skip self
            if (i == mac_module_index)
                continue;

            std::size_t start = self_mac_path.find("eth[");
            if (start == std::string::npos)
                throw cRuntimeError("mac was not found in the name!");
            start += 4; // move to first char after "server["
            std::size_t end = self_mac_path.find(']', start);
            if (end == std::string::npos)
                throw cRuntimeError("2) End not found!");
            std::string gpu_mac_path = self_mac_path.substr(0, start) +
                    std::to_string(i) +
                    self_mac_path.substr(end);

//            std::cout << "updated from " << self_mac_path << " to " <<
//                    gpu_mac_path << std::endl;

            AugmentedEtherMacFullDuplex* mac_module =
                    check_and_cast<AugmentedEtherMacFullDuplex*>(getModuleByPath(gpu_mac_path.c_str()));

            if (mac_module->is_pkt_lost_for_this_server(query_id, src_gpu_idx, seq_num, false))
                return true;
        }
    }

    return false;
}

void AugmentedEtherMacFullDuplex::last_pkt_arrived(unsigned long query_id,
        int src_gpu_idx,
        bool is_last_packet_origin) {

    Enter_Method("last_pkt_arrived");

    auto query_srcgpu_tuple = std::make_tuple(query_id, src_gpu_idx);
    last_pkt_arrived_record.insert(query_srcgpu_tuple);

    std::string self_mac_path = getFullPath();
    if (is_last_packet_origin) {
        // call last_packet_arrived for other gpus
        for (int i = 0; i < num_gpus; i++) {

            // skip self
            if (i == mac_module_index)
                continue;

            std::size_t start = self_mac_path.find("eth[");
            if (start == std::string::npos)
                throw cRuntimeError("mac was not found in the name!");
            start += 4; // move to first char after "server["
            std::size_t end = self_mac_path.find(']', start);
            if (end == std::string::npos)
                throw cRuntimeError("2) End not found!");
            std::string gpu_mac_path = self_mac_path.substr(0, start) +
                    std::to_string(i) +
                    self_mac_path.substr(end);

//            std::cout << "updated from " << self_mac_path << " to " <<
//                    gpu_mac_path << std::endl;

            AugmentedEtherMacFullDuplex* mac_module =
                    check_and_cast<AugmentedEtherMacFullDuplex*>(getModuleByPath(gpu_mac_path.c_str()));
            mac_module->last_pkt_arrived(query_id, src_gpu_idx, false);
        }
    }

    auto query_found_it = query_srcgpu_to_lostseqs.find(query_srcgpu_tuple);
    if (query_found_it == query_srcgpu_to_lostseqs.end())
        return;

    EV << ": Last packet arrived for query: " << query_id <<
            " and src gpu: " << src_gpu_idx <<
            " -- is the original receiver of the last packet!? " << is_last_packet_origin << std::endl;


    EV << "Requiring " << query_found_it->second.size() << " retransmission!" << std::endl;
    for (auto seqnum : query_found_it->second) {
        create_and_send_recovery_request(query_id, src_gpu_idx, seqnum);
    }
    query_srcgpu_to_lostseqs.erase(query_srcgpu_tuple);
    hierarchical_max_seqnum.erase(query_srcgpu_tuple);
}


void AugmentedEtherMacFullDuplex::create_recovery_request_version_of_pkt(Packet *packet,
        MacAddress recovery_mac_address) {

//    return;

    EV << "create_recovery_request_version_of_pkt" << endl;

    std::string pkt_name = packet->getName();
    if (pkt_name != "data") {
        std::cout << "packet name: " << packet->getName() << std::endl;
        throw cRuntimeError("create_recovery_request_version_of_pkt called for non-data packet!!");
    }

    // reset ttl and hopcount for original packet
    auto original_pkt = packet->dup();

    // no need to change the name
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();
    auto ip_header = packet->removeAtFront<Ipv4Header>();
    auto udp_header = packet->removeAtFront<UdpHeader>();
    auto chunk = packet->removeAtFront<GenericAppMsg>();
    auto fcs_header = packet->peekAtBack<EthernetFcs>(ETHER_FCS_BYTES);

    if (eth_header->getCollective_alg() != BCAST_PARTIAL_MULTICAST)
        throw cRuntimeError("For now, we assumed that random drop is only implemented for PEEL!");

    b fake_payload_length = B(64);
//    fake_payload_length -= phy_header->getChunkLength();
    fake_payload_length -= eth_header->getChunkLength();
    fake_payload_length -= ip_header->getChunkLength();
    fake_payload_length -= udp_header->getChunkLength();
    fake_payload_length -= fcs_header->getChunkLength();

    chunk->setChunkLength(fake_payload_length);
    packet->insertAtFront(chunk);

    // setIs_pkt_dropped
    packet->insertAtFront(udp_header);
    packet->insertAtFront(ip_header);

    eth_header->setIs_loss_recovery_request(true);
    eth_header->setOriginal_pkt(original_pkt);
    eth_header->setDest(recovery_mac_address);
    // disable all peel identifiers
    eth_header->setIs_pkt_dropped(false);
    eth_header->setIs_loss_recovery_response(false);
    eth_header->setDst_tor_idx(-1);
    eth_header->setMulticast_pkt_in_programmable_core(false);
    eth_header->setMulticast_pkt_in_CIDR_agg(false);
    eth_header->setMulticast_pkt_in_CIDR_core(false);
    packet->insertAtFront(eth_header);

    // remove the phy header and set front iterator to zero
//    packet->insertAtFront(phy_header);
//    packet->setFrontIteratorPosition(packetPosition);

    if (packet->getByteLength() < 64)
        throw cRuntimeError("packet->getBitLength() < B(64)");

}

void AugmentedEtherMacFullDuplex::send_p2p_recovery_request(int recovery_server_idx, Packet* pkt, int specific_source_mac_index) {

    EV << "send_p2p_recovery_request called for " << pkt->str() << std::endl;

    // get the mac address of the recovery server!
    std::string recovery_mac_path = getFullPath();

    // Replace server[X]
    {
        std::size_t start = recovery_mac_path.find("server[");
        if (start == std::string::npos)
            throw cRuntimeError("server[...] was not found in the path!");

        start += 7; // after "server["
        std::size_t end = recovery_mac_path.find(']', start);
        if (end == std::string::npos)
            throw cRuntimeError("Closing ] for server[...] was not found!");

        recovery_mac_path =
            recovery_mac_path.substr(0, start) +
            std::to_string(recovery_server_idx) +
            recovery_mac_path.substr(end);
    }

    // Replace eth[Y] only if requested
    // use a specific mac index when it is > -1 in hierarchical mode
    if (specific_source_mac_index >= 0) {
        EV << "Using specific mac: " << specific_source_mac_index << " when retrieving from the source!" << std::endl;
        std::size_t start = recovery_mac_path.find("eth[");
        if (start == std::string::npos)
            throw cRuntimeError("eth[...] was not found in the path!");

        start += 4; // after "eth["
        std::size_t end = recovery_mac_path.find(']', start);
        if (end == std::string::npos)
            throw cRuntimeError("Closing ] for eth[...] was not found!");

        recovery_mac_path =
            recovery_mac_path.substr(0, start) +
            std::to_string(specific_source_mac_index) +
            recovery_mac_path.substr(end);
    }

//    EV << self_mac_path << std::endl;
    EV << "recovery_mac_path: " << recovery_mac_path << std::endl;

    auto eth_header = pkt->peekAtFront<EthernetMacHeader>();
    if (getMacAddress() != eth_header->getDest()) {

        // properly set requested packet mac address if required!

//        std::cout << "original packet mac address: " << eth_header->getDest() << std::endl;
//        std::cout << "self mac address: " << getMacAddress() << std::endl;

        b packetPosition = pkt->getFrontOffset();
        pkt->setFrontIteratorPosition(b(0));
        auto phy_header = pkt->removeAtFront<EthernetPhyHeader>();
        auto m_ethernet_header = pkt->removeAtFront<EthernetMacHeader>();
        m_ethernet_header->setDest(getMacAddress());
        pkt->insertAtFront(m_ethernet_header);
        pkt->insertAtFront(phy_header);
        pkt->setFrontIteratorPosition(packetPosition);

        eth_header = pkt->peekAtFront<EthernetMacHeader>();
//        throw cRuntimeError("server mac address is not equal to the address saved in the packet!");
    }

    AugmentedEtherMacFullDuplex* recover_mac_module =
            check_and_cast<AugmentedEtherMacFullDuplex*>(getModuleByPath(recovery_mac_path.c_str()));

    EV << "recovery mac address: " << recover_mac_module->getMacAddress() << std::endl;

    create_recovery_request_version_of_pkt(pkt, recover_mac_module->getMacAddress());

    EV << "request recovery packet is " << pkt->str() << std::endl;

    pushPacketToQeueue(pkt);

    if (transmitState == TX_IDLE_STATE) {
        // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
        // it is nullptr before popping a packet
        EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
        if (!txQueue->isEmpty() && !currentTxFrame)
            popTxQueue();
        beginSendFrames();
    }

}

void AugmentedEtherMacFullDuplex::create_and_send_recovery_request(unsigned long query_id,
        int src_gpu_idx,
        unsigned long seqnum) {

    auto query_srcgpu_seqnum_tuple = std::make_tuple(query_id, src_gpu_idx, seqnum);
    auto query_srcgpu_tuple = std::make_tuple(query_id, src_gpu_idx);
    auto lost_pkt_itr = lost_pkt_map.find(query_srcgpu_seqnum_tuple);
    if (lost_pkt_itr == lost_pkt_map.end()) {
        std::cout << "query: " << query_id << std::endl;
        std::cout << "src gpu: " << src_gpu_idx << std::endl;
        std::cout << "seq num: " << seqnum << std::endl;
        throw cRuntimeError("Lost packet is not recorded!!!");
    }

    // comment if below out since
    // it might be missing if we send packet right away without actually scheduling it in hierarchical
//    if (query_srcgpu_to_lostseqs.find(query_srcgpu_tuple) == query_srcgpu_to_lostseqs.end())
//        throw cRuntimeError("Loss packet info missing... query_srcgpu_to_lostseqs.find(query_srcgpu_tuple) == query_srcgpu_to_lostseqs.end()");

    auto timeoutMsg = std::get<0>(lost_pkt_itr->second);
    Packet* pkt = std::get<1>(lost_pkt_itr->second);

//    std::cout << (timeoutMsg != nullptr) << std::endl;
//    std::cout << (timeoutMsg->isScheduled()) << std::endl;
    if (timeoutMsg != nullptr && timeoutMsg->isScheduled())
        cancelEvent(timeoutMsg);
    delete timeoutMsg;

    lost_pkt_map.erase(lost_pkt_itr);

    auto eth_header = pkt->peekAtFront<EthernetMacHeader>();
    std::list<unsigned long> involved_servers_list;
    std::set<unsigned long> involved_servers_set;

    for (int k = 0; k < eth_header->getInvolved_servers_listArraySize(); k++) {
        involved_servers_list.push_back(eth_header->getInvolved_servers_list(k));
        involved_servers_set.insert(eth_header->getInvolved_servers_list(k));
    }

    // find the node before you in the ring
    if (involved_servers_list.size() == 0)
        throw cRuntimeError("We need more than 0 involved servers here");

    int recovery_server_idx = -1;
    auto involved_server_itr = involved_servers_list.begin();
    int specific_source_mac_index = -1; // in hierarchical, when going to the source, we want to go to the actual source mac
    if (recovery_type == RING_BASED) {
        while (involved_server_itr != involved_servers_list.end()) {
            if ((*involved_server_itr) == server_module_index) {
    //            std::cout << server_module_index << " looking for " << (*involved_server_itr) << std::endl;
                if (involved_server_itr == involved_servers_list.begin()) {
                    std::cout << "Really!" << std::endl;
                    for (auto test : involved_servers_list)
                        std::cout << (test) << std::endl;
                    throw cRuntimeError("We are needing recovery at the source!!!!");
                }
                // read the previous server in the list
                involved_server_itr--;
                recovery_server_idx = (*involved_server_itr);
                break;
            }
            involved_server_itr++;
        }
    } else if (recovery_type == HIERARCHICAL) {
        auto custodian_elevator_idx_tuple = compute_custodian_elevator_nodes(
                query_id,
                src_gpu_idx,
                seqnum);

        int custodian_node_id = std::get<0>(custodian_elevator_idx_tuple);
        int elevator_node_id = std::get<1>(custodian_elevator_idx_tuple);

        if (server_module_index == custodian_node_id && server_module_index == elevator_node_id) {
            EV << "Server is both custodian and elevator... send to source" << std::endl;
            throw cRuntimeError("Server cannot be both custodian and elevator!");
            recovery_server_idx = (*involved_server_itr);
        } else if (server_module_index == custodian_node_id) {
            // should either contact the elevator or source if elevator is not in involved set
            if (involved_servers_set.find(elevator_node_id) == involved_servers_set.end()) {
                EV << "elevator is not in the set... send to the source" << std::endl;
                recovery_server_idx = (*involved_server_itr);
            } else {
                EV << "elevator is in the set... send to the elevator" << std::endl;
                recovery_server_idx = elevator_node_id;
            }
        } else {
            // non-custodians should contact the custodian if present in involved set or the source otherwise
            if (involved_servers_set.find(custodian_node_id) == involved_servers_set.end()) {
                EV << "custodian is not in the set... send to the source" << std::endl;
                recovery_server_idx = (*involved_server_itr);
            } else {
                EV << "custodian is in the set... send to the custodian" << std::endl;
                recovery_server_idx = custodian_node_id;
            }
        }

        if (recovery_server_idx == (*involved_server_itr)) {
            specific_source_mac_index = src_gpu_idx;
        }

    } else
        throw cRuntimeError("2) Unsupported recovery type!");

    if (recovery_server_idx < 0)
        throw cRuntimeError("Invalid recover server idx!!");

    EV << "recovery server: " << recovery_server_idx << std::endl;

    send_p2p_recovery_request(recovery_server_idx, pkt, specific_source_mac_index);

}

void AugmentedEtherMacFullDuplex::perform_deep_delete(Packet *pkt) {
    // for packet*
    EV << "Performing deep delete!" << std::endl;
    auto ethernet_header = pkt->peekAtFront<EthernetMacHeader>();
    for (int k = 0; k < ethernet_header->getCIDR_agg_pktArraySize(); k++) {
        delete ethernet_header->getCIDR_agg_pkt(k);
    }
    for (int k = 0; k < ethernet_header->getCIDR_core_pktArraySize(); k++) {
        delete ethernet_header->getCIDR_core_pkt(k);
    }
    delete pkt;
}

void AugmentedEtherMacFullDuplex::perform_deep_delete(const Packet *pkt) {
    // for const packet*
    perform_deep_delete(pkt->dup()); // deletes nested info
    delete pkt;
}



Packet* AugmentedEtherMacFullDuplex::create_base_pkt_deep_copy(Packet *base_packet) {

    // for peel, copy the base packet as the original packet for drop notification-based recovery
    auto new_pkt = base_packet->dup();

    // now we should make a deep copy of PEEL rules
    auto new_pkt_eth_header = new_pkt->removeAtFront<EthernetMacHeader>();

    // base packet is carrying the same packets as original
    // so we should not delete them
    // just erase them from base packet list and fill it with copies
    while(new_pkt_eth_header->getCIDR_agg_pktArraySize() != 0)
        new_pkt_eth_header->eraseCIDR_agg_pkt(0);
    while(new_pkt_eth_header->getCIDR_core_pktArraySize() != 0)
        new_pkt_eth_header->eraseCIDR_core_pkt(0);


    auto base_pkt_eth_header = base_packet->peekAtFront<EthernetMacHeader>();

    for (int k = 0; k < base_pkt_eth_header->getCIDR_agg_pktArraySize(); k++)
        new_pkt_eth_header->insertCIDR_agg_pkt(
                base_pkt_eth_header->getCIDR_agg_pkt(k)->dup());
    for (int k = 0; k < base_pkt_eth_header->getCIDR_core_pktArraySize(); k++)
        new_pkt_eth_header->insertCIDR_core_pkt(
                base_pkt_eth_header->getCIDR_core_pkt(k)->dup());

    new_pkt->insertAtFront(new_pkt_eth_header);

    return new_pkt;
}

void AugmentedEtherMacFullDuplex::appropriately_retransmit_packets_at_the_source(unsigned long flow_id,
        int src_gpu_idx,
        unsigned long seqnum,
        int max_ratrans_num) {

    if (recovery_type != HIERARCHICAL)
        throw cRuntimeError("appropriately_retransmit_packets_at_the_source is only for HIERARCHICAL recovery!");

    auto flow_srcgpu_seqnum_tuple = std::make_tuple(flow_id, src_gpu_idx, seqnum);
    if (packets_waiting_at_source.find(flow_srcgpu_seqnum_tuple) == packets_waiting_at_source.end())
        throw cRuntimeError("No info found: packets_waiting_at_source.find(flow_srcgpu_seqnum_tuple) == packets_waiting_at_source.end()");


    // set resent_from_source so that custodian does not send it to other later
    if (hierarchical_src_broadcast_threshold < 0 || hierarchical_src_broadcast_threshold > 1)
        throw cRuntimeError("hierarchical_src_broadcast_threshold < 0 || hierarchical_src_broadcast_threshold > 1");

    int rebcast_threshold = int(max_ratrans_num * hierarchical_src_broadcast_threshold);


    if (packets_waiting_at_source.find(flow_srcgpu_seqnum_tuple) == packets_waiting_at_source.end() ||
            packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.size() == 0)
        throw cRuntimeError("No info found at the source: packets_waiting_at_source.find(flow_srcgpu_seqnum_tuple) == packets_waiting_at_source.end()");

    bool should_rebroadcast = (packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.size() >= rebcast_threshold);

    EV << "max_ratrans_num: " << max_ratrans_num <<
            ", rebcast_threshold: " << rebcast_threshold <<
            ", num packets waiting at the source: " << packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.size() << std::endl;


    if (should_rebroadcast) {
        EV << "Source is re-broadcasting the packet..." << std::endl;

        auto flowid_seqnum_tuple = std::make_tuple(flow_id, seqnum);
        if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
            std::cout << "Looking for " << flow_id << " , " << seqnum << " in stored_base_packets_for_recovery" << std::endl;
            throw cRuntimeError("We don't have base record in stored_base_packets_for_recovery... that's imposible!");
        }

        // inject the broadcast message into the queue
        auto pkt_to_send = create_base_pkt_deep_copy(stored_base_packets_for_recovery[flowid_seqnum_tuple]);
        auto frame = pkt_to_send->removeAtFront<EthernetMacHeader>();
        frame->setHierarchical_resent_from_source(true);
        frame->setDont_drop_again(true);
        frame->setIs_loss_recovery_response(true);
        frame->setIs_loss_recovery_request(false);
        frame->setIs_pkt_dropped(false);
        pkt_to_send->insertAtFront(frame);
        pushPacketToQeueue(pkt_to_send);

        // delete the original messages
        for (auto original_pkt : packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second) {
            auto original_eth_header = original_pkt->peekAtFront<EthernetMacHeader>();
            if (original_eth_header->getCIDR_agg_pktArraySize() > 0 ||
                    original_eth_header->getCIDR_core_pktArraySize() > 0)
                throw cRuntimeError("At this point we assumed that original should not carry any nested CIDR packets!!");
            delete original_pkt;
        }
    } else {
        EV << "Source is re-transmitting packets separately" << std::endl;

        // here just make sure every packet is carrying the right dst ToR idx to call multicast downstream
        // original has already reached the destination so it doesn't carry any nested packets
        for (auto original_pkt : packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second) {
            auto original_eth_header = original_pkt->removeAtFront<EthernetMacHeader>();
            if (original_eth_header->getDst_tor_idx() < 0)
                throw cRuntimeError("Here, original packet should carry appropriate dst tor idx!");

            if (original_eth_header->getCIDR_agg_pktArraySize() > 0 ||
                    original_eth_header->getCIDR_core_pktArraySize() > 0)
                throw cRuntimeError("At this point we assumed that original should not carry any nested CIDR packets!!");

            original_eth_header->setHierarchical_resent_from_source(true);
            original_eth_header->setDont_drop_again(true);
            original_eth_header->setIs_loss_recovery_response(true);
            original_eth_header->setIs_loss_recovery_request(false);
            original_eth_header->setIs_pkt_dropped(false);
            original_pkt->insertAtFront(original_eth_header);

            EV << "source resending " << original_pkt->str() << std::endl;
            pushPacketToQeueue(original_pkt);
        }
    }

    packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.clear();
    packets_waiting_at_source.erase(flow_srcgpu_seqnum_tuple);

    if (transmitState == TX_IDLE_STATE) {
        // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
        // it is nullptr before popping a packet
        EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
        if (!txQueue->isEmpty() && !currentTxFrame)
            popTxQueue();
        beginSendFrames();
    }

}

void AugmentedEtherMacFullDuplex::handleSelfMessage(cMessage *msg)
{
    EV << "Self-message " << msg->str() << " received\n";

    std::string msg_name = msg->getName();

    if (msg_name == "send_later_timer") {

        unsigned long flow_id = msg->par("flow").longValue();
        unsigned long seqnum = msg->par("seqnum").longValue();
        auto flow_seqnum_tuple = std::make_tuple(flow_id, seqnum);
        if (send_later.find(flow_seqnum_tuple) == send_later.end())
            throw cRuntimeError("packet does not exist in send_later");

        pushPacketToQeueue(send_later[flow_seqnum_tuple]);
        send_later.erase(flow_seqnum_tuple);

        delete msg;

        if (transmitState == TX_IDLE_STATE) {
            // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
            // it is nullptr before popping a packet
            EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
            if (!txQueue->isEmpty() && !currentTxFrame)
                popTxQueue();
            beginSendFrames();
        }

    } else if (msg_name == "pkt_lost_timer") {

//        EV << "pkt_lost_timer received!" << std::endl;

        unsigned long query_id = msg->par("query").longValue();
        unsigned long seqnum = msg->par("seqnum").longValue();
        long srcgpu = msg->par("srcgpu").longValue();
        create_and_send_recovery_request(query_id, srcgpu, seqnum);
        auto query_srcgpu_tuple = std::make_tuple(query_id, srcgpu);
        query_srcgpu_to_lostseqs[query_srcgpu_tuple].erase(seqnum);
        if (query_srcgpu_to_lostseqs[query_srcgpu_tuple].size() == 0)
            query_srcgpu_to_lostseqs.erase(query_srcgpu_tuple);

        // dont' delete the message here. it is deleted in create_and_send_recovery_request as it is the countdown message
//        delete msg;

    } else if (msg_name == "src_retransmission_timer") {
        unsigned long flow_id = msg->par("flow").longValue();
        long srcgpu = msg->par("srcgpu").longValue();
        unsigned long seqnum = msg->par("seqnum").longValue();
        int max_ratrans_num = msg->par("max_ratrans_num").longValue();
        if (recovery_type != HIERARCHICAL)
            throw cRuntimeError("recovery_type != HIERARCHICAL when we received src_retransmission_timer");
        appropriately_retransmit_packets_at_the_source(flow_id, srcgpu, seqnum, max_ratrans_num);
        // delete the timer
        delete msg;
    } else if (msg == endTxMsg)
        handleEndTxPeriod();
    else if (msg == endIFGMsg)
        handleEndIFGPeriod();
    else if (msg == endPauseMsg)
        handleEndPausePeriod();
    else if (rate_control_type == dcqcn) {
        switch (msg->getKind()) {
            case CCFlowInfo::CC_SEND:
                handle_cc_send_timer(msg->par("flow_id").longValue(), msg->par("gpu_idx").longValue());
                break;
            case CCFlowInfo::DCQCN_INCREASE:
                handle_dcqcn_increase_timer(msg->par("flow_id").longValue(), msg->par("gpu_idx").longValue());
                break;
            case CCFlowInfo::DCQCN_ALPHA:
                handle_dcqcn_alpha_timer(msg->par("flow_id").longValue(), msg->par("gpu_idx").longValue());
                break;
            default:
                std::cout << msg->getName() << std::endl;
                throw cRuntimeError("DCQCN: Unknown message!");
        }
    } else if (rate_control_type == swift) {
        switch (msg->getKind()) {
            case CCFlowInfo::CC_SEND:
                handle_cc_send_timer(msg->par("flow_id").longValue(), msg->par("gpu_idx").longValue());
                break;
            default:
                throw cRuntimeError("Swift: Unknown message!");
        }
    } else if (rate_control_type == sharp) {
        switch (msg->getKind()) {
            case CCFlowInfo::CC_SEND:
                handle_cc_send_timer(msg->par("flow_id").longValue(), msg->par("gpu_idx").longValue());
                break;
            default:
                throw cRuntimeError("Swift: Unknown message!");
        }
    }
    else
        throw cRuntimeError("Unknown self message received!");
}

bool AugmentedEtherMacFullDuplex::isPacketAppropriateForMulticast(std::string pkt_name) {
    if (pkt_name.compare("data") == 0)
        return true;

    if (pkt_name.compare("treeInit") == 0)
        return true;

    if (pkt_name.compare("treeFin") == 0)
        return true;

    return false;
}

void AugmentedEtherMacFullDuplex::startFrameTransmission()
{
    ASSERT(currentTxFrame);
    EV_DETAIL << "Transmitting a copy of frame " << currentTxFrame << endl;
    Packet *frame = currentTxFrame->dup();    // note: we need to duplicate the frame because we emit a signal with it in endTxPeriod()
    const auto& hdr = frame->peekAtFront<EthernetMacHeader>();    // note: we need to duplicate the frame because we emit a signal with it in endTxPeriod()
    ASSERT(hdr);
    ASSERT(!hdr->getSrc().isUnspecified());

    std::string pkt_name = frame->getName();

    if (!hdr->getIs_pkt_dropped() &&
            !hdr->getIs_loss_recovery_request() &&
            !hdr->getIs_loss_recovery_response() &&
            (!isPacketAppropriateForMulticast(pkt_name) || !hdr->getOrca_pkt_seen_agent())) {

        //swift
        b position = frame->getFrontOffset();
        frame->setFrontIteratorPosition(b(0));
        auto eth_header = frame->removeAtFront<EthernetMacHeader>();

        auto ip_header = frame->removeAtFront<Ipv4Header>();
        simtime_t remote_queueing;
        if (!use_udp) {
            auto tcp_header = frame->peekAtFront<tcp::TcpHeader>();
            if (tcp_header->getRcvd_time_to_be_echod() > 0)
                remote_queueing = simTime() - tcp_header->getRcvd_time_to_be_echod();
            else
                remote_queueing = 0;
            EV << "SWIFT: rcvd time to be echod is " << tcp_header->getRcvd_time_to_be_echod() <<
                    " so remote queueing is " << remote_queueing << endl;
        } else
            remote_queueing = 0;

        std::string packet_name = frame->getName();
        if (use_udp && isPacketAppropriateForMulticast(packet_name)) {
            // only apply host_routing for data
            auto frame_dup = frame->dup();
            frame_dup->removeAtFront<UdpHeader>();
            auto chunk = frame_dup->removeAtFront<SliceChunk>();
            auto main_chunk = chunk->getChunk();
            Packet* temp = new Packet();
            temp->insertAtBack(main_chunk);
            auto payload = temp->popAtFront<GenericAppMsg>();
            bool use_host_assisted_routing = (payload->getDst_idx_listArraySize() > 0);
            if (use_host_assisted_routing) {
                EV << "use_udp && use_host_assisted_routing" << endl;

                eth_header->setDst_idx_listArraySize(payload->getDst_idx_listArraySize());
                for (int k = 0; k < payload->getDst_idx_listArraySize(); k++)
                    eth_header->setDst_idx_list(k, payload->getDst_idx_list(k));

                eth_header->setJump_to_idxArraySize(payload->getJump_to_idxArraySize());
                for (int k = 0; k < payload->getJump_to_idxArraySize(); k++) {
                    InnerList innerList;
                    innerList.setInnerArrayArraySize(payload->getJump_to_idx(k).getInnerArrayArraySize());
                    for (int t = 0; t < payload->getJump_to_idx(k).getInnerArrayArraySize(); t++) {
                        innerList.setInnerArray(t, payload->getJump_to_idx(k).getInnerArray(t));
                    }
                    eth_header->setJump_to_idx(k, innerList);
                }

                eth_header->setPorts_to_dest_idxArraySize(payload->getPorts_to_dest_idxArraySize());
                for(int k = 0; k < payload->getPorts_to_dest_idxArraySize(); k++) {
                    InnerList innerList;
                    innerList.setInnerArrayArraySize(payload->getPorts_to_dest_idx(k).getInnerArrayArraySize());
                    for (int t = 0; t < payload->getPorts_to_dest_idx(k).getInnerArrayArraySize(); t++) {
                        innerList.setInnerArray(t, payload->getPorts_to_dest_idx(k).getInnerArray(t));
        //                std::cout << payload->getPath_to_dest_idx(k).getInnerArray(t) << endl;
                    }
                    eth_header->setPorts_to_dest_idx(k, innerList);
        //            std::cout << "-----------------------------" << endl;
                }
        //        std::cout << "##################################" << endl << endl;

                // for failure recovery
                if (payload->getCurrent_tree_idx() >= 0) {
                    // only works for ace
                    // first fill the eth_header arrays then copy one into the port and jump list
                    EV << "Initializing tree idx to " << payload->getCurrent_tree_idx() << endl;
                    eth_header->setCurrent_tree_idx(payload->getCurrent_tree_idx());
                    for (int t = 0; t < payload->getAll_ports_to_dstArraySize(); ++t) {
                        const TwoDInnerList& twoDItem = payload->getAll_ports_to_dst(t);
                        eth_header->insertAll_ports_to_dst(twoDItem);
                    }
                    for (int t = 0; t < payload->getAll_jump_to_idxArraySize(); ++t) {
                        const TwoDInnerList& twoDItem = payload->getAll_jump_to_idx(t);
                        eth_header->insertAll_jump_to_idx(twoDItem);
                    }
                    fill_mcast_tree_info(eth_header.get());
                }

                // start_from_0 is related to the host itself
                eth_header->setStart_from_idx(1);

                EV << "getDst_idx_listArraySize(): " << eth_header->getDst_idx_listArraySize() << endl;
                EV << "getJump_to_idxArraySize(): " << eth_header->getJump_to_idxArraySize() << endl;
                EV << "getPorts_to_dest_idxArraySize(): " << eth_header->getPorts_to_dest_idxArraySize() << endl;

            }

            // INA
            eth_header->setIna_leaf_aggregation_num(payload->getIna_leaf_aggregation_num());
            eth_header->setIna_spine_aggregation_num(payload->getIna_spine_aggregation_num());
            eth_header->setIna_core_aggregation_num(payload->getIna_core_aggregation_num());
            eth_header->setTree_direction(TREE_UP);

            eth_header->setQuery_id(payload->getQuery_id());
            eth_header->setFlow_id(payload->getRequesterID());
            eth_header->setCollective_scale(payload->getCollective_scale());
            eth_header->setTotal_length(payload->getTotal_flow_size());
            eth_header->setCollective_alg(payload->getCollective_alg_type());
            eth_header->setDst_tor_idx(payload->getDst_tor_idx());
            eth_header->setSrc_gpu_idx(payload->getSrc_gpu_idx());
            eth_header->setIn_src_sharding(payload->getIn_src_sharding());
            eth_header->setUsing_orca(payload->getUsing_orca());
            eth_header->setUsing_elmo(payload->getUsing_elmo());
            if (rate_control_type == swift)
                eth_header->setSwift_send_time(simTime());
            if (packet_name.compare("data") == 0 && payload->getSeq_num() <= 0)
                throw cRuntimeError("Invalid seq number <= 0");
            eth_header->setSeq_num(payload->getSeq_num());

            eth_header->setIs_last_pkt(payload->getIs_last_pkt());
            for (int k = 0; k < payload->getInvolved_servers_listArraySize(); k++) {
                eth_header->insertInvolved_servers_list(
                        payload->getInvolved_servers_list(k));
            }

            delete temp;
            delete frame_dup;
        }
        eth_header->setRemote_queueing_time(remote_queueing);
        frame->insertAtFront(ip_header);
        frame->insertAtFront(eth_header);
        frame->setFrontIteratorPosition(position);
    } else if (!hdr->getIs_pkt_dropped() &&
            !hdr->getIs_loss_recovery_request() &&
            !hdr->getIs_loss_recovery_response()) {
        frame->setFrontIteratorPosition(b(0));
        frame->removeAtFront<EthernetPhyHeader>();
        frame->setFrontIteratorPosition(b(0));
    }

    inject_in_stored_base_packets_for_recovery(frame);

    // add preamble and SFD (Starting Frame Delimiter), then send out
    encapsulate(frame);

    light_throughput_bits_sent += frame->getBitLength();

    // send
    EV_INFO << "Transmission of " << frame << " started.\n";
    auto oldPacketProtocolTag = frame->removeTag<PacketProtocolTag>();
    frame->clearTags();
    auto newPacketProtocolTag = frame->addTag<PacketProtocolTag>();
    *newPacketProtocolTag = *oldPacketProtocolTag;
    delete oldPacketProtocolTag;
    auto signal = new EthernetSignal(frame->getName());
    signal->setSrcMacFullDuplex(duplexMode);
    if (sendRawBytes) {
        signal->encapsulate(new Packet(frame->getName(), frame->peekAllAsBytes()));
        delete frame;
    }
    else
        signal->encapsulate(frame);

    if (pkt_name.compare("data") == 0)
        lightSentDataPktNum++;

    send(signal, physOutGate);

    scheduleAt(transmissionChannel->getTransmissionFinishTime(), endTxMsg);
    changeTransmissionState(TRANSMITTING_STATE);
}

void AugmentedEtherMacFullDuplex::cc_cancel_all_timers(CCFlowInfo* flow_info) {
    if (flow_info->send_timer != nullptr && flow_info->send_timer->isScheduled())
        cancelEvent(flow_info->send_timer);

    if (rate_control_type != dcqcn && (flow_info->increase_timer != nullptr || flow_info->alpha_timer != nullptr))
        throw cRuntimeError("What are dcqcn timers running!!!?");
    if (flow_info->increase_timer != nullptr && flow_info->increase_timer->isScheduled())
        cancelEvent(flow_info->increase_timer);
    if (flow_info->alpha_timer != nullptr && flow_info->alpha_timer->isScheduled())
        cancelEvent(flow_info->alpha_timer);
}

void AugmentedEtherMacFullDuplex::cc_cancel_and_delete_all_timers(CCFlowInfo* flow_info) {
    cc_cancel_all_timers(flow_info);
    if (flow_info->send_timer != nullptr)
        delete flow_info->send_timer;
    if (flow_info->increase_timer != nullptr)
        delete flow_info->increase_timer;
    if (flow_info->alpha_timer != nullptr)
        delete flow_info->alpha_timer;
}

void AugmentedEtherMacFullDuplex::cc_schedule_timer(cMessage *timeoutMsg, double gap)
{
    EV << "cc_schedule_timer called" << endl;
    if (timeoutMsg->isScheduled()) {
        EV << "Timeout message " << timeoutMsg->str() << " is already scheduled" << endl;
        return;
    }
//    if (kind >= 0)
//        timeoutMsg->setKind(kind);
    simtime_t now = simTime();
    EV << "Scheduling " << timeoutMsg->getName() <<  " at " << (now + gap) << endl;
    scheduleAt(now + gap, timeoutMsg);
}

void AugmentedEtherMacFullDuplex::dcqcn_schedule_alpha_timer(CCFlowInfo* dcqcn_flow_info)
{
    EV << "dcqcn_schedule_alpha_timer called" << endl;
    double gap = dcqcn_flow_info->alpha_timer_gap;
    EV << "alpha gap is " << gap << endl;
    cc_schedule_timer(dcqcn_flow_info->alpha_timer, gap);
}

void AugmentedEtherMacFullDuplex::handle_dcqcn_alpha_timer(unsigned long flow_id, int gpu_idx) {

    if (rate_control_type != dcqcn)
        throw cRuntimeError("handle_dcqcn_alpha_timer called while rate_control_type != dcqcn");

    EV << "handle_dcqcn_alpha_timer for flow_id: " << flow_id << endl;

    std::tuple<int, unsigned long> gpu_id_flow_id_tuple = std::make_tuple(gpu_idx, flow_id);
    auto flow_id_found_itr = flow_id_to_info_mapper.find(gpu_id_flow_id_tuple);
    if (flow_id_found_itr == flow_id_to_info_mapper.end())
        throw cRuntimeError("handle_dcqcn_alpha_timer: How is flow not found!");

    EV << "Changing  alpha from " << flow_id_found_itr->second->alpha;
    flow_id_found_itr->second->alpha *= (1 - flow_id_found_itr->second->g);
    EV << " to " << flow_id_found_itr->second->alpha << endl;

    dcqcn_schedule_alpha_timer(flow_id_found_itr->second);

}

void AugmentedEtherMacFullDuplex::dcqcn_schedule_increase_timer(CCFlowInfo* dcqcn_flow_info)
{
    EV << "dcqcn_schedule_increase_timer called" << endl;
    double gap = dcqcn_flow_info->increase_timer_gap;
    EV << "increase gap is " << gap << endl;
    cc_schedule_timer(dcqcn_flow_info->increase_timer, gap);
}

void AugmentedEtherMacFullDuplex::handle_dcqcn_increase_timer(unsigned long flow_id, int gpu_idx) {

    if (rate_control_type != dcqcn)
        throw cRuntimeError("handle_dcqcn_increase_timer called while rate_control_type != dcqcn");

    EV << "handle_dcqcn_increase_timer for flow_id: " << flow_id << endl;

    std::tuple<int, unsigned long> gpu_id_flow_id_tuple = std::make_tuple(gpu_idx, flow_id);
    auto flow_id_found_itr = flow_id_to_info_mapper.find(gpu_id_flow_id_tuple);
    if (flow_id_found_itr == flow_id_to_info_mapper.end())
        throw cRuntimeError("handle_dcqcn_increase_timer: How is flow not found!");
    dcqcn_schedule_increase_timer(flow_id_found_itr->second);

    // do not do anything if the port is paused
    if (txQueue->pfc_status == txQueue->PFC_STOP)
        return;

    flow_id_found_itr->second->T++;
    dcqcn_activate_appropriate_rate_increase(flow_id_found_itr->second);


}

void AugmentedEtherMacFullDuplex::cc_schedule_send_timer(CCFlowInfo* flow_info)
{
    EV << "cc_schedule_send_timer called" << endl;
    EV << "Scheduling next send" << endl;
    // dcqcn_mtu is in bytes
    double gap = (dcqcn_mtu * 8 / flow_info->curr_rate);
    EV << "send gap is " << gap << endl;
//    std::cout << "rate: " << flow_info->curr_rate << " , gap: " << gap << endl;
    cc_schedule_timer(flow_info->send_timer, gap);
}

bool AugmentedEtherMacFullDuplex::should_packet_be_grouped(unsigned long query_id,
        int tor_group_idx,
        unsigned long seq_num,
        simtime_t controller_setup_finish_time) {
    EV << "controller finish time: " << controller_setup_finish_time << endl;
    bool is_controller_setup_finished =
            (simTime() >= controller_setup_finish_time);
    bool pkts_must_be_grouped = false;
    auto query_torgroup_touple = std::make_tuple(query_id, tor_group_idx);
    auto highest_seq_num_itr =
            query_torgroup_to_highest_seqnum_before_controller_setup.find(query_torgroup_touple);
    if (is_controller_setup_finished) {
        if (highest_seq_num_itr == query_torgroup_to_highest_seqnum_before_controller_setup.end()) {
            pkts_must_be_grouped = true;
        } else {
            if (highest_seq_num_itr->second < seq_num)
                pkts_must_be_grouped = true;
            else
                EV << "Some packet with seq num " << seq_num <<
                    " has already been sent. So, not grouping. highest_seq_num_itr->second: " <<
                    highest_seq_num_itr->second << endl;
        }
    } else {
        EV << "controller setup not finished!" << endl;
        if (highest_seq_num_itr == query_torgroup_to_highest_seqnum_before_controller_setup.end()) {
            query_torgroup_to_highest_seqnum_before_controller_setup.insert(
                    std::make_pair(query_torgroup_touple, seq_num));
            highest_seq_num_itr =
                    query_torgroup_to_highest_seqnum_before_controller_setup.find(query_torgroup_touple);
        }
        highest_seq_num_itr->second = std::max(seq_num,
                highest_seq_num_itr->second);
        EV << "Packets are not grouped... highest seq num: " <<
                highest_seq_num_itr->second << endl;
    }
//    std::cout << simTime() << endl;
    return pkts_must_be_grouped;
}

void AugmentedEtherMacFullDuplex::handle_cc_send_timer(unsigned long flow_id, int gpu_idx) {

    EV << "handle_cc_send_timer for flow_id: " << flow_id << endl;

    std::tuple<int, unsigned long> gpu_id_flow_id_tuple = std::make_tuple(gpu_idx, flow_id);
    auto flow_id_found_itr = flow_id_to_info_mapper.find(gpu_id_flow_id_tuple);
    if (flow_id_found_itr == flow_id_to_info_mapper.end())
        throw cRuntimeError("handle_cc_send_timer: How is flow not found!");

    EV << "Has more packets status? " << flow_id_found_itr->second->has_more_packets << ", "
            "pkt list size: " << flow_id_found_itr->second->packet_list.size() << endl;

    cc_schedule_send_timer(flow_id_found_itr->second);

    if (flow_id_found_itr->second->has_more_packets == BATCH_SEND &&
            flow_id_found_itr->second->packet_list.size() < packet_creation_batch_size) {
        if (flow_id_found_itr->second->application_name.compare("UDPLongAllgatherInitiatorApp") == 0) {
            UDPLongAllgatherInitiatorApp* app = check_and_cast<UDPLongAllgatherInitiatorApp*>(getModuleByPath(flow_id_found_itr->second->application_full_path.c_str()));
            flow_id_found_itr->second->has_more_packets = app->generate_next_batch(flow_id, gpu_idx);
        } else if (flow_id_found_itr->second->application_name.compare("UDPLongBroadcastPasserApp") == 0) {
            UDPLongBroadcastPasserApp* app = check_and_cast<UDPLongBroadcastPasserApp*>(getModuleByPath(flow_id_found_itr->second->application_full_path.c_str()));
            flow_id_found_itr->second->has_more_packets = app->generate_next_batch(flow_id, gpu_idx);
        } else
            throw cRuntimeError("Unrecognized app for implementing generate_next_batch");
        EV << "generate_next_batch returned " << flow_id_found_itr->second->has_more_packets << endl;
    }

    if (flow_id_found_itr->second->packet_list.size() == 0) {
        switch(flow_id_found_itr->second->has_more_packets) {
            case BATCH_TERMINATE:
                EV << ": No more packets to send for flow id: " << flow_id_found_itr->second->flow_id << ", deleting it's info" << endl;
                cc_cancel_and_delete_all_timers(flow_id_found_itr->second);
                delete flow_id_found_itr->second;
                flow_id_to_info_mapper.erase(flow_id_found_itr);
                return;
                break;
            case BATCH_SLEEP:
                EV << flow_id_found_itr->second->flow_id << ": going to sleep" << endl;
                cc_cancel_all_timers(flow_id_found_itr->second);
                return;
                break;
            case BATCH_SEND:
                EV << flow_id_found_itr->second->flow_id << ": not packets to send FOR NOW!" << endl;
                return;
                break;
            default:
                throw cRuntimeError("Unknown has_more_packets signal!");
        }
    }

    // do not do anything if the port is paused
    if (txQueue->pfc_status == txQueue->PFC_STOP)
        return;

    // not make host queue super congested
    if (queue_insert_thresh > 0 && txQueue->getNumPackets() > queue_insert_thresh) {
//        std::cout << queue_insert_thresh << endl;
//        std::cout << txQueue->getNumPackets() << endl;
//        std::cout << "----------------------" << endl;
        return;
    }

    auto packet = flow_id_found_itr->second->packet_list.front();

    if (flow_id_found_itr->second->algorithm == BCAST_INA) {
        if (rate_control_type != sharp)
            throw cRuntimeError("BCAST_INA only runs with sharp rate_control_type");

        // get relay unit for the upstream tor
        int upstream_leaf_idx = int (server_module_index / num_servers_under_each_leaf);
        auto topo_module = getParentModule()->getParentModule()->getParentModule();
        std::string relay_path;
        if (use_fattree) {
            relay_path = topo_module->getFullPath() +
                            ".edge[" + std::to_string(upstream_leaf_idx) +
                            "].relayUnit";
        } else {
            relay_path = topo_module->getFullPath() +
                            ".agg[" + std::to_string(upstream_leaf_idx) +
                            "].relayUnit";
        }
        BouncingIeee8021dRelay* relay_module =
                check_and_cast<BouncingIeee8021dRelay*>(getModuleByPath(relay_path.c_str()));

        EV << "Upstream tor relay is " << relay_module->getFullPath() << endl;

        auto frame = packet->peekAtFront<EthernetMacHeader>();
        if (relay_module->ina_has_free_aggregation_pool_spots(frame->getQuery_id(), frame->getSeq_num(),
                frame->getIna_leaf_aggregation_num(),
                frame->getIna_spine_aggregation_num(),
                frame->getIna_core_aggregation_num())) {
            EV << "Aggregation pool available... Allocating!!" << endl;
            relay_module->ina_allocate_free_aggregation_pool_spots(frame->getQuery_id(), frame->getSeq_num(),
                    frame->getIna_leaf_aggregation_num(),
                    frame->getIna_spine_aggregation_num(),
                    frame->getIna_core_aggregation_num());
        } else {
            EV << "Could not allocate resources!!" << endl;
            return;
        }
    }


    flow_id_found_itr->second->packet_list.pop_front();

    if (rate_control_type == dcqcn) {
        if (flow_id_found_itr->second->byte_counter > packet->getByteLength()) {
            EV << "Reducing byte counter from " << flow_id_found_itr->second->byte_counter;
            flow_id_found_itr->second->byte_counter -= packet->getByteLength();
            EV << " to " << flow_id_found_itr->second->byte_counter << endl;
        } else {
            EV << "Byte counter reaches 0. Increase BC and reset byte counter." << endl;
            flow_id_found_itr->second->BC++;
            flow_id_found_itr->second->byte_counter = par("dcqcn_init_byte_counter");
            dcqcn_activate_appropriate_rate_increase(flow_id_found_itr->second);
        }
    }

    bool should_push_packet = true;
    std::string pkt_name = packet->getName();

    if (should_push_packet &&
            partial_mcast_use_cidr_agg &&
            pkt_name.compare("data") == 0) {

        auto packet_dup = packet->dup();
        packet_dup->setFrontIteratorPosition(b(0));
        packet_dup->removeAtFront<EthernetMacHeader>();
        packet_dup->removeAtFront<Ipv4Header>();
        packet_dup->removeAtFront<UdpHeader>();
        packet_dup->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);
        auto chunk = packet_dup->removeAtFront<SliceChunk>();
        auto main_chunk = chunk->getChunk();
        Packet* temp = new Packet();
        temp->insertAtBack(main_chunk);
        auto payload = temp->popAtFront<GenericAppMsg>();

        EV << "1) Payload Cidr AGG group member count is " << payload->getAgg_cidr_group_member_count() << endl;
        EV << "1) Payload Cidr CORE group member count is " << payload->getCore_cidr_group_member_count() << endl;

        if (payload->getAgg_cidr_group_member_count() > 0) {
            std::string cidr_group_id = payload->getAgg_cidr_group_id();
            auto query_cidrid_seqnum_tuple = std::make_tuple(payload->getQuery_id(),
                    cidr_group_id,
                    payload->getSeq_num());
            auto query_cidrid_tuple = std::make_tuple(payload->getQuery_id(),
                    cidr_group_id);
            auto flowid_pkt_itr = query_cidrid_seqnum_to_flowid_packets.find(query_cidrid_seqnum_tuple);
            if (flowid_pkt_itr == query_cidrid_seqnum_to_flowid_packets.end()) {
                std::map<unsigned long, Packet*> tmp_map;
                query_cidrid_seqnum_to_flowid_packets.insert(std::make_pair(query_cidrid_seqnum_tuple, tmp_map));
                query_cidrid_to_candidate_flowid.insert(std::make_pair(query_cidrid_tuple, payload->getRequesterID()));
                flowid_pkt_itr = query_cidrid_seqnum_to_flowid_packets.find(query_cidrid_seqnum_tuple);
            }

//            auto candidate_flowid_itr = query_cidrid_to_candidate_flowid.find(query_cidrid_tuple);

            auto flowid_itr = flowid_pkt_itr->second.find(payload->getRequesterID());
            if (flowid_itr != flowid_pkt_itr->second.end()) {
                std::cout << "CIDR info:" << endl;
                std::cout << "Query idx: " << payload->getQuery_id() << endl;
                std::cout << "Tor group idx: " << payload->getTor_group_idx() << endl;
                std::cout << "seq num: " << payload->getSeq_num() << endl;
                std::cout << "flow id: " << payload->getRequesterID() << endl;
                std::cout << "AGG CIDR group id: " << payload->getAgg_cidr_group_id() << endl;
                throw cRuntimeError("1) CIDR: Seq num for this flow id and CIDR group/query has already been received! how?");
            }

            // insert these info in stored packets as they will be used later!
            auto m_eth_header = packet->removeAtFront<EthernetMacHeader>();
            if (use_udp && isPacketAppropriateForMulticast(pkt_name)) {
                if (payload->getDst_idx_listArraySize() > 0) {
                    EV << "use_udp && use_host_assisted_routing" << endl;

                    // pkt can be multicasted using CIDR in aggregate switches
                    m_eth_header->setMulticast_pkt_in_CIDR_agg(true);
                    if (partial_mcast_use_cidr_core && payload->getCore_cidr_group_member_count() > 0) {
                        // pkt can be multicasted using CIDR in core switches
                        m_eth_header->setMulticast_pkt_in_CIDR_core(true);
                    }

                    m_eth_header->setDst_idx_listArraySize(payload->getDst_idx_listArraySize());
                    for (int k = 0; k < payload->getDst_idx_listArraySize(); k++)
                        m_eth_header->setDst_idx_list(k, payload->getDst_idx_list(k));

                    m_eth_header->setJump_to_idxArraySize(payload->getJump_to_idxArraySize());
                    for (int k = 0; k < payload->getJump_to_idxArraySize(); k++) {
                        InnerList innerList;
                        innerList.setInnerArrayArraySize(payload->getJump_to_idx(k).getInnerArrayArraySize());
                        for (int t = 0; t < payload->getJump_to_idx(k).getInnerArrayArraySize(); t++) {
                            innerList.setInnerArray(t, payload->getJump_to_idx(k).getInnerArray(t));
                        }
                        m_eth_header->setJump_to_idx(k, innerList);
                    }

                    m_eth_header->setPorts_to_dest_idxArraySize(payload->getPorts_to_dest_idxArraySize());
                    for(int k = 0; k < payload->getPorts_to_dest_idxArraySize(); k++) {
                        InnerList innerList;
                        innerList.setInnerArrayArraySize(payload->getPorts_to_dest_idx(k).getInnerArrayArraySize());
                        for (int t = 0; t < payload->getPorts_to_dest_idx(k).getInnerArrayArraySize(); t++) {
                            innerList.setInnerArray(t, payload->getPorts_to_dest_idx(k).getInnerArray(t));
            //                std::cout << payload->getPath_to_dest_idx(k).getInnerArray(t) << endl;
                        }
                        m_eth_header->setPorts_to_dest_idx(k, innerList);
                    }
                    m_eth_header->setStart_from_idx(1);

                }
            }

            // INA
            m_eth_header->setIna_leaf_aggregation_num(payload->getIna_leaf_aggregation_num());
            m_eth_header->setIna_spine_aggregation_num(payload->getIna_spine_aggregation_num());
            m_eth_header->setIna_core_aggregation_num(payload->getIna_core_aggregation_num());
            m_eth_header->setTree_direction(TREE_UP);

            m_eth_header->setQuery_id(payload->getQuery_id());
            m_eth_header->setFlow_id(payload->getRequesterID());
            m_eth_header->setCollective_scale(payload->getCollective_scale());
            m_eth_header->setTotal_length(payload->getTotal_flow_size());
            m_eth_header->setCollective_alg(payload->getCollective_alg_type());
            m_eth_header->setDst_tor_idx(payload->getDst_tor_idx());
            m_eth_header->setSrc_gpu_idx(payload->getSrc_gpu_idx());
            m_eth_header->setIn_src_sharding(payload->getIn_src_sharding());
            m_eth_header->setUsing_orca(payload->getUsing_orca());
            m_eth_header->setUsing_elmo(payload->getUsing_elmo());
            if (rate_control_type == swift)
                m_eth_header->setSwift_send_time(simTime());
            if (payload->getSeq_num() <= 0)
                throw cRuntimeError("Wow... Invalid seq number <= 0");
            m_eth_header->setSeq_num(payload->getSeq_num());

            m_eth_header->setIs_last_pkt(payload->getIs_last_pkt());
            for (int k = 0; k < payload->getInvolved_servers_listArraySize(); k++) {
                m_eth_header->insertInvolved_servers_list(
                        payload->getInvolved_servers_list(k));
            }

            packet->insertAtFront(m_eth_header);

            flowid_pkt_itr->second.insert(std::make_pair(payload->getRequesterID(), packet));

            if (flowid_pkt_itr->second.size() == payload->getAgg_cidr_group_member_count()) {
                EV << "1) CIDR AGG Got all " << payload->getAgg_cidr_group_member_count() <<
                        " packets for CIDR group " << payload->getAgg_cidr_group_id() << endl;
                should_push_packet = true;
                auto query_cidrid_itr = query_cidrid_to_candidate_flowid.find(query_cidrid_tuple);
                if (query_cidrid_itr == query_cidrid_to_candidate_flowid.end())
                    throw cRuntimeError("1) Candidate flow id must exist in CIDR!");

                auto candidate_packet_itr = flowid_pkt_itr->second.find(
                        query_cidrid_itr->second);
                unsigned long candidate_flow_id = query_cidrid_itr->second;
                if (candidate_packet_itr == flowid_pkt_itr->second.end())
                    throw cRuntimeError("1) CIDR: Candidate pkt does not exist!");
                auto candidate_packet = candidate_packet_itr->second;
                auto candidate_packet_dup = candidate_packet->dup();
                b position = candidate_packet->getFrontOffset();
                candidate_packet->setFrontIteratorPosition(b(0));
                auto eth_header = candidate_packet->removeAtFront<EthernetMacHeader>();
                eth_header->insertCIDR_agg_pkt(candidate_packet_dup);
                for (auto pkt_itr = flowid_pkt_itr->second.begin();
                        pkt_itr != flowid_pkt_itr->second.end();
                        pkt_itr++) {
                    if (pkt_itr->first != candidate_packet_itr->first) {
                        eth_header->insertCIDR_agg_pkt(pkt_itr->second);
                    }
                }
                eth_header->setAgg_cidr_group_id(payload->getAgg_cidr_group_id());
                eth_header->setAgg_cidr_group_member_count(payload->getAgg_cidr_group_member_count());
                EV << "1) Grouped " << eth_header->getCIDR_agg_pktArraySize() <<
                        " packets for one CIDR aggregate" << endl;
                candidate_packet->insertAtFront(eth_header);
                candidate_packet->setFrontIteratorPosition(position);
                packet = candidate_packet;
                flowid_pkt_itr->second.clear();
                query_cidrid_seqnum_to_flowid_packets.erase(query_cidrid_seqnum_tuple);
                if (partial_mcast_use_cidr_core &&
                        payload->getCore_cidr_group_member_count() > 0) {
                    EV << "Packets must also get CIDR grouped for core...!" << endl;
                    // perform the same grouping as with AGG for core as well
                    std::string core_cidr_group_id = payload->getCore_cidr_group_id();
                    auto query_corecidrid_seqnum_tuple = std::make_tuple(
                            payload->getQuery_id(),
                            core_cidr_group_id,
                            payload->getSeq_num());
                    auto query_corecidrid_tuple = std::make_tuple(
                            payload->getQuery_id(),
                            core_cidr_group_id);
                    auto flowid_pkt_itr = query_corecidrid_seqnum_to_flowid_packets.find(
                            query_corecidrid_seqnum_tuple);
                    if (flowid_pkt_itr == query_corecidrid_seqnum_to_flowid_packets.end()) {
                        std::map<unsigned long, Packet*> tmp_map;
                        query_corecidrid_seqnum_to_flowid_packets.insert(
                                std::make_pair(query_corecidrid_seqnum_tuple,
                                        tmp_map));
                        query_corecidrid_to_candidate_flowid.insert(
                                std::make_pair(query_corecidrid_tuple,
                                        candidate_flow_id));
                        flowid_pkt_itr = query_corecidrid_seqnum_to_flowid_packets.find(
                                query_corecidrid_seqnum_tuple);
                    }

                    auto candidate_flowid_itr = query_corecidrid_to_candidate_flowid.find(
                            query_corecidrid_tuple);

                    auto flowid_itr = flowid_pkt_itr->second.find(candidate_flow_id);
                    if (flowid_itr != flowid_pkt_itr->second.end()) {
                        std::cout << "CORE CIDR info:" << endl;
                        std::cout << "Query idx: " << payload->getQuery_id() << endl;
                        std::cout << "Tor group idx: " << payload->getTor_group_idx() << endl;
                        std::cout << "seq num: " << payload->getSeq_num() << endl;
                        std::cout << "candidate flow id: " << candidate_flow_id << endl;
                        std::cout << "Core CIDR group id: " << payload->getCore_cidr_group_id() << endl;
                        throw cRuntimeError("1) CIDR Core: Seq num for this flow id and CIDR group/query has already been received! how?");
                    }

                    flowid_pkt_itr->second.insert(
                            std::make_pair(
                                    candidate_flow_id,
                                    packet));
                    if (flowid_pkt_itr->second.size() == payload->getCore_cidr_group_member_count()) {
                        EV << "1) CIDR Core Got all " << payload->getCore_cidr_group_member_count() <<
                                " packets for CIDR group " << payload->getCore_cidr_group_id() << endl;
                        should_push_packet = true;
                        auto query_cidrid_itr = query_corecidrid_to_candidate_flowid.find(
                                query_corecidrid_tuple);
                        if (query_cidrid_itr ==
                                query_corecidrid_to_candidate_flowid.end())
                            throw cRuntimeError("1) Candidate flow id must exist in CORE CIDR!");

                        auto candidate_packet_itr = flowid_pkt_itr->second.find(
                                query_cidrid_itr->second);
                        if (candidate_packet_itr == flowid_pkt_itr->second.end()) {
                            std::cout << "Looking for flow id: " << query_cidrid_itr->second << endl;
                            std::cout << "query id: " << payload->getQuery_id() << endl;
                            std::cout << "core cidr id: " << core_cidr_group_id << endl;
                            std::cout << "seq num: " << payload->getSeq_num() << endl;
                            std::cout << "Existing flow ids: " << endl;
                            for (auto tmp : flowid_pkt_itr->second)
                                std::cout << tmp.first << endl;
                            throw cRuntimeError("1) CIDR CORE: Candidate pkt does not exist!");
                        }
                        auto candidate_packet = candidate_packet_itr->second;
                        auto candidate_packet_dup = candidate_packet->dup();
                        b position = candidate_packet->getFrontOffset();
                        candidate_packet->setFrontIteratorPosition(b(0));
                        auto eth_header = candidate_packet->removeAtFront<EthernetMacHeader>();
                        eth_header->insertCIDR_core_pkt(candidate_packet_dup);
                        for (auto pkt_itr = flowid_pkt_itr->second.begin();
                                pkt_itr != flowid_pkt_itr->second.end();
                                pkt_itr++) {
                            if (pkt_itr->first != candidate_packet_itr->first) {
                                eth_header->insertCIDR_core_pkt(pkt_itr->second);
                            }
                        }
                        eth_header->setCore_cidr_group_id(payload->getCore_cidr_group_id());
                        eth_header->setCore_cidr_group_member_count(payload->getCore_cidr_group_member_count());
                        EV << "1) Grouped " << eth_header->getCIDR_core_pktArraySize() <<
                                " packets for one CIDR core" << endl;
                        candidate_packet->insertAtFront(eth_header);
                        candidate_packet->setFrontIteratorPosition(position);
                        packet = candidate_packet;
                        flowid_pkt_itr->second.clear();
//                        std::cout << getFullPath() << " removing " <<
//                                payload->getQuery_id() << ", " <<
//                                core_cidr_group_id << ", " <<
//                                payload->getSeq_num() << endl;
                        query_corecidrid_seqnum_to_flowid_packets.erase(
                                query_corecidrid_seqnum_tuple);
                    } else {
                        EV << "1) CORE CIDR: Expecting " << payload->getCore_cidr_group_member_count() <<
                                " for CIDR group " << payload->getCore_cidr_group_id() <<
                                " but got " << flowid_pkt_itr->second.size() << " so far!" << endl;
                        should_push_packet = false;
                    }
                }
            } else {
                EV << "1) Expecting " << payload->getAgg_cidr_group_member_count() <<
                        " for CIDR group " << payload->getAgg_cidr_group_id() <<
                        " but got " << flowid_pkt_itr->second.size() << " so far!" << endl;
                should_push_packet = false;
            }

        }

        delete temp;
        delete packet_dup;

    }

    // check if the packet should be sent
    // if should_push_packet is false here, it means that we are for now grouping packets for an agg
    if (partial_mcast_programmable_cores &&
            should_push_packet &&
            pkt_name.compare("data") == 0) {

        auto packet_dup = packet->dup();
        packet_dup->setFrontIteratorPosition(b(0));
        packet_dup->removeAtFront<EthernetMacHeader>();
        packet_dup->removeAtFront<Ipv4Header>();
        packet_dup->removeAtFront<UdpHeader>();
        packet_dup->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);
        auto chunk = packet_dup->removeAtFront<SliceChunk>();
        auto main_chunk = chunk->getChunk();
        Packet* temp = new Packet();
        temp->insertAtBack(main_chunk);
        auto payload = temp->popAtFront<GenericAppMsg>();

        bool pkts_must_be_grouped = should_packet_be_grouped(payload->getQuery_id(),
                payload->getTor_group_idx(),
                payload->getSeq_num(),
                payload->getController_setup_finish_time());

        if (pkts_must_be_grouped) {
            // if payload->getTor_group_idx() < 0, the packet is destined for the same pod and thus, not grouped
            if (payload->getTor_group_idx() >= 0) {
                EV << "1) Packets must be grouped!: " << payload->getTor_group_idx() <<
                        ", seq num: " << payload->getSeq_num() << endl;
                auto query_torgroup_seqnum_touple = std::make_tuple(payload->getQuery_id(),
                        payload->getTor_group_idx(),
                        payload->getSeq_num());
                auto flowid_pkt_itr = query_torgroup_seqnum_to_flowid_packets.find(
                        query_torgroup_seqnum_touple);
                if (flowid_pkt_itr == query_torgroup_seqnum_to_flowid_packets.end()) {
                    EV << "Info of query " << payload->getQuery_id() <<
                            ", tor group " << payload->getTor_group_idx() <<
                            ", and seq num " << payload->getSeq_num() <<
                            " not found!" << endl;
                    std::map<unsigned long, Packet*> tmp_map;
                    query_torgroup_seqnum_to_flowid_packets.insert(
                            std::make_pair(query_torgroup_seqnum_touple,
                            tmp_map));
                    flowid_pkt_itr = query_torgroup_seqnum_to_flowid_packets.find(
                            query_torgroup_seqnum_touple);
                }
                auto flowid_itr = flowid_pkt_itr->second.find(payload->getRequesterID());
                if (flowid_itr != flowid_pkt_itr->second.end()) {
                    std::cout << "Query idx: " << payload->getQuery_id() << endl;
                    std::cout << "Tor group idx: " << payload->getTor_group_idx() << endl;
                    std::cout << "seq num: " << payload->getSeq_num() << endl;
                    std::cout << "flow id: " << payload->getRequesterID() << endl;
                    throw cRuntimeError("Seq num for this flow id and tor group/query has already been received! how?");
                }

                // insert these info in stored packets as they will be used later!
                auto m_eth_header = packet->removeAtFront<EthernetMacHeader>();
                if (use_udp && isPacketAppropriateForMulticast(pkt_name)) {
                    if (payload->getDst_idx_listArraySize() > 0) {
                        EV << "use_udp && use_host_assisted_routing" << endl;

                        // pkt can be multicasted on programmable core
                        m_eth_header->setMulticast_pkt_in_programmable_core(true);

                        m_eth_header->setDst_idx_listArraySize(payload->getDst_idx_listArraySize());
                        for (int k = 0; k < payload->getDst_idx_listArraySize(); k++)
                            m_eth_header->setDst_idx_list(k, payload->getDst_idx_list(k));

                        m_eth_header->setJump_to_idxArraySize(payload->getJump_to_idxArraySize());
                        for (int k = 0; k < payload->getJump_to_idxArraySize(); k++) {
                            InnerList innerList;
                            innerList.setInnerArrayArraySize(payload->getJump_to_idx(k).getInnerArrayArraySize());
                            for (int t = 0; t < payload->getJump_to_idx(k).getInnerArrayArraySize(); t++) {
                                innerList.setInnerArray(t, payload->getJump_to_idx(k).getInnerArray(t));
                            }
                            m_eth_header->setJump_to_idx(k, innerList);
                        }

                        m_eth_header->setPorts_to_dest_idxArraySize(payload->getPorts_to_dest_idxArraySize());
                        for(int k = 0; k < payload->getPorts_to_dest_idxArraySize(); k++) {
                            InnerList innerList;
                            innerList.setInnerArrayArraySize(payload->getPorts_to_dest_idx(k).getInnerArrayArraySize());
                            for (int t = 0; t < payload->getPorts_to_dest_idx(k).getInnerArrayArraySize(); t++) {
                                innerList.setInnerArray(t, payload->getPorts_to_dest_idx(k).getInnerArray(t));
                //                std::cout << payload->getPath_to_dest_idx(k).getInnerArray(t) << endl;
                            }
                            m_eth_header->setPorts_to_dest_idx(k, innerList);
                        }
                        m_eth_header->setStart_from_idx(1);

                    }
                }

                // INA
                m_eth_header->setIna_leaf_aggregation_num(payload->getIna_leaf_aggregation_num());
                m_eth_header->setIna_spine_aggregation_num(payload->getIna_spine_aggregation_num());
                m_eth_header->setIna_core_aggregation_num(payload->getIna_core_aggregation_num());
                m_eth_header->setTree_direction(TREE_UP);

                m_eth_header->setQuery_id(payload->getQuery_id());
                m_eth_header->setFlow_id(payload->getRequesterID());
                m_eth_header->setCollective_scale(payload->getCollective_scale());
                m_eth_header->setTotal_length(payload->getTotal_flow_size());
                m_eth_header->setCollective_alg(payload->getCollective_alg_type());
                m_eth_header->setDst_tor_idx(payload->getDst_tor_idx());
                m_eth_header->setSrc_gpu_idx(payload->getSrc_gpu_idx());
                m_eth_header->setIn_src_sharding(payload->getIn_src_sharding());
                m_eth_header->setUsing_orca(payload->getUsing_orca());
                m_eth_header->setUsing_elmo(payload->getUsing_elmo());
                if (rate_control_type == swift)
                    m_eth_header->setSwift_send_time(simTime());
                if (payload->getSeq_num() <= 0)
                    throw cRuntimeError("Wow... Invalid seq number <= 0");
                m_eth_header->setSeq_num(payload->getSeq_num());

                m_eth_header->setIs_last_pkt(payload->getIs_last_pkt());
                for (int k = 0; k < payload->getInvolved_servers_listArraySize(); k++) {
                    m_eth_header->insertInvolved_servers_list(
                            payload->getInvolved_servers_list(k));
                }

                packet->insertAtFront(m_eth_header);

                flowid_pkt_itr->second.insert(std::make_pair(payload->getRequesterID(), packet));

                if (flowid_pkt_itr->second.size() == payload->getNum_msgs_in_tor_group()) {
                    EV << "Got all " << payload->getNum_msgs_in_tor_group() <<
                            " packets for tor group " << payload->getTor_group_idx() << endl;
                    should_push_packet = true;
                    std::tuple<unsigned long, int> query_torgroup_tuple = std::make_tuple(
                            payload->getQuery_id(),
                            payload->getTor_group_idx());
                    auto query_torgroup_itr = query_torgroup_to_candidate_flowid.find(query_torgroup_tuple);
                    if (query_torgroup_itr == query_torgroup_to_candidate_flowid.end()) {
                        EV << "Selecting " << payload->getRequesterID() <<
                                " as the candidate flow id for tor group!" << endl;
                        query_torgroup_to_candidate_flowid.insert(std::make_pair(query_torgroup_tuple,
                                payload->getRequesterID()));
                        query_torgroup_itr = query_torgroup_to_candidate_flowid.find(query_torgroup_tuple);
                    }
                    auto candidate_packet_itr = flowid_pkt_itr->second.find(query_torgroup_itr->second);
                    if (candidate_packet_itr == flowid_pkt_itr->second.end())
                        throw cRuntimeError("Candidate pkt does not exist!");
                    auto candidate_packet = candidate_packet_itr->second;
                    auto candidate_packet_dup = candidate_packet->dup();
                    b position = candidate_packet->getFrontOffset();
                    candidate_packet->setFrontIteratorPosition(b(0));
                    auto eth_header = candidate_packet->removeAtFront<EthernetMacHeader>();
                    eth_header->insertPartial_mcast_core_pkt(candidate_packet_dup);
                    for (auto pkt_itr = flowid_pkt_itr->second.begin();
                            pkt_itr != flowid_pkt_itr->second.end();
                            pkt_itr++) {
                        if (pkt_itr->first != candidate_packet_itr->first) {
                            eth_header->insertPartial_mcast_core_pkt(pkt_itr->second);
                        }
                    }
                    EV << "Added " << eth_header->getPartial_mcast_core_pktArraySize() <<
                            " packets as packet cores" << endl;

                    // this packet must traverse all the way to the tor and then the nested packets must have
                    // setMulticast_pkt_in_CIDR_agg = True
                    eth_header->setMulticast_pkt_in_CIDR_agg(false);

                    candidate_packet->insertAtFront(eth_header);
                    candidate_packet->setFrontIteratorPosition(position);
                    packet = candidate_packet;
                    flowid_pkt_itr->second.clear();
                    query_torgroup_seqnum_to_flowid_packets.erase(query_torgroup_seqnum_touple);
                } else {
                    EV << "Expecting " << payload->getNum_msgs_in_tor_group() <<
                            " for tor group " << payload->getTor_group_idx() <<
                            " but got " << flowid_pkt_itr->second.size() << " so far!" << endl;
                    should_push_packet = false;
                }
            }
        } else
            EV << simTime() << " -- 1) Packets should not be grouped! " <<
                payload->getController_setup_finish_time() << endl;

        delete temp;
        delete packet_dup;
    }

    if (should_push_packet)
        pushPacketToQeueue(packet);

    EV << "SEPEHR: TransmitState is " << transmitState << " and TX_IDLE_STATE is " << TX_IDLE_STATE << endl;
    EV << "SEPEHR: transmitState == TX_IDLE_STATE? " << (transmitState == TX_IDLE_STATE) << endl;

    if (transmitState == TX_IDLE_STATE) {
        // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
        // it is nullptr before popping a packet
        EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
        if (!txQueue->isEmpty() && !currentTxFrame)
            popTxQueue();
        beginSendFrames();
    }
}

void AugmentedEtherMacFullDuplex::apply_FR(CCFlowInfo* dcqcn_flow_info) {

    if (rate_control_type != dcqcn)
        throw cRuntimeError("apply_FR is dedicated to dcqcn!");

    EV << "Changing the rate in Fast Recovery!" << endl;
    EV << "Old rate is " << dcqcn_flow_info->curr_rate << endl;
    dcqcn_flow_info->curr_rate = (dcqcn_flow_info->curr_rate + dcqcn_flow_info->target_rate) / 2.0;
    dcqcn_flow_info->curr_rate = std::min(dcqcn_flow_info->curr_rate, dcqcn_flow_info->max_rate);
    EV << "New rate is " << dcqcn_flow_info->curr_rate << endl;
}

void AugmentedEtherMacFullDuplex::apply_HI(CCFlowInfo* dcqcn_flow_info) {
    if (rate_control_type != dcqcn)
        throw cRuntimeError("apply_HI is dedicated to dcqcn!");

    EV << "Changing the rate in Hyper Increase!" << endl;
    EV << "Old rate is " << dcqcn_flow_info->curr_rate << endl;
    EV << "Old target rate is " << dcqcn_flow_info->target_rate << endl;
    dcqcn_flow_info->target_rate += dcqcn_HI_factor;
    dcqcn_flow_info->target_rate = std::min(dcqcn_flow_info->target_rate, dcqcn_flow_info->max_rate);
    dcqcn_flow_info->curr_rate = (dcqcn_flow_info->curr_rate + dcqcn_flow_info->target_rate) / 2.0;
    dcqcn_flow_info->curr_rate = std::min(dcqcn_flow_info->curr_rate, dcqcn_flow_info->max_rate);
    EV << "New rate is " << dcqcn_flow_info->curr_rate << endl;
    EV << "New target rate is " << dcqcn_flow_info->target_rate << endl;

}

void AugmentedEtherMacFullDuplex::apply_AI(CCFlowInfo* dcqcn_flow_info) {
    if (rate_control_type != dcqcn)
        throw cRuntimeError("apply_AI is dedicated to dcqcn!");

    EV << "Changing the rate in Additive Increase!" << endl;
    EV << "Old rate is " << dcqcn_flow_info->curr_rate << endl;
    EV << "Old target rate is " << dcqcn_flow_info->target_rate << endl;
    dcqcn_flow_info->target_rate += dcqcn_AI_factor;
    dcqcn_flow_info->target_rate = std::min(dcqcn_flow_info->target_rate, dcqcn_flow_info->max_rate);
    dcqcn_flow_info->curr_rate = (dcqcn_flow_info->curr_rate + dcqcn_flow_info->target_rate) / 2.0;
    dcqcn_flow_info->curr_rate = std::min(dcqcn_flow_info->curr_rate, dcqcn_flow_info->max_rate);
    EV << "New rate is " << dcqcn_flow_info->curr_rate << endl;
    EV << "New target rate is " << dcqcn_flow_info->target_rate << endl;

}

void AugmentedEtherMacFullDuplex::dcqcn_activate_appropriate_rate_increase(CCFlowInfo* dcqcn_flow_info) {

    if (rate_control_type != dcqcn)
        throw cRuntimeError("dcqcn_activate_appropriate_rate_increase called while rate_control_type != dcqcn");

    EV << "dcqcn_activate_appropriate_rate_increase called for flow_id: " << dcqcn_flow_info->flow_id << endl;
    EV << "T is " << dcqcn_flow_info->T << " and BC is " << dcqcn_flow_info->BC << endl;
    if (std::max(dcqcn_flow_info->T, dcqcn_flow_info->BC) < dcqcn_F) {
        EV << "max(dcqcn_flow_info->T, dcqcn_flow_info->BC) < F: Apply Fast Recovery!" << endl;
        apply_FR(dcqcn_flow_info);
    } else if (std::min(dcqcn_flow_info->T, dcqcn_flow_info->BC) > dcqcn_F) {
        EV << "min(dcqcn_flow_info->T, dcqcn_flow_info->BC) > F: Apply Hyper Increase!" << endl;
        apply_HI(dcqcn_flow_info);
    } else {
        EV << "Apply additive increase!" << endl;
        apply_AI(dcqcn_flow_info);
    }

}

double AugmentedEtherMacFullDuplex::get_rate_dividier(int collective, int algorithm, bool optireduce_in_reduction_phase) {

    if (collective <= 0 || algorithm <= 0)
        throw cRuntimeError("invalid collective or algorithm!");

    // note that for combinational, in_src_reduce for ace is necessarily enabled
    double m_initial_rate_divider = -1;

    if (collective == ALL_TO_ALL_V) {
        // All-to-all(v) are not longer done with ace/marc! so set the rate to 1
        if (limit_all_to_all_rate)
            m_initial_rate_divider = num_flows_per_ml_query * 1.0;
        else
            m_initial_rate_divider = 1;
        // we don't want to get higher than link rate!
        if (m_initial_rate_divider < 1.0)
            throw cRuntimeError("How is m_initial_rate_divider < 1.0!");
        return m_initial_rate_divider;
    }

    // devider must be more than 1 if we have one collective and alg and less than 0 if we have combined
    if (dcqcn_initial_rate_devider >= 0 && dcqcn_initial_rate_devider <= 1)
        return 1.0;

    if (dcqcn_initial_rate_devider > 1.0) {
        // manually set, return the actual value only works when you have one collective/algorithm
        if (global_collective_alg < 0) {
            global_collective = collective;
            global_collective_alg = algorithm;
        }

        if (collective != global_collective || algorithm != global_collective_alg)
            throw cRuntimeError("Why is the dcqcn_initial_rate_devider manually set but we are seeing multiple collectives/algs?");

        // for optireduce, if we are doing broadcast with becca, divide it by the number of gpus
        // to allow higher rates since we are doing bandwidth-efficient broadcast
        if (algorithm == BCAST_OPTIREDUCE) {
            if (optireduce_in_reduction_phase || (!use_becca_optireduce && !use_cidr_optireduce)) {
//                std::cout << "Keeping rate divider at " << dcqcn_initial_rate_devider << endl;
            } else {

                if (num_flows_per_ml_query <= 0 || num_gpus <= 0) {
                    std::cout << num_flows_per_ml_query << endl;
                    std::cout << num_gpus << endl;
                    throw cRuntimeError("m_initial_rate_divider would become negative!");
                }

                m_initial_rate_divider = num_flows_per_ml_query * 1.0 / num_gpus;
                EV << "Changing rate divider to " << m_initial_rate_divider << endl;
                return m_initial_rate_divider;
            }
        }

        return dcqcn_initial_rate_devider;
    }

    // combinational collectives
    // tree is always 2
    if (algorithm == BCAST_BINARY_TREE || algorithm == BCAST_BINOMIAL_TREE)
        return 2.0;

    // ring is always 1.0
    if (algorithm == BCAST_RING)
        return 1.0;

    // optireduce without becca, the divider is 1
    if (algorithm == BCAST_OPTIREDUCE && !use_becca_optireduce && !use_cidr_optireduce) {
        return 1.0;
    }

    if (num_gpus < 1)
        throw cRuntimeError("Invalid gpu num!");
    if (num_flows_per_ml_query < 0)
        throw cRuntimeError("Invalid num_flows_per_ml_query");

    switch (collective) {
        case BROADCAST:
            m_initial_rate_divider = 1.0;
            break;
        case ALL_REDUCE:
            if (algorithm == BCAST_OPTIREDUCE) {
                if (!use_becca_optireduce)
                    throw cRuntimeError("Becca must have been enabled with optireduce at this point!");
                if (optireduce_in_reduction_phase) {
                    // every node is sending to all gpus
                    m_initial_rate_divider = num_flows_per_ml_query * 1.0;
                } else {
                    // every node is broadcasting to one gpu of of num_gpus
                    m_initial_rate_divider = num_flows_per_ml_query * 1.0 / num_gpus;
                }
            } else
                m_initial_rate_divider = num_flows_per_ml_query * 1.0 / num_gpus / num_gpus;
            break;
        case ALL_GATHER:
            m_initial_rate_divider = num_flows_per_ml_query * 1.0 / num_gpus;
            break;
        case REDUCE:
        case REDUCE_SCATTER:
            throw cRuntimeError("Reduce and Reduce Scatter are not longer done with ace/marc and must be with either ring/tree!");
            // REDUCE and REDUCE-scatter have in-src reduce but no in-host mcast so it should be divided by by num_gpus
            m_initial_rate_divider = num_flows_per_ml_query * 1.0 / num_gpus;
            break;
        case ALL_TO_ALL_V:
            throw cRuntimeError("ALL_TO_ALL_V should have been addressed by now!");
            break;
        default:
            throw cRuntimeError("Unknown collective!");
    }
    if (m_initial_rate_divider <= 0)
        throw cRuntimeError("Invalid m_initial_rate_divider!");

//    std::cout << "rate divider: " << m_initial_rate_divider << endl;

    // we don't want to get higher than link rate!
    if (m_initial_rate_divider < 1.0)
        return 1.0;
    return m_initial_rate_divider;

}

void AugmentedEtherMacFullDuplex::inject_in_stored_base_packets_for_recovery(Packet *packet) {

    auto eth_header = packet->peekAtFront<EthernetMacHeader>();
    std::string pkt_name = packet->getName();
    if ((recovery_type == DROP_NOTIF || recovery_type == HIERARCHICAL) &&
            isPacketAppropriateForMulticast(pkt_name) &&
            !eth_header->getIs_pkt_dropped() &&
            !eth_header->getIs_loss_recovery_request() &&
            !eth_header->getIs_loss_recovery_response() &&
            !eth_header->getDont_drop_again()) {
        unsigned long base_flowid;
        if (eth_header->getCidr_base_flow_id() <= 0) {
            base_flowid = eth_header->getFlow_id();
        } else {
            base_flowid = eth_header->getCidr_base_flow_id();
        }
        unsigned long seq_num = eth_header->getSeq_num();
        if (base_flowid <= 0)
            throw cRuntimeError("Injecting invalid flow id into the packet!");

        auto flowid_seqnum_tuple = std::make_tuple(base_flowid, seq_num);
        if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
            EV << "Injecting " << base_flowid << " , " << seq_num << " into stored_base_packets_for_recovery" << std::endl;
            stored_base_packets_for_recovery[flowid_seqnum_tuple] = create_base_pkt_deep_copy(packet);
        }
    }
}

void AugmentedEtherMacFullDuplex::pushPacketToQeueue(Packet *packet) {

    txQueue->pushPacket(packet);
}

void AugmentedEtherMacFullDuplex::handleUpperPacket(Packet *packet)
{
    EV_INFO << "Received " << packet->str() << " from upper layer2." << endl;

    numFramesFromHL++;
    emit(packetReceivedFromUpperSignal, packet);

    // bolt
    b position = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();
    eth_header->setTime_packet_sent_from_src(simTime());
    eth_header->setSource_server_idx(server_module_index);
//    eth_header->setTime_packet_sent_from_src(simTime());
    packet->insertAtFront(eth_header);
    packet->setFrontIteratorPosition(position);

    auto frame = packet->peekAtFront<EthernetMacHeader>();
    if (frame->getDest().equals(getMacAddress())) {
        throw cRuntimeError("logic error: frame %s from higher layer has local MAC address as dest (%s)",
                packet->getFullName(), frame->getDest().str().c_str());
    }

    if (packet->getDataLength() > MAX_ETHERNET_FRAME_BYTES) {    //FIXME two MAX FRAME BYTES in specif...
        throw cRuntimeError("packet from higher layer (%d bytes) exceeds maximum Ethernet frame size (%d)",
                (int)(packet->getByteLength()), B(MAX_ETHERNET_FRAME_BYTES).get());
    }

    if (!connected || disabled) {
        EV_WARN << (!connected ? "Interface is not connected" : "MAC is disabled") << " -- dropping packet " << packet << endl;
        PacketDropDetails details;
        details.setReason(INTERFACE_DOWN);
        emit(packetDroppedSignal, packet, &details);
        numDroppedPkFromHLIfaceDown++;
        delete packet;

        return;
    }

    // fill in src address if not set
    if (frame->getSrc().isUnspecified()) {
        frame = nullptr; // drop shared ptr
        auto newFrame = packet->removeAtFront<EthernetMacHeader>();
        newFrame->setSrc(getMacAddress());
        packet->insertAtFront(newFrame);
        frame = newFrame;
    }

    bool should_push_packet = true;

    if (rate_control_type >= 0) {
        EV << "Create new flow info if you don't already have it" << endl;
        auto packet_dup = packet->dup();
        packet_dup->setFrontIteratorPosition(b(0));
        packet_dup->removeAtFront<EthernetMacHeader>();
        packet_dup->removeAtFront<Ipv4Header>();
        packet_dup->removeAtFront<UdpHeader>();
        packet_dup->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);
        auto chunk = packet_dup->removeAtFront<SliceChunk>();
        auto main_chunk = chunk->getChunk();
        Packet* temp = new Packet();
        temp->insertAtBack(main_chunk);
        auto payload = temp->popAtFront<GenericAppMsg>();
        unsigned long flow_id = payload->getRequesterID();
        int m_collective = payload->getCollective_type();
        int m_collective_alg = payload->getCollective_alg_type();
        int src_gpu_idx = payload->getSrc_gpu_idx();
        bool optireduce_in_reduction_phase = payload->getOptireduce_in_reduction_phase(); // default is true and just used for optireduce
        unsigned long query_id = payload->getQuery_id();
        std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(src_gpu_idx, flow_id);
        std::string application_name = payload->getApp_name();
        std::string application_full_path = payload->getApp_full_path();

        if (packet_dup->getByteLength() != 0) {
            std::cout << packet->str() << endl;
            std::cout << packet_dup->str() << endl;
            throw cRuntimeError("Packet has multiple chunks of payload!");
        }
        delete temp;
        delete packet_dup;

        if (rate_control_type != sharp && m_collective_alg == BCAST_INA)
            throw cRuntimeError("BCAST_INA only runs with SHARP rate control!");

        auto flow_id_found_itr = flow_id_to_info_mapper.find(gpu_idx_flow_id_tuple);
        if (flow_id_found_itr == flow_id_to_info_mapper.end()) {
            EV << "This is the first packet we are receiving for flow_id: " << flow_id << endl;
            double init_alpha = par("dcqcn_init_alpha");
            double init_g = par("dcqcn_init_g");
            unsigned init_byte_counter = par("dcqcn_init_byte_counter");
            // reduce the curret packet from the byte counter
            init_byte_counter -= packet->getByteLength();
            double increase_timer_gap = par("increase_timer_gap");
            double alpha_timer_gap = par("alpha_timer_gap");
            CCFlowInfo* m_flow;
            if (rate_control_type == dcqcn) {
                if (init_byte_counter <= packet->getByteLength())
                    throw cRuntimeError("init_byte_counter <= packet->getByteLength()");
                m_flow = new CCFlowInfo(flow_id, init_alpha, init_g, get_link_util(), init_byte_counter,
                        increase_timer_gap, alpha_timer_gap, application_name, application_full_path, src_gpu_idx);
            } else if (rate_control_type == swift) {
                m_flow = new CCFlowInfo(flow_id, get_link_util(), application_name,
                        application_full_path, src_gpu_idx);
            } else if (rate_control_type == sharp) {
                m_flow = new CCFlowInfo(flow_id, get_link_util(), application_name,
                        application_full_path, src_gpu_idx);
            }

            m_flow->collective = m_collective;
            m_flow->algorithm = m_collective_alg;
            m_flow->query_id = query_id;

            if (packet_creation_batch_size > 0)
                m_flow->has_more_packets = BATCH_SEND;
            else
                m_flow->has_more_packets = BATCH_TERMINATE;
            double m_rate_divider = get_rate_dividier(m_collective, m_collective_alg, optireduce_in_reduction_phase);
            m_flow->curr_rate /= m_rate_divider;
            if (dcqcn_limit_max_rate) {
                m_flow->max_rate = m_flow->curr_rate;
                m_flow->target_rate = m_flow->curr_rate;
            }
            flow_id_to_info_mapper.insert(std::make_pair(
                    gpu_idx_flow_id_tuple,
                    m_flow));
            if (rate_control_type == dcqcn) {
                dcqcn_schedule_increase_timer(m_flow);
                dcqcn_schedule_alpha_timer(m_flow);
                cc_schedule_send_timer(m_flow);
            } else if (rate_control_type == swift) {
                // don't initialize swift_max_cwnd_pkt_num, swift_cwnd_pkt_num...
                // they will be initialized later
                cc_schedule_send_timer(m_flow);
            } else if (rate_control_type == sharp) {
                // don't initialize swift_max_cwnd_pkt_num, swift_cwnd_pkt_num...
                // they will be initialized later
                cc_schedule_send_timer(m_flow);
            }
        } else {
            EV << "We have information for flow_id: " << flow_id << endl;
            if (!flow_id_found_itr->second->send_timer->isScheduled()) {
                if (flow_id_found_itr->second->has_more_packets != BATCH_SLEEP)
                    throw cRuntimeError("How are we not asleep but the sender timer is not running!");
                cc_cancel_all_timers(flow_id_found_itr->second);
                EV << "In sleep mode, re-initiating timers" << endl;
                if (rate_control_type == dcqcn) {
                    dcqcn_schedule_increase_timer(flow_id_found_itr->second);
                    dcqcn_schedule_alpha_timer(flow_id_found_itr->second);
                    cc_schedule_send_timer(flow_id_found_itr->second);
                } else if (rate_control_type == swift) {
                    cc_schedule_send_timer(flow_id_found_itr->second);
                } else if (rate_control_type == sharp) {
                    cc_schedule_send_timer(flow_id_found_itr->second);
                }
                EV << flow_id_found_itr->second->flow_id << ": waking up" << endl;
            } else if (rate_control_type == dcqcn &&
                    (!flow_id_found_itr->second->increase_timer->isScheduled() ||
                    !flow_id_found_itr->second->alpha_timer->isScheduled()))
                throw cRuntimeError("How is sender timer running but others not running!");
            flow_id_found_itr->second->has_more_packets = BATCH_SEND;
            flow_id_found_itr->second->packet_list.push_back(packet);
            should_push_packet = false;
        }
    }

    addPaddingAndSetFcs(packet, MIN_ETHERNET_FRAME_BYTES);  // calculate valid FCS

    std::string pkt_name = packet->getName();

    if (should_push_packet &&
            partial_mcast_use_cidr_agg &&
            pkt_name.compare("data") == 0) {

        auto packet_dup = packet->dup();
        packet_dup->setFrontIteratorPosition(b(0));
        packet_dup->removeAtFront<EthernetMacHeader>();
        packet_dup->removeAtFront<Ipv4Header>();
        packet_dup->removeAtFront<UdpHeader>();
        packet_dup->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);
        auto chunk = packet_dup->removeAtFront<SliceChunk>();
        auto main_chunk = chunk->getChunk();
        Packet* temp = new Packet();
        temp->insertAtBack(main_chunk);
        auto payload = temp->popAtFront<GenericAppMsg>();

        EV << "2) Payload Cidr AGG group member count is " << payload->getAgg_cidr_group_member_count() << endl;
        EV << "2) Payload Cidr CORE group member count is " << payload->getCore_cidr_group_member_count() << endl;

        if (payload->getAgg_cidr_group_member_count() > 0) {
            std::string cidr_group_id = payload->getAgg_cidr_group_id();
            auto query_cidrid_seqnum_tuple = std::make_tuple(payload->getQuery_id(),
                    cidr_group_id,
                    payload->getSeq_num());
            auto query_cidrid_tuple = std::make_tuple(payload->getQuery_id(),
                    cidr_group_id);
            auto flowid_pkt_itr = query_cidrid_seqnum_to_flowid_packets.find(query_cidrid_seqnum_tuple);
            if (flowid_pkt_itr == query_cidrid_seqnum_to_flowid_packets.end()) {
                std::map<unsigned long, Packet*> tmp_map;
                query_cidrid_seqnum_to_flowid_packets.insert(std::make_pair(query_cidrid_seqnum_tuple, tmp_map));
                query_cidrid_to_candidate_flowid.insert(std::make_pair(query_cidrid_tuple, payload->getRequesterID()));
                flowid_pkt_itr = query_cidrid_seqnum_to_flowid_packets.find(query_cidrid_seqnum_tuple);
            }

            auto candidate_flowid_itr = query_cidrid_to_candidate_flowid.find(query_cidrid_tuple);

            auto flowid_itr = flowid_pkt_itr->second.find(payload->getRequesterID());
            if (flowid_itr != flowid_pkt_itr->second.end()) {
                std::cout << "CIDR info:" << endl;
                std::cout << "Query idx: " << payload->getQuery_id() << endl;
                std::cout << "Tor group idx: " << payload->getTor_group_idx() << endl;
                std::cout << "seq num: " << payload->getSeq_num() << endl;
                std::cout << "flow id: " << payload->getRequesterID() << endl;
                throw cRuntimeError("2) CIDR: Seq num for this flow id and CIDR group/query has already been received! how?");
            }

            // insert these info in stored packets as they will be used later!
            auto m_eth_header = packet->removeAtFront<EthernetMacHeader>();
            if (use_udp && isPacketAppropriateForMulticast(pkt_name)) {
                if (payload->getDst_idx_listArraySize() > 0) {
                    EV << "use_udp && use_host_assisted_routing" << endl;

                    // pkt can be multicasted using CIDR in aggregate switches
                    m_eth_header->setMulticast_pkt_in_CIDR_agg(true);
                    if (partial_mcast_use_cidr_core && payload->getCore_cidr_group_member_count() > 0) {
                        // pkt can be multicasted using CIDR in core switches
                        m_eth_header->setMulticast_pkt_in_CIDR_core(true);
                    }

                    m_eth_header->setDst_idx_listArraySize(payload->getDst_idx_listArraySize());
                    for (int k = 0; k < payload->getDst_idx_listArraySize(); k++)
                        m_eth_header->setDst_idx_list(k, payload->getDst_idx_list(k));

                    m_eth_header->setJump_to_idxArraySize(payload->getJump_to_idxArraySize());
                    for (int k = 0; k < payload->getJump_to_idxArraySize(); k++) {
                        InnerList innerList;
                        innerList.setInnerArrayArraySize(payload->getJump_to_idx(k).getInnerArrayArraySize());
                        for (int t = 0; t < payload->getJump_to_idx(k).getInnerArrayArraySize(); t++) {
                            innerList.setInnerArray(t, payload->getJump_to_idx(k).getInnerArray(t));
                        }
                        m_eth_header->setJump_to_idx(k, innerList);
                    }

                    m_eth_header->setPorts_to_dest_idxArraySize(payload->getPorts_to_dest_idxArraySize());
                    for(int k = 0; k < payload->getPorts_to_dest_idxArraySize(); k++) {
                        InnerList innerList;
                        innerList.setInnerArrayArraySize(payload->getPorts_to_dest_idx(k).getInnerArrayArraySize());
                        for (int t = 0; t < payload->getPorts_to_dest_idx(k).getInnerArrayArraySize(); t++) {
                            innerList.setInnerArray(t, payload->getPorts_to_dest_idx(k).getInnerArray(t));
            //                std::cout << payload->getPath_to_dest_idx(k).getInnerArray(t) << endl;
                        }
                        m_eth_header->setPorts_to_dest_idx(k, innerList);
                    }
                    m_eth_header->setStart_from_idx(1);

                }
            }

            // INA
            m_eth_header->setIna_leaf_aggregation_num(payload->getIna_leaf_aggregation_num());
            m_eth_header->setIna_spine_aggregation_num(payload->getIna_spine_aggregation_num());
            m_eth_header->setIna_core_aggregation_num(payload->getIna_core_aggregation_num());
            m_eth_header->setTree_direction(TREE_UP);

            m_eth_header->setQuery_id(payload->getQuery_id());
            m_eth_header->setFlow_id(payload->getRequesterID());
            m_eth_header->setCollective_scale(payload->getCollective_scale());
            m_eth_header->setTotal_length(payload->getTotal_flow_size());
            m_eth_header->setCollective_alg(payload->getCollective_alg_type());
            m_eth_header->setDst_tor_idx(payload->getDst_tor_idx());
            m_eth_header->setSrc_gpu_idx(payload->getSrc_gpu_idx());
            m_eth_header->setIn_src_sharding(payload->getIn_src_sharding());
            m_eth_header->setUsing_orca(payload->getUsing_orca());
            m_eth_header->setUsing_elmo(payload->getUsing_elmo());
            if (rate_control_type == swift)
                m_eth_header->setSwift_send_time(simTime());
            if (payload->getSeq_num() <= 0)
                throw cRuntimeError("Wow... Invalid seq number <= 0");
            m_eth_header->setSeq_num(payload->getSeq_num());

            m_eth_header->setIs_last_pkt(payload->getIs_last_pkt());
            for (int k = 0; k < payload->getInvolved_servers_listArraySize(); k++) {
                m_eth_header->insertInvolved_servers_list(
                        payload->getInvolved_servers_list(k));
            }

            packet->insertAtFront(m_eth_header);

            flowid_pkt_itr->second.insert(std::make_pair(payload->getRequesterID(), packet));

            if (flowid_pkt_itr->second.size() == payload->getAgg_cidr_group_member_count()) {
                EV << "2) CIDR Got all " << payload->getAgg_cidr_group_member_count() <<
                        " packets for CIDR group " << payload->getAgg_cidr_group_id() << endl;
                should_push_packet = true;
                auto query_cidrid_itr = query_cidrid_to_candidate_flowid.find(query_cidrid_tuple);
                if (query_cidrid_itr == query_cidrid_to_candidate_flowid.end())
                    throw cRuntimeError("1) Candidate flow id must exist in CIDR!");

                auto candidate_packet_itr = flowid_pkt_itr->second.find(query_cidrid_itr->second);
                if (candidate_packet_itr == flowid_pkt_itr->second.end())
                    throw cRuntimeError("2) CIDR: Candidate pkt does not exist!");
                auto candidate_packet = candidate_packet_itr->second;
                unsigned long candidate_flow_id = query_cidrid_itr->second;
                auto candidate_packet_dup = candidate_packet->dup();
                b position = candidate_packet->getFrontOffset();
                candidate_packet->setFrontIteratorPosition(b(0));
                auto eth_header = candidate_packet->removeAtFront<EthernetMacHeader>();
                eth_header->insertCIDR_agg_pkt(candidate_packet_dup);
                for (auto pkt_itr = flowid_pkt_itr->second.begin();
                        pkt_itr != flowid_pkt_itr->second.end();
                        pkt_itr++) {
                    if (pkt_itr->first != candidate_packet_itr->first) {
                        eth_header->insertCIDR_agg_pkt(pkt_itr->second);
                    }
                }
                eth_header->setAgg_cidr_group_id(payload->getAgg_cidr_group_id());
                eth_header->setAgg_cidr_group_member_count(payload->getAgg_cidr_group_member_count());
                EV << "2) Grouped " << eth_header->getCIDR_agg_pktArraySize() <<
                        " packets for one CIDR aggregate" << endl;
                candidate_packet->insertAtFront(eth_header);
                candidate_packet->setFrontIteratorPosition(position);
                packet = candidate_packet;
                flowid_pkt_itr->second.clear();
                query_cidrid_seqnum_to_flowid_packets.erase(query_cidrid_seqnum_tuple);
                if (partial_mcast_use_cidr_core && payload->getCore_cidr_group_member_count() > 0) {
                    EV << "Packets must also get CIDR grouped for core...!" << endl;
                    // perform the same grouping as with AGG for core as well
                    std::string core_cidr_group_id = payload->getCore_cidr_group_id();
                    auto query_corecidrid_seqnum_tuple = std::make_tuple(
                            payload->getQuery_id(),
                            core_cidr_group_id,
                            payload->getSeq_num());
                    auto query_corecidrid_tuple = std::make_tuple(
                            payload->getQuery_id(),
                            core_cidr_group_id);
                    auto flowid_pkt_itr = query_corecidrid_seqnum_to_flowid_packets.find(
                            query_corecidrid_seqnum_tuple);
                    if (flowid_pkt_itr == query_corecidrid_seqnum_to_flowid_packets.end()) {
                        std::map<unsigned long, Packet*> tmp_map;
                        query_corecidrid_seqnum_to_flowid_packets.insert(
                                std::make_pair(query_corecidrid_seqnum_tuple,
                                        tmp_map));
                        query_corecidrid_to_candidate_flowid.insert(
                                std::make_pair(query_corecidrid_tuple,
                                        candidate_flow_id));
                        flowid_pkt_itr = query_corecidrid_seqnum_to_flowid_packets.find(
                                query_corecidrid_seqnum_tuple);
                    }

                    auto candidate_flowid_itr = query_corecidrid_to_candidate_flowid.find(
                            query_corecidrid_tuple);

                    auto flowid_itr = flowid_pkt_itr->second.find(candidate_flow_id);
                    if (flowid_itr != flowid_pkt_itr->second.end()) {
                        std::cout << "CORE CIDR info:" << endl;
                        std::cout << "Query idx: " << payload->getQuery_id() << endl;
                        std::cout << "Tor group idx: " << payload->getTor_group_idx() << endl;
                        std::cout << "seq num: " << payload->getSeq_num() << endl;
                        std::cout << "candidate flow id: " << candidate_flow_id << endl;
                        std::cout << "Core CIDR group id: " << payload->getCore_cidr_group_id() << endl;
                        throw cRuntimeError("2) CIDR Core: Seq num for this flow id and CIDR group/query has already been received! how?");
                    }

                    flowid_pkt_itr->second.insert(
                            std::make_pair(
                                    candidate_flow_id, packet));
                    if (flowid_pkt_itr->second.size() == payload->getCore_cidr_group_member_count()) {
                        EV << "2) CIDR Core Got all " << payload->getCore_cidr_group_member_count() <<
                                " packets for CIDR group " << payload->getCore_cidr_group_id() << endl;
                        should_push_packet = true;
                        auto query_cidrid_itr = query_corecidrid_to_candidate_flowid.find(
                                query_corecidrid_tuple);
                        if (query_cidrid_itr ==
                                query_corecidrid_to_candidate_flowid.end())
                            throw cRuntimeError("2) Candidate flow id must exist in CORE CIDR!");

                        auto candidate_packet_itr = flowid_pkt_itr->second.find(
                                query_cidrid_itr->second);
                        if (candidate_packet_itr == flowid_pkt_itr->second.end())
                            throw cRuntimeError("2) CIDR CORE: Candidate pkt does not exist!");
                        auto candidate_packet = candidate_packet_itr->second;
                        auto candidate_packet_dup = candidate_packet->dup();
                        b position = candidate_packet->getFrontOffset();
                        candidate_packet->setFrontIteratorPosition(b(0));
                        auto eth_header = candidate_packet->removeAtFront<EthernetMacHeader>();
                        eth_header->insertCIDR_core_pkt(candidate_packet_dup);
                        for (auto pkt_itr = flowid_pkt_itr->second.begin();
                                pkt_itr != flowid_pkt_itr->second.end();
                                pkt_itr++) {
                            if (pkt_itr->first != candidate_packet_itr->first) {
                                eth_header->insertCIDR_core_pkt(pkt_itr->second);
                            }
                        }
                        eth_header->setCore_cidr_group_id(payload->getCore_cidr_group_id());
                        eth_header->setCore_cidr_group_member_count(payload->getCore_cidr_group_member_count());
                        EV << "2) Grouped " << eth_header->getCIDR_core_pktArraySize() <<
                                " packets for one CIDR core" << endl;
                        candidate_packet->insertAtFront(eth_header);
                        candidate_packet->setFrontIteratorPosition(position);
                        packet = candidate_packet;
                        flowid_pkt_itr->second.clear();
//                        std::cout << getFullPath() << " removing " <<
//                                payload->getQuery_id() << ", " <<
//                                core_cidr_group_id << ", " <<
//                                payload->getSeq_num() << endl;
                        query_corecidrid_seqnum_to_flowid_packets.erase(
                                query_corecidrid_seqnum_tuple);
                    } else {
                        EV << "2) CORE CIDR: Expecting " << payload->getCore_cidr_group_member_count() <<
                                " for CIDR group " << payload->getCore_cidr_group_id() <<
                                " but got " << flowid_pkt_itr->second.size() << " so far!" << endl;
                        should_push_packet = false;
                    }
                }
            } else {
                EV << "2) Expecting " << payload->getAgg_cidr_group_member_count() <<
                        " for CIDR group " << payload->getAgg_cidr_group_id() <<
                        " but got " << flowid_pkt_itr->second.size() << " so far!" << endl;
                should_push_packet = false;
            }
        }

        delete temp;
        delete packet_dup;

    }

    if (should_push_packet &&
            partial_mcast_programmable_cores &&
            pkt_name.compare("data") == 0) {

        auto packet_dup = packet->dup();
        packet_dup->setFrontIteratorPosition(b(0));
        packet_dup->removeAtFront<EthernetMacHeader>();
        packet_dup->removeAtFront<Ipv4Header>();
        packet_dup->removeAtFront<UdpHeader>();
        packet_dup->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);
        auto chunk = packet_dup->removeAtFront<SliceChunk>();
        auto main_chunk = chunk->getChunk();
        Packet* temp = new Packet();
        temp->insertAtBack(main_chunk);
        auto payload = temp->popAtFront<GenericAppMsg>();

        bool pkts_must_be_grouped = should_packet_be_grouped(payload->getQuery_id(),
                payload->getTor_group_idx(),
                payload->getSeq_num(),
                payload->getController_setup_finish_time());

        if (pkts_must_be_grouped) {
            // if payload->getTor_group_idx() < 0, the packet is destined for the same pod and thus, not grouped
            if (payload->getTor_group_idx() >= 0) {
                EV << "2) Packets must be grouped!: " << payload->getTor_group_idx() <<
                        ", seq num: " << payload->getSeq_num() << endl;
                auto query_torgroup_seqnum_touple = std::make_tuple(payload->getQuery_id(),
                        payload->getTor_group_idx(),
                        payload->getSeq_num());
                auto flowid_pkt_itr = query_torgroup_seqnum_to_flowid_packets.find(
                        query_torgroup_seqnum_touple);
                if (flowid_pkt_itr == query_torgroup_seqnum_to_flowid_packets.end()) {
                    EV << "Info of query " << payload->getQuery_id() <<
                            ", tor group " << payload->getTor_group_idx() <<
                            " and seq num " << payload->getSeq_num() << " not found!" << endl;
                    std::map<unsigned long, Packet*> tmp_map;
                    query_torgroup_seqnum_to_flowid_packets.insert(std::make_pair(
                            query_torgroup_seqnum_touple,
                            tmp_map));
                    flowid_pkt_itr = query_torgroup_seqnum_to_flowid_packets.find(
                            query_torgroup_seqnum_touple);
                }
                auto flowid_itr = flowid_pkt_itr->second.find(payload->getRequesterID());
                if (flowid_itr != flowid_pkt_itr->second.end()) {
                    std::cout << "Query id: " << payload->getQuery_id() << endl;
                    std::cout << "Tor group idx: " << payload->getTor_group_idx() << endl;
                    std::cout << "seq num: " << payload->getSeq_num() << endl;
                    std::cout << "flow id: " << payload->getRequesterID() << endl;
                    throw cRuntimeError("Seq num for this flow id and tor group has already been received! how?");
                }

                // insert these info in stored packets as they will be used later!
                auto m_eth_header = packet->removeAtFront<EthernetMacHeader>();
                if (use_udp && isPacketAppropriateForMulticast(pkt_name)) {
                    if (payload->getDst_idx_listArraySize() > 0) {
                        EV << "use_udp && use_host_assisted_routing" << endl;

                        // pkt can be multicasted on programmable core
                        m_eth_header->setMulticast_pkt_in_programmable_core(true);

                        m_eth_header->setDst_idx_listArraySize(payload->getDst_idx_listArraySize());
                        for (int k = 0; k < payload->getDst_idx_listArraySize(); k++)
                            m_eth_header->setDst_idx_list(k, payload->getDst_idx_list(k));

                        m_eth_header->setJump_to_idxArraySize(payload->getJump_to_idxArraySize());
                        for (int k = 0; k < payload->getJump_to_idxArraySize(); k++) {
                            InnerList innerList;
                            innerList.setInnerArrayArraySize(payload->getJump_to_idx(k).getInnerArrayArraySize());
                            for (int t = 0; t < payload->getJump_to_idx(k).getInnerArrayArraySize(); t++) {
                                innerList.setInnerArray(t, payload->getJump_to_idx(k).getInnerArray(t));
                            }
                            m_eth_header->setJump_to_idx(k, innerList);
                        }

                        m_eth_header->setPorts_to_dest_idxArraySize(payload->getPorts_to_dest_idxArraySize());
                        for(int k = 0; k < payload->getPorts_to_dest_idxArraySize(); k++) {
                            InnerList innerList;
                            innerList.setInnerArrayArraySize(payload->getPorts_to_dest_idx(k).getInnerArrayArraySize());
                            for (int t = 0; t < payload->getPorts_to_dest_idx(k).getInnerArrayArraySize(); t++) {
                                innerList.setInnerArray(t, payload->getPorts_to_dest_idx(k).getInnerArray(t));
                //                std::cout << payload->getPath_to_dest_idx(k).getInnerArray(t) << endl;
                            }
                            m_eth_header->setPorts_to_dest_idx(k, innerList);
                        }
                        m_eth_header->setStart_from_idx(1);

                    }
                }

                // INA
                m_eth_header->setIna_leaf_aggregation_num(payload->getIna_leaf_aggregation_num());
                m_eth_header->setIna_spine_aggregation_num(payload->getIna_spine_aggregation_num());
                m_eth_header->setIna_core_aggregation_num(payload->getIna_core_aggregation_num());
                m_eth_header->setTree_direction(TREE_UP);

                m_eth_header->setQuery_id(payload->getQuery_id());
                m_eth_header->setFlow_id(payload->getRequesterID());
                m_eth_header->setCollective_scale(payload->getCollective_scale());
                m_eth_header->setTotal_length(payload->getTotal_flow_size());
                m_eth_header->setCollective_alg(payload->getCollective_alg_type());
                m_eth_header->setDst_tor_idx(payload->getDst_tor_idx());
                m_eth_header->setSrc_gpu_idx(payload->getSrc_gpu_idx());
                m_eth_header->setIn_src_sharding(payload->getIn_src_sharding());
                m_eth_header->setUsing_orca(payload->getUsing_orca());
                m_eth_header->setUsing_elmo(payload->getUsing_elmo());
                if (rate_control_type == swift)
                    m_eth_header->setSwift_send_time(simTime());
                if (payload->getSeq_num() <= 0)
                    throw cRuntimeError("Wow... Invalid seq number <= 0");
                m_eth_header->setSeq_num(payload->getSeq_num());

                m_eth_header->setIs_last_pkt(payload->getIs_last_pkt());
                for (int k = 0; k < payload->getInvolved_servers_listArraySize(); k++) {
                    m_eth_header->insertInvolved_servers_list(
                            payload->getInvolved_servers_list(k));
                }

                packet->insertAtFront(m_eth_header);

                flowid_pkt_itr->second.insert(std::make_pair(payload->getRequesterID(), packet));
                if (flowid_pkt_itr->second.size() == payload->getNum_msgs_in_tor_group()) {
                    EV << "Got all " << payload->getNum_msgs_in_tor_group() <<
                            " packets for tor group " << payload->getTor_group_idx() << endl;
                    should_push_packet = true;
                    std::tuple<unsigned long, int> query_torgroup_tuple = std::make_tuple(
                            payload->getQuery_id(),
                            payload->getTor_group_idx());
                    auto query_torgroup_itr = query_torgroup_to_candidate_flowid.find(
                            query_torgroup_tuple);
                    if (query_torgroup_itr == query_torgroup_to_candidate_flowid.end()) {
                        EV << "Selecting " << payload->getRequesterID() <<
                                " as the candidate flow id for tor group!" << endl;
                        query_torgroup_to_candidate_flowid.insert(
                                std::make_pair(query_torgroup_tuple,
                                payload->getRequesterID()));
                        query_torgroup_itr = query_torgroup_to_candidate_flowid.find(
                                query_torgroup_tuple);
                    }
                    auto candidate_packet_itr = flowid_pkt_itr->second.find(query_torgroup_itr->second);
                    if (candidate_packet_itr == flowid_pkt_itr->second.end())
                        throw cRuntimeError("Candidate pkt does not exist!");
                    auto candidate_packet = candidate_packet_itr->second;
                    auto candidate_packet_dup = candidate_packet->dup();
                    b position = candidate_packet->getFrontOffset();
                    candidate_packet->setFrontIteratorPosition(b(0));
                    auto eth_header = candidate_packet->removeAtFront<EthernetMacHeader>();
                    eth_header->insertPartial_mcast_core_pkt(candidate_packet_dup);
                    for (auto pkt_itr = flowid_pkt_itr->second.begin();
                            pkt_itr != flowid_pkt_itr->second.end();
                            pkt_itr++) {
                        if (pkt_itr->first != candidate_packet_itr->first) {
                            eth_header->insertPartial_mcast_core_pkt(pkt_itr->second);
                        }
                    }
                    EV << "Added " << eth_header->getPartial_mcast_core_pktArraySize() <<
                            " packets as packet cores" << endl;

                    // this packet must traverse all the way to the tor and then the nested packets must have
                    // setMulticast_pkt_in_CIDR_agg = True
                    eth_header->setMulticast_pkt_in_CIDR_agg(false);

                    candidate_packet->insertAtFront(eth_header);
                    candidate_packet->setFrontIteratorPosition(position);
                    packet = candidate_packet;
                    flowid_pkt_itr->second.clear();
                    query_torgroup_seqnum_to_flowid_packets.erase(query_torgroup_seqnum_touple);
                } else {
                    EV << "Expecting " << payload->getNum_msgs_in_tor_group() <<
                            " for tor group " << payload->getTor_group_idx() <<
                            " but got " << flowid_pkt_itr->second.size() << " so far!" << endl;
                    should_push_packet = false;
                }
            }
        } else
            EV << " -- 2) Packets should not be grouped! " <<
                payload->getController_setup_finish_time() << endl;

        delete temp;
        delete packet_dup;
    }

    // store frame and possibly begin transmitting
    EV_DETAIL << "Frame " << frame << " arrived from higher layers, enqueueing\n";

    if (should_push_packet) {
        pushPacketToQeueue(packet);
    } else {
        if (rate_control_type < 0)
            throw cRuntimeError("How is packet not pushed without udp and a rate_control_type >= 0!");
        EV << "Not pushing the packet on the link" << endl;
    }

    EV << "SEPEHR: TransmitState is " << transmitState << " and TX_IDLE_STATE is " << TX_IDLE_STATE << endl;
    EV << "SEPEHR: transmitState == TX_IDLE_STATE? " << (transmitState == TX_IDLE_STATE) << endl;

    if (transmitState == TX_IDLE_STATE) {
        // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
        // it is nullptr before popping a packet
        EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
        if (!txQueue->isEmpty() && !currentTxFrame)
            popTxQueue();
        beginSendFrames();
    }
}

void AugmentedEtherMacFullDuplex::dcqcn_generate_congestion_notification(Packet* packet) {
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();
    auto ip_header = packet->removeAtFront<Ipv4Header>();
    auto udp_header = packet->removeAtFront<UdpHeader>();
    auto ethFcs = packet->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);

    auto chunk = packet->removeAtFront<SliceChunk>();
    auto main_chunk = chunk->getChunk();
    Packet* temp = new Packet();
    temp->insertAtBack(main_chunk);
    auto payload = temp->popAtFront<GenericAppMsg>();
    unsigned long flow_id = payload->getRequesterID();
    int algorithm_type = payload->getCollective_alg_type();
    int src_gpu_idx = payload->getSrc_gpu_idx();
    delete temp;

    bool should_generate_cnp = false;
    simtime_t now = simTime();
    std::tuple<int, unsigned long> gpu_id_flow_id_tuple = std::make_tuple(src_gpu_idx, flow_id);
    auto flow_id_found_itr = flow_id_to_receiver_cnp_generation_time_mapper.find(gpu_id_flow_id_tuple);
    if (flow_id_found_itr == flow_id_to_receiver_cnp_generation_time_mapper.end()) {
        should_generate_cnp = true;
        flow_id_to_receiver_cnp_generation_time_mapper.insert(
                std::pair<std::tuple<int, unsigned long>, simtime_t>(gpu_id_flow_id_tuple, now));
        flow_id_found_itr = flow_id_to_receiver_cnp_generation_time_mapper.find(gpu_id_flow_id_tuple);
    } else {
        if (now <= flow_id_found_itr->second)
            throw cRuntimeError("how is previous cnp generation time >= now?");
        should_generate_cnp = ((now - flow_id_found_itr->second) >= cnp_generation_gap);
    }

    if (!should_generate_cnp) {
        EV << "CNP generation is not allowed because the time since previous CNP is less than " << cnp_generation_gap << endl;
        return;
    }

    EV << "Generating dcqcn CNP (congestion notification) packet at receiver for flow_id: " << flow_id << endl;
    auto cnp_packet = new Packet("dcqcn_cnp");

    EV << "Creating UDP header" << endl;
    unsigned int org_udp_src_port = udp_header->getSourcePort();
    unsigned int org_udp_dst_port = udp_header->getDestinationPort();
    udp_header->setSourcePort(org_udp_dst_port);
    udp_header->setDestinationPort(org_udp_src_port);
    udp_header->setCrcMode(CRC_DISABLED);
    udp_header->setCrc(0x0000);
    udp_header->setTotalLengthField(udp_header->getChunkLength());
    cnp_packet->insertAtFront(udp_header);

    EV << "Creating IP header" << endl;
    auto org_src_ip_addr = ip_header->getSrcAddress();
    auto org_dst_ip_addr = ip_header->getDestAddress();
    ip_header->setSrcAddress(org_dst_ip_addr);
    ip_header->setDestAddress(org_src_ip_addr);
    ip_header->setTotalLengthField(ip_header->getChunkLength() + udp_header->getChunkLength());
    ip_header->setCrcMode(CRC_COMPUTED);
    ip_header->setCrc(0x0000);
    cnp_packet->insertAtFront(ip_header);

    EV << "Creating ETH header" << endl;
    auto org_src_mac_addr = eth_header->getSrc();
    auto org_dst_mac_addr = eth_header->getDest();
    eth_header->setSrc(org_dst_mac_addr);
    eth_header->setDest(org_src_mac_addr);
    eth_header->setPayload_length(b(0));
    eth_header->setOffset(b(0));
    eth_header->setTotal_length(b(0));
    // for CNP, ECN MUST be marked
    eth_header->setIs_ecn_marked(true);
    eth_header->setCnp_flow_id(flow_id);
    eth_header->setCnp_src_gpu_id(src_gpu_idx);
    eth_header->setOrca_pkt_seen_agent(false);
    eth_header->setRsbf_redundant_send(false);
    eth_header->setIs_drop_notif(false);
    eth_header->setDst_tor_idx(-1);
    eth_header->setIs_last_pkt(false);
    eth_header->setIs_pkt_dropped(false);
    eth_header->setIs_loss_recovery_request(false);
    eth_header->setIs_loss_recovery_response(false);
    cnp_packet->insertAtFront(eth_header);

    EV << "Inserting EthernetFcs" << endl;
    cnp_packet->insertAtBack(ethFcs);
    addPaddingAndSetFcs(cnp_packet, MIN_ETHERNET_FRAME_BYTES);  // calculate valid FCS

    EV << "Final cnp packet is " << cnp_packet->str() << endl;

    // prioritize sending cnp packets
    txQueue->insert_front_of_queue(cnp_packet);
    flow_id_found_itr->second = now;

    if (transmitState == TX_IDLE_STATE) {
        // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
        // it is nullptr before popping a packet
        EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
        if (!currentTxFrame)
            popTxQueue();
        beginSendFrames();
    }

}

void AugmentedEtherMacFullDuplex::handle_cnp_packet(unsigned long cnp_flow_id, int cnp_src_gpu_id) {

    EV << "handle_cnp_packet for flow_id: " << cnp_flow_id << endl;

    if (rate_control_type != dcqcn)
        throw cRuntimeError("CNP received without dcqcn!!");

    std::tuple<int, unsigned long> gpu_id_flow_id_tuple = std::make_tuple(cnp_src_gpu_id, cnp_flow_id);
    auto flow_id_found_itr = flow_id_to_info_mapper.find(gpu_id_flow_id_tuple);
    if (flow_id_found_itr == flow_id_to_info_mapper.end()) {
        EV << "Flow is already terminated and has been removed from flow_id_to_info_mapper" << endl;
        return;
    }

    // Do not cancel send timer
    if (flow_id_found_itr->second->increase_timer->isScheduled())
        cancelEvent(flow_id_found_itr->second->increase_timer);
    if (flow_id_found_itr->second->alpha_timer->isScheduled())
        cancelEvent(flow_id_found_itr->second->alpha_timer);

    EV << "Old rate is " << flow_id_found_itr->second->curr_rate << endl;
    EV << "Old target rate is " << flow_id_found_itr->second->target_rate << endl;
    EV << "Old alpha is " << flow_id_found_itr->second->alpha << endl;
    EV << "g is " << flow_id_found_itr->second->g << endl;

    flow_id_found_itr->second->target_rate = flow_id_found_itr->second->curr_rate;
    flow_id_found_itr->second->curr_rate *= (1 - (flow_id_found_itr->second->alpha / 2.0));
    // dcqcn_mtu is bytes and max rate is bps not Bps
    flow_id_found_itr->second->curr_rate = std::max(flow_id_found_itr->second->curr_rate, dcqcn_mtu*8);
    flow_id_found_itr->second->alpha = (1 - flow_id_found_itr->second->g) * flow_id_found_itr->second->alpha
            + flow_id_found_itr->second->g;

    EV << "New rate is " << flow_id_found_itr->second->curr_rate << endl;
    EV << "New target rate is " << flow_id_found_itr->second->target_rate << endl;
    EV << "New alpha is " << flow_id_found_itr->second->alpha << endl;

    flow_id_found_itr->second->sender_previous_cnp_reaction_time = simTime();
    flow_id_found_itr->second->BC = 0;
    flow_id_found_itr->second->byte_counter = par("dcqcn_init_byte_counter");
    flow_id_found_itr->second->T = 0;

    dcqcn_schedule_increase_timer(flow_id_found_itr->second);
    dcqcn_schedule_alpha_timer(flow_id_found_itr->second);

}

bool AugmentedEtherMacFullDuplex::is_tree_idx_down(unsigned long flow_id,
        int src_gpu, int tree_idx, simtime_t recovery_time) {
    simtime_t now = simTime();
    std::tuple<unsigned long, int, int> flowid_srcgpu_treeidx_tuple =
            std::make_tuple(flow_id, src_gpu, tree_idx);
    auto recoverytime_itr = flowid_srcgpu_treeidx_to_recoverytime.find(flowid_srcgpu_treeidx_tuple);
    if (recoverytime_itr == flowid_srcgpu_treeidx_to_recoverytime.end()) {
        if (recovery_time > now) {
            flowid_srcgpu_treeidx_to_recoverytime.insert(std::make_pair(flowid_srcgpu_treeidx_tuple,
                    recovery_time));
            return true;
        }
    } else {
        if (recovery_time > now) {
            recoverytime_itr->second = std::max(recoverytime_itr->second, recovery_time);
            return true;
        } else if (recoverytime_itr->second > now)
            return true;
        else    // the link is already recovered!
            flowid_srcgpu_treeidx_to_recoverytime.erase(recoverytime_itr);
    }
    return false;
}

void AugmentedEtherMacFullDuplex::fill_mcast_tree_info(EthernetMacHeader* eth_header, bool drop_notif) {

    int current_tree_idx = eth_header->getCurrent_tree_idx();
    if (drop_notif) {
        EV << "Function called as a result of drop notif, the current tree should be moved at least once!" << endl;
        current_tree_idx++;
        current_tree_idx %= eth_header->getAll_ports_to_dstArraySize();
    }

    if (current_tree_idx < 0)
        throw cRuntimeError("current_tree_idx < 0");

    while(is_tree_idx_down(eth_header->getFlow_id(),
            eth_header->getSrc_gpu_idx(),
            current_tree_idx)) {
        EV << "Tree idx changed from " << current_tree_idx;
        current_tree_idx++;
        EV << " to " << current_tree_idx << endl;
        current_tree_idx %= eth_header->getAll_ports_to_dstArraySize();
        if (current_tree_idx == eth_header->getCurrent_tree_idx())
            throw cRuntimeError("All trees are down!!");
    }

    eth_header->setCurrent_tree_idx(current_tree_idx);

    int nTrees = eth_header->getAll_ports_to_dstArraySize();
    if (current_tree_idx < 0 || current_tree_idx >= nTrees) {
        throw cRuntimeError("current_tree_idx=%d out of range [0,%d)", current_tree_idx, nTrees);
    }

    // Source: a TwoDInnerList (which holds InnerList[])
    const TwoDInnerList& srcTwoDPort = eth_header->getAll_ports_to_dst(current_tree_idx);
    const TwoDInnerList& srcTwoDJump = eth_header->getAll_jump_to_idx(current_tree_idx);

    // Clear destination first
    eth_header->setPorts_to_dest_idxArraySize(0);
    eth_header->setJump_to_idxArraySize(0);

    // Resize to exactly match source length
    int m = srcTwoDPort.getInnerArrayArraySize();
    eth_header->setPorts_to_dest_idxArraySize(m);
    // Copy each InnerList over
    for (int i = 0; i < m; ++i) {
        const InnerList& inner = srcTwoDPort.getInnerArray(i);
        eth_header->setPorts_to_dest_idx(i, inner);
    }

    // Resize to exactly match source length
    m = srcTwoDJump.getInnerArrayArraySize();
    eth_header->setJump_to_idxArraySize(m);
    // Copy each InnerList over
    for (int i = 0; i < m; ++i) {
        const InnerList& inner = srcTwoDJump.getInnerArray(i);
        eth_header->setJump_to_idx(i, inner);
    }
}

void AugmentedEtherMacFullDuplex::create_ack_header(Packet *ack_pkt) {

    EV << "Creating ACK packet from " << ack_pkt->str() << endl;

    // change the input packet to the notif
    ack_pkt->setName("ACK");
    b packetPosition = ack_pkt->getFrontOffset();
    ack_pkt->setFrontIteratorPosition(b(0));
    auto phy_header = ack_pkt->removeAtFront<EthernetPhyHeader>();
    auto eth_header = ack_pkt->removeAtFront<EthernetMacHeader>();
    auto ip_header = ack_pkt->removeAtFront<Ipv4Header>();
    auto udp_header = ack_pkt->removeAtFront<UdpHeader>();
    auto chunk = ack_pkt->removeAtFront<GenericAppMsg>();
    auto fcs_header = ack_pkt->peekAtBack<EthernetFcs>(ETHER_FCS_BYTES);

    b fake_payload_length = B(64);
//    fake_payload_length -= phy_header->getChunkLength();
    fake_payload_length -= eth_header->getChunkLength();
    fake_payload_length -= ip_header->getChunkLength();
    fake_payload_length -= udp_header->getChunkLength();
    fake_payload_length -= fcs_header->getChunkLength();

    chunk->setChunkLength(fake_payload_length);
    ack_pkt->insertAtFront(chunk);

    EV << "Creating UDP header" << endl;
    unsigned int org_udp_src_port = udp_header->getSourcePort();
    unsigned int org_udp_dst_port = udp_header->getDestinationPort();
    udp_header->setSourcePort(org_udp_dst_port);
    udp_header->setDestinationPort(org_udp_src_port);
    udp_header->setCrcMode(CRC_DISABLED);
    udp_header->setCrc(0x0000);
    udp_header->setTotalLengthField(udp_header->getChunkLength());
    ack_pkt->insertAtFront(udp_header);

    EV << "Creating IP header" << endl;
    auto org_src_ip_addr = ip_header->getSrcAddress();
    auto org_dst_ip_addr = ip_header->getDestAddress();
    ip_header->setSrcAddress(org_dst_ip_addr);
    ip_header->setDestAddress(org_src_ip_addr);
    ip_header->setTotalLengthField(ip_header->getChunkLength() + udp_header->getChunkLength());
    ip_header->setCrcMode(CRC_COMPUTED);
    ip_header->setCrc(0x0000);
    ack_pkt->insertAtFront(ip_header);

    EV << "Creating ETH header" << endl;
    auto org_src_mac_addr = eth_header->getSrc();
    auto org_dst_mac_addr = eth_header->getDest();
    eth_header->setSrc(org_dst_mac_addr);
    eth_header->setDest(org_src_mac_addr);
    eth_header->setPayload_length(b(0));
    eth_header->setOffset(b(0));
    eth_header->setTotal_length(b(0));
    eth_header->setIs_drop_notif(false);
    eth_header->setOriginal_pkt(nullptr);
    eth_header->setOrca_pkt_seen_agent(false);
    eth_header->setRsbf_redundant_send(false);
    eth_header->setDst_tor_idx(-1);
    eth_header->setHop_count(0);
    eth_header->setIs_last_pkt(false);
    eth_header->setIs_pkt_dropped(false);
    eth_header->setIs_loss_recovery_request(false);
    eth_header->setIs_loss_recovery_response(false);

    if (eth_header->getCidr_base_flow_id() >= 0) {
        EV << "The base flow id is different that what we got: " << eth_header->getCidr_base_flow_id()
                << " and base src gpu: " << eth_header->getCidr_base_src_gpu_idx() << endl;
        if (eth_header->getCidr_base_src_gpu_idx() < 0)
            throw cRuntimeError("How is eth_header->getCidr_base_src_gpu_idx() < 0");
        eth_header->setFlow_id(eth_header->getCidr_base_flow_id());
        eth_header->setSrc_gpu_idx(eth_header->getCidr_base_src_gpu_idx());
        eth_header->setCidr_base_flow_id(-1);
        eth_header->setCidr_base_src_gpu_idx(-1);
    }

    ack_pkt->insertAtFront(eth_header);

//    ack_pkt->insertAtFront(phy_header);
//    ack_pkt->setFrontIteratorPosition(packetPosition);

    if (ack_pkt->getByteLength() < 64)
        throw cRuntimeError("ack_pkt->getBitLength() < B(64)");
}

double AugmentedEtherMacFullDuplex::swift_cwnd_to_rate(double cwnd_pkt_num, simtime_t rtt) {
    double cwnd_bits = cwnd_pkt_num * dcqcn_mtu * 8.0;
    double rate = cwnd_bits / rtt.dbl();
    EV << "cwnd_bits: " << cwnd_bits << ", rtt: " << rtt << ", rate: " << rate << endl;
    return rate;
}

double AugmentedEtherMacFullDuplex::swift_rate_to_cwnd_pkt_num(double rate, simtime_t rtt) {
    double cwnd_bits = rate * rtt.dbl();
    double cwnd_pkt_num = cwnd_bits / (8.0 * dcqcn_mtu);
    EV << "rate: " << rate << ", rtt: " << rtt << ", cwnd_pkt_num: " << cwnd_pkt_num << endl;
    return cwnd_pkt_num;
}

simtime_t AugmentedEtherMacFullDuplex::swift_calculate_fabric_target_delay(int hop_counts, double cwnd) {
    EV << "swift_calculate_fabric_target_delay called. hop_counts: " << hop_counts <<
            ", cwnd: " << cwnd << endl;

    if (cwnd <= 0)
        throw cRuntimeError("Invalid cwnd!");

    if (hop_counts <= 1)
        throw cRuntimeError("hop_counts <= 1");

    double alpha = swift_fs_range /
            ((1.0/std::sqrt(swift_fs_min_cwnd)) - (1.0/std::sqrt(swift_fs_max_cwnd)));
    double beta = -1.0 * alpha / std::sqrt(swift_fs_max_cwnd);
    EV << "alpha is " << alpha << ", beta is " << beta <<
            ", and hop_count is " << hop_counts << endl;
    simtime_t target_delay = swift_base_target_delay +
    hop_counts * swift_per_hop_scaling_factor +
    std::max(0.0, std::min(alpha / std::sqrt(cwnd) + beta, swift_fs_range));
    EV << "target delay is " << target_delay << endl;
    return target_delay;
}

void AugmentedEtherMacFullDuplex::swift_update_send_rates(simtime_t send_time,
        unsigned long flow_id, int src_gpu_idx, int hop_count) {
    EV << "AugmentedEtherMacFullDuplex::swift_update_send_rates called" << endl;

    std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(src_gpu_idx, flow_id);
    auto flow_id_found_itr = flow_id_to_info_mapper.find(gpu_idx_flow_id_tuple);
    if (flow_id_found_itr == flow_id_to_info_mapper.end()) {
        EV << "Flow is finished so we can't update the rate" << endl;
        return;
    }
    auto m_flow = flow_id_found_itr->second;

    simtime_t now = simTime();
    simtime_t delay = now - send_time;

    if (delay <= 0)
        throw cRuntimeError("How is dealy <= 0!");

    double cwnd_pkt_num = swift_rate_to_cwnd_pkt_num(m_flow->curr_rate, delay);

    bool should_decrease_rate = ((now -
            m_flow->swift_last_rate_decrease) >=
            delay);

    simtime_t fabric_target_delay = swift_calculate_fabric_target_delay(hop_count,
            cwnd_pkt_num);
    EV << "delay is " << delay << " and target delay is " << fabric_target_delay << endl;
    EV << "curr_rate before changing: " << m_flow->curr_rate << endl;
    if (delay <= fabric_target_delay) {
        // AI
        double cwnd_addition = 0;
        EV << "Applying AI to fcwnd" << endl;
        if (cwnd_pkt_num >= 1) {
            cwnd_addition = swift_ai / cwnd_pkt_num * 1.0; // we have one ack per packet
        } else {
            cwnd_addition = swift_ai * 1.0;
        }

        if (cwnd_addition <= 0)
            throw cRuntimeError("No AI addition indicated!");
        double ai_rate_addition = swift_cwnd_to_rate(cwnd_addition, delay);
        EV << "increasing rate by : " << ai_rate_addition << endl;
        m_flow->curr_rate += ai_rate_addition;

    } else {
        // MD
        EV << "Applying MD to fcwnd -- should increase? " << should_decrease_rate << endl;
        if (should_decrease_rate) {
            EV << "delay is " << delay << " and target delay is " << fabric_target_delay << endl;
            EV << "MD: " << 1 - swift_beta * ((delay - fabric_target_delay) / delay) <<
                    " , " <<
                    1.0 - swift_max_mdf << endl;
            m_flow->curr_rate *=
                    std::max(1 - swift_beta * ((delay - fabric_target_delay) / delay),
                    1.0 - swift_max_mdf);
            m_flow->swift_last_rate_decrease = now;
        }
    }


    double min_rate = swift_cwnd_to_rate(0.001, delay);

    m_flow->curr_rate = std::max(
            min_rate,
            m_flow->curr_rate);
    m_flow->curr_rate = std::min(
            m_flow->max_rate,
            m_flow->curr_rate);

    EV << "curr_rate after changing: " << m_flow->curr_rate << endl;


}

unsigned long AugmentedEtherMacFullDuplex::computeHash(const std::string s) {
    return static_cast<unsigned long>(std::hash<std::string>{}(s));
}

std::tuple<int,int> AugmentedEtherMacFullDuplex::compute_custodian_elevator_nodes(unsigned long queryid,
        int srcgpu, unsigned long seqnum) {

    // servers under the same ToR must share the same custodian and elevator nodes
    int tor_idx = int (server_module_index / num_servers_under_each_leaf);

    std::string base_id = std::to_string(tor_idx) +
            std::to_string(queryid) +
            std::to_string(srcgpu) +
            std::to_string(seqnum);
    std::string custodian_id = base_id + "1";
    std::string elevator_id = base_id + "2";

    unsigned long custodian_hash = computeHash(custodian_id);
    unsigned long elevator_hash = computeHash(elevator_id);

    // custodian and elevator being the same can cause loop problems
    if (custodian_hash % num_servers_under_each_leaf ==
            elevator_hash % num_servers_under_each_leaf)
        elevator_hash++;

    EV << "custodian_hash: " << custodian_hash << ", elevator_hash: " << elevator_hash << std::endl;

    int base_server_idx = server_module_index - (server_module_index % num_servers_under_each_leaf);
    int custodian_node_idx = base_server_idx + (custodian_hash % num_servers_under_each_leaf);
    int elevator_node_idx = base_server_idx + (elevator_hash % num_servers_under_each_leaf);

    // custodian and elevator being the same can cause loop problems
    if (custodian_node_idx == elevator_node_idx)
        throw cRuntimeError("custodian_node_idx == elevator_node_idx... they should be different");

    EV << "custodian node: " << custodian_node_idx <<
            ", elevator node id: " << elevator_node_idx << std::endl;

    return std::make_tuple(custodian_node_idx, elevator_node_idx);
}

unsigned int AugmentedEtherMacFullDuplex::get_max_num_retrans_requests(Packet* packet) {
    auto tmp_eth_header = packet->peekAtFront<EthernetMacHeader>();

    unsigned long base_flowid;
    if (tmp_eth_header->getCidr_base_flow_id() <= 0) {
        base_flowid = tmp_eth_header->getFlow_id();
    } else {
        base_flowid = tmp_eth_header->getCidr_base_flow_id();
    }
    auto flowid_seqnum_tuple = std::make_tuple(base_flowid, tmp_eth_header->getSeq_num());
    if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
        std::cout << "Looking for " << base_flowid << " , " << tmp_eth_header->getSeq_num() << " in stored_base_packets_for_recovery" << std::endl;
        throw cRuntimeError("We need base record in stored_base_packets_for_recovery here!");
    }

    Packet* pkt = stored_base_packets_for_recovery[flowid_seqnum_tuple];

    if (tmp_eth_header->getCollective_alg() != BCAST_PARTIAL_MULTICAST)
        throw cRuntimeError("get_max_num_retrans_requests is not implemented for anything other than PEEL.");

    auto eth_header = pkt->peekAtFront<EthernetMacHeader>();

    unsigned long agg_cidr_group_member_count = eth_header->getAgg_cidr_group_member_count();
    unsigned long core_cidr_group_member_count = eth_header->getCore_cidr_group_member_count();

    EV << "agg_cidr_group_member_count: " << agg_cidr_group_member_count <<
            ", core_cidr_group_member_count: " << core_cidr_group_member_count << std::endl;

    int member_count;

    if (core_cidr_group_member_count <= 1)
        member_count = agg_cidr_group_member_count;
    else
        member_count = agg_cidr_group_member_count * core_cidr_group_member_count;

    return std::max(1, member_count);
}

unsigned int AugmentedEtherMacFullDuplex::get_max_num_retrans_requests(int agg_cidr_group_member_count, int core_cidr_group_member_count) {

    EV << "agg_cidr_group_member_count: " << agg_cidr_group_member_count <<
            ", core_cidr_group_member_count: " << core_cidr_group_member_count << std::endl;

    int member_count;

    if (core_cidr_group_member_count <= 1)
        member_count = agg_cidr_group_member_count;
    else
        member_count = agg_cidr_group_member_count * core_cidr_group_member_count;

    return std::max(1, member_count);
}

void AugmentedEtherMacFullDuplex::notify_src_of_completed(unsigned long base_flowid, int src_gpu_idx,
                unsigned long seq_num, int src_server_idx) {


    // get the mac address of the recovery server!
    std::string src_mac_path = getFullPath();

    {
        std::size_t start = src_mac_path.find("server[");
        if (start == std::string::npos)
            throw cRuntimeError("server[...] was not found in the path!");

        start += 7; // after "server["
        std::size_t end = src_mac_path.find(']', start);
        if (end == std::string::npos)
            throw cRuntimeError("Closing ] for server[...] was not found!");

        src_mac_path =
                src_mac_path.substr(0, start) +
            std::to_string(src_server_idx) +
            src_mac_path.substr(end);
    }

    {
        std::size_t start = src_mac_path.find("eth[");
        if (start == std::string::npos)
            throw cRuntimeError("eth[...] was not found in the path!");

        start += 4; // after "eth["
        std::size_t end = src_mac_path.find(']', start);
        if (end == std::string::npos)
            throw cRuntimeError("Closing ] for eth[...] was not found!");

        src_mac_path =
                src_mac_path.substr(0, start) +
            std::to_string(src_gpu_idx) +
            src_mac_path.substr(end);
    }

    EV << "src_mac_path: " << src_mac_path << std::endl;

    AugmentedEtherMacFullDuplex* src_mac_module =
                check_and_cast<AugmentedEtherMacFullDuplex*>(getModuleByPath(src_mac_path.c_str()));
    src_mac_module->pkt_successfully_rcvd(base_flowid, seq_num, server_module_index);

}

void AugmentedEtherMacFullDuplex::pkt_successfully_rcvd(unsigned long base_flowid,
                unsigned long seq_num, int receiver_server_idx) {

    Enter_Method("pkt_successfully_rcvd");

    auto flowid_seqnum_tuple = std::make_tuple(base_flowid, seq_num);

    if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
        // why this matters
        // since we are re-broadcasting, a node might get a packet twice and notify the source twice
        // if before replicated notification, the seq num actually completes, we delete it and then when the redundant
        // notif arrives, we don't have any data in stored_base_packets_for_recovery... so we shouldn't do anything!
        EV << base_flowid << " , " << seq_num << " is already complete! no need to do anything!" << std::endl;
        return;
    }

    if (successful_receivers_recorded.find(flowid_seqnum_tuple) == successful_receivers_recorded.end()) {
        std::set<int> tmp_set;
        successful_receivers_recorded[flowid_seqnum_tuple] = tmp_set;
    }

    successful_receivers_recorded[flowid_seqnum_tuple].insert(receiver_server_idx);

    Packet* base_pkt = stored_base_packets_for_recovery[flowid_seqnum_tuple];
    auto base_pkt_eth_header = base_pkt->peekAtFront<EthernetMacHeader>();

    int max_num_receivers;
    int collective_alg = base_pkt_eth_header->getCollective_alg();
    if (collective_alg == BCAST_PARTIAL_MULTICAST) {

        // this representes number of tors
        int max_num_retrans_requests = get_max_num_retrans_requests(base_pkt_eth_header->getAgg_cidr_group_member_count(),
                base_pkt_eth_header->getCore_cidr_group_member_count());

        max_num_receivers = max_num_retrans_requests * num_servers_under_each_leaf;

        int receiver_tor = int(receiver_server_idx / num_servers_under_each_leaf);
        int src_tor = int(server_module_index / num_servers_under_each_leaf);
        if (src_tor == receiver_tor) {
            EV << "Receiver and source are under the same tor, reduce expected receivers by 1" << std::endl;
            max_num_receivers--;
        }
    } else if (collective_alg == BCAST_NETWORK_ASSISTED) {
        max_num_receivers = std::ceil(base_pkt_eth_header->getCollective_scale() * 1.0 / num_gpus);
    } else
        max_num_receivers = 1;  // unicast cases

    EV << base_flowid << " , " << seq_num << std::endl;
    EV << "# of expected successfull receives: " << max_num_receivers <<
            ", actual receives: " << successful_receivers_recorded[flowid_seqnum_tuple].size() << std::endl;

    if (successful_receivers_recorded[flowid_seqnum_tuple].size() >= (max_num_receivers)) {
        EV << base_flowid << "," << seq_num << " completed... remove base packet" << std::endl;
//        successful_receivers_recorded.erase(flowid_seqnum_tuple);
        if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
            std::cout << "Looking for " << base_flowid << " , " << seq_num << " in stored_base_packets_for_recovery" << std::endl;
            throw cRuntimeError("when was base packet deleted!!!");
        }

        auto base_pkt = stored_base_packets_for_recovery[flowid_seqnum_tuple];
        auto ethernet_header = base_pkt->peekAtFront<EthernetMacHeader>();

        unsigned long base_flowid;
        if (ethernet_header->getCidr_base_flow_id() <= 0) {
            base_flowid = ethernet_header->getFlow_id();
        } else {
            base_flowid = ethernet_header->getCidr_base_flow_id();
        }
        auto flow_srcgpu_seqnum_tuple = std::make_tuple(base_flowid,
                ethernet_header->getSrc_gpu_idx(),
                ethernet_header->getSeq_num());
        if (packets_waiting_at_source.find(flow_srcgpu_seqnum_tuple) != packets_waiting_at_source.end()) {
            // this packet is done... cancel whatever request you have for it
            if (packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first != nullptr &&
                    packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first->isScheduled())
                cancelEvent(packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first);
            delete packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first;
            for (auto m_pkt : packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second)
                delete m_pkt;
            packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.clear();
            packets_waiting_at_source.erase(flow_srcgpu_seqnum_tuple);
        }

        perform_deep_delete(base_pkt);
        stored_base_packets_for_recovery.erase(flowid_seqnum_tuple);

    }
}

int AugmentedEtherMacFullDuplex::get_hierarchical_pkt_num_gap_threshold() {

    if (hierarchical_seqnum_gap_bdp_multiplier <= 0)
        throw cRuntimeError("hierarchical_seqnum_gap_bdp_multiplier <= 0");

    if (hierarchical_seqnum_gap_bdp_multiplier < 1)
        throw cRuntimeError("hierarchical_seqnum_gap_bdp_multiplier < 1... ARE U SURE!?");

    // since our seq num is in packet number, this function returns BDP devided by MTU

    /*
     * BDP = min_RTT * bw (for now I hardcode 100 Gbps)
     * min_RTT is almost hop_count * mtu / bw * 2 (round-trip)
     * (ignoring small propagation delays
     * BDP is almost hop_count * mtu * 2
     * so BDP/MTU in pkt num is hop_count * 2
     */

    int hop_count = -1;
    if (use_fattree) {
        hop_count = 6;
    } else {
        hop_count = 4;
    }

    int pkt_num_bdp = 2 * hop_count;

    int scale_pkt_num_bdp = int(hierarchical_seqnum_gap_bdp_multiplier * pkt_num_bdp);

    EV << "scale_pkt_num_bdp: " << scale_pkt_num_bdp << std::endl;

    return scale_pkt_num_bdp;
}

void AugmentedEtherMacFullDuplex::processMsgFromNetwork(EthernetSignal *signal)
{
    EV_INFO << signal << " received from network." << endl;

    light_throughput_bits_rcvd += signal->getBitLength();

    if (!connected || disabled) {
        EV_WARN << (!connected ? "Interface is not connected" : "MAC is disabled") << " -- dropping msg " << signal << endl;
        if (typeid(*signal) == typeid(EthernetSignal)) {    // do not count JAM and IFG packets
            auto packet = check_and_cast<Packet *>(signal->decapsulate());
            delete signal;
            decapsulate(packet);
            PacketDropDetails details;
            details.setReason(INTERFACE_DOWN);
            emit(packetDroppedSignal, packet, &details);
            delete packet;
            numDroppedIfaceDown++;
        }
        else
            delete signal;

        return;
    }

    if (signal->getSrcMacFullDuplex() != duplexMode)
        throw cRuntimeError("Ethernet misconfiguration: MACs on the same link must be all in full duplex mode, or all in half-duplex mode");

    if (dynamic_cast<EthernetFilledIfgSignal *>(signal))
        throw cRuntimeError("There is no burst mode in full-duplex operation: EtherFilledIfg is unexpected");
    bool hasBitError = signal->hasBitError();
    auto packet = check_and_cast<Packet *>(signal->decapsulate());
    std::string pkt_name = packet->getName();
    delete signal;
    EV << "Signal is a packet: " << packet->str() << endl;
    totalSuccessfulRxTime += packet->getDuration();
    decapsulate(packet);
    emit(packetReceivedFromLowerSignal, packet);

    auto ethernet_header = packet->peekAtFront<EthernetMacHeader>();

//    std::cout << getFullPath() << ": " << ethernet_header->getCidr_base_flow_id() << std::endl;
//    std::cout << getFullPath() << ": " << ethernet_header->getCidr_base_src_gpu_idx() << std::endl;
//    std::cout << "----------------------------" << std::endl;

    // drop if it's redundant RSBF packet
    if (ethernet_header->getRsbf_redundant_send()) {
        EV << "RSBF redundant sent! Dropping..." << endl;
        delete packet;
        return;
    }

    if (pkt_name.compare("treeFin") == 0) {
        EV << "treeFin packet received... dropping!!" << endl;
        delete packet;
        return;
    }

//    std::cout << ethernet_header->getFlow_id() << std::endl;
//    std::cout << ethernet_header->getSrc_gpu_idx() << std::endl;
//    std::cout << ethernet_header->getTotal_length() << std::endl;
//    std::cout << "----------------" << std::endl;

//    std::cout << getFullPath() << std::endl;
//    for (int i = 0; i < ethernet_header->getDst_idx_listArraySize(); i++) {
//        std::cout << ethernet_header->getDst_idx_list(i) << ", ";
//    }
//    std::cout << std::endl;
//    std::cout << "-----------------------" << std::endl;

    if (int(ethernet_header->getIs_pkt_dropped()) +
            int(ethernet_header->getIs_loss_recovery_request()) +
            int(ethernet_header->getIs_loss_recovery_response()) > 1) {
        std::cout << packet->str() << std::endl;
        std::cout << ethernet_header->getIs_pkt_dropped() << std::endl;
        std::cout << ethernet_header->getIs_loss_recovery_request() << std::endl;
        std::cout << ethernet_header->getIs_loss_recovery_response() << std::endl;
        throw cRuntimeError("A packet cannot be dropped, recovery request and recovery response at the same time!");
    }

    if (ethernet_header->getIs_pkt_dropped()) {

        // this is different from is_drop_notif
        // drop notif is for when you are using switch generated drop notifications
        EV << "dropped packet is " << packet->str() << std::endl;
        auto original_pkt = ethernet_header->getOriginal_pkt()->dup();
        delete ethernet_header->getOriginal_pkt();
        delete packet;
        packet = original_pkt;
        EV << "original packet is " << packet->str() << std::endl;

        b packetPosition = packet->getFrontOffset();
        packet->setFrontIteratorPosition(b(0));
        auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
        auto eth_header = packet->removeAtFront<EthernetMacHeader>();

        eth_header->setIs_pkt_dropped(false);
        eth_header->setIs_loss_recovery_request(false);
        eth_header->setIs_loss_recovery_response(false);
        packet->insertAtFront(eth_header);
        packet->insertAtFront(phy_header);
        packet->setFrontIteratorPosition(packetPosition);

        ethernet_header = packet->peekAtFront<EthernetMacHeader>();

        EV << getFullPath() << ": Recording lost packet --- query: " << ethernet_header->getQuery_id() <<
                ", src gpu: " << ethernet_header->getSrc_gpu_idx() <<
                ", seqnum: " << ethernet_header->getSeq_num() << std::endl;

        if (recovery_type == RING_BASED) {

            EV << "# servers involved in the collective: " <<
                    ethernet_header->getInvolved_servers_listArraySize() << std::endl;
            if (ethernet_header->getInvolved_servers_listArraySize() == 0)
                throw cRuntimeError("recovery_type == RING_BASED && \
                                eth_header->getInvolved_servers_listArraySize() == 0");
            for (int k = 0; k < ethernet_header->getInvolved_servers_listArraySize(); k++) {
                EV << ethernet_header->getInvolved_servers_list(k) << ", ";
            }
            EV << std::endl;

            auto query_srcgpu_seqnum_tuple = std::make_tuple(
                    ethernet_header->getQuery_id(),
                    ethernet_header->getSrc_gpu_idx(),
                    ethernet_header->getSeq_num());

            auto query_srcgpu_tuple = std::make_tuple(
                    ethernet_header->getQuery_id(),
                    ethernet_header->getSrc_gpu_idx());

            if (lost_pkt_map.find(query_srcgpu_seqnum_tuple) != lost_pkt_map.end()) {
                EV << ethernet_header->getQuery_id() << "," <<
                        ethernet_header->getSeq_num() << " already exists... delete packet!" << std::endl;
                delete packet;
//                throw cRuntimeError("Multiple notifications for the same packet received!");
                return;
            }

            auto pkt_lost_timer = new cMessage("pkt_lost_timer");
            lost_pkt_map.insert(std::make_pair(query_srcgpu_seqnum_tuple,
                    std::make_tuple(pkt_lost_timer, packet)));

            pkt_lost_timer->addPar("query") = ethernet_header->getQuery_id();
            pkt_lost_timer->addPar("seqnum") = ethernet_header->getSeq_num();
            pkt_lost_timer->addPar("srcgpu") = ethernet_header->getSrc_gpu_idx();

            if (query_srcgpu_to_lostseqs.find(query_srcgpu_tuple) ==
                    query_srcgpu_to_lostseqs.end()) {
                std::set<unsigned long> tmp_set;
                query_srcgpu_to_lostseqs[query_srcgpu_tuple] = tmp_set;
            }
            query_srcgpu_to_lostseqs[query_srcgpu_tuple].insert(
                    ethernet_header->getSeq_num());

            query_srcgpu_to_waitedseqs.insert(query_srcgpu_seqnum_tuple);

            if (recovery_wait_time <= 0)
                throw cRuntimeError("recovery_wait_time <= 0");

            double wait_time = recovery_wait_time;
            if (last_pkt_arrived_record.find(query_srcgpu_tuple) != last_pkt_arrived_record.end()) {
                EV << "Last packet already arrived... no need to wait... request re-transmission right away!!" << std::endl;
                wait_time = 0;
            } else
                EV << "did not find " << ethernet_header->getQuery_id() << "," <<
                    ethernet_header->getSrc_gpu_idx() << " in last_pkt_arrived_record" << std::endl;


            scheduleAt(simTime() + wait_time, pkt_lost_timer);

        } else if (recovery_type == HIERARCHICAL) {

            EV << "# servers involved in the collective: " <<
                    ethernet_header->getInvolved_servers_listArraySize() << std::endl;
            if (ethernet_header->getInvolved_servers_listArraySize() == 0)
                throw cRuntimeError("recovery_type == HIERARCHICAL && \
                                eth_header->getInvolved_servers_listArraySize() == 0");

            auto query_srcgpu_seqnum_tuple = std::make_tuple(
                    ethernet_header->getQuery_id(),
                    ethernet_header->getSrc_gpu_idx(),
                    ethernet_header->getSeq_num());

            auto query_srcgpu_tuple = std::make_tuple(
                    ethernet_header->getQuery_id(),
                    ethernet_header->getSrc_gpu_idx());

            if (lost_pkt_map.find(query_srcgpu_seqnum_tuple) != lost_pkt_map.end()) {
                EV << ethernet_header->getQuery_id() << "," <<
                        ethernet_header->getSeq_num() << " already exists... delete packet!" << std::endl;
                // original already lost its nested data so no need for deep delete
                delete packet;
//                throw cRuntimeError("Multiple notifications for the same packet received!");
                return;
            }

            long hierarchical_pkt_num_gap_threshold = get_hierarchical_pkt_num_gap_threshold();
            if (hierarchical_pkt_num_gap_threshold <= 1)
                throw cRuntimeError("hierarchical_seq_num_gap_threshold must be > 1");

            // when should we send packet right away
            // if the last packet for this flow has already arrived
            // or
            // max seq num - missing packet seq num > hierarchical_seq_num_gap_threshold
            bool should_pkt_be_sent_right_way = (last_pkt_arrived_record.find(query_srcgpu_tuple) != last_pkt_arrived_record.end()) ||
                    (hierarchical_max_seqnum.find(query_srcgpu_tuple) != hierarchical_max_seqnum.end() &&
                            hierarchical_max_seqnum[query_srcgpu_tuple] - ethernet_header->getSeq_num() >= hierarchical_pkt_num_gap_threshold);

            if (should_pkt_be_sent_right_way) {

                EV << "Must send packet right away!!" << std::endl;

                unsigned long m_queryid = ethernet_header->getQuery_id();
                unsigned long m_seqnum = ethernet_header->getSeq_num();
                int m_srcgpu = ethernet_header->getSrc_gpu_idx();
                // as we are sending the request right away...
                // no need to record anything in query_srcgpu_to_lostseqs
                query_srcgpu_to_waitedseqs.insert(query_srcgpu_seqnum_tuple);
                lost_pkt_map.insert(std::make_pair(query_srcgpu_seqnum_tuple,
                        std::make_tuple(nullptr, packet)));
                create_and_send_recovery_request(m_queryid, m_srcgpu, m_seqnum);

            } else {

                EV << "must keep packet with a timer for now as the gap condition is not met!" << std::endl;

                auto pkt_lost_timer = new cMessage("pkt_lost_timer");
                lost_pkt_map.insert(std::make_pair(query_srcgpu_seqnum_tuple,
                        std::make_tuple(pkt_lost_timer, packet)));

                pkt_lost_timer->addPar("query") = ethernet_header->getQuery_id();
                pkt_lost_timer->addPar("seqnum") = ethernet_header->getSeq_num();
                pkt_lost_timer->addPar("srcgpu") = ethernet_header->getSrc_gpu_idx();

                if (query_srcgpu_to_lostseqs.find(query_srcgpu_tuple) ==
                        query_srcgpu_to_lostseqs.end()) {
                    std::set<unsigned long> tmp_set;
                    query_srcgpu_to_lostseqs[query_srcgpu_tuple] = tmp_set;
                }
                query_srcgpu_to_lostseqs[query_srcgpu_tuple].insert(
                        ethernet_header->getSeq_num());

                query_srcgpu_to_waitedseqs.insert(query_srcgpu_seqnum_tuple);

                if (recovery_wait_time <= 0)
                    throw cRuntimeError("recovery_wait_time <= 0");

                // for now don't set the timer and only rely on either the last packet or more packets comming in
                // might want to change this later
//                scheduleAt(simTime() + recovery_wait_time, pkt_lost_timer);

            }

        } else {
            std::cout << "Recovery type: " << recovery_type << std::endl;
            throw cRuntimeError("Unsupported recovery_type!");
        }

        return;
    }
    if (ethernet_header->getIs_loss_recovery_request()) {

        EV << "Loss recovery request received" << std::endl;
        EV << "recovery request packet is " << packet->str() << std::endl;

        b with_phy_header_position = packet->getFrontOffset();;

        auto original_pkt = ethernet_header->getOriginal_pkt()->dup();
        perform_deep_delete(ethernet_header->getOriginal_pkt());
        perform_deep_delete(packet);
        packet = original_pkt;

        b packetPosition = packet->getFrontOffset();
        packet->setFrontIteratorPosition(b(0));
        auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
        auto eth_header = packet->removeAtFront<EthernetMacHeader>();
        eth_header->setIs_pkt_dropped(false);
        eth_header->setIs_loss_recovery_request(false);
        eth_header->setIs_loss_recovery_response(true);
        eth_header->setDst_tor_idx(-1);
        eth_header->setMulticast_pkt_in_programmable_core(false);
        eth_header->setMulticast_pkt_in_CIDR_agg(false);
        eth_header->setMulticast_pkt_in_CIDR_core(false);
        packet->insertAtFront(eth_header);

        // no need to add phy_header again and keep the position as 0
//        packet->insertAtFront(phy_header);
//        packet->setFrontIteratorPosition(packetPosition);

        ethernet_header = packet->peekAtFront<EthernetMacHeader>();
        std::list<unsigned long> involved_servers_list;
        for (int k = 0; k < ethernet_header->getInvolved_servers_listArraySize(); k++) {
            involved_servers_list.push_back(ethernet_header->getInvolved_servers_list(k));
        }
        // find the node before you in the ring
        if (involved_servers_list.size() == 0)
            throw cRuntimeError("We need more than 0 involved servers here");
        auto involved_server_itr = involved_servers_list.begin();
        int src_server_idx = (*involved_server_itr);

        EV << "corresponding recovery response packet is " << packet->str() << std::endl;

        int custodian_node_id = -1;
        int elevator_node_id = -1;
        if (recovery_type == HIERARCHICAL) {
            auto custodian_elevator_idx_tuple = compute_custodian_elevator_nodes(
                    ethernet_header->getQuery_id(),
                    ethernet_header->getSrc_gpu_idx(),
                    ethernet_header->getSeq_num());

            custodian_node_id = std::get<0>(custodian_elevator_idx_tuple);
            elevator_node_id = std::get<1>(custodian_elevator_idx_tuple);
        }

        if (is_pkt_lost_for_this_server(ethernet_header->getQuery_id(),
                ethernet_header->getSrc_gpu_idx(),
                ethernet_header->getSeq_num(),
                true)) {
            // whether it is ring-based and we're waiting for the neighbor to send us
            // or we are custodian and have taken proper action (contact elevator/source)
            // we must wait!packet
            // only difference is that if we are the elevator, then we must contact source
            if (server_module_index == elevator_node_id) {
                // if this is true, then we are hierarchical and this node is elevator
                EV << "Elevator got the request from custodian... send it to source: " << src_server_idx << std::endl;
                // create a fake phy header just for processing
                auto phy_header = makeShared<EthernetPhyHeader>();
                packet->insertAtFront(phy_header);
                packet->setFrontIteratorPosition(with_phy_header_position);
                send_p2p_recovery_request(src_server_idx, packet, ethernet_header->getSrc_gpu_idx());
            } else {
                // either ring-based or custodian
                EV << "Packet is also lost for us... keep waiting..." << std::endl;
                auto query_srcgpu_seqnum_tuple = std::make_tuple(ethernet_header->getQuery_id(),
                                ethernet_header->getSrc_gpu_idx(),
                                ethernet_header->getSeq_num());
                if (recovery_request_waitlist.find(query_srcgpu_seqnum_tuple) ==
                        recovery_request_waitlist.end()) {
                    std::list<Packet*> tmp_list;
                    recovery_request_waitlist[query_srcgpu_seqnum_tuple] = tmp_list;
                }
                recovery_request_waitlist[query_srcgpu_seqnum_tuple].push_back(packet);
            }
        } else {

            if (recovery_type == HIERARCHICAL && server_module_index == src_server_idx) {
                EV << "source received re-transmission request in hierarchical mode..." << std::endl;
                // when in hierarchical mode, sources behavior changes with the situation
                // at the source to isolated different multicast trees from one another, we use base_flow id

                unsigned long base_flowid;
                if (ethernet_header->getCidr_base_flow_id() <= 0) {
                    base_flowid = ethernet_header->getFlow_id();
                } else {
                    base_flowid = ethernet_header->getCidr_base_flow_id();
                }

                auto flow_srcgpu_seqnum_tuple = std::make_tuple(base_flowid,
                        ethernet_header->getSrc_gpu_idx(),
                        ethernet_header->getSeq_num());

                // if base packet is already deleted... no need to re-transmit
                auto flowid_seqnum_tuple = std::make_tuple(base_flowid, ethernet_header->getSeq_num());
                if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
                    EV << "Packet is already successfully delivered... no need to re-transmit...." << std::endl;
                    delete packet;
                    return;
                }

                int max_num_retrans_requests = get_max_num_retrans_requests(packet);

                if (packets_waiting_at_source.find(flow_srcgpu_seqnum_tuple) == packets_waiting_at_source.end()) {
                    std::list<Packet*> tmp_list;
                    auto src_retransmission_timer = new cMessage("src_retransmission_timer");
                    src_retransmission_timer->addPar("flow") = base_flowid;
                    src_retransmission_timer->addPar("seqnum") = ethernet_header->getSeq_num();
                    src_retransmission_timer->addPar("srcgpu") = ethernet_header->getSrc_gpu_idx();
                    src_retransmission_timer->addPar("max_ratrans_num") = max_num_retrans_requests;
                    packets_waiting_at_source[flow_srcgpu_seqnum_tuple] = std::make_pair(src_retransmission_timer, tmp_list);
                }

                if (hierarchical_src_recovery_wait_time <= 0)
                    throw cRuntimeError("Invalid hierarchical_src_recovery_wait_time");

                // push the packet and schedule the timer if it is not already scheduled!
                packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.push_back(packet);
                if (!packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first->isScheduled()) {
                    scheduleAt(simTime() + hierarchical_src_recovery_wait_time, packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first);
                }

                EV << "max_num_retrans_requests: " << max_num_retrans_requests <<
                        ", num_dropped: " << packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.size() << std::endl;

                // we can re-broadcast early just when packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.size() >= max_num_retrans_requests
                // if we re-broadcast like when it is 50% of max num, then for the same dropped packet we might re-broadcast multiple times
                bool should_rebroadcast = (packets_waiting_at_source[flow_srcgpu_seqnum_tuple].second.size() >= max_num_retrans_requests);

                // now let's see if we need to just re-broadcast early
                if (should_rebroadcast) {

                    EV << "Enough packet's are missing... re-transmit right away" << std::endl;

                    // cancel and delete the timer
                    if (packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first->isScheduled())
                        cancelEvent(packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first);
                    delete packets_waiting_at_source[flow_srcgpu_seqnum_tuple].first;

                    // now re-transmit the packet

                    appropriately_retransmit_packets_at_the_source(base_flowid,
                            ethernet_header->getSrc_gpu_idx(),
                            ethernet_header->getSeq_num(),
                            max_num_retrans_requests);
                }
            } else {
                // whether in ring-based, a custodian, or elevator, just send it to whose asking for it
                EV << "We have the packet... re-transmitting..." << std::endl;

                pushPacketToQeueue(packet);

                if (transmitState == TX_IDLE_STATE) {
                    // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
                    // it is nullptr before popping a packet
                    EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
                    if (!txQueue->isEmpty() && !currentTxFrame)
                        popTxQueue();
                    beginSendFrames();
                }
            }
        }

        return;
    }

    if (ethernet_header->getIs_loss_recovery_response()) {

        EV << "Response received..." << std::endl;

        auto query_srcgpu_seqnum_tuple = std::make_tuple(ethernet_header->getQuery_id(),
                ethernet_header->getSrc_gpu_idx(),
                ethernet_header->getSeq_num());

        erase_from_waitedseqs(ethernet_header->getQuery_id(),
                ethernet_header->getSrc_gpu_idx(),
                ethernet_header->getSeq_num(),
                true);

        fullfill_waiting_requests(ethernet_header->getQuery_id(),
                ethernet_header->getSrc_gpu_idx(),
                ethernet_header->getSeq_num(),
                true,
                ethernet_header->getHierarchical_resent_from_source());
    }

    if (ethernet_header->getUsing_orca() &&
            ((initiate_tree && pkt_name.compare("treeInit") == 0) ||
                    (!initiate_tree && pkt_name.compare("data") == 0))) {
        if (!ethernet_header->getOrca_pkt_seen_agent() &&
                ethernet_header->getHop_count() > 2) {
            EV << "This is the agent, send the packet back into the network" << endl;
            // the original packet should be sent up
            auto pkt_dup = packet->dup();
            b position = pkt_dup->getFrontOffset();
            pkt_dup->setFrontIteratorPosition(b(0));
            auto phy_header = pkt_dup->removeAtFront<EthernetPhyHeader>();
            auto frame = pkt_dup->removeAtFront<EthernetMacHeader>();
            frame->setOrca_pkt_seen_agent(true);
            pkt_dup->insertAtFront(frame);
            pkt_dup->insertAtFront(phy_header);
            pkt_dup->setFrontIteratorPosition(position);
            pushPacketToQeueue(pkt_dup);
//            txQueue->pushPacket(pkt_dup);

            if (transmitState == TX_IDLE_STATE) {
                // Since we might be in IDLE state due to PFC halt while currentTX is not nullptr, make sure
                // it is nullptr before popping a packet
                EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
                if (!txQueue->isEmpty() && !currentTxFrame)
                    popTxQueue();
                beginSendFrames();
            }
        }
    }

    // treeInit should be dropped after it is checked for orca
    if (pkt_name.compare("treeInit") == 0) {
        EV << "treeInit packet received... dropping!!" << endl;
        delete packet;
        return;
    }

    if (ethernet_header->getIs_drop_notif()) {
        // drop notif is for when you are using switch generated drop notifications

        const Packet* tmp_original_pkt = ethernet_header->getOriginal_pkt();
        auto original_ethernet_header = tmp_original_pkt->peekAtFront<EthernetMacHeader>();

        unsigned long base_flowid;
        if (original_ethernet_header->getCidr_base_flow_id() <= 0) {
            base_flowid = original_ethernet_header->getFlow_id();
        } else {
            base_flowid = original_ethernet_header->getCidr_base_flow_id();
        }
        auto flowid_seqnum_tuple = std::make_tuple(base_flowid, original_ethernet_header->getSeq_num());
        if (stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()) {
            EV << "Expected packet is already completed." << std::endl;
            perform_deep_delete(tmp_original_pkt);
            delete packet;
            return;
//
//            std::cout << "Looking for " << base_flowid << " , " << original_ethernet_header->getSeq_num() << " in stored_base_packets_for_recovery" << std::endl;
//            throw cRuntimeError("stored_base_packets_for_recovery.find(flowid_seqnum_tuple) == stored_base_packets_for_recovery.end()");
        }
        Packet* original_pkt = create_base_pkt_deep_copy(stored_base_packets_for_recovery[flowid_seqnum_tuple]);
        // we want to re-broadcast so delete the original
        // deep delete the original packet as it might have left out info in it
        perform_deep_delete(tmp_original_pkt);
        // delete the notif packet
        delete packet;

        EV << "Drop notif received!! Removing notif... Re-sending original/base packet: " <<
                original_pkt->str() << endl;
//        delete ethernet_header->getOriginal_pkt();

        // original packet doesn't have phyheader anymore
        original_pkt->setFrontIteratorPosition(b(0));
//        original_pkt->removeAtFront<EthernetPhyHeader>();
        auto eth_header = original_pkt->removeAtFront<EthernetMacHeader>();

        // used for recovery
        unsigned long m_flow_id = eth_header->getFlow_id();
        unsigned long m_seq_num = eth_header->getSeq_num();

        if (eth_header->getCurrent_tree_idx() >= 0) {
            EV << "We need to shift the tree..." << endl;
            simtime_t recovery_time = ethernet_header->getRecovery_time();
//            if (recovery_time == 0)
//                throw cRuntimeError("Invalid recovery time from notif!");
            // bring tree down till recover time
            EV << "Bringing tree " << eth_header->getCurrent_tree_idx() << " down till " << recovery_time << endl;
            is_tree_idx_down(eth_header->getFlow_id(),
                        eth_header->getSrc_gpu_idx(),
                        eth_header->getCurrent_tree_idx(),
                        recovery_time);
            fill_mcast_tree_info(eth_header.get(), true);
        }

        eth_header->setHop_count(0);
        eth_header->setDont_drop_again(true);

        original_pkt->insertAtFront(eth_header);

        // with multicast, we might get multiple drop notifs for the same packet, so impose a timed reaction frequency
        bool limit_reaction = false;
        double m_wait_time = recovery_wait_time;    // for unicast based mechanism, notif recovery is in fact timer-based
        switch (eth_header->getCollective_alg()) {
            case BCAST_NETWORK_ASSISTED:
            case BCAST_PARTIAL_MULTICAST:
                limit_reaction = true;
                EV << "using timed reaction!" << std::endl;
                m_wait_time = 0;
                break;
            default:
                if (m_wait_time <= 0)
                    throw cRuntimeError("m_wait_time <= 0 for unicast collective!");
                break;
        }

        bool should_retransmit_packet = true;

        if (limit_reaction) {
            auto flowid_seqnum_tuple = std::make_tuple(m_flow_id, m_seq_num);

            EV << "flow id: " << m_flow_id << ", seq num: " << m_seq_num << std::endl;

            if (previously_retransmitted.find(flowid_seqnum_tuple) == previously_retransmitted.end()) {
                previously_retransmitted.insert(flowid_seqnum_tuple);
            } else
                should_retransmit_packet = false;   // do not re-transmit already re-transmitted packets
        }

        if (should_retransmit_packet) {
            // no need to wait, send right away
            if (m_wait_time <= 0) {
                EV << "Re-transmitting packet: " << original_pkt->str() << std::endl;

                pushPacketToQeueue(original_pkt);
    //            txQueue->pushPacket(original_pkt);

                if (transmitState == TX_IDLE_STATE) {
                    EV << "SEPEHR: Checking if queue is empty? " << txQueue->isEmpty() << endl;
                    if (!txQueue->isEmpty() && !currentTxFrame)
                        popTxQueue();
                    beginSendFrames();
                }
            } else {
                auto tmp_eth_header = original_pkt->peekAtFront<EthernetMacHeader>();
                auto flowid_seqnum_tuple = std::make_tuple(tmp_eth_header->getFlow_id(),
                        tmp_eth_header->getSeq_num());
                if (send_later.find(flowid_seqnum_tuple) != send_later.end())
                    throw cRuntimeError("The same packet is being re-transmitted multiple times at once!!");
                send_later[flowid_seqnum_tuple] = original_pkt;

                auto send_later_timer = new cMessage("send_later_timer");
                send_later_timer->addPar("flow") = tmp_eth_header->getFlow_id();
                send_later_timer->addPar("seqnum") = tmp_eth_header->getSeq_num();
                scheduleAt(simTime() + m_wait_time, send_later_timer);

            }
        } else {
            // do a deep delete
            perform_deep_delete(original_pkt);
        }

        return;
    }

    // packet is healthy and should move up
    if (recovery_type == HIERARCHICAL && pkt_name == "data") {

        // send recovery request for packets that are waiting
        // update max seq num

        auto query_srcgpu_tuple = std::make_tuple(ethernet_header->getQuery_id(),
                ethernet_header->getSrc_gpu_idx());
        if (hierarchical_max_seqnum.find(query_srcgpu_tuple) == hierarchical_max_seqnum.end())
            hierarchical_max_seqnum[query_srcgpu_tuple] = 0;
        hierarchical_max_seqnum[query_srcgpu_tuple] =
                std::max(hierarchical_max_seqnum[query_srcgpu_tuple],
                        ethernet_header->getSeq_num());


        auto query_found_it = query_srcgpu_to_lostseqs.find(query_srcgpu_tuple);
        if (query_found_it != query_srcgpu_to_lostseqs.end()) {

            long hierarchical_pkt_num_gap_threshold = get_hierarchical_pkt_num_gap_threshold();
            if (hierarchical_pkt_num_gap_threshold <= 1)
                throw cRuntimeError("hierarchical_seq_num_gap_threshold must be > 1");

            while (query_found_it->second.size() != 0) {
                // lost seqs are sorted
                auto lowest_seq = (*query_found_it->second.begin());
                EV << "lowest seq: " << lowest_seq << ", max seq: " <<
                        hierarchical_max_seqnum[query_srcgpu_tuple] << std::endl;
                if (hierarchical_max_seqnum[query_srcgpu_tuple] -
                        lowest_seq >= hierarchical_pkt_num_gap_threshold) {
                    EV << "Sending request for seq num " << lowest_seq << std::endl;
                    create_and_send_recovery_request(ethernet_header->getQuery_id(),
                            ethernet_header->getSrc_gpu_idx(),
                            lowest_seq);
                    query_found_it->second.erase(lowest_seq);
                } else {
                    EV << "Other seqs do not meet the gap condition... for them, keep waiting..." << std::endl;
                    break;
                }
            }

            if (query_found_it->second.size() == 0)
                query_srcgpu_to_lostseqs.erase(query_srcgpu_tuple);
        }

    }

    bool last_packet_arrived = false;
    if ((recovery_type == RING_BASED || recovery_type == HIERARCHICAL) && pkt_name == "data")
        last_packet_arrived = ethernet_header->getIs_last_pkt();

    if (recovery_type == HIERARCHICAL ||
            recovery_type == DROP_NOTIF) {
        unsigned long base_flowid;
        if (ethernet_header->getCidr_base_flow_id() <= 0) {
            base_flowid = ethernet_header->getFlow_id();
        } else {
            base_flowid = ethernet_header->getCidr_base_flow_id();
        }
        std::list<unsigned long> involved_servers_list;
        for (int k = 0; k < ethernet_header->getInvolved_servers_listArraySize(); k++) {
            involved_servers_list.push_back(ethernet_header->getInvolved_servers_list(k));
        }
        // find the node before you in the ring
        int src_server_idx;
        if (involved_servers_list.size() == 0) {
            if (ethernet_header->getCollective_alg() == BCAST_PARTIAL_MULTICAST)
                throw cRuntimeError("for peel, We need more than 0 involved servers here");

            // for other collectives just contact the source
            src_server_idx = ethernet_header->getSource_server_idx();
            if (src_server_idx < 0)
                throw cRuntimeError("Invalid src_server_idx");

        } else {
            auto involved_server_itr = involved_servers_list.begin();
            src_server_idx = (*involved_server_itr);
        }
        // with our implementation, every server gets each packet only once
        notify_src_of_completed(base_flowid, ethernet_header->getSrc_gpu_idx(),
                ethernet_header->getSeq_num(), src_server_idx);
    }


    int pfc_signal = ethernet_header->getPfc_signal();
    if (pfc_signal != -1) {
        EV << "PFC Signal received: " << pfc_signal << endl;
//        std::cout << simTime() << ": " << getFullPath() << endl;
        if (pfc_signal == txQueue->PFC_STOP) {
            EV << "PFC: Stop sending..." << endl;
            txQueue->pfc_status = txQueue->PFC_STOP;
//            std::cout << "Signalled to stop sending..." << endl;
        }
        else if (pfc_signal == txQueue->PFC_RESUME) {
            EV << "PFC: Resume sending..." << endl;
//            std::cout << "Signalled to resume sending..." << endl;
            txQueue->pfc_status = txQueue->PFC_RESUME;
            // we might be in IDLE state but currentTxFrame might not be nullptr as it's send was halted by PFC
            if (!txQueue->isEmpty() && !currentTxFrame)
                popTxQueue();
            // it might not be idle since it's sending CNP
            if (transmitState == TX_IDLE_STATE) {
                EV_DETAIL << "No incoming carrier signals detected, frame clear to send\n";
                beginSendFrames();
            }
//            else
//                throw cRuntimeError("PFC_RESUME: transmitState != TX_IDLE_STATE");
        }
        else
            throw cRuntimeError("Unknown PFC status: %d", txQueue->pfc_status);
//        std::cout << "--------------" << endl;
        delete packet;
        return;
    }

    std::string packet_name = packet->getName();
    bool is_dcqcn_cnp = (packet_name.find("dcqcn_cnp") != std::string::npos);

    if (!is_dcqcn_cnp && (hasBitError || !verifyCrcAndLength(packet))) {
        EV_WARN << "hasBitError || !verifyCrcAndLength(packet): Dropping packet" << endl;
        numDroppedBitError++;
        PacketDropDetails details;
        details.setReason(INCORRECTLY_RECEIVED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }

    // swift: add the Time_packet_received_at_nic
//    std::cout << "Swift: Adding Time_packet_received_at_nic" << endl;
    b position = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto frame = packet->removeAtFront<EthernetMacHeader>();
    frame->setTime_packet_received_at_nic(simTime());
    packet->insertAtFront(frame);
    packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(position);

    EtherMacBase::calculate_fabric_delay(frame->getFabric_delay_time_sent_from_source());

    if (dropFrameNotForUs(packet, frame))
        return;

    if (pkt_name.compare("data") == 0) {
        lightRcvdDataPktNum++;

        if (measureOOO) {
            unsigned long mseq_num = frame->getSeq_num();

            if (mseq_num < 1)
                throw cRuntimeError("seq num should be >= 1");

            unsigned long mquery_id = frame->getQuery_id();
            auto ooo_info_found_itr = query_seq_map.find(mquery_id);
            if (ooo_info_found_itr == query_seq_map.end()) {
                std::set<unsigned long> temp_set;
                query_seq_map.insert(std::make_pair(mquery_id, std::make_pair(1, temp_set)));
                ooo_info_found_itr = query_seq_map.find(mquery_id);
            }

            if (mseq_num == ooo_info_found_itr->second.first) {
                EV << "Expected sequence number arrived, move it forward!" << endl;
                ooo_info_found_itr->second.first++;
                while (ooo_info_found_itr->second.second.find(ooo_info_found_itr->second.first) !=
                        ooo_info_found_itr->second.second.end()) {
                    ooo_info_found_itr->second.second.erase(ooo_info_found_itr->second.first);
                    ooo_info_found_itr->second.first++;
                }
            } else if (mseq_num > ooo_info_found_itr->second.first) {
                // count only the ones that arrive earlier than expected
                lightOOOdatacount++;
                ooo_info_found_itr->second.second.insert(mseq_num);
            }
        }
    }

    if (rate_control_type == swift) {
        if (pkt_name.compare("data") == 0) {
            EV << "Got data pkt... sending ACK" << endl;
            auto ack_pkt = packet->dup();
            create_ack_header(ack_pkt);
            pushPacketToQeueue(ack_pkt);
//            txQueue->pushPacket(ack_pkt);
            // in case queue was previously empty
            if (!txQueue->isEmpty() && !currentTxFrame)
                popTxQueue();
            if (transmitState == TX_IDLE_STATE) {
                EV_DETAIL << "No incoming carrier signals detected, frame clear to send\n";
                beginSendFrames();
            }
        } else if (pkt_name.compare("ACK") == 0) {
            simtime_t send_time = ethernet_header->getSwift_send_time();
            simtime_t now = simTime();
            if (send_time <= 0 || send_time >= now) {
                std::cout << send_time << endl;
                throw cRuntimeError("send_time <= 0 || send_time >= now");
            }
            // hop count is 2 * ack hop count
            swift_update_send_rates(ethernet_header->getSwift_send_time(),
                    ethernet_header->getFlow_id(),
                    ethernet_header->getSrc_gpu_idx(),
                    2*(ethernet_header->getHop_count()+1));
            delete packet;
            return;
        }
    } else if (rate_control_type == dcqcn) {
        std::string packet_name = packet->getName();
        auto eth_header = packet->peekAtFront<EthernetMacHeader>();
        if (packet_name.find("data") != std::string::npos) {
            if (eth_header->getIs_ecn_marked()) {
                EV << "Packet's ECN is marked." << endl;
                auto packet_dup = packet->dup();
                dcqcn_generate_congestion_notification(packet_dup);
                delete packet_dup;
            }
        } else if (packet_name.find("dcqcn_cnp") != std::string::npos) {
            unsigned long cnp_flow_id = eth_header->getCnp_flow_id();
            int cnp_src_gpu_id = eth_header->getCnp_src_gpu_id();
            std::tuple<int, unsigned long> gpu_id_flow_id_tuple = std::make_tuple(cnp_src_gpu_id, cnp_flow_id);

            EV << "CNP packet received from the network for flow_id: " << cnp_flow_id << " and cnp_src_gpu_id: " << cnp_src_gpu_id << endl;
//            std::cout << "Yay... we got CNP... now we should impelement the rest" << endl;
            delete packet;
            bool should_react_cnp = true;
            std::map<std::tuple<int, unsigned long>, simtime_t>::iterator flow_id_found_itr;
            simtime_t now = simTime();
            if (sender_cnp_gap_applied) {
                flow_id_found_itr = flow_id_to_sender_cnp_generation_time_mapper.find(gpu_id_flow_id_tuple);
                if (flow_id_found_itr == flow_id_to_sender_cnp_generation_time_mapper.end()) {
                    flow_id_to_sender_cnp_generation_time_mapper.insert(
                            std::make_pair(gpu_id_flow_id_tuple, now));
                    flow_id_found_itr = flow_id_to_sender_cnp_generation_time_mapper.find(gpu_id_flow_id_tuple);
                } else {
                    if (now <= flow_id_found_itr->second)
                        throw cRuntimeError("how is previous cnp reaction time >= now?");
                    should_react_cnp = ((now - flow_id_found_itr->second) >= cnp_generation_gap);
                }
            }
            if (should_react_cnp) {
                if (sender_cnp_gap_applied)
                    flow_id_found_itr->second = now;
                handle_cnp_packet(cnp_flow_id, cnp_src_gpu_id);
            }
            return;
        }
    } else if (rate_control_type == sharp) {
        // todo: impelemnt potential receiver feedbacks for ina
    }

    unsigned long query_id = frame->getQuery_id();
    unsigned long seqnum = frame->getSeq_num();
    int src_gpu_idx = frame->getSrc_gpu_idx();

    if (frame->getTypeOrLength() == ETHERTYPE_FLOW_CONTROL) {
        const auto& controlFrame = currentTxFrame->peekDataAt<EthernetControlFrame>(frame->getChunkLength(), b(-1));
        if (controlFrame->getOpCode() == ETHERNET_CONTROL_PAUSE) {
            auto pauseFrame = check_and_cast<const EthernetPauseFrame *>(controlFrame.get());
            int pauseUnits = pauseFrame->getPauseTime();
            delete packet;
            numPauseFramesRcvd++;
            emit(rxPausePkUnitsSignal, pauseUnits);
            processPauseCommand(pauseUnits);
        }
        else {
            EV_INFO << "Received unknown ethernet flow control frame" << frame << " dropped." << endl;
            delete packet;
        }
    }
    else {
        EV_INFO << "Reception of " << frame << " successfully completed." << endl;
        processReceivedDataFrame(packet, frame);
    }

    if (last_packet_arrived) {
        last_pkt_arrived(query_id, src_gpu_idx, true);
    }
}

void AugmentedEtherMacFullDuplex::handleEndIFGPeriod()
{
//    ASSERT(nullptr == currentTxFrame);
    if (transmitState != WAIT_IFG_STATE)
        throw cRuntimeError("Not in WAIT_IFG_STATE at the end of IFG period");

    // End of IFG period, okay to transmit
    EV_DETAIL << "IFG elapsed in AugmentedEtherMacFullDuplex" << endl;

    if (!txQueue->isEmpty() && !currentTxFrame)
        popTxQueue();
    beginSendFrames();
}

void AugmentedEtherMacFullDuplex::handleEndTxPeriod()
{
    // we only get here if transmission has finished successfully
    if (transmitState != TRANSMITTING_STATE)
        throw cRuntimeError("Model error: End of transmission, and incorrect state detected");

    if (nullptr == currentTxFrame)
        throw cRuntimeError("Model error: Frame under transmission cannot be found");

    numFramesSent++;
    numBytesSent += currentTxFrame->getByteLength();
    emit(packetSentToLowerSignal, currentTxFrame);    //consider: emit with start time of frame

    const auto& header = currentTxFrame->peekAtFront<EthernetMacHeader>();
    if (header->getTypeOrLength() == ETHERTYPE_FLOW_CONTROL) {
        const auto& controlFrame = currentTxFrame->peekDataAt<EthernetControlFrame>(header->getChunkLength(), b(-1));
        if (controlFrame->getOpCode() == ETHERNET_CONTROL_PAUSE) {
            const auto& pauseFrame = CHK(dynamicPtrCast<const EthernetPauseFrame>(controlFrame));
            numPauseFramesSent++;
            emit(txPausePkUnitsSignal, pauseFrame->getPauseTime());
        }
    }

    EV_INFO << "Transmission of " << currentTxFrame << " successfully completed.\n";
    deleteCurrentTxFrame();
    lastTxFinishTime = simTime();

    if (pauseUnitsRequested > 0) {
        // if we received a PAUSE frame recently, go into PAUSE state
        EV_DETAIL << "Going to PAUSE mode for " << pauseUnitsRequested << " time units\n";

        scheduleEndPausePeriod(pauseUnitsRequested);
        pauseUnitsRequested = 0;
    }
    else {
        EV_DETAIL << "Start IFG period\n";
        scheduleEndIFGPeriod();
    }
}

void AugmentedEtherMacFullDuplex::finish()
{
    EtherMacBase::finish();

    simtime_t t = simTime();
    simtime_t totalRxChannelIdleTime = t - totalSuccessfulRxTime;
    recordScalar("rx channel idle (%)", 100 * (totalRxChannelIdleTime / t));
    recordScalar("rx channel utilization (%)", 100 * (totalSuccessfulRxTime / t));
}

void AugmentedEtherMacFullDuplex::handleEndPausePeriod()
{
//    ASSERT(nullptr == currentTxFrame);
    if (transmitState != PAUSE_STATE)
        throw cRuntimeError("End of PAUSE event occurred when not in PAUSE_STATE!");

    EV_DETAIL << "Pause finished, resuming transmissions\n";
    if (!txQueue->isEmpty())
        popTxQueue();
    beginSendFrames();
}

void AugmentedEtherMacFullDuplex::processReceivedDataFrame(Packet *packet, const Ptr<const EthernetMacHeader>& frame)
{
    // statistics
    unsigned long curBytes = packet->getByteLength();
    numFramesReceivedOK++;
    numBytesReceivedOK += curBytes;
    emit(rxPkOkSignal, packet);

    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::ethernetMac);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ethernetMac);
    if (interfaceEntry)
        packet->addTagIfAbsent<InterfaceInd>()->setInterfaceId(interfaceEntry->getInterfaceId());

    std::string name = getFullPath();
    int hop_count = frame->getHop_count();
    if (name.find("server") != std::string::npos && hop_count != 0) {
        emit(packetHopCountSignal, hop_count + 1);
    }

    numFramesPassedToHL++;
    emit(packetSentToUpperSignal, packet);
    // pass up to upper layer
    EV_INFO << "Sending " << packet << " to upper layer.\n";
    send(packet, upperLayerOutGateId);
}

void AugmentedEtherMacFullDuplex::processPauseCommand(int pauseUnits)
{
    if (transmitState == TX_IDLE_STATE) {
        EV_DETAIL << "PAUSE frame received, pausing for " << pauseUnitsRequested << " time units\n";
        if (pauseUnits > 0)
            scheduleEndPausePeriod(pauseUnits);
    }
    else if (transmitState == PAUSE_STATE) {
        EV_DETAIL << "PAUSE frame received, pausing for " << pauseUnitsRequested
                  << " more time units from now\n";
        cancelEvent(endPauseMsg);

        // Terminate PAUSE if pauseUnits == 0; Extend PAUSE if pauseUnits > 0
        scheduleEndPausePeriod(pauseUnits);
    }
    else {
        // transmitter busy -- wait until it finishes with current frame (endTx)
        // and then it'll go to PAUSE state
        EV_DETAIL << "PAUSE frame received, storing pause request\n";
        pauseUnitsRequested = pauseUnits;
    }
}

void AugmentedEtherMacFullDuplex::scheduleEndIFGPeriod()
{
//    ASSERT(nullptr == currentTxFrame);
    changeTransmissionState(WAIT_IFG_STATE);
    simtime_t endIFGTime = simTime() + (b(INTERFRAME_GAP_BITS).get() / curEtherDescr->txrate);
    scheduleAt(endIFGTime, endIFGMsg);
}

void AugmentedEtherMacFullDuplex::scheduleEndPausePeriod(int pauseUnits)
{
//    ASSERT(nullptr == currentTxFrame);
    // length is interpreted as 512-bit-time units
    simtime_t pausePeriod = ((pauseUnits * PAUSE_UNIT_BITS) / curEtherDescr->txrate);
    scheduleAt(simTime() + pausePeriod, endPauseMsg);
    changeTransmissionState(PAUSE_STATE);
}

bool AugmentedEtherMacFullDuplex::is_allowed_to_send_by_PFC() {
    EV << "AugmentedEtherMacFullDuplex::is_allowed_to_send_by_PFC()" << endl;
    if (txQueue->pfc_status == txQueue->PFC_RESUME) {
        EV << "Send status is PFC_RESUME. Returning true." << endl;
        return true;
    }
    if (currentTxFrame) {
        std::string current_tx_packet_name = currentTxFrame->getName();
        if (current_tx_packet_name.find("dcqcn_cnp") != std::string::npos) {
            EV << "Current TX frame is DCQCN CNP. Returning true." << endl;
            return true;
        }
        if (current_tx_packet_name.find("ACK") != std::string::npos) {
            EV << "Current TX frame is ACK. Returning true." << endl;
            return true;
        }
    }
    if (txQueue->getNumPackets() > 0) {
        std::string first_packet_name = txQueue->get_index(0)->getName();
        if (first_packet_name.find("dcqcn_cnp") != std::string::npos) {
            EV << "We have CNP packet in the queue. Returning true" << endl;
            return true;
        }
        if (first_packet_name.find("ACK") != std::string::npos) {
            EV << "We have ACK packet in the queue. Returning true" << endl;
            return true;
        }
    }
    EV << "Not allowed to send by PFC. Returning false." << endl;
    return false;

}

void AugmentedEtherMacFullDuplex::beginSendFrames()
{

    EV << "AugmentedEtherMacFullDuplex::beginSendFrames" << endl;
    if (currentTxFrame && is_allowed_to_send_by_PFC()) {
        // Other frames are queued, transmit next frame
        EV_DETAIL << "Transmit next frame in output queue\n";
        startFrameTransmission();
    }
    else {
        // No more frames set transmitter to idle
        changeTransmissionState(TX_IDLE_STATE);
        if (txQueue->pfc_status != txQueue->PFC_RESUME)
            EV << "Not allowed to send by PFC. Changing to IDLE" << endl;
        else
            EV_DETAIL << "No more frames to send, transmitter set to idle\n";
    }
}

