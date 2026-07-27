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

//#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "./UDPLongBroadcastPasserApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include <iostream>
#include <fstream>
#include <sqlite3.h>
#include <list>
#include "inet/transportlayer/udp/Udp.h"
#include "inet/applications/common/SocketTag_m.h"
#include <random>
#include "./UDPLongAllgatherInitiatorApp.h"
#include <regex>

#define INIT_PORT 80
#define MAX_PORT_NUM 65450      // 65535 - 85

using namespace inet;

Define_Module(UDPLongBroadcastPasserApp);

simsignal_t UDPLongBroadcastPasserApp::flowEndedSignal = registerSignal("flowEnded");
simsignal_t UDPLongBroadcastPasserApp::flowEndedQueryIDSignal = registerSignal("flowEndedQueryID");
simsignal_t UDPLongBroadcastPasserApp::flowPassedSignal = registerSignal("flowPassed");;

UDPLongBroadcastPasserApp::~UDPLongBroadcastPasserApp()
{
    for (auto itr = flowid_passingmsg_mapper.begin(); itr != flowid_passingmsg_mapper.end(); itr++) {
        delete itr->second;
    }
    for (auto itr = flowid_receivedmsg_mapper.begin(); itr != flowid_receivedmsg_mapper.end(); itr++) {
        delete itr->second;
    }
    flowid_receivedmsg_mapper.clear();
    flowid_bitsrcvd_mapper.clear();
    flowid_passingmsg_mapper.clear();
}

double UDPLongBroadcastPasserApp::get_processing_delay(int gpu_idx,
        int collective_type,
        int collective_alg,
        int tree_traversal_dir,
        bool optireduce_in_reduction_phase) {
    Enter_Method("get_processing_delay");

    if (collective_alg <= 0 || collective_type <= 0)
        throw cRuntimeError("collective_alg <= 0 || collective_type <= 0 (3)");

    // we only consider extra latency for reduce
    if (collective_type != REDUCE && collective_type != REDUCE_SCATTER &&
            collective_type != ALL_REDUCE)
        return 0;

    // broadcast phase of optireduce has no processing
    if (collective_alg == BCAST_OPTIREDUCE && !optireduce_in_reduction_phase)
        return 0;

    if (processing_delay_mean <= 0 || processing_delay_stddev <= 0)
        return 0;

    if (processing_delays.size() != num_gpus) {
        std::cout << "num_gpus: " << num_gpus << endl;
        std::cout << "processing_delays.size: " << processing_delays.size() << endl;
        throw cRuntimeError("processing_delays.size() != num_gpus");
    }

    if (gpu_idx >= processing_delays.size())
        throw cRuntimeError("gpu_idx >= processing_delays.size()");

    if (collective_alg == BCAST_BINARY_TREE) {
        if (tree_traversal_dir < 0)
            throw cRuntimeError("get_processing_delay: unknown traversal dir!");
        if (tree_traversal_dir == TREE_DOWN) {
            EV << "The processing delay while going down the tree is 0" << endl;
            return 0;
        }
    }

    auto delay_itr = processing_delays.begin();
    std::advance(delay_itr, gpu_idx);
    double delay = (*delay_itr);
    if (delay < 0)
        throw cRuntimeError("delay < 0");
    return delay;
}

void UDPLongBroadcastPasserApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        app_index = getIndex();
        parent_index = getParentModule()->getIndex();
        mss = par("mss");
        packet_creation_batch_size = par("packet_creation_batch_size");

        nvlink_speed_gbps = int(par("nvlink_speed_gbps"));
        num_gpus = int(par("num_gpus"));
        mcast_in_host = bool(par("mcast_in_host"));

        num_servers_under_each_leaf = int(getAncestorPar("num_servers"));
        fattree_k = int(par("k"));
        num_spines = int(par("num_spines"));
        num_leafs = int(par("num_aggs"));


//        processing_delay = par("processing_delay");
        processing_delay_mean = par("processing_delay_mean");
        processing_delay_stddev = par("processing_delay_stddev");

        optireduce_incast_factor = int(par("optireduce_incast_factor"));

        if (optireduce_incast_factor <= 0) {
            EV << "Changing incast factor " << optireduce_incast_factor << endl;
            optireduce_incast_factor = INT_MAX - 1;
        }

        use_becca_optireduce = bool(par("use_becca_optireduce"));
        use_cidr_optireduce = bool(par("use_cidr_optireduce"));

        if (processing_delay_mean > 0 && processing_delay_stddev > 0) {
            std::string seed_str = std::to_string(parent_index) + std::to_string(app_index);
            seed = std::atoi(seed_str.c_str());
            seed += repetition_num;

            std::mt19937 generator(seed);
            std::normal_distribution<double> distribution(processing_delay_mean, processing_delay_stddev);

            for (int i = 0; i < num_gpus; i++) {
                double delay = distribution(generator);
                if (delay < 0)
                    delay = 0;
                processing_delays.push_back(delay);
            }
        }

        // consider orca overhead
        use_orca = (bool)par("use_orca");

        // use rsbf
        use_rsbf = bool(par("use_rsbf"));

        /*
         * mss zero means do not fragment data in the application layer (This causes
         * problems with pipelined binomial tree because the ip layer waits for all
         * the fragments before passing them up)
         * */
        if (mss < 0)
            throw cRuntimeError("mss < 0");

        use_fattree = bool(par("use_fattree"));

        // autoccl
        use_autoccl = bool(par("use_autoccl"));
        if (use_autoccl) {
            num_flows_per_ml_query = par("num_flows_per_ml_query");
            autoccl_allreduce_only = bool(par("autoccl_allreduce_only"));
            if (num_flows_per_ml_query <= 0)
                throw cRuntimeError("Invalid num_flows_per_ml_query for autoccl's use!");
        }

    } else if (stage == INITSTAGE_APPLICATION_LAYER) {
        const char *localAddress = par("localAddress");
        int localPort = par("localPort");
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);
        socket.setCallback(this);
    }
}

int UDPLongBroadcastPasserApp::retrieve_original_flow_size(int m_collective_type,
        int m_collective_alg,
        double flow_size) {

    if (!use_autoccl)
        throw cRuntimeError("retrieve_original_flow_size called while autoccl is off!");

    if (m_collective_type <= 0 || m_collective_alg <= 0)
        throw cRuntimeError("Invalid collective type or alg!");


    if (m_collective_type != BROADCAST &&
            m_collective_type != ALL_GATHER &&
            m_collective_type != ALL_REDUCE)
        // we don't care about other collectives in autoccl for now
        throw cRuntimeError("retrieve_original_flow_size called with wrong collective type!");

    if (m_collective_alg != BCAST_RING && m_collective_alg != BCAST_BINARY_TREE)
        throw cRuntimeError("retrieve_original_flow_size called with wrong collective algorithm!");


    /*
     * In all reduce, we have N gpus each having flow_size chunks of data
     * assume each have N different chunks of data
     * at the end we want all corresponding chunks to be reduced
     * and also available at all nodes
     */
    if (m_collective_type == ALL_REDUCE &&
            (m_collective_alg == BCAST_RING ||
                    m_collective_alg == BCAST_OPTIREDUCE)) {
        // at ring each sender starts the broadcast for only one shard
//        std::cout << "Flow size retreived form " << flow_size << "B to " << flow_size * num_flows_per_ml_query << "B." << std::endl;
        return flow_size * num_flows_per_ml_query;
    }

//    std::cout << "Retrieved flow size remains the same: " << flow_size << "B" << std::endl;
    return flow_size;
}

void UDPLongBroadcastPasserApp::handleStartOperation(LifecycleOperation *operation)
{
    EV << "SEPEHR: UDPLongBroadcastPasserApp:: handleStartOperation called." << endl;

//    std::string collective_alg_type_str = par("collective_alg_type_str");
//    if (collective_alg_type_str.compare("singlesender") == 0) {
//        collective_alg_type = BCAST_SINGLE_SENDER;
//    } else if (collective_alg_type_str.compare("binomialtree") == 0) {
//        collective_alg_type = BCAST_BINOMIAL_TREE;
//    } else if (collective_alg_type_str.compare("binarytree") == 0) {
//        collective_alg_type = BCAST_BINARY_TREE;
//    } else if (collective_alg_type_str.compare("network_assisted") == 0) {
//        collective_alg_type = BCAST_NETWORK_ASSISTED;
//    } else if (collective_alg_type_str.compare("ring") == 0) {
//        collective_alg_type = BCAST_RING;
//    } else
//        throw cRuntimeError("Broadcast type not specified!");

    // repetition_num = atoi(getEnvir()->getConfigEx()->getVariable(CFGVAR_REPETITION));
    repetition_num = 0;

    is_message_splitted = par("is_message_splitted");
    if (is_message_splitted) {
        // setting splitted_chunk_size_bytes to -1 means that we pass packets as they arrive
        splitted_chunk_size_bytes = par("splitted_chunk_size_bytes");
        splitted_subflow_num = par("splitted_subflow_num");
        if (splitted_chunk_size_bytes != -1 && splitted_subflow_num != -1)
            throw cRuntimeError("Multiple techniques selected for splitting the message");
        if (splitted_chunk_size_bytes == -1 && splitted_subflow_num == -1)
            throw cRuntimeError("No splitting technique selected!");
        if (splitted_chunk_size_bytes != -1 && splitted_chunk_size_bytes <= 0)
            throw cRuntimeError("splitted_chunk_size_bytes <= 0");
        if (splitted_subflow_num != -1 && splitted_subflow_num <= 0)
            throw cRuntimeError("splitted_subflow_num <= 0");
    }
    mtu = par("mtu");
}

void UDPLongBroadcastPasserApp::finish()
{
    ApplicationBase::finish();
}

int UDPLongBroadcastPasserApp::generate_next_batch(unsigned long flow_id, int gpu_idx) {
    EV << "Generating next batch for flow id " << flow_id << endl;
    Enter_Method("generate_next_batch");
    std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(gpu_idx, flow_id);
    auto msg_info_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
    if (msg_info_found_itr == flowid_passingmsg_mapper.end())
        return BATCH_TERMINATE;
    if (msg_info_found_itr->second->msg_size <= 0 && msg_info_found_itr->second->original_flow_terminated) {
        throw cRuntimeError("msg_info_found_itr->second->msg_size <= 0 && msg_info_found_itr->second->original_flow_terminated");
    }
    EV << "Generating next batch for flow id: " << flow_id << ", msg_size is " << msg_info_found_itr->second->msg_size << endl;

    if (packet_creation_batch_size <= 0)
        throw cRuntimeError("generate_next_batch must not be called for packet_creation_batch_size <= 0");
    if (mss == 0)
        throw cRuntimeError("mss cannot be 0 when generate_next_batch is called!");

    if (msg_info_found_itr->second->msg_size <= 0) {
        EV << "Flow is not finished but we have no message to send for now!. Go to sleep." << endl;
        return BATCH_SLEEP;
    }

    int remaining_packet_num = packet_creation_batch_size;
    // remaining_packet_num = -1 means that send all the packets right away
    while (msg_info_found_itr->second->msg_size > 0 && remaining_packet_num != 0) {
        EV << "msg_info_found_itr->second->msg_size is " << msg_info_found_itr->second->msg_size << endl;

        B m_chunk_length;
        if (msg_info_found_itr->second->msg_size > mss) {
            m_chunk_length = B(mss);
            msg_info_found_itr->second->msg_size -= mss;
        } else {
            m_chunk_length = B(msg_info_found_itr->second->msg_size);
            msg_info_found_itr->second->msg_size = 0;
        }

        auto seq_num_found_itr = gpu_idx_flow_id_to_seq_num.find(gpu_idx_flow_id_tuple);
        if (seq_num_found_itr == gpu_idx_flow_id_to_seq_num.end()) {
            std::cout << "Flow id: " << msg_info_found_itr->second->flow_id << endl;
            throw cRuntimeError("sequence number not found!");
        }
        seq_num_found_itr->second += 1;

        const auto& payload = makeShared<GenericAppMsg>();
        Packet *packet = new Packet("data");
        unsigned long query_id = msg_info_found_itr->second->query_id;
        payload->setQuery_id(query_id);
        payload->setChunkLength(m_chunk_length);
        payload->setServerClose(false);
        payload->addTag<CreationTimeTag>()->setCreationTime(msg_info_found_itr->second->initiated_time);
        payload->setRequesterID(msg_info_found_itr->second->flow_id);
        payload->setRequested_time(msg_info_found_itr->second->initiated_time);
        payload->setTotal_flow_size(B(msg_info_found_itr->second->total_flow_size));
        payload->setApp_name("UDPLongBroadcastPasserApp");
        payload->setApp_full_path(getFullPath().c_str());
        payload->setSrc_gpu_idx(msg_info_found_itr->second->src_gpu);
        payload->setDst_gpu_idx(msg_info_found_itr->second->dst_gpu);
        EV << "src gpu: " << msg_info_found_itr->second->src_gpu <<
                ", dst gpu: " << msg_info_found_itr->second->dst_gpu << endl;


        if ((msg_info_found_itr->second->m_collective_type != ALL_REDUCE &&
                msg_info_found_itr->second->m_collective_alg != BCAST_RING) &&
                msg_info_found_itr->second->responsibility.size() != msg_info_found_itr->second->responsiblity_flow_ids.size()) {
            std::cout << "responsibility size: " << msg_info_found_itr->second->responsibility.size() << endl;
            std::cout << "flowid size: " << msg_info_found_itr->second->responsiblity_flow_ids.size() << endl;
            throw cRuntimeError("Inconsistant size between responsibility and responsiblity_flow_ids 5");
        }

        payload->setRing_end_server_idx(msg_info_found_itr->second->ring_end_server_idx);
        payload->setRing_end_gpu_idx(msg_info_found_itr->second->ring_end_gpu_idx);

        payload->setTree_search_type(msg_info_found_itr->second->tree_search_type);
        payload->setTree_traversal_dir(msg_info_found_itr->second->tree_traversal_dir);

        payload->setCollective_type(msg_info_found_itr->second->m_collective_type);
        payload->setCollective_alg_type(msg_info_found_itr->second->m_collective_alg);

        payload->setAgg_cidr_group_id(msg_info_found_itr->second->agg_cidr_group_id.c_str());
        payload->setAgg_cidr_group_member_count(msg_info_found_itr->second->agg_cidr_group_member_count);
        payload->setCore_cidr_group_id(msg_info_found_itr->second->core_cidr_group_id.c_str());
        payload->setCore_cidr_group_member_count(msg_info_found_itr->second->core_cidr_group_member_count);
        payload->setDst_tor_idx(msg_info_found_itr->second->dst_tor_idx);

        if (msg_info_found_itr->second->m_collective_alg == BCAST_OPTIREDUCE &&
                use_cidr_optireduce) {
            for (auto itr = msg_info_found_itr->second->network_assisted_destination_idx_list.begin();
                    itr != msg_info_found_itr->second->network_assisted_destination_idx_list.end(); itr++)
                payload->insertDst_idx_list(*itr);

            for (auto itr = msg_info_found_itr->second->network_assisted_flow_ids_list.begin();
                    itr != msg_info_found_itr->second->network_assisted_flow_ids_list.end(); itr++)
                payload->insertFlow_ids_list(*itr);
        }

        payload->setSeq_num(seq_num_found_itr->second);

        payload->setCollective_scale(msg_info_found_itr->second->num_dsts);
        payload->setOptireduce_in_reduction_phase(msg_info_found_itr->second->optireduce_in_reduction_phase);

        if (msg_info_found_itr->second->responsibility.size() > 0) {
            for (auto it = msg_info_found_itr->second->responsibility.begin();
                    it != msg_info_found_itr->second->responsibility.end(); it++)
                payload->insertResponsibility_list(*it);

            for (auto it = msg_info_found_itr->second->responsibility_gpu.begin();
                    it != msg_info_found_itr->second->responsibility_gpu.end(); it++)
                payload->insertResponsibility_gpu_list(*it);

            for (auto it = msg_info_found_itr->second->responsiblity_flow_ids.begin();
                    it != msg_info_found_itr->second->responsiblity_flow_ids.end(); it++)
                payload->insertResponsibility_flow_id_list(*it);
        }

        if (msg_info_found_itr->second->m_collective_alg == BCAST_OPTIREDUCE && use_becca_optireduce) {

            for (auto itr = msg_info_found_itr->second->network_assisted_destination_idx_list.begin();
                    itr != msg_info_found_itr->second->network_assisted_destination_idx_list.end(); itr++)
                payload->insertDst_idx_list(*itr);

            for (auto itr = msg_info_found_itr->second->network_assisted_flow_ids_list.begin();
                    itr != msg_info_found_itr->second->network_assisted_flow_ids_list.end(); itr++)
                payload->insertFlow_ids_list(*itr);

            // read the appropriate list of jumps and ports for flow_id
            if (msg_info_found_itr->second->lists_of_lists_of_jumps.size() == 0 ||
                    msg_info_found_itr->second->lists_of_lists_of_ports.size() == 0)
                throw cRuntimeError("List of jumps/ports must not be empty for becca allreduce!");
            if (msg_info_found_itr->second->lists_of_lists_of_jumps.size() != msg_info_found_itr->second->lists_of_lists_of_ports.size())
                throw cRuntimeError("msg_info->lists_of_lists_of_jumps.size() != msg_info->lists_of_lists_of_ports.size()");

            UDPLongAllgatherInitiatorApp* initiator_app = check_and_cast<UDPLongAllgatherInitiatorApp*>(getModuleByPath(msg_info_found_itr->second->initiator_app_path.c_str()));
            unsigned int random_idx = initiator_app->getLbTreeIdx(msg_info_found_itr->second->query_id, msg_info_found_itr->second->src_gpu);
            random_idx %= msg_info_found_itr->second->lists_of_lists_of_jumps.size();

            auto _list_of_jumps_itr = msg_info_found_itr->second->lists_of_lists_of_jumps.begin();
            std::advance(_list_of_jumps_itr, random_idx);
            auto _list_of_ports_itr = msg_info_found_itr->second->lists_of_lists_of_ports.begin();
            std::advance(_list_of_ports_itr, random_idx);
            if ((*_list_of_jumps_itr).size() != (*_list_of_ports_itr).size())
                throw cRuntimeError("(*_list_of_jumps_itr).size() != (*_list_of_ports_itr).size()2");


            for (auto itr1 = (*_list_of_jumps_itr).begin();
                    itr1 != (*_list_of_jumps_itr).end(); itr1++) {
                InnerList innerList;
                innerList.setInnerArrayArraySize(itr1->size());
                auto itr2 = itr1->begin();
                for (size_t j = 0; j < itr1->size(); j++) {
                    innerList.setInnerArray(j, (*itr2));
                    itr2++;
                }
                if (itr2 != itr1->end())
                    throw cRuntimeError("jump_to_idx_list: itr2 != itr1->end()");
                payload->insertJump_to_idx(innerList);
            }

            for (auto itr1 = (*_list_of_ports_itr).begin();
                    itr1 != (*_list_of_ports_itr).end(); itr1++) {
                InnerList innerList;
                innerList.setInnerArrayArraySize(itr1->size());
                auto itr2 = itr1->begin();
                for (size_t j = 0; j < itr1->size(); j++) {
                    innerList.setInnerArray(j, (*itr2));
                    itr2++;
                }
                if (itr2 != itr1->end())
                    throw cRuntimeError("ports_to_dst_list: itr2 != itr1->end()");
                payload->insertPorts_to_dest_idx(innerList);
            }
        }

        packet->insertAtBack(payload);

        EV << "sending data. " << msg_info_found_itr->second->msg_size << " bytes left." << endl;
        EV << "SEPEHR: sending data with flow ID: " << msg_info_found_itr->second->flow_id << endl;

        msg_info_found_itr->second->out_socket->sendTo(packet, msg_info_found_itr->second->destination, msg_info_found_itr->second->dest_port);
        remaining_packet_num--;
        if (remaining_packet_num == 0)
            EV << "More packets are remaining but batch limit has been reached 2" << endl;
    }
    EV << "original_flow_terminated: " << msg_info_found_itr->second->original_flow_terminated << endl;
    EV << "msg_size: " << msg_info_found_itr->second->msg_size << endl;
    if (msg_info_found_itr->second->msg_size == 0
            && msg_info_found_itr->second->original_flow_terminated) {
        msg_info_found_itr->second->responsibility.clear();
        msg_info_found_itr->second->responsiblity_flow_ids.clear();
        gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
        unsigned long query_id = msg_info_found_itr->second->query_id;
        int collective_alg = msg_info_found_itr->second->m_collective_alg;
        int src_gpu_idx = msg_info_found_itr->second->src_gpu;
        delete msg_info_found_itr->second;
        flowid_passingmsg_mapper.erase(msg_info_found_itr);
        if (collective_alg == BCAST_OPTIREDUCE) {
            optireduce_send_next(query_id, src_gpu_idx);
        }
        return BATCH_TERMINATE;
    }
    return BATCH_SEND;
}

void UDPLongBroadcastPasserApp::pass_msg(int server_idx, BCastCustomCollectiveMsgPasserInfo* msg_info)
{

    EV << "UDPLongBroadcastPasserApp::pass_msg Called." << endl;

    EV << "dest idx, flow id, msg size, total flow size, query id, dst gpu_idx" << endl;
    EV << server_idx << ", " << msg_info->flow_id << ", " << msg_info->msg_size << ", "
            << msg_info->total_flow_size << ", " << msg_info->query_id <<
            ", " << msg_info->dst_gpu << endl;

    EV << "collective type: " << msg_info->m_collective_type << ", algorithm: " << msg_info->m_collective_alg << endl;
    if(msg_info->m_collective_type <= 0 || msg_info->m_collective_alg <= 0)
        throw cRuntimeError("msg_info->m_collective_type <= 0 || msg_info->m_collective_alg <= 0 (2)");

    auto socket_found = destination_to_socket_mapper.find(std::make_tuple(server_idx, msg_info->dst_gpu));
    if (socket_found == destination_to_socket_mapper.end())
        throw cRuntimeError("We're sending the request but cannot find the socket!");
    long socket_id = socket_found->second->getSocketId();

    unsigned long send_bytes = msg_info->msg_size;
    simtime_t now = simTime();
    int dest_port = 80;
    L3Address destination = get_destination(server_idx, msg_info->dst_gpu);

    unsigned long tmp_mss;
    if (mss == 0) {
        // send all data at once
        tmp_mss = send_bytes;
    } else
        tmp_mss = mss;

    msg_info->initiated_time = now;
    msg_info->out_socket = socket_found->second;
    msg_info->dest_port = dest_port;
    msg_info->destination = destination;
//    if (msg_info->flow_id == 127298) {
//        std::cout << getFullPath() << ": destination is " << msg_info->destination << endl;
//    }

    int remaining_packet_num = packet_creation_batch_size;
    // remaining_packet_num = -1 means that send all the packets right away
    while (send_bytes > 0 && remaining_packet_num != 0) {
        EV << "send bytes is " << send_bytes << endl;

        B m_chunk_length;
        if (send_bytes > tmp_mss) {
            m_chunk_length = B(tmp_mss);
            send_bytes -= tmp_mss;
        } else {
            m_chunk_length = B(send_bytes);
            send_bytes = 0;
        }

        std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(msg_info->src_gpu, msg_info->flow_id);
        auto seq_num_found_itr = gpu_idx_flow_id_to_seq_num.find(gpu_idx_flow_id_tuple);
        if (seq_num_found_itr == gpu_idx_flow_id_to_seq_num.end()) {
            gpu_idx_flow_id_to_seq_num.insert(std::make_pair(gpu_idx_flow_id_tuple, 0));
            seq_num_found_itr = gpu_idx_flow_id_to_seq_num.find(gpu_idx_flow_id_tuple);
        }
        seq_num_found_itr->second += 1;

        const auto& payload = makeShared<GenericAppMsg>();
        Packet *packet = new Packet("data");
//        packet->addTag<SocketInd>()->setSocketId(socket_id);
        unsigned long query_id = msg_info->query_id;
        payload->setQuery_id(query_id);
        payload->setChunkLength(m_chunk_length);
        payload->setServerClose(false);
        payload->addTag<CreationTimeTag>()->setCreationTime(now);
        payload->setRequesterID(msg_info->flow_id);
        payload->setRequested_time(now);
        payload->setTotal_flow_size(B(msg_info->total_flow_size));
        payload->setApp_name("UDPLongBroadcastPasserApp");
        payload->setApp_full_path(getFullPath().c_str());
        payload->setSrc_gpu_idx(msg_info->src_gpu);
        payload->setDst_gpu_idx(msg_info->dst_gpu);
        EV << "src gpu: " << msg_info->src_gpu <<
                ", dst gpu: " << msg_info->dst_gpu << endl;
        msg_info->check_metrics();

        payload->setRing_end_server_idx(msg_info->ring_end_server_idx);
        payload->setRing_end_gpu_idx(msg_info->ring_end_gpu_idx);

        payload->setTree_search_type(msg_info->tree_search_type);
        payload->setTree_traversal_dir(msg_info->tree_traversal_dir);

        payload->setCollective_type(msg_info->m_collective_type);
        payload->setCollective_alg_type(msg_info->m_collective_alg);

        payload->setAgg_cidr_group_id(msg_info->agg_cidr_group_id.c_str());
        payload->setAgg_cidr_group_member_count(msg_info->agg_cidr_group_member_count);
        payload->setCore_cidr_group_id(msg_info->core_cidr_group_id.c_str());
        payload->setCore_cidr_group_member_count(msg_info->core_cidr_group_member_count);
        payload->setDst_tor_idx(msg_info->dst_tor_idx);

        if (msg_info->m_collective_alg == BCAST_OPTIREDUCE &&
                use_cidr_optireduce) {
            for (auto itr = msg_info->network_assisted_destination_idx_list.begin();
                    itr != msg_info->network_assisted_destination_idx_list.end(); itr++)
                payload->insertDst_idx_list(*itr);

            for (auto itr = msg_info->network_assisted_flow_ids_list.begin();
                    itr != msg_info->network_assisted_flow_ids_list.end(); itr++)
                payload->insertFlow_ids_list(*itr);
        }

        EV << "agg cidr: " << msg_info->agg_cidr_group_id << ", core cidr: " <<
                msg_info->core_cidr_group_id <<
                ", dst tor: " << msg_info->dst_tor_idx << endl;

        payload->setSeq_num(seq_num_found_itr->second);

        payload->setCollective_scale(msg_info->num_dsts);
        payload->setOptireduce_in_reduction_phase(msg_info->optireduce_in_reduction_phase);

        if (msg_info->responsibility.size() > 0) {
            for (auto it = msg_info->responsibility.begin(); it != msg_info->responsibility.end(); it++)
                payload->insertResponsibility_list(*it);

            for (auto it = msg_info->responsibility_gpu.begin(); it != msg_info->responsibility_gpu.end(); it++)
                payload->insertResponsibility_gpu_list(*it);

            for (auto it = msg_info->responsiblity_flow_ids.begin(); it != msg_info->responsiblity_flow_ids.end(); it++)
                payload->insertResponsibility_flow_id_list(*it);
        }

        if (msg_info->m_collective_alg == BCAST_OPTIREDUCE && use_becca_optireduce) {

            for (auto itr = msg_info->network_assisted_destination_idx_list.begin();
                    itr != msg_info->network_assisted_destination_idx_list.end(); itr++)
                payload->insertDst_idx_list(*itr);

            for (auto itr = msg_info->network_assisted_flow_ids_list.begin();
                    itr != msg_info->network_assisted_flow_ids_list.end(); itr++)
                payload->insertFlow_ids_list(*itr);

            // read the appropriate list of jumps and ports for flow_id
            if (msg_info->lists_of_lists_of_jumps.size() == 0 || msg_info->lists_of_lists_of_ports.size() == 0)
                throw cRuntimeError("List of jumps/ports must not be empty for becca allreduce!");
            if (msg_info->lists_of_lists_of_jumps.size() != msg_info->lists_of_lists_of_ports.size())
                throw cRuntimeError("msg_info->lists_of_lists_of_jumps.size() != msg_info->lists_of_lists_of_ports.size()");

            UDPLongAllgatherInitiatorApp* initiator_app = check_and_cast<UDPLongAllgatherInitiatorApp*>(getModuleByPath(msg_info->initiator_app_path.c_str()));
            unsigned int random_idx = initiator_app->getLbTreeIdx(msg_info->query_id, msg_info->src_gpu);
            random_idx %= msg_info->lists_of_lists_of_jumps.size();

            auto _list_of_jumps_itr = msg_info->lists_of_lists_of_jumps.begin();
            std::advance(_list_of_jumps_itr, random_idx);
            auto _list_of_ports_itr = msg_info->lists_of_lists_of_ports.begin();
            std::advance(_list_of_ports_itr, random_idx);
            if ((*_list_of_jumps_itr).size() != (*_list_of_ports_itr).size())
                throw cRuntimeError("(*_list_of_jumps_itr).size() != (*_list_of_ports_itr).size()2");


            for (auto itr1 = (*_list_of_jumps_itr).begin();
                    itr1 != (*_list_of_jumps_itr).end(); itr1++) {
                InnerList innerList;
                innerList.setInnerArrayArraySize(itr1->size());
                auto itr2 = itr1->begin();
                for (size_t j = 0; j < itr1->size(); j++) {
                    innerList.setInnerArray(j, (*itr2));
                    itr2++;
                }
                if (itr2 != itr1->end())
                    throw cRuntimeError("jump_to_idx_list: itr2 != itr1->end()");
                payload->insertJump_to_idx(innerList);
            }

            for (auto itr1 = (*_list_of_ports_itr).begin();
                    itr1 != (*_list_of_ports_itr).end(); itr1++) {
                InnerList innerList;
                innerList.setInnerArrayArraySize(itr1->size());
                auto itr2 = itr1->begin();
                for (size_t j = 0; j < itr1->size(); j++) {
                    innerList.setInnerArray(j, (*itr2));
                    itr2++;
                }
                if (itr2 != itr1->end())
                    throw cRuntimeError("ports_to_dst_list: itr2 != itr1->end()");
                payload->insertPorts_to_dest_idx(innerList);
            }
        }

        packet->insertAtBack(payload);

        EV << "sending data. " << send_bytes << " bytes left" << endl;
        EV << "SEPEHR: sending data with flow ID: " << msg_info->flow_id << endl;

        socket_found->second->sendTo(packet, destination, dest_port);
        remaining_packet_num--;
        if (remaining_packet_num == 0)
            EV << "More packets are remaining but batch limit has been reached" << endl;
    }
    msg_info->msg_size = send_bytes;
}

int UDPLongBroadcastPasserApp::get_local_port() {

    std::string udp_module_str = getParentModule()->getFullPath() + ".udp";
    Udp *udp_module = check_and_cast<Udp *>(getModuleByPath(udp_module_str.c_str()));
    int local_port_padding, local_port;
    local_port_padding = udp_module->port_padding;
    udp_module->port_padding += 1;
    if ((INIT_PORT + udp_module->port_padding) % MAX_PORT_NUM == 0)
        udp_module->port_padding = 1;

    local_port = INIT_PORT + local_port_padding;
    return local_port;
}

void UDPLongBroadcastPasserApp::init_socket(int server_idx, int gpu_idx) {
    EV << "UDPLongBroadcastPasserApp::init_socket for server " << server_idx
            << " gpu " << gpu_idx << endl;
    auto socket_found_itr = destination_to_socket_mapper.find(std::make_tuple(server_idx, gpu_idx));
    if (socket_found_itr == destination_to_socket_mapper.end()) {
        EV << "creating new socket" << endl;
        UdpSocket* socket = new UdpSocket();
        socket->setOutputGate(gate("socketOut"));
        const char *localAddress = par("localAddress");

        int local_port = get_local_port();
        EV << "socket is binding. Local address: " << localAddress << ", local_port: " << local_port << endl;
        socket->bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), local_port);

        int timeToLive = par("timeToLive");
        if (timeToLive != -1)
            socket->setTimeToLive(timeToLive);

//        int dscp = par("dscp");
//        if (dscp != -1)
//            socket->setDscp(dscp);
//
//        const char *multicastInterface = par("multicastInterface");
//        if (multicastInterface[0]) {
//            IInterfaceTable *ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
//            InterfaceEntry *ie = ift->findInterfaceByName(multicastInterface);
//            if (!ie)
//                throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
//            socket->setMulticastOutputInterface(ie->getInterfaceId());
//        }

//        bool receiveBroadcast = par("receiveBroadcast");
//        if (receiveBroadcast)
//            socket->setBroadcast(true);

//        bool joinLocalMulticastGroups = par("joinLocalMulticastGroups");
//        if (joinLocalMulticastGroups) {
//            MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
//            socket->joinLocalMulticastGroups(mgl);
//        }

        socket->setCallback(this);

        destination_to_socket_mapper.insert(std::make_pair(std::make_tuple(server_idx, gpu_idx), socket));
    } else
        EV << "socket already exists" << endl;
}

L3Address UDPLongBroadcastPasserApp::get_destination(int server_idx, int gpu_idx) {
    std::string connect_address_str = "server[" + std::to_string(server_idx) +
            "]%eth" + std::to_string(gpu_idx);
    const char* connect_address = connect_address_str.c_str();
    L3Address destination;
    L3AddressResolver().tryResolve(connect_address, destination);
    return destination;
}

simtime_t UDPLongBroadcastPasserApp::get_intra_host_pass_delay(unsigned long msg_size_bytes) {
    Enter_Method("get_intra_host_pass_delay");
    EV << "Adding in-host pass delay" << endl;
    // add nvlink/nvswitch latency to pass time
    double nvlink_speed_bps = nvlink_speed_gbps * std::pow(10, 9);
    simtime_t nvlink_delay = (msg_size_bytes * 8.0) / (nvlink_speed_bps);
    // going to nvswitch and then going to the next GPU
    nvlink_delay *= 2.0;
    return nvlink_delay;
}

int UDPLongBroadcastPasserApp::find_parent(int size, int search_type, int element_idx_in_list) {

    EV << "Finding parent for element_idx=" << element_idx_in_list << endl;

    if (search_type == TREE_LOW && element_idx_in_list == 0) {
        EV << "We reached the root for tree low" << endl;
        return -1;
    }

    if (search_type == TREE_HIGH && element_idx_in_list == size - 1) {
        EV << "We reached the root for tree high" << endl;
        return -1;
    }

    if (size % 2 != 0)
        throw cRuntimeError("We didn't impelement double binary tree for odd number of nodes!");

    int start = 0;
    int end = size - 1;
    int previous_mid = -1;
    int mid = -1;

    switch (search_type) {
        case TREE_LOW:
            previous_mid = 0;
            break;
        case TREE_HIGH:
            previous_mid = size - 1;
            break;
        default:
            break;
    }

    while (end > start) {
        double mid_double = double(start + end) / 2.0;
        switch (search_type) {
            case TREE_LOW:
                mid = ceil(mid_double);
                break;
            case TREE_HIGH:
                mid = floor(mid_double);
                break;
            default:
                throw cRuntimeError("Unknown tree search type.");
        }
        if (element_idx_in_list == mid) {
            EV << "Parent found with element index of " << previous_mid << endl;
            return previous_mid;
        }
        if ((search_type == TREE_LOW && start == 0 && end == 1) ||
                (search_type == TREE_HIGH && start == size-2 && end == size-1)) {
            throw cRuntimeError("find_parent: Node not found in tree!");
        }

        previous_mid = mid;
        if (element_idx_in_list > mid)
            start = mid;
        else
            end = mid;
    }

    throw cRuntimeError("Parent not found!!");
}

int* UDPLongBroadcastPasserApp::find_children(int size, int search_type, int element_idx_in_list) {
    int* children_list = new int[2];
    children_list[0] = -1;
    children_list[1] = -1;

    EV << "Finding children for element_idx=" << element_idx_in_list << endl;

    if (size % 2 != 0)
        throw cRuntimeError("We didn't impelement double binary tree for odd number of nodes!");

    if (search_type == TREE_LOW && element_idx_in_list == 0) {
        EV << "We are at the root for tree low" << endl;
        int mid = ceil((size - 1)/2.0);
        children_list[0] = mid;
        return children_list;
    }

    if (search_type == TREE_HIGH && element_idx_in_list == size - 1) {
        EV << "We are at the root for tree high" << endl;
        int mid = floor((size - 1)/2.0);
        children_list[0] = mid;
        return children_list;
    }

    int start = 0;
    int end = size - 1;
    int mid = -1;

    while (end > start) {
        double mid_double = double(start + end) / 2.0;
        switch (search_type) {
            case TREE_LOW:
                mid = ceil(mid_double);
                break;
            case TREE_HIGH:
                mid = floor(mid_double);
                break;
            default:
                throw cRuntimeError("Unknown tree search type.");
        }
        if (element_idx_in_list == mid) {
            EV << "Node found. Start is " << start << " and end is " << end << endl;
            break;
        }
        if ((search_type == TREE_LOW && start == 0 && end == 1) ||
                (search_type == TREE_HIGH && start == size-2 && end == size-1)) {
            throw cRuntimeError("find_children: Node not found in tree!");
        }

        if (element_idx_in_list > mid)
            start = mid;
        else
            end = mid;
    }


    EV << "After the while loop, start is " << start <<
            ", mid is " << mid <<
            ", end is " << end << endl;

    int child0_idx = -1;
    if (mid - start > 1) {
        double child0_idx_double = double(start + mid) / 2.0;
        switch (search_type) {
            case TREE_LOW:
                child0_idx = ceil(child0_idx_double);
                break;
            case TREE_HIGH:
                child0_idx = floor(child0_idx_double);
                break;
            default:
                break;
        }
    }
    if (search_type == TREE_HIGH && start == 0 && mid == 1) {
        child0_idx = 0;
    }
    children_list[0] = child0_idx;
    EV << "Child0 index is " << child0_idx << endl;

    int child1_idx = -1;
    if (end - mid > 1) {
        double child1_idx_double = double(mid + end) / 2.0;
        switch (search_type) {
            case TREE_LOW:
                child1_idx = ceil(child1_idx_double);
                break;
            case TREE_HIGH:
                child1_idx = floor(child1_idx_double);
                break;
            default:
                break;
        }
    }
    if (search_type == TREE_LOW && mid == size-2 && end == size-1) {
        child1_idx = size - 1;
    }
    children_list[1] = child1_idx;
    EV << "Child1 index is " << child1_idx << endl;

    return children_list;
}

int UDPLongBroadcastPasserApp::get_num_children(BCastCustomCollectiveMsgPasserInfo* record_msg_info) {
    EV << "UDPLongBroadcastPasserApp::get_num_children called!" << endl;

    // check if there are any repititive servers in a query
    unsigned long num_dsts = record_msg_info->num_dsts;
    int* dst_server_idx_list = new int[num_dsts];
    int* dst_gpu_idx_list = new int[num_dsts];
    unsigned long* dst_server_flow_ids_list = new unsigned long[num_dsts];
    int idx = 0;
    auto flowid_it = record_msg_info->responsiblity_flow_ids.begin();
    auto gpu_it = record_msg_info->responsibility_gpu.begin();
    for (auto it = record_msg_info->responsibility.begin(); it != record_msg_info->responsibility.end(); it++) {
        dst_server_idx_list[idx] = (*it);
        dst_gpu_idx_list[idx] = (*gpu_it);
        dst_server_flow_ids_list[idx] = (*flowid_it);
        idx++;
        flowid_it++;
        gpu_it++;
    }

    int element_idx_in_list = -1;
    for (int k = 0; k < record_msg_info->num_dsts; k++) {
        if (dst_server_idx_list[k] == parent_index && dst_gpu_idx_list[k] == record_msg_info->src_gpu) {
            element_idx_in_list = k;
            EV << "element_idx_in_list is " << k << endl;
            break;
        }
    }

    int* children_element_idx_list = find_children(num_dsts,
            record_msg_info->tree_search_type, element_idx_in_list);

    int num_children = 0;
    for (int k = 0; k < 2; k++) {
        if (children_element_idx_list[k] >= 0)
            num_children++;
    }

    delete[] children_element_idx_list;
    children_element_idx_list = nullptr;
    delete[] dst_server_idx_list;
    dst_server_idx_list = nullptr;
    delete[] dst_gpu_idx_list;
    dst_gpu_idx_list = nullptr;
    delete[] dst_server_flow_ids_list;
    dst_server_flow_ids_list = nullptr;

    EV << "num_children is " << num_children << endl;
    return num_children;
}

void UDPLongBroadcastPasserApp::pass_bcast_binary_tree(BCastCustomCollectiveMsgPasserInfo* record_msg_info) {
    EV << "UDPLongBroadcastPasserApp::pass_bcast_binary_tree" << endl;

    unsigned long query_id = record_msg_info->query_id;
    unsigned long num_dsts = record_msg_info->num_dsts;

    int curr_gpu = record_msg_info->src_gpu;

    if (num_dsts == 0)
        throw cRuntimeError("How is num_dsts == 0 in pass_bcast_binary_tree");

    emit(flowPassedSignal, record_msg_info->flow_id);

    unsigned long flow_size = record_msg_info->msg_size;

    // check if there are any repititive servers in a query
    int* dst_server_idx_list = new int[num_dsts];
    int* dst_gpu_idx_list = new int[num_dsts];
    unsigned long* dst_server_flow_ids_list = new unsigned long[num_dsts];
    int idx = 0;
    auto flowid_it = record_msg_info->responsiblity_flow_ids.begin();
    auto gpu_it = record_msg_info->responsibility_gpu.begin();
    for (auto it = record_msg_info->responsibility.begin(); it != record_msg_info->responsibility.end(); it++) {
        dst_server_idx_list[idx] = (*it);
        dst_gpu_idx_list[idx] = (*gpu_it);
        dst_server_flow_ids_list[idx] = (*flowid_it);
        idx++;
        flowid_it++;
        gpu_it++;
    }

    int element_idx_in_list = -1;
    for (int k = 0; k < num_dsts; k++) {
        if (dst_server_idx_list[k] == parent_index && dst_gpu_idx_list[k] == curr_gpu) {
            element_idx_in_list = k;
            EV << "element_idx_in_list is " << k << endl;
            break;
        }
    }
    if (element_idx_in_list < 0)
        throw cRuntimeError("element was not found in list!");

    if (record_msg_info->tree_traversal_dir != TREE_UP &&
            record_msg_info->tree_traversal_dir != TREE_DOWN) {
        std::cout << "Tree traversal dir is " << record_msg_info->tree_traversal_dir << endl;
        throw cRuntimeError("Unknown tree traversal dir");
    }

//    unsigned long m_flow_id = dst_server_flow_ids_list[element_idx_in_list];
    unsigned long m_flow_id = dst_server_flow_ids_list[curr_gpu];

    if (record_msg_info->tree_traversal_dir == TREE_UP) {
        EV << "record_msg_info->tree_traversal_dir is TREE_UP" << endl;
        int tree_parent_index = find_parent(num_dsts,
                record_msg_info->tree_search_type, element_idx_in_list);
        EV << "tree_parent_index is " << tree_parent_index << endl;
        if (tree_parent_index < 0) {
            EV << "Root has been reached" << endl;
            record_msg_info->tree_traversal_dir = TREE_DOWN;
//            std::cout << simTime() << ": " << getFullPath() << ": flow id updated from " << m_flow_id << " to " << dst_server_flow_ids_list[1] <<
//                    " and " << dst_server_flow_ids_list[2] << ", msg_size is " << flow_size << endl;

        } else {
            int tree_parent_server_idx = dst_server_idx_list[tree_parent_index];
            int tree_parent_gpu_idx = dst_gpu_idx_list[tree_parent_index];

            EV << "Tree parent server idx: " << tree_parent_server_idx << " and gpu idx: " << tree_parent_gpu_idx << endl;
            std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, m_flow_id);
            auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
                auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(m_flow_id, flow_size,
                        record_msg_info->total_flow_size, query_id, curr_gpu);
                passing_msg->is_new_flow = true;
                passing_msg->dst_gpu = tree_parent_gpu_idx;
                passing_msg->num_dsts = record_msg_info->num_dsts;
                for (int t = 0; t < num_dsts; t++) {
                    passing_msg->responsibility.push_back(dst_server_idx_list[t]);
                    passing_msg->responsibility_gpu.push_back(dst_gpu_idx_list[t]);
                    passing_msg->responsiblity_flow_ids.push_back(dst_server_flow_ids_list[t]);
                }
                flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
                flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            } else {
                flowid_msgpassing_found_itr->second->is_new_flow = false;
                flowid_msgpassing_found_itr->second->msg_size += flow_size;
            }
            EV << "passing message size is: " << flowid_msgpassing_found_itr->second->msg_size << endl;
            flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;
            flowid_msgpassing_found_itr->second->tree_search_type = record_msg_info->tree_search_type;
            flowid_msgpassing_found_itr->second->tree_traversal_dir = record_msg_info->tree_traversal_dir;

            flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
            flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

            flowid_msgpassing_found_itr->second->check_metrics();

            if (flowid_msgpassing_found_itr->second->dst_gpu != tree_parent_gpu_idx) {
                std::cout << "Dst gpu: " << flowid_msgpassing_found_itr->second->dst_gpu << endl;
                std::cout << "tree_parent_gpu_idx: " << tree_parent_gpu_idx << endl;
                throw cRuntimeError("Dst gpu is not equal to the gpu to which we are creating the connection!");
            }

//            if (flowid_msgpassing_found_itr->second->flow_id == 127305)
//                std::cout << getFullPath() << "(curr_gpu:" << curr_gpu << ") Sending up " << flowid_msgpassing_found_itr->second->msg_size << " to server " <<
//                    tree_parent_server_idx << " and gpu " << flowid_msgpassing_found_itr->second->dst_gpu << endl;

            // check if the gpu is inside the same host
            if (tree_parent_server_idx == parent_index) {
                EV << "Dst GPU is in the same host as Src GPU" << endl;
                // next gpu (dst gpu) again has processing delay
                simtime_t processing_delay = get_processing_delay(flowid_msgpassing_found_itr->second->dst_gpu,
                        flowid_msgpassing_found_itr->second->m_collective_type,
                        flowid_msgpassing_found_itr->second->m_collective_alg,
                        record_msg_info->tree_traversal_dir);
                simtime_t pass_delay = get_intra_host_pass_delay(flow_size);

                schedule_finish_if_required(flowid_msgpassing_found_itr->second, processing_delay);
                schedule_pass(flowid_msgpassing_found_itr->second, processing_delay + pass_delay);

                // just erase it becasue the message is completely passed but don't delete it because
                // message will be delivered somewhere else
                flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
            } else {

                // In case we should reach another host
                init_socket(tree_parent_server_idx, tree_parent_gpu_idx);
                /*
                 * If flowid_msgpassing_found_itr->second->msg_size != flow_size,
                 * then we know that the message had some bytes pending generate signal from mac
                 * which means that we should only add to those bytes and wait for the signal
                 */
                if (flowid_msgpassing_found_itr->second->msg_size == flow_size)
                    pass_msg(tree_parent_server_idx, flowid_msgpassing_found_itr->second);

                if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                        flowid_msgpassing_found_itr->second->original_flow_terminated) {
                    flowid_msgpassing_found_itr->second->responsibility.clear();
                    flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
                    flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
                    gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
                    delete flowid_msgpassing_found_itr->second;
                    flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
                }
            }
        }
    }

    if (record_msg_info->tree_traversal_dir == TREE_DOWN) {
        EV << "record_msg_info->tree_traversal_dir is TREE_DOWN" << endl;
        int* children_element_idx_list = find_children(num_dsts,
                record_msg_info->tree_search_type, element_idx_in_list);
        if (children_element_idx_list[0] >= 0 && children_element_idx_list[0] == children_element_idx_list[1]) {
            std::cout << "Find_children returned a same (" << children_element_idx_list[0] << ") node as both children" << endl;
            throw cRuntimeError("Find_children returned a same node as both children");
        }

        for (int k = 0; k < 2; k++) {
            if (children_element_idx_list[k] >= 0) {
                int tree_child_server_idx = dst_server_idx_list[children_element_idx_list[k]];
                int tree_child_gpu_idx = dst_gpu_idx_list[children_element_idx_list[k]];
                // we need different flow_ids for different children!
//                m_flow_id = dst_server_flow_ids_list[num_gpus + 2*curr_gpu + k];
                m_flow_id = dst_server_flow_ids_list[num_gpus + k];
//                if (m_flow_id == 127298 && parent_index == 62) {
//                    std::cout << "cur_gpu is: " << curr_gpu << endl;
//                    std::cout << tree_child_server_idx << endl;
//                    std::cout << tree_child_gpu_idx << endl;
//                }
//                unsigned long m_flow_id = dst_server_flow_ids_list[element_idx_in_list];
                EV << "Tree child server idx: " << tree_child_server_idx <<
                        " and gpu idx: " << tree_child_gpu_idx << " with flow id: " << m_flow_id << endl;
                std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, m_flow_id);
                auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);

                if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
                    auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(m_flow_id, flow_size,
                            record_msg_info->total_flow_size, query_id, curr_gpu);
                    passing_msg->is_new_flow = true;
                    passing_msg->dst_gpu = tree_child_gpu_idx;
                    passing_msg->num_dsts = record_msg_info->num_dsts;
                    for (int t = 0; t < num_dsts; t++) {
                        passing_msg->responsibility.push_back(dst_server_idx_list[t]);
                        passing_msg->responsibility_gpu.push_back(dst_gpu_idx_list[t]);
                        passing_msg->responsiblity_flow_ids.push_back(dst_server_flow_ids_list[t]);
                    }
                    flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
                    flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
                } else {
                    flowid_msgpassing_found_itr->second->is_new_flow = false;
                    flowid_msgpassing_found_itr->second->msg_size += flow_size;
                }
                flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;
                flowid_msgpassing_found_itr->second->tree_search_type = record_msg_info->tree_search_type;
                flowid_msgpassing_found_itr->second->tree_traversal_dir = record_msg_info->tree_traversal_dir;

                flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
                flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

                flowid_msgpassing_found_itr->second->check_metrics();

                if (flowid_msgpassing_found_itr->second->dst_gpu != tree_child_gpu_idx) {
                    std::cout << "Flow id: " << flowid_msgpassing_found_itr->second->flow_id << endl;
                    std::cout << "Dst gpu: " << flowid_msgpassing_found_itr->second->dst_gpu << endl;
                    std::cout << "tree_child_gpu_idx: " << tree_child_gpu_idx << endl;
                    throw cRuntimeError("Dst gpu is not equal to the gpu to which we are creating the connection2!");
                }

//                if (flowid_msgpassing_found_itr->second->flow_id == 127305)
//                    std::cout << getFullPath() << "(curr_gpu:" << curr_gpu << "): Sending down " << flowid_msgpassing_found_itr->second->msg_size << " to server " <<
//                    tree_child_server_idx << " and gpu " << tree_child_gpu_idx << endl;


                // check if the gpu is inside the same host
                if (tree_child_server_idx == parent_index) {
                    EV << "Dst GPU is in the same host as Src GPU" << endl;
                    // next gpu (dst gpu) again has processing delay
                    simtime_t processing_delay = get_processing_delay(flowid_msgpassing_found_itr->second->dst_gpu,
                            flowid_msgpassing_found_itr->second->m_collective_type,
                            flowid_msgpassing_found_itr->second->m_collective_alg,
                            record_msg_info->tree_traversal_dir);
                    simtime_t pass_delay = get_intra_host_pass_delay(flow_size);

                    schedule_finish_if_required(flowid_msgpassing_found_itr->second, processing_delay);
                    schedule_pass(flowid_msgpassing_found_itr->second, processing_delay + pass_delay);

                    // just erase it becasue the message is completely passed but don't delete it because
                    // message will be delivered somewhere else
                    flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
                } else {

                    // In case we should reach another host
                    init_socket(tree_child_server_idx, tree_child_gpu_idx);
                    /*
                     * If flowid_msgpassing_found_itr->second->msg_size != flow_size,
                     * then we know that the message had some bytes pending generate signal from mac
                     * which means that we should only add to those bytes and wait for the signal
                     */
                    if (flowid_msgpassing_found_itr->second->msg_size == flow_size)
                        pass_msg(tree_child_server_idx, flowid_msgpassing_found_itr->second);

                    if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                            flowid_msgpassing_found_itr->second->original_flow_terminated) {
                        flowid_msgpassing_found_itr->second->responsibility.clear();
                        flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
                        flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
                        gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
                        delete flowid_msgpassing_found_itr->second;
                        flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
                    }
                }
            }
        }
        delete[] children_element_idx_list;
        children_element_idx_list = nullptr;
    }

    delete[] dst_server_idx_list;
    dst_server_idx_list = nullptr;
    delete[] dst_gpu_idx_list;
    dst_gpu_idx_list = nullptr;
    delete[] dst_server_flow_ids_list;
    dst_server_flow_ids_list = nullptr;
}

void UDPLongBroadcastPasserApp::pass_bcast_binomial_tree(BCastCustomCollectiveMsgPasserInfo* record_msg_info) {
    EV << "UDPLongBroadcastPasserApp::pass_bcast_binomialbinomial)" << endl;

    unsigned long query_id = record_msg_info->query_id;
    unsigned long num_dsts = record_msg_info->num_dsts;

    int curr_gpu = record_msg_info->src_gpu;

    if (num_dsts == 0) {
        EV << "There is no one to pass the message to" << endl;
        return;
    }

    emit(flowPassedSignal, record_msg_info->flow_id);


    unsigned long flow_size = record_msg_info->msg_size;

    // check if there are any repititive servers in a query
    int dst_server_idx_list[num_dsts];
    int dst_gpu_idx_list[num_dsts];
    int dst_server_flow_ids_list[num_dsts];
    int idx = 0;
    auto flowid_it = record_msg_info->responsiblity_flow_ids.begin();
    auto gpu_it = record_msg_info->responsibility_gpu.begin();
    for (auto it = record_msg_info->responsibility.begin(); it != record_msg_info->responsibility.end(); it++) {
        dst_server_idx_list[idx] = (*it);
        dst_gpu_idx_list[idx] = (*gpu_it);
        dst_server_flow_ids_list[idx] = (*flowid_it);
        idx++;
        flowid_it++;
        gpu_it++;
    }

    unsigned int start = 0;
    unsigned int end = num_dsts;
    std::unordered_map<unsigned long, std::list<int>> flowid_responsibility_list_mapper;
    std::unordered_map<unsigned long, std::list<int>> flowid_responsibility_gpu_list_mapper;
    std::unordered_map<unsigned long, std::list<unsigned long>> flowid_responsibility_flowids_list_mapper;
    std::unordered_map<unsigned long, int> flowid_dstidx_mapper;
    std::unordered_map<unsigned long, int> flowid_dstgpu_mapper;
    while (true) {
        unsigned int mid = int((end-start)/2);
        auto mid_found = flowid_responsibility_list_mapper.find(dst_server_flow_ids_list[mid]);
        if (mid_found != flowid_responsibility_list_mapper.end())
            throw cRuntimeError("Mid should not exist in flowid_responsibility_list_mapper");
        std::list<int> temp_list;
        std::list<int> temp_gpu_list;
        std::list<unsigned long> temp_flowids_list;
        for (int i = mid+1; i < end; i++) {
            temp_list.push_back(dst_server_idx_list[i]);
            temp_gpu_list.push_back(dst_gpu_idx_list[i]);
            temp_flowids_list.push_back(dst_server_flow_ids_list[i]);
        }
        flowid_responsibility_list_mapper.insert(std::make_pair(dst_server_flow_ids_list[mid], temp_list));
        flowid_responsibility_gpu_list_mapper.insert(std::make_pair(dst_server_flow_ids_list[mid], temp_gpu_list));
        flowid_dstidx_mapper.insert(std::make_pair(dst_server_flow_ids_list[mid], dst_server_idx_list[mid]));
        flowid_dstgpu_mapper.insert(std::make_pair(dst_server_flow_ids_list[mid], dst_gpu_idx_list[mid]));
        flowid_responsibility_flowids_list_mapper.insert(std::make_pair(dst_server_flow_ids_list[mid], temp_flowids_list));
        end = mid;
        if (end <= start)
            break;
    }

    if (flowid_responsibility_list_mapper.size() !=
            flowid_responsibility_flowids_list_mapper.size() ||
            flowid_responsibility_list_mapper.size() !=
                        flowid_responsibility_gpu_list_mapper.size())
        throw cRuntimeError("LongBroadcastPasserApp:flowid_responsibility_list_mapper.size() != flowid_responsibility_flowids_list_mapper.size()");


    for (auto itr = flowid_responsibility_list_mapper.begin();
            itr != flowid_responsibility_list_mapper.end(); itr++){
        auto dst_server_idx_found_itr = flowid_dstidx_mapper.find(itr->first);
        if (dst_server_idx_found_itr == flowid_dstidx_mapper.end())
            throw cRuntimeError("No destination idx assiged to flow id");
        int server_idx = dst_server_idx_found_itr->second;

        auto dst_gpu_idx_found_itr = flowid_dstgpu_mapper.find(itr->first);
        if (dst_gpu_idx_found_itr == flowid_dstgpu_mapper.end())
            throw cRuntimeError("dst_gpu_idx_found_itr == flowid_dstgpu_mapper.end()");
        int gpu_idx = dst_gpu_idx_found_itr->second;

        std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, itr->first);
        auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
        if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
            auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(itr->first, flow_size,
                    record_msg_info->total_flow_size, query_id, curr_gpu);
            passing_msg->is_new_flow = true;
            passing_msg->responsibility = itr->second;
            passing_msg->num_dsts = itr->second.size();
            passing_msg->dst_gpu = gpu_idx;

            auto flow_id_list_itr_found = flowid_responsibility_flowids_list_mapper.find(itr->first);
            if (flow_id_list_itr_found == flowid_responsibility_flowids_list_mapper.end())
                throw cRuntimeError("LongBroadcastPasserApp::flow_id_list_itr_found == flowid_responsibility_flowids_list_mapper.end()");
            passing_msg->responsiblity_flow_ids = flow_id_list_itr_found->second;

            auto gpu_id_list_itr_found = flowid_responsibility_gpu_list_mapper.find(itr->first);
            if (gpu_id_list_itr_found == flowid_responsibility_gpu_list_mapper.end())
                throw cRuntimeError("LongBroadcastPasserApp::gpu_id_list_itr_found == flowid_responsibility_gpu_list_mapper.end()");
            passing_msg->responsibility_gpu = gpu_id_list_itr_found->second;

            flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
            flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
        } else {
            flowid_msgpassing_found_itr->second->is_new_flow = false;
            flowid_msgpassing_found_itr->second->msg_size += flow_size;
        }
        flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;

        flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
        flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

        flowid_msgpassing_found_itr->second->check_metrics();
        EV << "Passing to server " << server_idx << " with flow_id " << itr->first << endl;
        // check if the gpu is inside the same host
        if (server_idx == parent_index) {
            EV << "Dst GPU is in the same host as Src GPU" << endl;
            // next gpu (dst gpu) again has processing delay
            simtime_t processing_delay = get_processing_delay(flowid_msgpassing_found_itr->second->dst_gpu,
                    flowid_msgpassing_found_itr->second->m_collective_type,
                    flowid_msgpassing_found_itr->second->m_collective_alg);
            simtime_t pass_delay = get_intra_host_pass_delay(flow_size);

            schedule_finish_if_required(flowid_msgpassing_found_itr->second, processing_delay);
            schedule_pass(flowid_msgpassing_found_itr->second, processing_delay + pass_delay);

            // just erase it becasue the message is completely passed but don't delete it because
            // message will be delivered somewhere else
            flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);


        } else {
            init_socket(server_idx, gpu_idx);
            /*
             * If flowid_msgpassing_found_itr->second->msg_size != flow_size,
             * then we know that the message had some bytes pending generate signal from mac
             * which means that we should only add to those bytes and wait for the signal
             */
            if (flowid_msgpassing_found_itr->second->msg_size == flow_size)
                pass_msg(server_idx, flowid_msgpassing_found_itr->second);

            if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                    flowid_msgpassing_found_itr->second->original_flow_terminated) {
                flowid_msgpassing_found_itr->second->responsibility.clear();
                flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
                flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
                gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
                delete flowid_msgpassing_found_itr->second;
                flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
            }
        }
    }
}

void UDPLongBroadcastPasserApp::optireduce_send_next(unsigned long query_id, int src_gpu_idx) {

    // send_next is meaningless with BECCA optireduce
    if (use_becca_optireduce || use_cidr_optireduce)
        return;

    EV << "UDPLongBroadcastPasserApp::optireduce_send_next for query: " << query_id
                << " and source_gpu_idx: " << src_gpu_idx << endl;
    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
    auto msg_record_found_itr = optireduce_query_gpu_msg_record.find(query_src_gpu_pair);
    if (msg_record_found_itr == optireduce_query_gpu_msg_record.end())
        return;

    int max_snd_num = optireduce_incast_factor;

    while (max_snd_num > 0 && msg_record_found_itr->second.size() > 0) {
        EV << "Sending next message!" << endl;
        auto m_msg = msg_record_found_itr->second.front();
        msg_record_found_itr->second.pop_front();
        std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(m_msg->src_gpu, m_msg->flow_id);
        auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
        if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end())
            throw cRuntimeError("flowid_msgpassing_found_itr must be found in flowid_passingmsg_mapper at this point");
        if (flowid_msgpassing_found_itr->second->optireduce_dst_server < 0 ||
                flowid_msgpassing_found_itr->second->optireduce_dst_gpu < 0)
            throw cRuntimeError("Invalid optireduce_dst_server or optireduce_dst_gpu");

        // check if the gpu is inside the same host
        if (flowid_msgpassing_found_itr->second->optireduce_dst_server == parent_index) {
            EV << "Dst GPU is in the same host as Src GPU" << endl;
            // Broadcast has 0 processing, no need to schedule pass
            simtime_t processing_delay = 0;
            schedule_finish_if_required(flowid_msgpassing_found_itr->second, processing_delay);
            // just erase it becasue the message is completely passed also delete it since
            // it's not gonna be delivered anywhere else
            delete flowid_msgpassing_found_itr->second;
            flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
        } else {
            EV << "Sending to someone outside of the host!" << endl;
            // In case we should reach another host
            init_socket(flowid_msgpassing_found_itr->second->optireduce_dst_server,
                    flowid_msgpassing_found_itr->second->optireduce_dst_gpu);

            /*
             * If flowid_msgpassing_found_itr->second->msg_size != flow_size,
             * then we know that the message had some bytes pending generate signal from mac
             * which means that we should only add to those bytes and wait for the signal
             */
            if (flowid_msgpassing_found_itr->second->msg_size ==
                    flowid_msgpassing_found_itr->second->total_flow_size)
                pass_msg(flowid_msgpassing_found_itr->second->optireduce_dst_server,
                        flowid_msgpassing_found_itr->second);

            if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                    flowid_msgpassing_found_itr->second->original_flow_terminated) {
                flowid_msgpassing_found_itr->second->responsibility.clear();
                flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
                flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
                gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
                delete flowid_msgpassing_found_itr->second;
                flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
            } else
                max_snd_num--;
        }

    }

    // If someone else removed this, you would get nullptrexp without this!
    msg_record_found_itr = optireduce_query_gpu_msg_record.find(query_src_gpu_pair);
    if (msg_record_found_itr == optireduce_query_gpu_msg_record.end())
        return;
    if (msg_record_found_itr->second.size() == 0)
        optireduce_query_gpu_msg_record.erase(msg_record_found_itr);
}

void UDPLongBroadcastPasserApp::pass_bcast_optireduce(BCastCustomCollectiveMsgPasserInfo* record_msg_info) {
    EV << simTime() << ": UDPLongBroadcastPasserApp::pass_bcast_optireduce()" << endl;

    if (optireduce_incast_factor <= 0)
        throw cRuntimeError("invalid optireduce_incast_factor");

    // todo: we must add orca and rsbf to becca optireduce as well ;-)
    if (use_becca_optireduce && (use_rsbf || use_orca))
        throw cRuntimeError("BECCA optireduce does not use rsbf or orca yet!!!");

    // disable incast factor for optireduce becca
    if (use_becca_optireduce || use_cidr_optireduce)
        optireduce_incast_factor = INT_MAX-1; // some large value that is definitely larger than our scales

    // we are now at the gpu which is passing so we should use src gpu
    unsigned long query_id = record_msg_info->query_id;
    unsigned long num_dst_servers_per_query = record_msg_info->responsibility.size();
    int curr_gpu = record_msg_info->src_gpu;

    if (!record_msg_info->optireduce_in_reduction_phase)
        throw cRuntimeError("optireduce_in_reduction_phase MUST be true here!!");

    if (num_dst_servers_per_query == 0) {
        EV << "There is no one to pass the message to" << endl;
        return;
    }

    EV << "OptiReduce entering the broadcast phase" << endl;

    std::tuple<int, unsigned long> gpu_idx_query_id_tuple = std::make_tuple(curr_gpu, query_id);

    auto completed_flows_itr = gpu_query_to_completed_flows.find(gpu_idx_query_id_tuple);
    if (completed_flows_itr->second.size() != record_msg_info->num_dsts - 1) {
        std::cout << completed_flows_itr->second.size() << endl;
        std::cout << record_msg_info->num_dsts << endl;
        throw cRuntimeError("If not chunking, why are we passing without all flows completing!");
    }
    // exclude the node itself from the responsibility
    if (completed_flows_itr->second.size() != record_msg_info->responsibility.size() - 1)
        throw cRuntimeError("completed_flows_itr->second.size() != record_msg_info->responsibility.size() - 1");


    emit(flowPassedSignal, record_msg_info->flow_id);
    unsigned long flow_size = record_msg_info->msg_size;

    int element_idx;
    int list_size = record_msg_info->responsibility_gpu.size();
    int dst_server_idx_list[list_size];
    int gpu_idx_list[list_size];
    unsigned long flow_id_list[list_size];
    auto gpu_idx_it = record_msg_info->responsibility_gpu.begin();
    auto flow_id_itr = completed_flows_itr->second.begin();
    int idx = 0;

    // for optireduce_becca
    unsigned long candidate_flow_id;
    int candidate_dst_server;
    int candidate_dst_gpu = curr_gpu; // send to the same gpu as the source gpu in other hosts
    for (auto dst_it = record_msg_info->responsibility.begin();
            dst_it != record_msg_info->responsibility.end();
            dst_it++) {
        dst_server_idx_list[idx] = (*dst_it);
        gpu_idx_list[idx] = (*gpu_idx_it);
        if (dst_server_idx_list[idx] == parent_index &&
                gpu_idx_list[idx] == curr_gpu) {
            // not gonna be used
            flow_id_list[idx] = 0;
            element_idx = idx;
        } else {
            flow_id_list[idx] = (*flow_id_itr);
            flow_id_itr++;
            if (dst_server_idx_list[idx] != parent_index) {
                // don't want it to loop back!
                candidate_flow_id = flow_id_list[idx];
                candidate_dst_server = dst_server_idx_list[idx];
            }
        }
        gpu_idx_it++;
        idx++;
    }
    record_msg_info->responsibility.clear();
    record_msg_info->responsibility_gpu.clear();
    record_msg_info->responsiblity_flow_ids.clear();
    completed_flows_itr->second.clear();
    gpu_query_to_completed_flows.erase(completed_flows_itr);


    if (use_becca_optireduce) {
        EV << "Boradcasting similar to BECCA..." << endl;

        UDPLongAllgatherInitiatorApp* initiator_app = check_and_cast<UDPLongAllgatherInitiatorApp*>(getModuleByPath(record_msg_info->initiator_app_path.c_str()));

        auto list_of_ports = initiator_app->optireduce_get_port_list(query_id, curr_gpu);
        auto list_of_jumps = initiator_app->optireduce_get_jump_list(query_id, curr_gpu);

        if (list_of_jumps.size() != list_of_ports.size())
            throw cRuntimeError("list_of_jumps.size() != list_of_ports.size()");

        std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, candidate_flow_id);
        auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
        if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
           auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(candidate_flow_id, flow_size,
                   record_msg_info->total_flow_size, query_id, curr_gpu);
           passing_msg->is_new_flow = true;
           passing_msg->dst_gpu = candidate_dst_gpu;
           passing_msg->num_dsts = 0;

           flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
           flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
        } else {
           throw cRuntimeError("This should not happen with optireduce since we have not further chunking2!");
        //                flowid_msgpassing_found_itr->second->is_new_flow = false;
        //                flowid_msgpassing_found_itr->second->msg_size += flow_size;
        }
        flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;

        flowid_msgpassing_found_itr->second->ring_end_server_idx = record_msg_info->ring_end_server_idx;
        flowid_msgpassing_found_itr->second->ring_end_gpu_idx = record_msg_info->ring_end_gpu_idx;

        flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
        flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

        flowid_msgpassing_found_itr->second->optireduce_dst_server = candidate_dst_server;
        flowid_msgpassing_found_itr->second->optireduce_dst_gpu = candidate_dst_gpu;

        flowid_msgpassing_found_itr->second->optireduce_in_reduction_phase = false;

        // read the original initiator and replace it with your own initiator index
        flowid_msgpassing_found_itr->second->initiator_app_path = record_msg_info->initiator_app_path;

//        std::cout << getFullPath() << ": " << simTime() << endl;
        std::unordered_set<int> dst_servers_in_this_query_set;
        dst_servers_in_this_query_set.insert(parent_index);
        for (int i = 0; i < list_size; i++) {
            int m_dst_server_idx = dst_server_idx_list[i];
            int m_gpu_idx = gpu_idx_list[i];
            unsigned long m_flow_id = flow_id_list[idx];

            // filter_out_self_data
            if (m_dst_server_idx == parent_index)
                continue;

            auto dst_server_idx_found = dst_servers_in_this_query_set.find(m_dst_server_idx);
            if (dst_server_idx_found != dst_servers_in_this_query_set.end())
                continue;
            dst_servers_in_this_query_set.insert(m_dst_server_idx);
            flowid_msgpassing_found_itr->second->network_assisted_destination_idx_list.push_back(m_dst_server_idx);
            flowid_msgpassing_found_itr->second->network_assisted_dst_gpu_idx_list.push_back(candidate_dst_gpu);
            flowid_msgpassing_found_itr->second->network_assisted_flow_ids_list.push_back(m_flow_id);

//            std::cout << m_dst_server_idx << endl;
        }
//        std::cout << "---------------" << endl;

        flowid_msgpassing_found_itr->second->lists_of_lists_of_jumps = list_of_jumps;
        flowid_msgpassing_found_itr->second->lists_of_lists_of_ports = list_of_ports;

        init_socket(candidate_dst_server, candidate_dst_gpu);
        pass_msg(candidate_dst_server, flowid_msgpassing_found_itr->second);

        if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                flowid_msgpassing_found_itr->second->original_flow_terminated) {
            flowid_msgpassing_found_itr->second->responsibility.clear();
            flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
            flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
            flowid_msgpassing_found_itr->second->network_assisted_destination_idx_list.clear();
            flowid_msgpassing_found_itr->second->network_assisted_dst_gpu_idx_list.clear();
            flowid_msgpassing_found_itr->second->network_assisted_flow_ids_list.clear();
            flowid_msgpassing_found_itr->second->lists_of_lists_of_jumps.clear();
            flowid_msgpassing_found_itr->second->lists_of_lists_of_ports.clear();
            gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
            delete flowid_msgpassing_found_itr->second;
            flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
        }

    } else if (use_cidr_optireduce) {

        UDPLongAllgatherInitiatorApp* initiator_app = check_and_cast<UDPLongAllgatherInitiatorApp*>(
                getModuleByPath(record_msg_info->initiator_app_path.c_str()));

        if (!initiator_app->partial_mcast_use_cidr_agg)
            throw cRuntimeError("partial_mcast_use_cidr_agg should both be true with optireduce!!");

        // maps each ToR idx to [(server idx, gpu idx, flow id)]
        std::map<int, std::list<std::tuple<int, int, unsigned long>>> tor_idx_to_flow_info_map;
        std::unordered_set<int> dst_servers_in_this_query_set;
        dst_servers_in_this_query_set.insert(parent_index);
        // store the CIDR groups per pod -- later used for cidr at core
        std::map<unsigned long, std::set<std::string>> dst_pod_to_tor_cidr_groups;
        std::unordered_map<unsigned long, std::unordered_set<unsigned long>> cidr_pod_idx_to_dst_tor_idx_list;
        std::unordered_set<unsigned long> cidr_dst_pod_idx_set;
        for (int i = 0; i < list_size; i++) {
            int m_dst_server_idx = dst_server_idx_list[i];

            if (m_dst_server_idx == parent_index)
                continue;

            int m_gpu_idx = gpu_idx_list[i];
            unsigned long m_flow_id = flow_id_list[i];

            auto dst_server_idx_found = dst_servers_in_this_query_set.find(m_dst_server_idx);
            if (dst_server_idx_found != dst_servers_in_this_query_set.end())
                continue;

            if (m_flow_id == 0)
                throw cRuntimeError("Invalid m_flow_id");

            dst_servers_in_this_query_set.insert(m_dst_server_idx);
            std::tuple<int, int, unsigned long> flow_info_tuple = std::make_tuple(m_dst_server_idx,
                    m_gpu_idx,
                    m_flow_id);
            int tor_idx = initiator_app->get_tor_idx_for_server(m_dst_server_idx);
            auto tor_idx_itr = tor_idx_to_flow_info_map.find(tor_idx);
            if (tor_idx_itr == tor_idx_to_flow_info_map.end()) {
                std::list<std::tuple<int, int, unsigned long>> tmp_list;
                tor_idx_to_flow_info_map.insert(std::make_pair(tor_idx, tmp_list));
                tor_idx_itr = tor_idx_to_flow_info_map.find(tor_idx);
            }
            tor_idx_itr->second.push_back(flow_info_tuple);

            int pod_idx = initiator_app->get_pod_idx_for_tor(tor_idx);
            // for cidr agg
            // assign dst tors that are not src tors to cidr info
            auto pod_found_itr = cidr_pod_idx_to_dst_tor_idx_list.find(pod_idx);
            if (pod_found_itr == cidr_pod_idx_to_dst_tor_idx_list.end()) {
                std::unordered_set<unsigned long> tmp_set;
                cidr_pod_idx_to_dst_tor_idx_list.insert(std::make_pair(pod_idx,
                        tmp_set));
                pod_found_itr = cidr_pod_idx_to_dst_tor_idx_list.find(pod_idx);
            }
            pod_found_itr->second.insert(tor_idx);

            // for cidr core
            if (initiator_app->partial_mcast_use_cidr_core) {
                cidr_dst_pod_idx_set.insert(pod_idx);
                std::set<std::string> tmp_set;
                dst_pod_to_tor_cidr_groups.insert(std::make_pair(pod_idx, tmp_set));
            }
        }

        int src_tor_idx = initiator_app->get_tor_idx_for_server(parent_index);
        int src_pod_idx = initiator_app->get_pod_idx_for_tor(src_tor_idx);

        std::list<unsigned long> flow_ids_tmp_list;
        for (auto m_flow_id : flow_id_list) {
            if (m_flow_id > 0)
                flow_ids_tmp_list.push_back(m_flow_id);
        }
        unsigned long flow_ids_list[flow_ids_tmp_list.size()];
        int m_idx = 0;
        for (auto m_flow_ids : flow_ids_tmp_list) {
            flow_ids_list[m_idx] = m_flow_ids;
            m_idx++;
        }

        // this is empty if !partial_mcast_use_cidr_agg
        std::unordered_map<unsigned long, std::string> cidr_tor_group_ids;  // tor idx --> id
        std::unordered_map<std::string, unsigned long> agg_cidr_group_member_count_map; // id --> tor member count

        if (cidr_pod_idx_to_dst_tor_idx_list.size() == 0)
            throw cRuntimeError("cidr_pod_idx_to_dst_tor_idx_list.size() should not be zero!");

        for (auto pod_idx_itr = cidr_pod_idx_to_dst_tor_idx_list.begin();
                pod_idx_itr != cidr_pod_idx_to_dst_tor_idx_list.end();
                pod_idx_itr++) {
            EV << "CIDR at agg: Processing pod " << pod_idx_itr->first << endl;
            if (initiator_app->agg_cidr_max_bit_size == 0)
                throw cRuntimeError("agg_cidr_max_bit_size should be > 0");
            // std::unordered_map<unsigned long, std::string>
            auto tor_index_to_prefix_info = initiator_app->CIRD_minimal_prefix_cover(
                    pod_idx_itr->second,
                    initiator_app->agg_cidr_max_bit_size,
                    initiator_app->CIDR_AGG);

            for (auto tor_idx_itr = tor_index_to_prefix_info.begin();
                    tor_idx_itr != tor_index_to_prefix_info.end();
                    tor_idx_itr++) {

                // do not include the src tor
                if (tor_idx_itr->first == src_tor_idx)
                    continue;

                auto cidr_group_id = std::to_string(pod_idx_itr->first) +
                        "_" + tor_idx_itr->second;
                EV << "Cird group index corresponding to tor " << tor_idx_itr->first <<
                        " is " << cidr_group_id << endl;
                cidr_tor_group_ids.insert(std::make_pair(tor_idx_itr->first, cidr_group_id));
                auto cidr_group_itr = agg_cidr_group_member_count_map.find(cidr_group_id);
                if (cidr_group_itr == agg_cidr_group_member_count_map.end()) {
                    agg_cidr_group_member_count_map.insert(std::make_pair(cidr_group_id, 0));
                    cidr_group_itr = agg_cidr_group_member_count_map.find(cidr_group_id);
                }
                cidr_group_itr->second++;

                // for cidr core
                if (initiator_app->partial_mcast_use_cidr_core) {
                    // store unique aggregate cidr groups we have under each pod
                    auto dst_pod_itr = dst_pod_to_tor_cidr_groups.find(pod_idx_itr->first);
                    if (dst_pod_itr == dst_pod_to_tor_cidr_groups.end())
                        throw cRuntimeError("Dst pod must exist here!");
                    dst_pod_itr->second.insert(cidr_group_id);
                }
            }
        }

        // this is empty if !partial_mcast_use_cidr_core
        std::unordered_map<std::string, unsigned long> core_cidr_group_member_count_map; // id --> pod member count
        int core_cidr_group_index = -1;
        std::map<std::string, std::string> agg_cidr_id_to_core_cidr_id;

        if (cidr_dst_pod_idx_set.size() != dst_pod_to_tor_cidr_groups.size())
            throw cRuntimeError("cidr_dst_pod_idx_set.size() != dst_pod_to_tor_cidr_groups.size()");

        // if cidr_dst_pod_idx_set.size() should either be > 1 when source pod is remaining
        // or be == 1 while the remaining element is not source pod
        while (cidr_dst_pod_idx_set.size() > 1 ||
                (cidr_dst_pod_idx_set.size() == 1 &&
                        (*cidr_dst_pod_idx_set.begin()) != src_pod_idx)) {
            core_cidr_group_index++;

            EV << "\n\n\nCIDR at core. Repeat time: " << core_cidr_group_index << endl;
            if (initiator_app->core_cidr_max_bit_size == 0)
                throw cRuntimeError("core_cidr_max_bit_size should be > 0");
            // std::unordered_map<unsigned long, std::string>
            auto pod_index_to_prefix_info = initiator_app->CIRD_minimal_prefix_cover(
                    cidr_dst_pod_idx_set,
                    initiator_app->core_cidr_max_bit_size,
                    initiator_app->CIDR_CORE);

            for (auto pod_idx_itr = pod_index_to_prefix_info.begin();
                    pod_idx_itr != pod_index_to_prefix_info.end();
                    pod_idx_itr++) {

                // do not include the src pod
                if (pod_idx_itr->first == src_pod_idx)
                    continue;

                // we only have one DC and group of pods so we always put X in the beginning
                auto cidr_group_id = std::to_string(core_cidr_group_index) + "_" + pod_idx_itr->second;
                EV << "Cird group index corresponding to pod " << pod_idx_itr->first <<
                        " is " << cidr_group_id << endl;

                // assign one of the aggregate cidr groups under this pod to the new core cidr group id
                auto dst_pod_itr = dst_pod_to_tor_cidr_groups.find(pod_idx_itr->first);
                if (dst_pod_itr == dst_pod_to_tor_cidr_groups.end() || dst_pod_itr->second.size() == 0)
                    throw cRuntimeError("Dst pod has no AGG CIDR group assigned...");
                auto candidate_agg_cidr_group_itr = dst_pod_itr->second.begin();
                agg_cidr_id_to_core_cidr_id.insert(std::make_pair(*candidate_agg_cidr_group_itr, cidr_group_id));
                dst_pod_itr->second.erase(candidate_agg_cidr_group_itr);

                auto cidr_group_itr = core_cidr_group_member_count_map.find(cidr_group_id);
                if (cidr_group_itr == core_cidr_group_member_count_map.end()) {
                    core_cidr_group_member_count_map.insert(std::make_pair(cidr_group_id, 0));
                    cidr_group_itr = core_cidr_group_member_count_map.find(cidr_group_id);
                }
                cidr_group_itr->second++;
            }

            // remove dst pods for whom all agg cidr groups under them have been covered by the core
            for (auto dst_pod_itr : dst_pod_to_tor_cidr_groups) {
                // don't remove the source pod so that we can use it for groupings
                if (dst_pod_itr.first != src_pod_idx && dst_pod_itr.second.size() == 0) {
                    cidr_dst_pod_idx_set.erase(dst_pod_itr.first);
                }
            }
        }

        EV << "We're sending to a total of " << tor_idx_to_flow_info_map.size() << " tors." << endl;

        EV << "num_dst_servers_per_query update from " << num_dst_servers_per_query;
        num_dst_servers_per_query = flow_ids_tmp_list.size();
        EV << " to " << num_dst_servers_per_query << endl;

        std::map<int, int> tor_to_candidate_server_idx_map;
        std::map<int, int> tor_to_candidate_gpu_idx_map;
        int flow_id_list_idx = -1;
        std::map<int, int> pod_to_tor_num;
        std::map<int, std::list<BCastCustomCollectiveMsgPasserInfo*>> tor_group_to_msg_list;

        for (auto it = tor_idx_to_flow_info_map.begin();
                it != tor_idx_to_flow_info_map.end();
                it++) {
            if (it->second.size() == 0)
                throw cRuntimeError("How is the list empty!!");
            int pod_idx = initiator_app->get_pod_idx_for_tor(it->first);

            // we want to assign different flow ids to chunks of the same flow sent by different gpus
            flow_id_list_idx++;

            if (flow_id_list_idx >= num_dst_servers_per_query)
                throw cRuntimeError("flow_id_list_idx >= num_dst_servers_per_query!");
            int candidate_flow_id = flow_ids_list[flow_id_list_idx];

            std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, candidate_flow_id);
            auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
               auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(candidate_flow_id, flow_size,
                       record_msg_info->total_flow_size, query_id, curr_gpu);
               passing_msg->is_new_flow = true;
               passing_msg->num_dsts = 0;

               flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
               flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            } else
               throw cRuntimeError("This should not happen with optireduce since we have not further chunking3!");

            flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;

            flowid_msgpassing_found_itr->second->ring_end_server_idx = record_msg_info->ring_end_server_idx;
            flowid_msgpassing_found_itr->second->ring_end_gpu_idx = record_msg_info->ring_end_gpu_idx;

            flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
            flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

            flowid_msgpassing_found_itr->second->optireduce_in_reduction_phase = false;

            // read the original initiator and replace it with your own initiator index
            flowid_msgpassing_found_itr->second->initiator_app_path = record_msg_info->initiator_app_path;

            for (auto flow_info_list_it = it->second.begin(); flow_info_list_it != it->second.end(); flow_info_list_it++) {
                flowid_msgpassing_found_itr->second->network_assisted_destination_idx_list.push_back(std::get<0>(*flow_info_list_it));
                flowid_msgpassing_found_itr->second->network_assisted_dst_gpu_idx_list.push_back(std::get<1>(*flow_info_list_it));
                flowid_msgpassing_found_itr->second->network_assisted_flow_ids_list.push_back(std::get<2>(*flow_info_list_it));
            }
            flowid_msgpassing_found_itr->second->dst_tor_idx = it->first;

            // cidr AGG
            auto cird_tor_info_itr = cidr_tor_group_ids.find(it->first);
            if (cird_tor_info_itr == cidr_tor_group_ids.end() && it->first != src_tor_idx) {
                std::cout << "src tor: " << src_tor_idx << endl;
                std::cout << "dst tor: " << it->first << endl;
                throw cRuntimeError("No cidr group found for Tor and dst_tor != src_tor");
            }
            if (cird_tor_info_itr != cidr_tor_group_ids.end()) {
                flowid_msgpassing_found_itr->second->agg_cidr_group_id = cird_tor_info_itr->second;
                auto cidr_group_cnt_itr = agg_cidr_group_member_count_map.find(cird_tor_info_itr->second);
                if (cidr_group_cnt_itr == agg_cidr_group_member_count_map.end())
                    throw cRuntimeError("No cidr group cnt found for cidr group!");
                flowid_msgpassing_found_itr->second->agg_cidr_group_member_count = cidr_group_cnt_itr->second;
                EV << "Tor " << it->first << " is assigned to CIDR group " <<
                        flowid_msgpassing_found_itr->second->agg_cidr_group_id << " and group cnt is " <<
                        flowid_msgpassing_found_itr->second->agg_cidr_group_member_count << endl;
            }

            // core cidr
            if (initiator_app->partial_mcast_use_cidr_core) {
                auto cird_pod_info_itr = agg_cidr_id_to_core_cidr_id.find(flowid_msgpassing_found_itr->second->agg_cidr_group_id);
                if (cird_pod_info_itr == agg_cidr_id_to_core_cidr_id.end() &&
                        pod_idx != src_pod_idx) {
                    std::cout << "agg_cidr_group_id: " << flowid_msgpassing_found_itr->second->agg_cidr_group_id << endl;
                    std::cout << "src pod: " << src_pod_idx << endl;
                    std::cout << "dst pod: " << pod_idx << endl;
                    throw cRuntimeError("No cidr group found for pod and dst_pod != src_pod");
                }
                if (cird_pod_info_itr != agg_cidr_id_to_core_cidr_id.end()) {
                    flowid_msgpassing_found_itr->second->core_cidr_group_id = cird_pod_info_itr->second;
                    auto cidr_group_cnt_itr = core_cidr_group_member_count_map.find(cird_pod_info_itr->second);
                    if (cidr_group_cnt_itr == core_cidr_group_member_count_map.end())
                        throw cRuntimeError("No cidr group cnt found for core cidr group!");
                    flowid_msgpassing_found_itr->second->core_cidr_group_member_count = cidr_group_cnt_itr->second;
                    EV << "Pod " << pod_idx << " is assigned to CIDR group " <<
                            flowid_msgpassing_found_itr->second->core_cidr_group_id << " and group cnt is " <<
                            flowid_msgpassing_found_itr->second->core_cidr_group_member_count << endl;
                }
            }

            if (flowid_msgpassing_found_itr->second->network_assisted_destination_idx_list.size() == 0 ||
                    flowid_msgpassing_found_itr->second->network_assisted_dst_gpu_idx_list.size() == 0)
                throw cRuntimeError("Invalid network assited list size!");

            // check if we already have a socket to this destination
            int candidate_server_idx = (*flowid_msgpassing_found_itr->second->network_assisted_destination_idx_list.begin());
            int candidate_gpu_idx = (*flowid_msgpassing_found_itr->second->network_assisted_dst_gpu_idx_list.begin());

            auto dst_tor_found_itr = tor_to_candidate_server_idx_map.find(it->first);
            if (dst_tor_found_itr == tor_to_candidate_server_idx_map.end()) {
                tor_to_candidate_server_idx_map.insert(
                        std::make_pair(it->first, candidate_server_idx));
                tor_to_candidate_gpu_idx_map.insert(
                        std::make_pair(it->first, candidate_gpu_idx));
            } else {
                candidate_server_idx = dst_tor_found_itr->second;
                candidate_gpu_idx = tor_to_candidate_gpu_idx_map.find(it->first)->second;
            }

            EV << "candidate_server_idx: " << candidate_server_idx <<
                    ", candidate_gpu_idx: " << candidate_gpu_idx << endl;

            EV << "Pod idx: " << pod_idx << ", src pod idx: " << src_pod_idx << endl;

            EV << "Sending message right away" << endl;

            flowid_msgpassing_found_itr->second->dst_gpu = candidate_gpu_idx;

            init_socket(candidate_server_idx, candidate_gpu_idx);
            pass_msg(candidate_server_idx, flowid_msgpassing_found_itr->second);

            if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                    flowid_msgpassing_found_itr->second->original_flow_terminated) {
                flowid_msgpassing_found_itr->second->responsibility.clear();
                flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
                flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
                flowid_msgpassing_found_itr->second->network_assisted_destination_idx_list.clear();
                flowid_msgpassing_found_itr->second->network_assisted_dst_gpu_idx_list.clear();
                flowid_msgpassing_found_itr->second->network_assisted_flow_ids_list.clear();
                flowid_msgpassing_found_itr->second->lists_of_lists_of_jumps.clear();
                flowid_msgpassing_found_itr->second->lists_of_lists_of_ports.clear();
                gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
                delete flowid_msgpassing_found_itr->second;
                flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
            }
        }
    } else {
        EV << "Boradcasting similar to OptiReduce..." << endl;
        std::pair<unsigned long, int> query_gpu_idx_pair = std::make_pair(query_id, curr_gpu);
        auto msg_record_found = optireduce_query_gpu_msg_record.find(query_gpu_idx_pair);
        if (msg_record_found == optireduce_query_gpu_msg_record.end()) {
            std::list<BCastCustomCollectiveMsgPasserInfo*> tmp_list;
            optireduce_query_gpu_msg_record.insert(std::make_pair(query_gpu_idx_pair, tmp_list));
            msg_record_found = optireduce_query_gpu_msg_record.find(query_gpu_idx_pair);
        }

        // apply round robin
        for (int i = element_idx+1; i < list_size; i++) {
            int server_idx = dst_server_idx_list[i];
            int gpu_idx = gpu_idx_list[i];
            unsigned long flow_id = flow_id_list[i];

            std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, flow_id);
            auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
                auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(flow_id, flow_size,
                        record_msg_info->total_flow_size, query_id, curr_gpu);
                passing_msg->is_new_flow = true;
                passing_msg->dst_gpu = gpu_idx;
                passing_msg->num_dsts = 0;

                flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
                flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            } else {
                throw cRuntimeError("This should not happen with optireduce since we have not further chunking!");
//                flowid_msgpassing_found_itr->second->is_new_flow = false;
//                flowid_msgpassing_found_itr->second->msg_size += flow_size;
            }
            flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;

            flowid_msgpassing_found_itr->second->ring_end_server_idx = record_msg_info->ring_end_server_idx;
            flowid_msgpassing_found_itr->second->ring_end_gpu_idx = record_msg_info->ring_end_gpu_idx;

            flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
            flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

            flowid_msgpassing_found_itr->second->optireduce_dst_server = server_idx;
            flowid_msgpassing_found_itr->second->optireduce_dst_gpu = gpu_idx;

            flowid_msgpassing_found_itr->second->optireduce_in_reduction_phase = false;
            flowid_msgpassing_found_itr->second->initiator_app_path = record_msg_info->initiator_app_path;

            flowid_msgpassing_found_itr->second->check_metrics();

            msg_record_found->second.push_back(flowid_msgpassing_found_itr->second);
        }

        for (int i = 0; i < element_idx; i++) {
            int server_idx = dst_server_idx_list[i];
            int gpu_idx = gpu_idx_list[i];
            unsigned long flow_id = flow_id_list[i];

            std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, flow_id);
            auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
                auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(flow_id, flow_size,
                        record_msg_info->total_flow_size, query_id, curr_gpu);
                passing_msg->is_new_flow = true;
                passing_msg->dst_gpu = gpu_idx;
                passing_msg->num_dsts = 0;

                flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
                flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
            } else {
                flowid_msgpassing_found_itr->second->is_new_flow = false;
                flowid_msgpassing_found_itr->second->msg_size += flow_size;
            }
            flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;

            flowid_msgpassing_found_itr->second->ring_end_server_idx = record_msg_info->ring_end_server_idx;
            flowid_msgpassing_found_itr->second->ring_end_gpu_idx = record_msg_info->ring_end_gpu_idx;

            flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
            flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

            flowid_msgpassing_found_itr->second->optireduce_dst_server = server_idx;
            flowid_msgpassing_found_itr->second->optireduce_dst_gpu = gpu_idx;

            flowid_msgpassing_found_itr->second->optireduce_in_reduction_phase = false;
            flowid_msgpassing_found_itr->second->initiator_app_path = record_msg_info->initiator_app_path;

            flowid_msgpassing_found_itr->second->check_metrics();

            msg_record_found->second.push_back(flowid_msgpassing_found_itr->second);
        }

        optireduce_send_next(query_id, curr_gpu);
    }
}

void UDPLongBroadcastPasserApp::pass_bcast_multiwrite(Packet* pkt, GenericAppMsg* payload) {
    /*
     * When the current node is just a relay and we should just pass
     */
    EV << "UDPLongBroadcastPasserApp::pass_bcast_multiwrite_relay()" << endl;


    std::set<int> dst_server_set;
    // here all the responsibilites that are dst servers remain untouched
    // since we are in a relay node not a dst node
    // otherwise, this function wouldn't have been called!
    int num_dst_servers = (payload->getResponsibility_listArraySize());
    int dst_server_idx_list[num_dst_servers];
    int gpu_idx_list[num_dst_servers];
    unsigned long flow_id_list[num_dst_servers];
    for (int k = 0; k < payload->getResponsibility_listArraySize(); k++){
        // in dst_server_idx there can be this server's index, so filter it out
        int m_dst_server_idx = payload->getResponsibility_list(k);

        if (m_dst_server_idx == parent_index)
            throw cRuntimeError("Relay node must not be among dst node list!");

        if (dst_server_set.find(m_dst_server_idx) != dst_server_set.end())
            throw cRuntimeError("At passer, servers must be unique for multiwrite!");

        dst_server_set.insert(m_dst_server_idx);
        dst_server_idx_list[k] = m_dst_server_idx;
        gpu_idx_list[k] = payload->getResponsibility_gpu_list(k);
        flow_id_list[k] = payload->getResponsibility_flow_id_list(k);
    }

    int src_leaf_idx = int(parent_index * 1.0 / num_servers_under_each_leaf);
    int src_pod_idx = 0;
    if (use_fattree) {
        src_pod_idx = int(parent_index * 1.0 / (num_servers_under_each_leaf * fattree_k / 2.0));
    }

    // server idx to which we should connect to --> list of <dst servers under this candidate, dst gpus, flow ids>
    std::map<int, std::list<std::tuple<int, int, unsigned long>>> candidate_server_to_dst_info;

    for (int i = 0; i < num_dst_servers; i++) {
        if (dst_server_idx_list[i] == parent_index)
            throw cRuntimeError("parent_index must have been removed already!");

        int dst_leaf_idx = int(dst_server_idx_list[i] * 1.0 / num_servers_under_each_leaf);
        int dst_pod_idx = 0;
        if (use_fattree) {
            dst_pod_idx = int(dst_server_idx_list[i] * 1.0 /
                    (num_servers_under_each_leaf * fattree_k / 2.0));
        }

        int candidate_server;
        if (src_pod_idx != dst_pod_idx) {
            EV << "source and dst do not share a pod so we shoudl select a random candidate under the pod" << std::endl;
            int pod_start_server = dst_pod_idx * int(num_servers_under_each_leaf * fattree_k / 2.0);
            // end is not in the range [start, end)
            int pod_end_server = pod_start_server + int(num_servers_under_each_leaf * fattree_k / 2.0);
            // this ensures that all the servers under the same pod go to the same candidate
            std::string hash_input = getFullName();
            hash_input += std::to_string(dst_pod_idx);
            candidate_server = pod_start_server +
                    (std::hash<std::string>{}(hash_input) %
                    int(num_servers_under_each_leaf * fattree_k / 2.0));
        } else if (src_leaf_idx != dst_leaf_idx) {
            EV << "source and dst share the pod but not the leaf, so select a random candidate under the leaf" << std::endl;
            int leaf_start_server = dst_leaf_idx * num_servers_under_each_leaf;
            int leaf_end_server = leaf_start_server + num_servers_under_each_leaf;
            // this ensures that all the servers under the same leaf go to the same candidate
            std::string hash_input = getFullName();
            hash_input += std::to_string(dst_leaf_idx);
            candidate_server = leaf_start_server +
                    (std::hash<std::string>{}(hash_input) % num_servers_under_each_leaf);
        } else {
            EV << "source and dst are under the same leaf. send it directly to the dst" << std::endl;
            candidate_server = dst_server_idx_list[i];
        }

        EV << "Chosen candidate server: " << candidate_server << std::endl;
        if (candidate_server_to_dst_info.find(candidate_server) ==
                candidate_server_to_dst_info.end()) {
            std::list<std::tuple<int, int, unsigned long>> tmp_list;
            candidate_server_to_dst_info[candidate_server] = tmp_list;
        }

        EV << "candidate: " << candidate_server << " --> " <<
                "server: " << dst_server_idx_list[i] <<
                ", gpu: " << gpu_idx_list[i] <<
                ", flow id: " << flow_id_list[i] << std::endl;

        candidate_server_to_dst_info[candidate_server].push_back(
                std::make_tuple(dst_server_idx_list[i], gpu_idx_list[i], flow_id_list[i]));
    }

    for (auto candidate_itr = candidate_server_to_dst_info.begin();
            candidate_itr != candidate_server_to_dst_info.end();
            candidate_itr++) {
        int server_idx;
        int gpu_idx;
        unsigned long flow_id;

        if (candidate_itr->second.size() == 1) {
            auto m_tuple = (*candidate_itr->second.begin());
            server_idx = std::get<0>(m_tuple);
            gpu_idx = std::get<1>(m_tuple);
            flow_id = std::get<2>(m_tuple);
        } else if (candidate_itr->second.size() > 1) {
            auto m_tuple = (*candidate_itr->second.begin());
            server_idx = candidate_itr->first;
            // if the candidate is already in the list, use the appropriate gpu and flowid
            bool info_found = false;
            for (auto tmp_it : candidate_itr->second) {
                if (server_idx == std::get<0>(tmp_it)) {
                    gpu_idx = std::get<1>(tmp_it);
                    flow_id = std::get<2>(tmp_it);
                    EV << "Server is part of dsts, using gpu: " << gpu_idx <<
                            ", and flow_id: " << flow_id << std::endl;
                    info_found = true;
                    break;
                }
            }
            if (!info_found) {
                gpu_idx = std::get<1>(m_tuple);
                flow_id = std::get<2>(m_tuple);
                EV << "Server is NOT part of dsts, using gpu: " << gpu_idx <<
                        ", and flow_id: " << flow_id << std::endl;
            }
        } else
            throw cRuntimeError("We have a candidate assigned to empty list!?");

        EV << "Chunk length is: " << payload->getChunkLength() << std::endl;
        auto new_msg = new BCastCustomCollectiveMsgPasserInfo(flow_id,
                int(payload->getChunkLength().get() / 8.0),
                int(payload->getTotal_flow_size().get() / 8.0),
                payload->getQuery_id(),
                payload->getDst_gpu_idx());
        for (auto itr = candidate_itr->second.begin();
                itr != candidate_itr->second.end();
                itr++) {
            auto m_tuple = (*itr);
            int tmp_server_idx = std::get<0>(m_tuple);
            int tmp_gpu_idx = std::get<1>(m_tuple);
            int tmp_flow_id = std::get<2>(m_tuple);
            new_msg->responsibility.push_back(tmp_server_idx);
            new_msg->responsibility_gpu.push_back(tmp_gpu_idx);
            new_msg->responsiblity_flow_ids.push_back(tmp_flow_id);
        }
        new_msg->num_dsts = new_msg->responsibility.size();
        new_msg->is_new_flow = true;
        new_msg->dst_gpu = gpu_idx;

        new_msg->m_collective_type = payload->getCollective_type();
        new_msg->m_collective_alg = BCAST_MULTIWRITE;

        if (server_idx == parent_index) {
            throw cRuntimeError("transmission to self does not work with multiwrite!");
        } else {
            EV << "passer: Sending to server " << server_idx << ", gpu " << gpu_idx << std::endl;
            init_socket(server_idx, gpu_idx);
            pass_msg(server_idx, new_msg);
        }
    }

}

void UDPLongBroadcastPasserApp::pass_bcast_multiwrite(BCastCustomCollectiveMsgPasserInfo* record_msg_info) {
    /*
     * When the current node is a destination
     */
    EV << "UDPLongBroadcastPasserApp::pass_bcast_multiwrite_dst()" << endl;

    // here, we are at one of the dsts so responsibility size is reduced by 1
    int num_dst_servers = (record_msg_info->responsibility.size() - 1);
    if (num_dst_servers == 0) {
        EV << "There is no one to pass the message to" << endl;
        return;
    }

    unsigned long query_id = record_msg_info->query_id;
    int curr_gpu = record_msg_info->src_gpu;
    unsigned long flow_id = 0;

    std::set<int> dst_server_set;
    int dst_server_idx_list[num_dst_servers];
    int gpu_idx_list[num_dst_servers];
    unsigned long flow_id_list[num_dst_servers];
    int idx = 0;
    auto responsibility_gpu_itr = record_msg_info->responsibility_gpu.begin();
    auto responsibility_flowid_itr = record_msg_info->responsiblity_flow_ids.begin();
    for (auto responsibility_dst_server : record_msg_info->responsibility){
        // in dst_server_idx there can be this server's index, so filter it out
        int m_dst_server_idx = responsibility_dst_server;

        if (m_dst_server_idx == parent_index) {

            if (curr_gpu != (*responsibility_gpu_itr)) {
                std::cout << "server: " << parent_index << std::endl;
                std::cout << (*responsibility_gpu_itr) << std::endl;
                std::cout << curr_gpu << std::endl;
                throw cRuntimeError("The current gpu of record_msg_info does not match our expected gpu!!");
            }

            if (record_msg_info->flow_id != (*responsibility_flowid_itr)) {
                std::cout << (*responsibility_flowid_itr) << std::endl;
                std::cout << record_msg_info->flow_id << std::endl;
                throw cRuntimeError("The flow id of record_msg_info does not match our expected flow id!");
            }

            responsibility_gpu_itr++;
            responsibility_flowid_itr++;
            continue;
        }

        if (dst_server_set.find(m_dst_server_idx) != dst_server_set.end())
            throw cRuntimeError("At passer, servers must be unique for multiwrite!");

        dst_server_set.insert(m_dst_server_idx);
        dst_server_idx_list[idx] = m_dst_server_idx;
        gpu_idx_list[idx] = (*responsibility_gpu_itr);
        flow_id_list[idx] = (*responsibility_flowid_itr);
        idx++;
        responsibility_gpu_itr++;
        responsibility_flowid_itr++;
    }

    emit(flowPassedSignal, record_msg_info->flow_id);
    unsigned long flow_size = record_msg_info->msg_size;

    EV << "message size is: " << flow_size << std::endl;

    int src_leaf_idx = int(parent_index * 1.0 / num_servers_under_each_leaf);
    int src_pod_idx = 0;
    if (use_fattree) {
        src_pod_idx = int(parent_index * 1.0 / (num_servers_under_each_leaf * fattree_k / 2.0));
    }

    EV << "src: " << parent_index << ", src_leaf: " << src_leaf_idx <<
                    ", src_pod: " << src_pod_idx << std::endl;

    // server idx to which we should connect to --> list of <dst servers under this candidate, dst gpus, flow ids>
    std::map<int, std::list<std::tuple<int, int, unsigned long>>> candidate_server_to_dst_info;

    for (int i = 0; i < num_dst_servers; i++) {
        if (dst_server_idx_list[i] == parent_index)
            throw cRuntimeError("parent_index must have been removed already!");

        int dst_leaf_idx = int(dst_server_idx_list[i] * 1.0 / num_servers_under_each_leaf);
        int dst_pod_idx = 0;
        if (use_fattree) {
            dst_pod_idx = int(dst_server_idx_list[i] * 1.0 /
                    (num_servers_under_each_leaf * fattree_k / 2.0));
        }

        EV << "dst: " << dst_server_idx_list[i] << ", dst_leaf: " << dst_leaf_idx <<
                ", dst_pod: " << dst_pod_idx << std::endl;

        int candidate_server;
        if (src_pod_idx != dst_pod_idx) {
            EV << "source and dst do not share a pod so we shoudl select a random candidate under the pod" << std::endl;
            int pod_start_server = dst_pod_idx * int(num_servers_under_each_leaf * fattree_k / 2.0);
            // end is not in the range [start, end)
            int pod_end_server = pod_start_server + int(num_servers_under_each_leaf * fattree_k / 2.0);
            // this ensures that all the servers under the same pod go to the same candidate
            std::string hash_input = getFullName();
            hash_input += std::to_string(dst_pod_idx);
            candidate_server = pod_start_server +
                    (std::hash<std::string>{}(hash_input) %
                    int(num_servers_under_each_leaf * fattree_k / 2.0));
        } else if (src_leaf_idx != dst_leaf_idx) {
            EV << "source and dst share the pod but not the leaf, so select a random candidate under the leaf" << std::endl;
            int leaf_start_server = dst_leaf_idx * num_servers_under_each_leaf;
            int leaf_end_server = leaf_start_server + num_servers_under_each_leaf;
            // this ensures that all the servers under the same leaf go to the same candidate
            std::string hash_input = getFullName();
            hash_input += std::to_string(dst_leaf_idx);
            candidate_server = leaf_start_server +
                    (std::hash<std::string>{}(hash_input) % num_servers_under_each_leaf);
        } else {
            EV << "source and dst are under the same leaf. send it directly to the dst" << std::endl;
            candidate_server = dst_server_idx_list[i];
        }

        EV << "Chosen candidate server: " << candidate_server << std::endl;
        if (candidate_server_to_dst_info.find(candidate_server) ==
                candidate_server_to_dst_info.end()) {
            std::list<std::tuple<int, int, unsigned long>> tmp_list;
            candidate_server_to_dst_info[candidate_server] = tmp_list;
        }

        EV << "candidate: " << candidate_server << " --> " <<
                "server: " << dst_server_idx_list[i] <<
                ", gpu: " << gpu_idx_list[i] <<
                ", flow id: " << flow_id_list[i] << std::endl;

        candidate_server_to_dst_info[candidate_server].push_back(
                std::make_tuple(dst_server_idx_list[i], gpu_idx_list[i], flow_id_list[i]));
    }

    for (auto candidate_itr = candidate_server_to_dst_info.begin();
            candidate_itr != candidate_server_to_dst_info.end();
            candidate_itr++) {
        int server_idx;
        int gpu_idx;
        unsigned long flow_id;

        if (candidate_itr->second.size() == 1) {
            EV << "Server " << candidate_itr->first << " is only candidate for itself!" << std::endl;
            auto m_tuple = (*candidate_itr->second.begin());
            server_idx = std::get<0>(m_tuple);
            gpu_idx = std::get<1>(m_tuple);
            flow_id = std::get<2>(m_tuple);
        } else if (candidate_itr->second.size() > 1) {
            auto m_tuple = (*candidate_itr->second.begin());
            server_idx = candidate_itr->first;
            // if the candidate is already in the list, use the appropriate gpu and flowid
            bool info_found = false;
            for (auto tmp_it : candidate_itr->second) {
                if (server_idx == std::get<0>(tmp_it)) {
                    gpu_idx = std::get<1>(tmp_it);
                    flow_id = std::get<2>(tmp_it);
                    EV << "Server is part of dsts, using gpu: " << gpu_idx <<
                            ", and flow_id: " << flow_id << std::endl;
                    info_found = true;
                    break;
                }
            }
            if (!info_found) {
                gpu_idx = std::get<1>(m_tuple);
                flow_id = std::get<2>(m_tuple);
                EV << "Server is NOT part of dsts, using gpu: " << gpu_idx <<
                        ", and flow_id: " << flow_id << std::endl;
            }
        } else
            throw cRuntimeError("We have a candidate assigned to empty list!?");

        EV << "Chunk length is: " << flow_size << std::endl;
        auto new_msg = new BCastCustomCollectiveMsgPasserInfo(flow_id,
                flow_size,
                record_msg_info->total_flow_size,
                query_id,
                curr_gpu);
        for (auto itr = candidate_itr->second.begin();
                itr != candidate_itr->second.end();
                itr++) {
            auto m_tuple = (*itr);
            int tmp_server_idx = std::get<0>(m_tuple);
            int tmp_gpu_idx = std::get<1>(m_tuple);
            int tmp_flow_id = std::get<2>(m_tuple);
            new_msg->responsibility.push_back(tmp_server_idx);
            new_msg->responsibility_gpu.push_back(tmp_gpu_idx);
            new_msg->responsiblity_flow_ids.push_back(tmp_flow_id);
        }
        new_msg->num_dsts = new_msg->responsibility.size();
        new_msg->is_new_flow = true;
        new_msg->dst_gpu = gpu_idx;

        new_msg->m_collective_type = record_msg_info->m_collective_type;
        new_msg->m_collective_alg = BCAST_MULTIWRITE;

        if (server_idx == parent_index) {
            throw cRuntimeError("transmission to self does not work with multiwrite!");
        } else {
            EV << "passer: Sending to server " << server_idx << ", gpu " << gpu_idx << std::endl;
            init_socket(server_idx, gpu_idx);
            pass_msg(server_idx, new_msg);
        }
    }
}

void UDPLongBroadcastPasserApp::pass_bcast_ring(BCastCustomCollectiveMsgPasserInfo* record_msg_info) {
    EV << "UDPLongBroadcastPasserApp::pass_bcast_ring()" << endl;

    unsigned long query_id = record_msg_info->query_id;
    unsigned long num_dst_servers_per_query = record_msg_info->responsibility.size();
    int curr_gpu = record_msg_info->src_gpu;

    if (num_dst_servers_per_query == 0) {
        EV << "There is no one to pass the message to" << endl;
        return;
    }

    if (record_msg_info->ring_end_server_idx > -1) {
        // check if you should enter the all gather phase of all reduce
        if (parent_index == record_msg_info->ring_end_server_idx &&
                curr_gpu == record_msg_info->ring_end_gpu_idx) {
            EV << "Starting the all gather phase of all reduce: " << record_msg_info->responsibility.size() << endl;
            record_msg_info->ring_end_server_idx = -1;
            record_msg_info->ring_end_gpu_idx = -1;
            // only change flow id if we're switching phases (like from reduce-scatter to allgather in allreduce
            record_msg_info->responsiblity_flow_ids.pop_front();
        }
    }

    emit(flowPassedSignal, record_msg_info->flow_id);


    unsigned long flow_size = record_msg_info->msg_size;

    int server_idx = record_msg_info->responsibility.front();
    record_msg_info->responsibility.pop_front();
    int gpu_idx = record_msg_info->responsibility_gpu.front();
    record_msg_info->responsibility_gpu.pop_front();
    unsigned long flow_id = record_msg_info->responsiblity_flow_ids.front();
//    record_msg_info->responsiblity_flow_ids.pop_front();
//    std::cout << getFullPath() << " -- flow id is " << flow_id << ", curr gpu: " << curr_gpu << endl;

    // only change flow id if we're switching phases (like from reduce-scatter to allgather in allreduce
    if (record_msg_info->ring_end_server_idx >= 0) {
        // we will still need to loop back in the future
        EV << "We're in the reduce scatter phase. We will still need to loop back in the future." << endl;
        record_msg_info->responsibility.push_back(server_idx);
        record_msg_info->responsibility_gpu.push_back(gpu_idx);
//        record_msg_info->responsiblity_flow_ids.push_back(flow_id);
    }

    std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, flow_id);
    auto flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
    if (flowid_msgpassing_found_itr == flowid_passingmsg_mapper.end()) {
        auto passing_msg = new BCastCustomCollectiveMsgPasserInfo(flow_id, flow_size,
                record_msg_info->total_flow_size, query_id, curr_gpu);
        passing_msg->is_new_flow = true;
        passing_msg->dst_gpu = gpu_idx;
        for (auto itr = record_msg_info->responsibility.begin(); itr != record_msg_info->responsibility.end(); itr++)
            passing_msg->responsibility.push_back(*itr);
        for (auto itr = record_msg_info->responsibility_gpu.begin();
                itr != record_msg_info->responsibility_gpu.end(); itr++)
            passing_msg->responsibility_gpu.push_back(*itr);
        for (auto itr = record_msg_info->responsiblity_flow_ids.begin();
                itr != record_msg_info->responsiblity_flow_ids.end(); itr++)
            passing_msg->responsiblity_flow_ids.push_back(*itr);
        passing_msg->num_dsts = passing_msg->responsibility.size();

        flowid_passingmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, passing_msg));
        flowid_msgpassing_found_itr = flowid_passingmsg_mapper.find(gpu_idx_flow_id_tuple);
    } else {
        flowid_msgpassing_found_itr->second->is_new_flow = false;
        flowid_msgpassing_found_itr->second->msg_size += flow_size;
    }
    flowid_msgpassing_found_itr->second->original_flow_terminated = record_msg_info->original_flow_terminated;
    flowid_msgpassing_found_itr->second->ring_end_server_idx = record_msg_info->ring_end_server_idx;
    flowid_msgpassing_found_itr->second->ring_end_gpu_idx = record_msg_info->ring_end_gpu_idx;

    flowid_msgpassing_found_itr->second->m_collective_type = record_msg_info->m_collective_type;
    flowid_msgpassing_found_itr->second->m_collective_alg = record_msg_info->m_collective_alg;

    flowid_msgpassing_found_itr->second->check_metrics();

    // check if the gpu is inside the same host
    if (server_idx == parent_index) {
        EV << "Dst GPU is in the same host as Src GPU" << endl;
        // next gpu (dst gpu) again has processing delay
        simtime_t processing_delay = get_processing_delay(flowid_msgpassing_found_itr->second->dst_gpu,
                flowid_msgpassing_found_itr->second->m_collective_type,
                flowid_msgpassing_found_itr->second->m_collective_alg);
        simtime_t pass_delay = get_intra_host_pass_delay(flow_size);

        schedule_finish_if_required(flowid_msgpassing_found_itr->second, processing_delay);
        schedule_pass(flowid_msgpassing_found_itr->second, processing_delay + pass_delay);

        // just erase it becasue the message is completely passed but don't delete it because
        // message will be delivered somewhere else
        flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
    } else {
        // In case we should reach another host
        init_socket(server_idx, gpu_idx);
        /*
         * If flowid_msgpassing_found_itr->second->msg_size != flow_size,
         * then we know that the message had some bytes pending generate signal from mac
         * which means that we should only add to those bytes and wait for the signal
         */
        if (flowid_msgpassing_found_itr->second->msg_size == flow_size)
            pass_msg(server_idx, flowid_msgpassing_found_itr->second);

        if (flowid_msgpassing_found_itr->second->msg_size == 0 &&
                flowid_msgpassing_found_itr->second->original_flow_terminated) {
            flowid_msgpassing_found_itr->second->responsibility.clear();
            flowid_msgpassing_found_itr->second->responsibility_gpu.clear();
            flowid_msgpassing_found_itr->second->responsiblity_flow_ids.clear();
            gpu_idx_flow_id_to_seq_num.erase(gpu_idx_flow_id_tuple);
            delete flowid_msgpassing_found_itr->second;
            flowid_passingmsg_mapper.erase(flowid_msgpassing_found_itr);
        }
    }
}

void UDPLongBroadcastPasserApp::handleMessageWhenUp(cMessage *msg)
{
    std::string msg_name = msg->getName();
    unsigned long storage_loc;
    BCastCustomCollectiveMsgPasserInfo* msg_info;
    bool should_erase;

    //for binary tree
    std::unordered_map<int, std::unordered_map<unsigned long, unsigned long>>::iterator query_id_found_itr;
    std::unordered_map<unsigned long, unsigned long>::iterator src_gpu_found_itr;
    std::unordered_map<unsigned long, unsigned long> src_gpu_map;
    bool should_pass = true;

    if (msg_name.compare("custom_delay") == 0) {
        EV << "self message received: kind: " << msg->getKind() << endl;
        switch (msg->getKind()) {
            case PASS:
                storage_loc = msg->par("storage_loc").longValue();
                msg_info = pop_from_storage(storage_loc);
                EV << "src gpu: " << msg_info->src_gpu <<
                        ", dst gpu: " << msg_info->dst_gpu << endl;
                if (msg_info->src_gpu < 0 || msg_info->dst_gpu < 0)
                    throw cRuntimeError("A message rcvd with msg_info->src_gpu < 0 || msg_info->dst_gpu < 0");
                msg_info->src_gpu = msg_info->dst_gpu;
                msg_info->dst_gpu = -1;
                // msg might get deleted but does not get erased from flowid_passingmsg_mapper

                switch (msg_info->m_collective_alg) {
                    case BCAST_SINGLE_SENDER:
                    case BCAST_NETWORK_ASSISTED:
                    case BCAST_INA:
                    case BCAST_PARTIAL_MULTICAST:
                        break;
                    case BCAST_BINOMIAL_TREE:
                        pass_bcast_binomial_tree(msg_info);
                        break;
                    case BCAST_RING:
                        pass_bcast_ring(msg_info);
                        break;
                    case BCAST_MULTIWRITE:
                        pass_bcast_multiwrite(msg_info);
                        break;
                    case BCAST_OPTIREDUCE:
                        if (msg_info->optireduce_in_reduction_phase)
                            pass_bcast_optireduce(msg_info);
                        break;
                    case BCAST_BINARY_TREE:
                        // every node only passes a message from a query id once in all-reduce
                        if (msg_info->m_collective_type==ALL_REDUCE && msg_info->tree_traversal_dir == TREE_UP) {

                            std::tuple<int, unsigned long> gpu_idx_query_id_tuple = std::make_tuple(msg_info->src_gpu,
                                    msg_info->query_id);
                            auto candidate_flow_id_found_itr = query_sent_up.find(gpu_idx_query_id_tuple);
                            if (candidate_flow_id_found_itr != query_sent_up.end()) {
                                EV << "A flow with id=" << candidate_flow_id_found_itr->second <<
                                        " is already passed up for query_id=" << msg_info->query_id <<
                                        " and gpu_idx=" << msg_info->src_gpu << endl;
                                if (msg_info->flow_id == candidate_flow_id_found_itr->second)
                                    should_pass = true;
                                else
                                    should_pass = false;
                            } else {
                                int num_children = get_num_children(msg_info);
                                EV << "No candidate flow has yet been chosen for" <<
                                    " query_id=" << msg_info->query_id <<
                                    " and gpu_idx=" << msg_info->src_gpu <<
                                    ", expected number of children: " << num_children << endl;
                                auto observed_flows_set_found_itr = observed_flow_ids_sets.find(gpu_idx_query_id_tuple);
                                if (observed_flows_set_found_itr == observed_flow_ids_sets.end()) {
                                    EV << "No flows are observed so far!" << endl;
                                    std::unordered_set<unsigned long> observed_flows_set;
                                    observed_flow_ids_sets.insert(std::make_pair(gpu_idx_query_id_tuple, observed_flows_set));
                                    observed_flows_set_found_itr = observed_flow_ids_sets.find(gpu_idx_query_id_tuple);
                                }
                                observed_flows_set_found_itr->second.insert(msg_info->flow_id);
                                if (observed_flows_set_found_itr->second.size() > num_children)
                                    throw cRuntimeError("How did we see more flow ids than the number of children!");
                                if (observed_flows_set_found_itr->second.size() == num_children) {
                                    EV << "The data from the last child is received!" << endl;
                                    query_sent_up.insert(std::make_pair(gpu_idx_query_id_tuple, msg_info->flow_id));
                                    should_pass = true;
                                } else {
                                    should_pass = false;
                                }

                            }
                        }

                        if (should_pass) {
//                            std::cout << simTime() << ": " << getFullPath() << " -- gpu: " << msg_info->src_gpu << ": trying to pass msg size of " << msg_info->msg_size << endl;
                            pass_bcast_binary_tree(msg_info);
                        }
                        break;
                    default:
                        throw cRuntimeError("Unsupported collective for PASS timer");
                }
                msg_info->responsibility.clear();
                msg_info->responsibility_gpu.clear();
                msg_info->responsiblity_flow_ids.clear();
                delete msg_info;
                break;
            case FINISH:
                storage_loc = msg->par("storage_loc").longValue();
                msg_info = pop_from_storage(storage_loc);
                if (!msg_info->original_flow_terminated)
                    throw cRuntimeError("How is message not terminated but finished is scheduled!");
                EV << "Passer: Flow with flow id " << msg_info->flow_id << " is completed!" << endl;
                if (msg_info->flow_id < 0 || msg_info->query_id < 0)
                    throw cRuntimeError("Invalid message info!");
                emit(flowEndedSignal, msg_info->flow_id);
                emit(flowEndedQueryIDSignal, msg_info->query_id);

                if (use_autoccl &&
                        (msg_info->m_collective_type == BROADCAST || msg_info->m_collective_type == ALL_GATHER || msg_info->m_collective_type == ALL_REDUCE) &&
                        (msg_info->m_collective_alg == BCAST_RING || msg_info->m_collective_alg == BCAST_BINARY_TREE)) {
                    unsigned long flow_size = retrieve_original_flow_size(
                            msg_info->m_collective_type,
                            msg_info->m_collective_alg,
                            msg_info->total_flow_size);

//                    std::cout << "AUTOCCL: Reporting completion of a flow of size " <<
//                            flow_size << " to the initiator." << std::endl;
//                    std::cout << "Collective: " << msg_info->m_collective_type << ", algorithm: " <<
//                            msg_info->m_collective_alg << std::endl;

                    UDPLongAllgatherInitiatorApp::autoccl_inject_flow_finish(use_autoccl,
                            autoccl_allreduce_only,
                            msg_info->m_collective_type,
                            msg_info->m_collective_alg,
                            msg_info->query_id,
                            flow_size);
                }

                delete msg_info;
                break;
            default:
                throw cRuntimeError("Msg with unknown kind received!");
                break;
        }
        delete msg;
    } else
        socket.processMessage(msg);
}

void UDPLongBroadcastPasserApp::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    EV << "UDPLongBroadcastPasserApp::socketDataArrived" << endl;
    dataArrived(packet);
}

// todo: might not need these two
void UDPLongBroadcastPasserApp::optireduce_update_rcvd_subflows() {
    throw cRuntimeError("Here for more chunking for optiReduce.");
}

bool UDPLongBroadcastPasserApp::optireduce_should_pass_subflows() {
    throw cRuntimeError("Here for more chunking for optiReduce.");
}

unsigned long UDPLongBroadcastPasserApp::insert_msg_storage(BCastCustomCollectiveMsgPasserInfo* msg_info) {
    unsigned long curr_storage_id = storage_id;
    storage.insert(std::make_pair(curr_storage_id, msg_info));
    // we first cap and then add because storage id should never be zero
    storage_id %= storage_cap;
    storage_id++;
    auto storage_id_found_itr = storage.find(storage_id);
    if (storage_id_found_itr != storage.end())
        throw cRuntimeError("next storage id is already there! How?");
    if (storage_id <= 0)
        throw cRuntimeError("How can storage id be <= 0?");
    return curr_storage_id;
}

BCastCustomCollectiveMsgPasserInfo* UDPLongBroadcastPasserApp::pop_from_storage(unsigned long storage_loc) {
    auto storage_id_found_itr = storage.find(storage_loc);
    if (storage_id_found_itr == storage.end())
        throw cRuntimeError("Item not found in storage");
    auto msg_info = storage_id_found_itr->second;
    storage.erase(storage_id_found_itr);
    return msg_info;
}

void UDPLongBroadcastPasserApp::passed_from_initiator(Packet *msg) {
    Enter_Method("passed_from_initiator");
    EV << "Passing the message to passer from initiator." << endl;
    // When passed from initiator, passer only starts processing when the message is complete as
    // the message comes from the initiator as a whole
    dataArrived(msg, true);
}

void UDPLongBroadcastPasserApp::dataArrived(Packet *msg, bool just_complete_message)
{

    EV << "UDPLongBroadcastPasserApp::dataArrived" << endl;
    EV << "SEPEHR: Message rcved: " << msg << endl;
    auto msg_dup = msg->dup();
    auto chunk = msg_dup->removeAtFront<SliceChunk>();
    std::map<std::tuple<int, unsigned long>, BCastCustomCollectiveMsgPasserInfo*>::iterator record_msg_found_itr;
    bool unused_optireduce_flow = false;    // when a flow is completed but not gonna be used anymore
    b total_length = b(0);
    while (true) {
        auto chunk_length = chunk->getLength();

        auto chunk_offset = chunk->getOffset();
        auto main_chunk = chunk->getChunk();
        Packet* temp = new Packet();
        temp->insertAtBack(main_chunk);
        auto payload = temp->popAtFront<GenericAppMsg>();
        auto total_length = payload->getTotal_flow_size();

        EV << "Removed bytes: " << payload->getRsbf_removed_bytes() << endl;
        chunk_length += B(payload->getRsbf_removed_bytes());

        if (payload->getUsing_elmo()) {
//            std::cout << "getUsing_elmo: True" << endl;
            if (payload->getUseful_data_bytes() == 0) {
                std::cout << "Useful data (B): " << payload->getUseful_data_bytes() << endl;
                throw cRuntimeError("Invalid useful data entered for elmo!");
            }
            chunk_length = B(payload->getUseful_data_bytes());
            EV << "Chunk length updated to: " << chunk_length << endl;
        }

        unsigned long flow_id = payload->getRequesterID();
        unsigned long query_id = payload->getQuery_id();
        unsigned long seq_num = payload->getSeq_num();
        int curr_gpu = payload->getDst_gpu_idx();
        int src_gpu = payload->getSrc_gpu_idx();
        int m_collective_type = payload->getCollective_type();
        int m_collective_alg_type = payload->getCollective_alg_type();
        if (m_collective_alg_type == BCAST_NETWORK_ASSISTED ||
                m_collective_alg_type == BCAST_PARTIAL_MULTICAST) {
            // get correct flow id
            for (int k = 0; k < payload->getDst_idx_listArraySize(); k++) {
                if (payload->getDst_idx_list(k) == parent_index) {
                    EV << "Changing flow id from " << flow_id;
                    flow_id = payload->getFlow_ids_list(k);
                    EV << " to " << flow_id << endl;
                    break;
                }
            }
            if (flow_id == 0)
                throw cRuntimeError("How is flow id still 0!!");
        }

        if (m_collective_alg_type == BCAST_MULTIWRITE) {

            if (payload->getResponsibility_flow_id_listArraySize() !=
                    payload->getResponsibility_gpu_listArraySize() ||
                    payload->getResponsibility_gpu_listArraySize() !=
                    payload->getResponsibility_listArraySize()) {
                throw cRuntimeError("multiwire: responsibility array sizes don't match!");
            }

            bool server_is_relay = true; // for servers that are not actually part of the collective
            for (int k = 0; k < payload->getResponsibility_listArraySize(); k++) {
                if (parent_index == payload->getResponsibility_list(k)) {
                    server_is_relay = false;
                    break;
                }
            }

            if (server_is_relay) {
                EV << "Server is relay node... no need to do anything... just pass the packet" << std::endl;
                auto payload_dup = payload->dup();
                pass_bcast_multiwrite(msg, payload_dup);
                delete payload_dup;
                delete temp;
                delete msg_dup;
                delete msg;
                return;
            }
        }

        bool in_src_sharding = payload->getIn_src_sharding();
        if (in_src_sharding) {
            // update total length to original flow size
//            std::cout << "old total length: " << total_length << endl;
            total_length = b(payload->getPartial_mcast_original_flow_size_bytes() * 8);
//            std::cout << "new total length: " << total_length << endl;
        }

        EV << "received packet src gpu: " << payload->getSrc_gpu_idx() << ", dst gpu: " << payload->getDst_gpu_idx() << endl;


//        std::cout << getFullPath() << ": " << flow_id << " -- " << curr_gpu << endl;

        std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu, flow_id);
        std::tuple<int, unsigned long> gpu_idx_query_id_tuple = std::make_tuple(curr_gpu, query_id);
        std::tuple<int, int, unsigned long> srcgpu_dstgpu_flowid_tuple = std::make_tuple(src_gpu, curr_gpu, flow_id);

        auto seq_num_set_itr = arrived_seq_nums_set.find(srcgpu_dstgpu_flowid_tuple);
        if (seq_num_set_itr == arrived_seq_nums_set.end()) {
            auto seq_num_history_found = seq_num_complete_flow_history.find(srcgpu_dstgpu_flowid_tuple);
            if (seq_num_history_found == seq_num_complete_flow_history.end()) {
                EV << "Seq num set initiated: " << seq_num << endl;
                std::set<unsigned long> tmp_set;
                tmp_set.insert(seq_num);
                arrived_seq_nums_set.insert(std::make_pair(srcgpu_dstgpu_flowid_tuple, tmp_set));
                seq_num_complete_flow_history.insert(srcgpu_dstgpu_flowid_tuple);
            } else {
                // if flow is completed and we get packets for it again...
                EV << "This flow with this seq num is completed before...Dropping" << endl;
                delete temp;
                delete msg_dup;
                delete msg;
                return;
            }
        } else {
            auto seq_num_found_itr = seq_num_set_itr->second.find(seq_num);
            if (seq_num_found_itr == seq_num_set_itr->second.end()) {
                EV << "Seq num added: " << seq_num << endl;
                seq_num_set_itr->second.insert(seq_num);
            } else {
                EV << "packet previously received with seq num " << seq_num << "... drop it!" << endl;
                delete temp;
                delete msg_dup;
                delete msg;
                return;
            }
        }

        record_msg_found_itr = flowid_receivedmsg_mapper.find(gpu_idx_flow_id_tuple);
        if (record_msg_found_itr == flowid_receivedmsg_mapper.end())
            flowid_receivedmsg_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, new BCastCustomCollectiveMsgPasserInfo()));
        record_msg_found_itr = flowid_receivedmsg_mapper.find(gpu_idx_flow_id_tuple);
        auto flow_id_found_itr = flowid_bitsrcvd_mapper.find(gpu_idx_flow_id_tuple);
        if (flow_id_found_itr == flowid_bitsrcvd_mapper.end()) {
            // this is a new flow
            flowid_bitsrcvd_mapper.insert(std::make_pair(gpu_idx_flow_id_tuple, chunk_length));
            flow_id_found_itr = flowid_bitsrcvd_mapper.find(gpu_idx_flow_id_tuple);
        } else {
            flow_id_found_itr->second += chunk_length;
        }

        EV << "Flow " << flow_id << " -- Total length: " << total_length << ", rcvd: " << flow_id_found_itr->second << endl;

        if (flow_id_found_itr->second > total_length) {
            std::cout << getFullPath() << ": gpu_idx: " << std::get<0>(flow_id_found_itr->first) << endl;
            std::cout << getFullPath() << ": flow_id: " << std::get<1>(flow_id_found_itr->first) << endl;
            std::cout << "Tree traversal: " << payload->getTree_traversal_dir() << endl;
            std::cout << "Tree search type: " << payload->getTree_search_type() << endl;
            std::cout << "flow_id_found_itr->second: " << flow_id_found_itr->second << endl;
            std::cout << "total_length: " << total_length << endl;
            throw cRuntimeError("flow_id_found_itr->second > total_length.get()");
        }

        bool is_flow_complete = false;
        if (m_collective_alg_type == BCAST_OPTIREDUCE && payload->getOptireduce_in_reduction_phase()) {
            auto completed_flows_itr = gpu_query_to_completed_flows.find(gpu_idx_query_id_tuple);
            if (completed_flows_itr == gpu_query_to_completed_flows.end()) {
                std::set<unsigned long> tmp_set;
                gpu_query_to_completed_flows.insert(std::make_pair(gpu_idx_query_id_tuple, tmp_set));
                completed_flows_itr = gpu_query_to_completed_flows.find(gpu_idx_query_id_tuple);
            }
            if (flow_id_found_itr->second == total_length) {
                completed_flows_itr->second.insert(flow_id);
                if (completed_flows_itr->second.size() > payload->getCollective_scale() - 1)
                    throw cRuntimeError("Number of completed flows surpass the scale!");
                if (completed_flows_itr->second.size() == payload->getCollective_scale() - 1) {
                    is_flow_complete = true;
                    EV << "All flows for query: " << query_id << " and gpu: " << curr_gpu << " received!" << endl;
                } else {
                    // flow is completed but is not gonna be used or passed
                    unused_optireduce_flow = true;
                    EV << completed_flows_itr->second.size() << " flows completed out of " <<
                            (payload->getCollective_scale() - 1) << endl;
                    // cosider it as complete
                    emit(flowEndedSignal, flow_id);
                    emit(flowEndedQueryIDSignal, query_id);
                }

                // remove the bits received info for this flow
                std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu,flow_id);
                auto bits_received_found = flowid_bitsrcvd_mapper.find(gpu_idx_flow_id_tuple);
                if (bits_received_found == flowid_bitsrcvd_mapper.end())
                    throw cRuntimeError("How is flow info absent in flowid_bitsrcvd_mapper");
                flowid_bitsrcvd_mapper.erase(bits_received_found);
            }
        } else {
            is_flow_complete = (flow_id_found_itr->second == total_length);

//            if (m_collective_type == ALL_REDUCE)
//                std::cout << "total length: " << total_length << ", rcvd: " << flow_id_found_itr->second << std::endl;

            if (is_flow_complete) {
                // remove the bits received info for this flow
                std::tuple<int, unsigned long> gpu_idx_flow_id_tuple = std::make_tuple(curr_gpu,flow_id);
                auto bits_received_found = flowid_bitsrcvd_mapper.find(gpu_idx_flow_id_tuple);
                if (bits_received_found == flowid_bitsrcvd_mapper.end())
                    throw cRuntimeError("How is flow info absent in flowid_bitsrcvd_mapper");
                flowid_bitsrcvd_mapper.erase(bits_received_found);
            }
        }

        if (is_flow_complete) {
            // End time should be exactly the time the packet is received even if there was a reordering
            record_msg_found_itr->second->should_pass = true;
            record_msg_found_itr->second->original_flow_terminated = true;

            // INA
            ina_query_to_flow_id.erase(query_id);
            ina_query_to_gpu_idx.erase(query_id);

            auto seq_num_set_itr = arrived_seq_nums_set.find(srcgpu_dstgpu_flowid_tuple);
            if (seq_num_set_itr != arrived_seq_nums_set.end())
                seq_num_set_itr->second.clear();
            arrived_seq_nums_set.erase(srcgpu_dstgpu_flowid_tuple);

            EV << "Data is complete." << endl;
        } else if (chunk_length + chunk_offset > total_length)
            throw cRuntimeError("chunk_length + chunk_offset > total_length");
        else if (m_collective_alg_type == BCAST_MULTIWRITE) {
            record_msg_found_itr->second->should_pass = true;
        } else
            EV << "Still waiting for more data..." << endl;

        if (record_msg_found_itr->second->total_flow_size == 0) {
            // change from bits to bytes since flow size is in bytes
            record_msg_found_itr->second->total_flow_size = (total_length.get()/8);

            record_msg_found_itr->second->flow_id = flow_id;
            record_msg_found_itr->second->query_id = query_id;
            record_msg_found_itr->second->src_gpu = payload->getSrc_gpu_idx();
            record_msg_found_itr->second->dst_gpu = payload->getDst_gpu_idx();

            record_msg_found_itr->second->ring_end_server_idx = payload->getRing_end_server_idx();
            record_msg_found_itr->second->ring_end_gpu_idx = payload->getRing_end_gpu_idx();

            record_msg_found_itr->second->tree_search_type = payload->getTree_search_type();
            record_msg_found_itr->second->tree_traversal_dir = payload->getTree_traversal_dir();

            record_msg_found_itr->second->m_collective_type = payload->getCollective_type();
            record_msg_found_itr->second->m_collective_alg = payload->getCollective_alg_type();

            record_msg_found_itr->second->optireduce_in_reduction_phase = payload->getOptireduce_in_reduction_phase();

            // replace with your own initiator app
            record_msg_found_itr->second->initiator_app_path = payload->getApp_full_path();
            std::regex pattern(R"(server\[\d+\])");
            std::string replacement = "server[" + std::to_string(parent_index) + "]";
            record_msg_found_itr->second->initiator_app_path =
                    std::regex_replace(record_msg_found_itr->second->initiator_app_path, pattern, replacement);

            if ((record_msg_found_itr->second->m_collective_type != ALL_REDUCE &&
                    record_msg_found_itr->second->m_collective_alg != BCAST_RING) &&
                    (payload->getResponsibility_flow_id_listArraySize() != payload->getResponsibility_listArraySize() ||
                    payload->getResponsibility_flow_id_listArraySize() != payload->getResponsibility_gpu_listArraySize())) {
                std::cout << "responsibility: " << payload->getResponsibility_listArraySize() << endl;
                std::cout << "responsibility gpu: " << payload->getResponsibility_gpu_listArraySize() << endl;
                std::cout << "responsibility flow id: " << payload->getResponsibility_flow_id_listArraySize() << endl;
                throw cRuntimeError("payload->getResponsibility_flow_id_listArraySize() != payload->getResponsibility_listArraySize()");
            }
            if (payload->getResponsibility_listArraySize() != 0) {
                for (int k = 0; k < payload->getResponsibility_listArraySize(); k++) {

                    record_msg_found_itr->second->responsibility.push_back(payload->getResponsibility_list(k));
                    record_msg_found_itr->second->responsiblity_flow_ids.push_back(payload->getResponsibility_flow_id_list(k));
                    record_msg_found_itr->second->responsibility_gpu.push_back(payload->getResponsibility_gpu_list(k));
                }
                record_msg_found_itr->second->num_dsts = record_msg_found_itr->second->responsibility.size();
            }
        }
        else if (record_msg_found_itr->second->total_flow_size != (total_length.get()/8) ||
                record_msg_found_itr->second->flow_id != flow_id ||
                record_msg_found_itr->second->query_id != query_id ||
                record_msg_found_itr->second->responsibility.size() != payload->getResponsibility_listArraySize()) {
            std::cout << msg->str() << endl;
            std::cout << record_msg_found_itr->second->total_flow_size << " vs " << (total_length.get()/8) << endl;
            std::cout << record_msg_found_itr->second->flow_id << " vs " << flow_id << endl;
            std::cout <<  record_msg_found_itr->second->query_id << " vs " << query_id << endl;
            std::cout <<  record_msg_found_itr->second->responsibility.size() << " vs " << payload->getResponsibility_listArraySize() << endl;
            throw cRuntimeError("multiple chunks in one message without matching info!");
        } else {
            // total flow is not zero, we already have some data for this!
            if (m_collective_alg_type != BCAST_INA &&
                    m_collective_alg_type != BCAST_PARTIAL_MULTICAST &&
                    !in_src_sharding &&
                    (record_msg_found_itr->second->src_gpu != payload->getSrc_gpu_idx() ||
                    record_msg_found_itr->second->dst_gpu != payload->getDst_gpu_idx())) {
                std::cout << "expected src: " << record_msg_found_itr->second->src_gpu << ", dst: " << record_msg_found_itr->second->dst_gpu << endl;
                std::cout << "payload src: " << payload->getSrc_gpu_idx() << ", dst: " << payload->getDst_gpu_idx() << endl;
                throw cRuntimeError("The src/dst gpu idx from payload does not match the record!");
            }
        }

        // change from bits to bytes since flow size is in bytes
        record_msg_found_itr->second->msg_size += (chunk_length.get()/8);

        // for all_reduce and reduce_scatter the message is already splitted into chunks when it arrives
        bool message_splitted_when_sent = record_msg_found_itr->second->m_collective_type == REDUCE_SCATTER ||
                record_msg_found_itr->second->m_collective_alg == BCAST_OPTIREDUCE ||
                (record_msg_found_itr->second->m_collective_alg == BCAST_RING &&
                        record_msg_found_itr->second->m_collective_type == ALL_REDUCE);

        if (!just_complete_message && is_message_splitted && !message_splitted_when_sent) {
            unsigned int split_thresh;
            if (splitted_chunk_size_bytes > 0)
                split_thresh = splitted_chunk_size_bytes;
            else if (splitted_subflow_num > 0)
                split_thresh = (record_msg_found_itr->second->total_flow_size /
                        splitted_subflow_num);
            else
                throw cRuntimeError("Unknown pipelining method!");
            if (record_msg_found_itr->second->msg_size >= split_thresh)
                record_msg_found_itr->second->should_pass = true;
        }

        delete temp;
        if (msg_dup->getByteLength() == 0)
            break;
        chunk = msg_dup->removeAtFront<SliceChunk>();
    }

    record_msg_found_itr->second->verify_info();
    delete msg_dup;
    delete msg;

    EV_INFO << "reply arrived\n";

    if (unused_optireduce_flow) {
        if (record_msg_found_itr->second->should_pass)
            throw cRuntimeError("Should pass MUST be off when flow is unused!");
        // remove unused flow info
        flowid_receivedmsg_mapper.erase(record_msg_found_itr);
    }

    if (record_msg_found_itr->second->should_pass) {
        // the gpu on this host is actually dst gpu not src gpu
        // src gpu is for the sender host
        EV << "Passing the message" << endl;
        simtime_t processing_and_finish_delay = get_processing_delay(record_msg_found_itr->second->dst_gpu,
                record_msg_found_itr->second->m_collective_type,
                record_msg_found_itr->second->m_collective_alg,
                record_msg_found_itr->second->tree_traversal_dir,
                record_msg_found_itr->second->optireduce_in_reduction_phase);
        simtime_t pass_delay = 0;

        if (mcast_in_host &&
                (record_msg_found_itr->second->m_collective_alg == BCAST_NETWORK_ASSISTED ||
                        record_msg_found_itr->second->m_collective_alg == BCAST_PARTIAL_MULTICAST)) {
            EV << "Adding in-host multicast delay" << endl;
            pass_delay = get_intra_host_pass_delay(record_msg_found_itr->second->msg_size);
        }

//        std::cout << processing_and_finish_delay + pass_delay << endl;

        record_msg_found_itr->second->check_metrics();
        schedule_finish_if_required(record_msg_found_itr->second, processing_and_finish_delay);
        schedule_pass(record_msg_found_itr->second, processing_and_finish_delay + pass_delay);
        flowid_receivedmsg_mapper.erase(record_msg_found_itr);
    }

}

void UDPLongBroadcastPasserApp::schedule_finish_if_required(BCastCustomCollectiveMsgPasserInfo* msg_info, simtime_t delay) {
    EV << "UDPLongBroadcastPasserApp::schedule_finish_if_required" << endl;
    // schedule finish/processing
    if (msg_info->original_flow_terminated) {
        auto finish_msg = new BCastCustomCollectiveMsgPasserInfo();
        finish_msg->original_flow_terminated = msg_info->original_flow_terminated;
        finish_msg->flow_id = msg_info->flow_id;
        finish_msg->query_id = msg_info->query_id;
        finish_msg->src_gpu = msg_info->src_gpu;
        finish_msg->dst_gpu = msg_info->dst_gpu;
        finish_msg->m_collective_alg = msg_info->m_collective_alg;
        finish_msg->m_collective_type = msg_info->m_collective_type;
        finish_msg->total_flow_size = msg_info->total_flow_size;

        unsigned long storage_loc = insert_msg_storage(finish_msg);
        EV << "processing delay is " << delay << endl;
        simtime_t now = simTime();
        EV << "Scheduling receive for " << now + delay << endl;
        EV << "Storage loc: " << storage_loc << endl;
        cMessage *finishTimeoutMsg = new cMessage("custom_delay");
        finishTimeoutMsg->setKind(FINISH);
        finishTimeoutMsg->addPar("storage_loc") = storage_loc;
        scheduleAt(now + delay, finishTimeoutMsg);
    }
}

void UDPLongBroadcastPasserApp::schedule_pass(BCastCustomCollectiveMsgPasserInfo* msg_info, simtime_t delay) {
    // schedule pass delay
    EV << "UDPLongBroadcastPasserApp::schedule_pass" << endl;
    unsigned long storage_loc = insert_msg_storage(msg_info);
    EV << "pass delay is " << delay << endl;
    simtime_t now = simTime();
    EV << "Scheduling receive for " << now + delay << endl;
    EV << "Storage loc: " << storage_loc << endl;
    cMessage *timeoutMsg = new cMessage("custom_delay");
    timeoutMsg->setKind(PASS);
    timeoutMsg->addPar("storage_loc") = storage_loc;
    scheduleAt(now + delay, timeoutMsg);
}

void UDPLongBroadcastPasserApp::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void UDPLongBroadcastPasserApp::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void UDPLongBroadcastPasserApp::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();

    char buf[100];
    getDisplayString().setTagArg("t", 0, buf);
}

void UDPLongBroadcastPasserApp::handleStopOperation(LifecycleOperation *operation)
{
    for (auto itr = destination_to_socket_mapper.begin(); itr != destination_to_socket_mapper.end(); itr++) {
        itr->second->close();
    }
    destination_to_socket_mapper.clear();
//    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void UDPLongBroadcastPasserApp::handleCrashOperation(LifecycleOperation *operation)
{
    throw cRuntimeError("UDPLongBroadcastPasserApp::Operation creashed!");
}


