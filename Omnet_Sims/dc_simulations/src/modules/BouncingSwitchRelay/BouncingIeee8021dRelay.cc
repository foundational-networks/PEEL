// Copyright (C) 2013 OpenSim Ltd.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
// Author: Benjamin Martin Seregi

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/ieee8022/Ieee8022LlcHeader_m.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/transportlayer/tcp_common/TcpHeader_m.h"
#include "inet/transportlayer/udp/UdpHeader_m.h"
#include "inet/linklayer/ethernet/EtherPhyFrame_m.h"
#include "inet/queueing/queue/PacketQueue.h"
#include "BouncingIeee8021dRelay.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include <sqlite3.h>
#include <random>
#include <regex>

using namespace inet;

Define_Module(BouncingIeee8021dRelay);

// bolt vertigo src generation options
#define BOLT_VERTIGO_ORG_PKT    0 // sending SRC packet for the packet that faces full queue
#define BOLT_VERTIGO_DEF_PKT    1 // sending SRC packet for the deflected packets
#define BOLT_VERTIGO_ORG_DEF_PKT    2 // sending SRC for both packets stated above

// Selective reaction for CC
#define QUANTILE_SEL_REACTION 0
#define NONBURSTY_ONLY_SEL_REACTION 1

// Picky deflection of some packets
#define BURSTY_ONLY_PICKY_DEF 0

// partial deployment
#define PAR_DEP_BURSTY_VS_NONBURSTY 0

#define TTL 250


simsignal_t BouncingIeee8021dRelay::feedBackPacketDroppedSignal = registerSignal("feedBackPacketDropped");
simsignal_t BouncingIeee8021dRelay::feedBackPacketDroppedPortSignal = registerSignal("feedBackPacketDroppedPort");
simsignal_t BouncingIeee8021dRelay::feedBackPacketGeneratedSignal = registerSignal("feedBackPacketGenerated");
simsignal_t BouncingIeee8021dRelay::bounceLimitPassedSignal = registerSignal("bounceLimitPassed");
simsignal_t BouncingIeee8021dRelay::burstyPacketReceivedSignal = registerSignal("burstyPacketReceived");

BouncingIeee8021dRelay::BouncingIeee8021dRelay()
{
}

BouncingIeee8021dRelay::~BouncingIeee8021dRelay()
{

    if (failure_or_recovery_notif_pkt != nullptr) {
//        std::cout << getFullPath() << ": deleting " << failure_or_recovery_notif_pkt->str() << endl;
        delete failure_or_recovery_notif_pkt;
    }

    recordScalar("lightInRelayPacketDropCounter", light_in_relay_packet_drop_counter);
    recordScalar("lightTreeCounter", tree_ids_set.size());
    recordScalar("maxSimultaneousTrees", max_simultaneous_trees);

    tree_ids_set.clear();

    if (failed_ports.size() > 0)
        std::cout << getFullPath() << " still has " << failed_ports.size() << " failed ports!" << endl;

    for (auto query_seqnum_to_port_pkt_itr : query_seqnum_to_port_pkt_map) {
        for (auto port_pkt_itr : query_seqnum_to_port_pkt_itr.second)
            delete port_pkt_itr.second;
        query_seqnum_to_port_pkt_itr.second.clear();
    }
    query_seqnum_to_port_pkt_map.clear();
}

static int callback_incremental_deployment(void *data, int argc, char **argv, char **azColName){
   int i;
   BouncingIeee8021dRelay *relay_object = (BouncingIeee8021dRelay*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          int result = std::stoi(argv[i]);
          if (result != 1 && result != 0) {
              throw cRuntimeError("Incremental deflection is neither 0 nor 1!");
          }
          relay_object->deployed_with_deflection = result;
          return 0;
      }
   }

   throw cRuntimeError("No result found for incremental deflection identifier!");
   return -1;
}

void BouncingIeee8021dRelay::read_inc_deflection_properties(std::string incremental_deployment_identifier, std::string input_file_name)
{

    sqlite3* DB;
    char *zErrMsg = 0;
    int rc;
    std::string sql;
    int exit = 0;

    std::string column_name = incremental_deployment_identifier;
    std::string incremental_deployment_table_name = "deflection_identifiers";

    exit = sqlite3_open(input_file_name.c_str(), &DB);
    if (exit) {
        throw cRuntimeError(sqlite3_errmsg(DB));
        return;
    }
    sql = "SELECT " + column_name + " from " + incremental_deployment_table_name;
    rc = sqlite3_exec(DB, sql.c_str(), callback_incremental_deployment, (void*)this, &zErrMsg);
    if( rc != SQLITE_OK ) {
      throw cRuntimeError("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(DB);
    return;
}

void BouncingIeee8021dRelay::fill_port_to_mac_address_map() {
    auto address_list = macTable->getAllAddresses();
    if (address_list.size() == 0)
        throw cRuntimeError("Why don't we have any addresses stored in the table!");
    for (auto address : address_list) {
        auto interface_ids = macTable->getInterfaceIdForAddress(address);
        if (interface_ids.size() == 0)
            throw cRuntimeError("How is destInterfaceIds empty!");
        for (auto interface_id : interface_ids) {
            auto ie = ifTable->getInterfaceById(interface_id);
            int port_idx = ie->getIndex();
            auto port_itr = port_to_mac_address_map.find(port_idx);
            if (port_itr == port_to_mac_address_map.end()) {
                std::set<MacAddress> tmp_set;
                port_to_mac_address_map.insert(std::make_pair(port_idx, tmp_set));
                port_itr = port_to_mac_address_map.find(port_idx);
            }
            auto address_itr = mac_address_to_port_map.find(address.str());
            if (address_itr == mac_address_to_port_map.end()) {
                std::set<unsigned int> tmp_set;
                mac_address_to_port_map.insert(std::make_pair(address.str(), tmp_set));
                address_itr = mac_address_to_port_map.find(address.str());
            }
            port_itr->second.insert(address);
            address_itr->second.insert(port_idx);
        }
    }
    if (port_to_mac_address_map.size() == 0)
        throw cRuntimeError("port_to_mac_address_map.size() == ");
}

void BouncingIeee8021dRelay::fill_port_to_failure_range() {
    simtime_t startTime = 0;
    simtime_t stopTime = SimTime::parse(getEnvir()->getConfig()->getConfigValue("sim-time-limit")).inUnit(SIMTIME_US);
    stopTime -= 20000; // let 20ms (20000us) for recovery
    stopTime /= 1000000.0;
    std::cout << "stop time is " << stopTime << endl;

    // use one seed per switch to exactly fail the same ports at the same time as long as the prob is the same
    std::string seed_str = getFullPath();
    std::size_t seed_val = std::hash<std::string>{}(seed_str);

    // for deciding if a port should fail
    std::mt19937 rng(seed_val);  // Mersenne Twister engine
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // for deciding the failure period
    if (failure_period_mean <= 0 ||
            failure_period_std <= 0)
        throw cRuntimeError("Cannot generate failure period due to invalid dist info");
    std::mt19937 generator(static_cast<unsigned int>(seed_val));
    std::normal_distribution<double> distribution(failure_period_mean, failure_period_std);

    while (startTime < stopTime) {
        double failure_period = distribution(generator);
        while (failure_period <= 0)
            failure_period = distribution(generator);

        bool some_port_failed = false;
        for (int port_idx = 0; port_idx < ifTable->getNumInterfaces(); port_idx++) {

            auto port_itr = port_to_failure_range.find(port_idx);
            if (port_itr == port_to_failure_range.end()) {
                std::list<std::array<simtime_t, 2>> tmp_list;
                port_to_failure_range.insert(std::make_pair(port_idx, tmp_list));
            }

            // find the neighbor's relay unit and correct port to fail or recover if needed
            auto neighbor_path_itr = port_idx_to_neighboring_switch_path.find(port_idx);
            if (neighbor_path_itr == port_idx_to_neighboring_switch_path.end()) {
                EV << "Neighbor for port " << port_idx << " is not a switch. Don't fail!" << endl;
                continue;
            }

            double random_value = dist(rng);
            bool should_fail_link = random_value < random_fail_prob;
            if (should_fail_link) {
                std::array<simtime_t, 2> range = { startTime, startTime + failure_period };
                std::cout << getFullPath() << ": port " << port_idx << " fails from " << startTime << " to " << startTime + failure_period << endl;
                port_to_failure_range[port_idx].push_back(range);
                some_port_failed = true;
            }
        }
        if (some_port_failed)
            std::cout << "-----------------------" << endl;
        // give 20% extra failure period as grace period
        startTime += 1.2 * failure_period;
    }
}

simtime_t BouncingIeee8021dRelay::get_failure_end_time(unsigned int port_idx) {
    auto port_itr = port_to_failure_range.find(port_idx);
    if (port_itr == port_to_failure_range.end()) {
        std::cout << port_idx << endl;
        throw cRuntimeError("No failure info found for port!");
    }
    auto now = simTime();

    for (auto range : port_itr->second) {
        if (now >= range[0] && now < range[1])
            return range[1];
    }
    return 0;
}

void BouncingIeee8021dRelay::initialize(int stage)
{
    LayeredProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // statistics
//        std::cout << getFullPath() << ": " << stage << ": INITSTAGE_LOCAL" << endl;
        numDispatchedBDPUFrames = numDispatchedNonBPDUFrames = numDeliveredBDPUsToSTP = 0;
        numReceivedBPDUsFromSTP = numReceivedNetworkFrames = numDroppedFrames = 0;
        isStpAware = par("hasStp");

        macTable = getModuleFromPar<LSIMacAddressTable>(par("macTableModule"), this);
        ifTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);

        use_ecmp = getAncestorPar("useECMP");
        use_power_of_n_lb = getAncestorPar("use_power_of_n_lb");
        random_power_factor = getAncestorPar("random_power_factor");
        use_per_pkt_lb = getAncestorPar("use_per_pkt_lb");
        processing_delay_mean = getAncestorPar("processing_delay_mean");
        processing_delay_stddev = getAncestorPar("processing_delay_stddev");

        std::string switch_name = getParentModule()->getFullName();
        seed = std::atoi(switch_name.c_str());
        uint32 repetition_num = atoi(getEnvir()->getConfigEx()->getVariable(CFGVAR_REPETITION));
        seed += repetition_num;
        srand(seed);
        num_gpus = int(getAncestorPar("num_gpus"));
        num_servers_under_each_leaf = int(getAncestorPar("num_servers"));
        num_spines = int(getAncestorPar("num_spines"));   // num spines
        num_leafs = int(getAncestorPar("num_aggs"));     // num leafs

        if (processing_delay_mean > 0 && processing_delay_stddev > 0) {

            std::mt19937 generator(seed);
            std::normal_distribution<double> distribution(processing_delay_mean, processing_delay_stddev);
            while (true) {
                double delay = distribution(generator);
                if (delay <= 0)
                    continue;
                reduction_delay = delay;
                break;
            }
        } else if (processing_delay_mean > 0) {
            reduction_delay = processing_delay_mean;
        }

//        use_host_assisted_routing = getAncestorPar("use_host_assisted_routing");
//        if (use_host_assisted_routing && !use_ecmp)
//            throw cRuntimeError("ecmp should also be enabled with use_host_assisted_routing");

        if ((!use_ecmp && !use_power_of_n_lb && !use_per_pkt_lb)) {
            // TODO if you want to have the option to don't use no LB, comment out this if.
            throw cRuntimeError("No load balancing technique used. Are you sure you want this?");
        }

        if (int(use_ecmp) + int(use_power_of_n_lb) + int(use_per_pkt_lb) > 1)
            throw cRuntimeError("More than one LB technique is chosen.");

        learn_mac_addresses = par("learn_mac_addresses");

        //NAIVE DEFLECTION
        bounce_naively = getAncestorPar("bounce_naively");
        naive_deflection_idx = getAncestorPar("naive_deflection_idx");

        //DIBS
        bounce_randomly = getAncestorPar("bounce_randomly");
        filter_out_full_ports = getAncestorPar("filter_out_full_ports");
        approximate_random_deflection = getAncestorPar("approximate_random_deflection");

        // Vertigo
        bounce_on_same_path = getAncestorPar("bounce_on_same_path");
        random_power_bounce_factor = getAncestorPar("random_power_bounce_factor");
        use_memory = getAncestorPar("use_memory");
        random_power_memory_size = getAncestorPar("random_power_memory_size");
        random_power_bounce_memory_size = getAncestorPar("random_power_bounce_memory_size");

        // Power of N bouncing
        bounce_randomly_v2 = getAncestorPar("bounce_randomly_v2");
        use_v2_pifo = getAncestorPar("use_v2_pifo");
        drop_bounced_in_relay = getAncestorPar("drop_bounced_in_relay");

        // dctcp
        dctcp_mark_deflected_packets_only = getAncestorPar("dctcp_mark_deflected_packets_only");

        // bolt
        use_bolt_queue = getAncestorPar("use_bolt_queue");
        use_bolt_with_vertigo_queue = getAncestorPar("use_bolt_with_vertigo_queue");
        if (use_bolt_queue && use_bolt_with_vertigo_queue)
            throw cRuntimeError("use_bolt_queue && use_bolt_with_vertigo_queue");
        use_bolt = use_bolt_queue || use_bolt_with_vertigo_queue;
        use_pifo_bolt_reaction = getAncestorPar("use_pifo_bolt_reaction");
        ignore_cc_thresh_for_deflected_packets = getAncestorPar("ignore_cc_thresh_for_deflected_packets");
        if (use_pifo_bolt_reaction && (!use_bolt_with_vertigo_queue || ignore_cc_thresh_for_deflected_packets))
            throw cRuntimeError("use_pifo_bolt_reaction && (!use_bolt_with_vertigo_queue || ignore_cc_thresh_for_deflected_packets)");

        // pFabric
        use_pfabric = getAncestorPar("use_pfabric");

        // Vertigo priority queue
        use_vertigo_prio_queue = getAncestorPar("use_vertigo_prio_queue");

        // PABO
        bounce_probabilistically = getAncestorPar("bounce_probabilistically");
        utilization_thresh = getAncestorPar("utilization_thresh");
        bounce_probability_lambda = getAncestorPar("bounce_probability_lambda");

        // selective network feedback
        apply_selective_net_reaction = getAncestorPar("apply_selective_net_reaction");
        std::string selective_net_reaction_type_string = getAncestorPar("selective_net_reaction_type_string");
        if (apply_selective_net_reaction) {
            if (use_pifo_bolt_reaction)
                throw cRuntimeError("apply_selective_net_reaction && use_pifo_bolt_reaction");
            if (selective_net_reaction_type_string.find("quantile") != std::string::npos) {
                selective_net_reaction_type = QUANTILE_SEL_REACTION;
                sel_reaction_alpha = getAncestorPar("sel_reaction_alpha");
                if (sel_reaction_alpha < 0 || sel_reaction_alpha > 1)
                    throw cRuntimeError("sel_reaction_alpha < 0 || sel_reaction_alpha > 1");
            }
            else if (selective_net_reaction_type_string.find("nonbursty") != std::string::npos) {
                selective_net_reaction_type = NONBURSTY_ONLY_SEL_REACTION;
            } else
                throw cRuntimeError("unknown selective feedback paradigm!");
        }

        apply_picky_deflection = getAncestorPar("apply_picky_deflection");
        std::string picky_deflection_type_string = getAncestorPar("picky_deflection_type_string");
        if (apply_picky_deflection) {
            if (picky_deflection_type_string.find("bursty") != std::string::npos) {
                picky_deflection_type = BURSTY_ONLY_PICKY_DEF;
            } else {
                throw cRuntimeError("No picky deflection paradigm set!");
            }
        }

        if (bounce_randomly_v2 && use_bolt_with_vertigo_queue) {
            std::string src_enabled_packet_type_str = getAncestorPar("src_enabled_packet_type");
            if (src_enabled_packet_type_str.compare("ORG") == 0)
                src_enabled_packet_type = BOLT_VERTIGO_ORG_PKT;
            else if (src_enabled_packet_type_str.compare("DEF") == 0)
                src_enabled_packet_type = BOLT_VERTIGO_DEF_PKT;
            else if (src_enabled_packet_type_str.compare("ORG_DEF") == 0)
                src_enabled_packet_type = BOLT_VERTIGO_ORG_DEF_PKT;
            else
                throw cRuntimeError("We don't know for which packet we should generate SRC while working with Vertigo!");
        }

        // incremental deployment
        switch_module = getParentModule();
        incremental_deployment = getAncestorPar("incremental_deployment");
        // switches
        if (incremental_deployment) {
            std::string incremental_deployment_identifier =
                                switch_module->getName() +
                                std::to_string(switch_module->getIndex());
            std::string incremental_deployment_file_name = getAncestorPar("incremental_deployment_file_name");
            int repetition_num = atoi(getEnvir()->getConfigEx()->getVariable(CFGVAR_REPETITION));
            std::string rep_num_string = std::to_string(repetition_num);
            incremental_deployment_file_name += ("_" + rep_num_string + "_rep.db");
            read_inc_deflection_properties(incremental_deployment_identifier, incremental_deployment_file_name);
            can_deflect = (deployed_with_deflection == 1);
            deflection_graph_partitioned = getAncestorPar("deflection_graph_partitioned");
            add_queue_idx_to_deflected = getAncestorPar("add_queue_idx_to_deflected");
            if (add_queue_idx_to_deflected) {
                std::string deflected_queue_idx_association_paradigm_str = getAncestorPar("deflected_queue_idx_association_paradigm");
                if (deflected_queue_idx_association_paradigm_str.compare("bursty_vs_nonbursty") == 0) {
                    deflected_queue_idx_association_paradigm = PAR_DEP_BURSTY_VS_NONBURSTY;
                } else
                    throw cRuntimeError("No deflected_queue_idx_association_paradigm indicated!");
            }
        } else {
            can_deflect = bounce_naively || bounce_randomly || bounce_on_same_path || bounce_randomly_v2 ||
                    bounce_probabilistically;
        }

        if (bounce_naively && naive_deflection_idx < 0)
            throw cRuntimeError("bounce_naively && naive_deflection_idx < 0");

        if (use_memory && (random_power_memory_size <= 0 || random_power_bounce_memory_size <= 0)) {
            throw cRuntimeError("use_memory && (random_power_memory_size <= 0 || random_power_bounce_memory_size <= 0");
        }

        if (int(bounce_naively) + int(bounce_randomly) + int(bounce_on_same_path) + int(bounce_randomly_v2)
                + int(bounce_probabilistically) > 1)
            throw cRuntimeError("Two bouncing approaches chosen. WTF?");

        if (bounce_probabilistically && learn_mac_addresses)
            throw cRuntimeError("learning mac addresses should be off when using PABO!");

        if (int(use_v2_pifo) + int(use_pfabric) + int(use_vertigo_prio_queue) > 1)
            throw cRuntimeError("Cannot set use_v2_pifo, use_pfabric, or use_vertigo_prio_queue at the same time");

        if (bounce_randomly_v2 && !use_vertigo_prio_queue && !use_v2_pifo && !use_pfabric)
            throw cRuntimeError("How are we using v2 bouncing without v2 pifo or pFabric");

        send_header_of_dropped_packet_to_receiver = getAncestorPar("send_header_of_dropped_packet_to_receiver");

        std::string module_path_string = switch_name + ".eth[" + std::to_string(0) + "].mac.queue";
        if (send_header_of_dropped_packet_to_receiver)
            module_path_string += ".mainQueue";
        cModule* queue_module = getModuleByPath(module_path_string.c_str());
        std::string queue_module_name = queue_module->getModuleType()->getFullName();

        bool have_v2pifo_queues = queue_module_name.find("V2PIFO") != std::string::npos;
        if (use_v2_pifo && !have_v2pifo_queues)
            throw cRuntimeError("We're planning to use v2pifo, why we don't have v2pifo queues?");
        else if (!use_vertigo_prio_queue && !use_v2_pifo && have_v2pifo_queues)
            throw cRuntimeError("We are not using v2pifo, why we still have v2pifo queues?");

        bool have_pFabric_queues = queue_module_name.find("pFabric") != std::string::npos;
        if (use_pfabric && !have_pFabric_queues)
            throw cRuntimeError("We're planning to use pFabric, why we don't have pFabric queues?");
        else if (!use_pfabric && have_pFabric_queues)
            throw cRuntimeError("We are not using pFabric, why we still have pFabric queues?");
        if (!use_pfabric && !use_v2_pifo && send_header_of_dropped_packet_to_receiver)
            throw cRuntimeError("send_header_of_dropped_packet_to_receive only works if we are using v2_pifo!");

        if (!bounce_randomly_v2 && send_header_of_dropped_packet_to_receiver)
            throw cRuntimeError("send_header_of_dropped_packet_to_receive only works if we are using v2 bouncing!");

        bool have_vertigo_prio_queues = queue_module_name.find("V2PIFOPrioQueue") != std::string::npos ||
                queue_module_name.find("V2PIFOCanaryQueue") != std::string::npos ||
                queue_module_name.find("V2PIFOBoltQueue") != std::string::npos;
        if (use_vertigo_prio_queue && !have_vertigo_prio_queues)
            throw cRuntimeError("We're planning to use V2PIFOPrioQueue, CanaryQueue, or BoltQueue, why we don't have V2PIFOPrioQueue/CanaryPrioQueue queues?");
        if (!use_vertigo_prio_queue && !use_pfabric && !use_v2_pifo && send_header_of_dropped_packet_to_receiver)
            throw cRuntimeError("send_header_of_dropped_packet_to_receive only works if we are using v2_pifo!");

        if (!bounce_randomly_v2 && send_header_of_dropped_packet_to_receiver)
            throw cRuntimeError("send_header_of_dropped_packet_to_receive only works if we are using v2 bouncing!");

        std::string v2pifo_queue_type_str = getAncestorPar("v2pifo_queue_type");

        use_udp = bool(getAncestorPar("use_udp"));

        // use rsbf
        use_rsbf = bool(getAncestorPar("use_rsbf"));

        use_cepheus = bool(getAncestorPar("use_cepheus"));

        random_fail_prob = double(getAncestorPar("random_fail_prob"));
        if (random_fail_prob > 0) {
            std::string drop_type_str = getAncestorPar("drop_type");
            if (drop_type_str.compare("fail") == 0) {
                drop_type = DROP_FAIL;
                failure_period_mean = double(getAncestorPar("failure_period_mean"));
                failure_period_std = double(getAncestorPar("failure_period_std"));
                if (failure_period_mean <= 0 ||
                        failure_period_std <= 0)
                    throw cRuntimeError("invalid failure_period_mean or failure_period_std");
            } else
                throw cRuntimeError("Unknown drop type!");

            std::string recovery_type_str = getAncestorPar("recovery_type");
            if (recovery_type_str.compare("notif") == 0) {
                recovery_type = DROP_NOTIF;
            } else if (recovery_type_str.compare("deflect") == 0) {
                recovery_type = DEFLECT_ON_DROP;
            } else
                throw cRuntimeError("Unknown drop recovery type!");
        }

        use_fattree = bool(getAncestorPar("use_fattree"));
        fattree_k = int(getAncestorPar("k"));

        initiate_tree = bool(getAncestorPar("initiate_tree"));

        partial_mcast_programmable_cores = bool(getAncestorPar("partial_mcast_programmable_cores"));

        partial_mcast_use_cidr_agg = bool(getAncestorPar("partial_mcast_use_cidr_agg"));
        partial_mcast_use_cidr_core = bool(getAncestorPar("partial_mcast_use_cidr_core"));

        // INA
        mtu_bytes = int(par("dcqcn_mtu"));
        int ina_aggregation_pool_capacity_bytes = int(par("ina_aggregation_pool_capacity_bytes"));
        if (ina_aggregation_pool_capacity_bytes > 0) {
            if (mtu_bytes < 1500) {
                std::cout << "mtu_bytes: " << mtu_bytes << endl;
                throw cRuntimeError("mtu < 1500 B ... if you are sure about this, comment this out!");
            }
            ina_aggregation_pool_spots_remaining = std::ceil(ina_aggregation_pool_capacity_bytes * 1.0 / mtu_bytes);
            ina_aggregation_pool_spots_max = ina_aggregation_pool_spots_remaining;
            EV << "ina_aggregation_pool_spots max cap: " << ina_aggregation_pool_spots_remaining << endl;
        }

        // For incremental deployment we want this for all switches
        if (incremental_deployment || getParentModule()->getIndex() == 0) {
//            std::cout << switch_module->getFullName() <<
//                    ": You're setting for forwarding and bouncing is: " << endl <<
//                    "use_ecmp: " << use_ecmp << endl <<
//                    "use_power_of_n_lb: " << use_power_of_n_lb << endl <<
//                    "bounce_naively: " << bounce_naively << endl <<
//                    "bounce_randomly: " << bounce_randomly << endl <<
//                    "filter_out_full_ports: " << filter_out_full_ports << endl <<
//                    "approximate_random_deflection: " << approximate_random_deflection << endl <<
//                    "bounce_on_same_path: " << bounce_on_same_path << endl <<
//                    "bounce_randomly_v2: " << bounce_randomly_v2 << endl <<
//                    "use_v2_pifo: " << use_v2_pifo << endl <<
//                    "incremental_deployment: " << incremental_deployment << endl <<
//                    "deflection_graph_partitioned: " << deflection_graph_partitioned << endl <<
//                    "use_pFabric: " << use_pfabric << endl <<
//                    "use_vertigo_prio_queue: " << use_vertigo_prio_queue << endl <<
//                    "send_header_of_dropped_packet_to_receiver: " << send_header_of_dropped_packet_to_receiver << endl <<
//                    "can_deflect: " << can_deflect << endl <<
//                    "use_memory: " << use_memory << endl <<
//                    "random_power_memory_size: " << random_power_memory_size << endl <<
//                    "random_power_bounce_memory_size: " << random_power_bounce_memory_size << endl <<
//                    "bounce_probabilistically: " << bounce_probabilistically << endl <<
//                    "v2pifo_queue_type_str: " << v2pifo_queue_type_str << endl <<
//                    "use_bolt_queue: " << use_bolt_queue << endl <<
//                    "use_bolt_with_vertigo_queue: " << use_bolt_with_vertigo_queue << endl <<
//                    "apply_selective_net_reaction: " << apply_selective_net_reaction << endl <<
//                    "add_queue_idx_to_deflected: " << add_queue_idx_to_deflected << endl <<
//                    "use_host_assisted_routing: " << use_host_assisted_routing << endl <<
//                    "use_udp: " << use_udp << endl;
//            std::cout << "**********************************************************" << endl;
        }
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
//        std::cout << getFullPath() << ": " << stage << ": INITSTAGE_LINK_LAYER" << endl;
        registerService(Protocol::ethernetMac, gate("upperLayerIn"), gate("ifIn"));
        registerProtocol(Protocol::ethernetMac, gate("ifOut"), gate("upperLayerOut"));

        //TODO FIX Move it at least to STP module (like in ANSA's CDP/LLDP)
        if(isStpAware) {
            registerAddress(MacAddress::STP_MULTICAST_ADDRESS);
        }

        WATCH(bridgeAddress);
        WATCH(numReceivedNetworkFrames);
        WATCH(numDroppedFrames);
        WATCH(numReceivedBPDUsFromSTP);
        WATCH(numDeliveredBDPUsToSTP);
        WATCH(numDispatchedNonBPDUFrames);

        int bounce_naively_counter = 0;

        std::string switch_name = switch_module->getFullName();
        total_buffer_size = b(0);
        for (int i = 0; i < ifTable->getNumInterfaces(); i++) {
           std::string module_path_string = switch_name + ".eth[" + std::to_string(i) + "].mac.queue";
           queueing::PacketQueue *queue = check_and_cast<queueing::PacketQueue *>(getModuleByPath(module_path_string.c_str()));
           total_buffer_size += queue->getMaxTotalLength();
        }
        free_buffer_capacity = total_buffer_size;

        if (getParentModule()->getIndex() == 0) {
            std::cout << switch_module->getFullName() <<
                    ": total_buffer capacity: " << total_buffer_size << endl;
            std::cout << "**********************************************************" << endl;
        }

        if (can_deflect) {
            std::string other_side_input_module_path;
            bool is_other_side_input_module_path_server;
            for (int i = 0; i < ifTable->getNumInterfaces(); i++) {
               other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ i)->getPathEndGate()->getFullPath();
               is_other_side_input_module_path_server = (other_side_input_module_path.find("server") != std::string::npos);
               if (!is_other_side_input_module_path_server) {
//                   std::cout << other_side_input_module_path << " is not a host." << endl;
                   if (bounce_naively) {
                       if (bounce_naively_counter == naive_deflection_idx) {
                           port_idx_connected_to_switch_neioghbors.push_back(i);
                           break;
                       }
                       bounce_naively_counter++;
                   } else if (incremental_deployment && deflection_graph_partitioned) {
                       std::string relay_name = "";
                       int counter = 0;
                       int dots_seen = 0;
                       while (dots_seen < 2) {
                           relay_name += other_side_input_module_path[counter];
                           if (other_side_input_module_path[counter] == '.') {
                               dots_seen += 1;
                           }
                           counter += 1;
                       }
                       relay_name += "relayUnit";
                       BouncingIeee8021dRelay *relay = check_and_cast<BouncingIeee8021dRelay *>(getModuleByPath(relay_name.c_str()));
                       if (relay->can_deflect) {
                           EV << getFullPath() << ": adding " << relay_name << " to neighbor list for deflection!" << endl;
                           port_idx_connected_to_switch_neioghbors.push_back(i);
                       }
                   } else {
                       port_idx_connected_to_switch_neioghbors.push_back(i);
                   }
               }
            }
            if (port_idx_connected_to_switch_neioghbors.size() == 0)
                throw cRuntimeError("can_deflect is on but no deflection option is added. why?");
        }


        // add ports connected to servers
        std::string other_side_input_module_path;
        bool is_other_side_input_module_path_server;
        for (int i = 0; i < ifTable->getNumInterfaces(); i++) {
           other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ i)->getPathEndGate()->getFullPath();
           is_other_side_input_module_path_server = (other_side_input_module_path.find("server") != std::string::npos);
           if (is_other_side_input_module_path_server) {
               std::regex pattern(R"(server\[(\d+)\])");
               int server_idx = -1;
               std::smatch match;
               if (std::regex_search(other_side_input_module_path, match, pattern)) {
                   server_idx = std::stoi(match[1]);
               } else
                   throw cRuntimeError("Server index did not found!!!");
               if (server_idx < 0)
                   throw cRuntimeError("How is server_idx < 0");
               auto server_itr = server_idx_to_port_map.find(server_idx);
               if (server_itr == server_idx_to_port_map.end()) {
                   std::set<int> tmp_set;
                   server_idx_to_port_map.insert(std::make_pair(server_idx, tmp_set));
                   server_itr = server_idx_to_port_map.find(server_idx);
               }
               server_itr->second.insert(i);
//               std::cout << getFullPath() << " adding port " << i << " for server " << server_idx << endl;
           } else {
//               std::cout << other_side_input_module_path << endl;
               std::string suffix = ".phys$i";
               size_t pos = other_side_input_module_path.rfind(suffix);
               // If the suffix is at the end, remove it
               if (pos != std::string::npos && pos == other_side_input_module_path.size() - suffix.size()) {
                   other_side_input_module_path = other_side_input_module_path.substr(0, pos);
               } else
                   throw cRuntimeError("How is not .phys$i found!");
               port_idx_to_neighboring_switch_path.insert(std::make_pair(i, other_side_input_module_path));
           }
        }

        if (use_memory) {
            for (int i = 0; i < random_power_bounce_memory_size; i++)
                deflection_memory.push_back(-1);
        }

        if (random_fail_prob > 0) {
            if (port_to_mac_address_map.size() == 0)
                fill_port_to_mac_address_map();
            if (port_to_failure_range.size() == 0)
                fill_port_to_failure_range();
        }
    }
}

void BouncingIeee8021dRelay::registerAddress(MacAddress mac)
{
    registerAddresses(mac, mac);
}

void BouncingIeee8021dRelay::registerAddresses(MacAddress startMac, MacAddress endMac)
{
    registeredMacAddresses.insert(MacAddressPair(startMac, endMac));
}

void BouncingIeee8021dRelay::handleLowerPacket(Packet *packet)
{
    // messages from network
    numReceivedNetworkFrames++;
    std::string switch_name = this->getParentModule()->getFullName();

    EV_INFO << "Received " << packet << " from network." << endl;
    delete packet->removeTagIfPresent<DispatchProtocolReq>();

    std::string pkt_name = packet->getName();
    int input_port_idx = packet->getTag<InputPortTag>()->getInput_port_idx();

    // handle failure or recovery
    if (drop_type == DROP_FAIL) {
        if (pkt_name.compare("fail") == 0) {
            fail_or_recover_message_received(packet, input_port_idx, NOTIF_LINK_FAILURE);
            delete packet;
            return;
        }
        if (pkt_name.compare("recover") == 0) {
            fail_or_recover_message_received(packet, input_port_idx, NOTIF_LINK_RECOVERY);
            delete packet;
            return;
        }
    }

    bool dispatch_drop_notif = false;
    if (pkt_name.compare("drop_notif") == 0) {
        EV << "Handling drop notif..." << endl;
        auto eth_header = packet->peekAtFront<EthernetMacHeader>();

        bool more_ports_available = false;
        int input_interface_id = ifTable->getInterface(input_port_idx)->getInterfaceId();
        std::list<int> destInterfaceIds = macTable->getInterfaceIdForAddress(eth_header->getDest());
        for (auto dst_interface_id : destInterfaceIds)
            if (dst_interface_id != input_interface_id) {
                more_ports_available = true;
                break;
            }

        if (more_ports_available && recovery_type == DEFLECT_ON_DROP) {
            EV << "Reverting the packet back to its original form with name " <<
                    eth_header->getOriginal_pkt_name() << endl;
            packet->setName(eth_header->getOriginal_pkt_name());
        } else {
            EV << "Propagate drop notif..." << endl;
            dispatch_drop_notif = true;
        }
    }

    if (dispatch_drop_notif) {
        EV << "Dispatching drop notif... " << packet->str() << endl;
        b pkt_position = packet->getFrontOffset();
        packet->setFrontIteratorPosition(b(0));
        auto m_phy_header =
                packet->removeAtFront<EthernetPhyHeader>();
        auto m_eth_header =
                packet->removeAtFront<EthernetMacHeader>();
        if (m_eth_header->getInput_ports_passedArraySize() == 0)
            throw cRuntimeError("2) How is getInput_ports_passedArraySize() == 0");
        unsigned int output_port = m_eth_header->getInput_ports_passed(
                m_eth_header->getInput_ports_passedArraySize() - 1);
        m_eth_header->eraseInput_ports_passed(
                m_eth_header->getInput_ports_passedArraySize() - 1);
        packet->insertAtFront(m_eth_header);
        packet->insertAtFront(m_phy_header);
        packet->setFrontIteratorPosition(pkt_position);
        EV << "Notif packet is being sent to port " << output_port << endl;
        auto ie = ifTable->getInterface(output_port);
        dispatch(packet, ie);
    } else
        handleAndDispatchFrame(packet);
}

void BouncingIeee8021dRelay::handleUpperPacket(Packet *packet)
{

//    if (use_host_assisted_routing)
    if (packet_has_host_instructions(packet))
        throw cRuntimeError("HandleUpperPacket is not implemented with use_host_assisted_routing. Problem with getInterfaceById");
    const auto& frame = packet->peekAtFront<EthernetMacHeader>();

    InterfaceReq* interfaceReq = packet->findTag<InterfaceReq>();
    int interfaceId =
            interfaceReq == nullptr ? -1 : interfaceReq->getInterfaceId();

    if (interfaceId != -1) {
        InterfaceEntry *ie = ifTable->getInterfaceById(interfaceId);
        chooseDispatchType(packet, ie);
    } else if (frame->getDest().isBroadcast()) {    // broadcast address
        broadcast(packet, -1);
    } else {
        std::list<int> outInterfaceId = macTable->getInterfaceIdForAddress(frame->getDest());
        // Not known -> broadcast
        if (outInterfaceId.size() == 0) {
            EV_DETAIL << "Destination address = " << frame->getDest()
                                      << " unknown, broadcasting frame " << frame
                                      << endl;

            throw cRuntimeError("2)Destination address not known. Broadcasting the frame. For DCs based on you're setting this shouldn't happen.");
            broadcast(packet, -1);
        } else {
            InterfaceEntry *ie = ifTable->getInterfaceById(interfaceId);
            chooseDispatchType(packet, ie);
        }
    }
}

bool BouncingIeee8021dRelay::isForwardingInterface(InterfaceEntry *ie)
{
    if (isStpAware) {
        if (!ie->getProtocolData<Ieee8021dInterfaceData>())
            throw cRuntimeError("Ieee8021dInterfaceData not found for interface %s", ie->getFullName());
        return ie->getProtocolData<Ieee8021dInterfaceData>()->isForwarding();
    }
    return true;
}

void BouncingIeee8021dRelay::broadcast(Packet *packet, int arrivalInterfaceId)
{
    if (!learn_mac_addresses) {
        throw cRuntimeError("Even though a learning is off, a packet is "
                "being broadcasted. If global ARP is set. This can actually"
                "mean that the tables are not created correctly and some "
                "packets are being broadcasted!");
    }
    EV_DETAIL << "Broadcast frame " << packet << endl;

    auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
    packet->clearTags();
    auto newPacketProtocolTag = packet->addTag<PacketProtocolTag>();
    *newPacketProtocolTag = *oldPacketProtocolTag;
    delete oldPacketProtocolTag;
    packet->trim();

    int numPorts = ifTable->getNumInterfaces();
    EV_DETAIL << "SEPEHR: number of ports are: " << numPorts << endl;
    EV_DETAIL << "SEPEHR: arrival ID is: " << arrivalInterfaceId << endl;
    EV_DETAIL << "SEPEHR: arrival ID index is: " << arrivalInterfaceId - 100 << endl;


    std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ arrivalInterfaceId - 100)->getPathEndGate()->getFullPath();
    bool is_other_side_input_module_path_spine = (other_side_input_module_path.find("spine") != std::string::npos);


    for (int i = 0; i < numPorts; i++) {
        InterfaceEntry *ie = ifTable->getInterface(i);

        if (ie->isLoopback() || !ie->isBroadcast())
            continue;
        std::string other_side_output_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ i)->getPathEndGate()->getFullPath();
        bool is_other_side_output_module_path_spine = (other_side_output_module_path.find("spine") != std::string::npos);
        if (is_other_side_input_module_path_spine && is_other_side_output_module_path_spine) {
            EV_DETAIL << "SEPEHR: Came from upper layer and should not go to the upper layer" << endl;
            continue;
        }
//        bool is_other_side_output_module_path_client = (other_side_output_module_path.find("client") != std::string::npos);
//        if (is_other_side_output_module_path_client) {
//            std::string protocol = packet->getName();
//            if (protocol.find("arpREQ") != std::string::npos) {
//                EV << "arpREQ going to client. No!" << endl;
//                continue;
//            }
//        }
        if (ie->getInterfaceId() != arrivalInterfaceId && isForwardingInterface(ie)) {
            chooseDispatchType(packet->dup(), ie);
        }
    }
    delete packet;
}

namespace {
bool isBpdu(Packet *packet, const Ptr<const EthernetMacHeader>& hdr)
{
    if (isIeee8023Header(*hdr)) {
        const auto& llc = packet->peekDataAt<Ieee8022LlcHeader>(hdr->getChunkLength());
        return (llc->getSsap() == 0x42 && llc->getDsap() == 0x42 && llc->getControl() == 3);
    }
    else
        return false;
}
}

const uint64_t string_to_mac(std::string const& s) {
    unsigned char a[6];
    int last = -1;
    int rc = sscanf(s.c_str(), "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx%n",
                    a + 0, a + 1, a + 2, a + 3, a + 4, a + 5,
                    &last);
    if(rc != 6 || s.size() != last)
        throw std::runtime_error("invalid mac address format " + s);
    return
        uint64_t(a[0]) << 40 |
        uint64_t(a[1]) << 32 | (
            // 32-bit instructions take fewer bytes on x86, so use them as much as possible.
            uint32_t(a[2]) << 24 |
            uint32_t(a[3]) << 16 |
            uint32_t(a[4]) << 8 |
            uint32_t(a[5])
        );
}

bool BouncingIeee8021dRelay::ina_all_shards_received(int leaf_aggregation_num,
        int spine_aggregation_num,
        int core_aggregation_num,
        int shard_num) {

    if (isSwitchToR()) {
        if (leaf_aggregation_num < shard_num) {
            std::cout << leaf_aggregation_num << endl;
            std::cout << shard_num << endl;
            throw cRuntimeError("leaf_aggregation_num < shard_num");
        }
        EV << leaf_aggregation_num - shard_num << " shards remaining!" << endl;
        return leaf_aggregation_num == shard_num;
    } else if (isSwitchAggregate()) {
        if (spine_aggregation_num < shard_num) {
            std::cout << spine_aggregation_num << endl;
            std::cout << shard_num << endl;
            throw cRuntimeError("spine_aggregation_num < shard_num");
        }
        EV << spine_aggregation_num - shard_num << " shards remaining!" << endl;
        return spine_aggregation_num == shard_num;
    } else if (use_fattree && isSwitchCore()) {
        if (core_aggregation_num < shard_num) {
            std::cout << core_aggregation_num << endl;
            std::cout << shard_num << endl;
            throw cRuntimeError("core_aggregation_num < shard_num");
        }
        EV << core_aggregation_num - shard_num << " shards remaining!" << endl;
        return core_aggregation_num == shard_num;
    }

    throw cRuntimeError("Unknown switch type!");
}

void BouncingIeee8021dRelay::handleAndDispatchFrame(Packet *packet)
{

    std::string packet_name = packet->getName();
    b packet_position = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto& phy_header = packet->removeAtFront<EthernetPhyHeader>();
    const auto& frame2 = packet->removeAtFront<EthernetMacHeader>();

    bool using_ina = (packet_name.compare("data") == 0 && frame2->getCollective_alg() == BCAST_INA);
    if (using_ina) {
        if (num_gpus <= 0)
            throw cRuntimeError("using_ina && num_gpus <= 0");
        if (num_servers_under_each_leaf <= 0)
            throw cRuntimeError("using_ina && num_servers_under_each_leaf <= 0");
        if (use_fattree) {
            if (fattree_k <= 0)
                throw cRuntimeError("fattree_k <= 0");
        } else {
            if (num_leafs <= 0 || num_spines <= 0)
                throw cRuntimeError("num_leafs <= 0 || num_spines <= 0");
        }
    }

    int hop_count = frame2->getHop_count();
    hop_count++;
    unsigned long m_query_id = frame2->getQuery_id();
    unsigned long m_seq_num = frame2->getSeq_num();
    int tree_direction = frame2->getTree_direction();
    EV << "SEPEHR: packet hop count is " << hop_count << endl;
    frame2->setHop_count(hop_count);
    packet->insertAtFront(frame2);
    packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(packet_position);
    const auto& frame = packet->peekAtFront<EthernetMacHeader>();

    if (frame->getIs_bursty()) {
        emit(burstyPacketReceivedSignal, string_to_mac(frame->getDest().str()));
    }

    int arrivalInterfaceId = packet->getTag<InterfaceInd>()->getInterfaceId();
    InterfaceEntry *arrivalInterface = ifTable->getInterfaceById(arrivalInterfaceId);
    Ieee8021dInterfaceData *arrivalPortData = arrivalInterface->findProtocolData<Ieee8021dInterfaceData>();
    if (isStpAware && arrivalPortData == nullptr)
        throw cRuntimeError("Ieee8021dInterfaceData not found for interface %s", arrivalInterface->getFullName());
    if (learn_mac_addresses && !(frame->isFB() || frame->getAllow_same_input_output())) {
        learn(frame->getSrc(), arrivalInterfaceId);
    }

    // if it is tree up, wait to get all the shards to start aggregating!
    // if we are going down, we don't need to aggregate!
    if (using_ina &&
            tree_direction == TREE_UP) {
        if (m_query_id == 0)
            throw cRuntimeError("m_query_id == 0");
        std::pair<unsigned long, unsigned long> query_seqnum_pair = std::make_pair(m_query_id, m_seq_num);
        auto query_seqnum_found_itr = query_seqnum_to_port_pkt_map.find(query_seqnum_pair);
        if (query_seqnum_found_itr == query_seqnum_to_port_pkt_map.end()) {
            std::map<InterfaceEntry*, Packet*> port_pkt_map;
            query_seqnum_to_port_pkt_map.insert(std::make_pair(query_seqnum_pair, port_pkt_map));
            query_seqnum_found_itr = query_seqnum_to_port_pkt_map.find(query_seqnum_pair);
        }

        query_seqnum_found_itr->second.insert(std::make_pair(arrivalInterface, packet));

        if (!ina_all_shards_received(frame2->getIna_leaf_aggregation_num(),
                frame2->getIna_spine_aggregation_num(),
                frame2->getIna_core_aggregation_num(),
                query_seqnum_found_itr->second.size())) {
            EV << "Still waiting for more shards...!" << endl;
        } else {
            // keep the original packet and pass it up/down!
            EV << "All shards received... start reduction!" << endl;

            double extra_reduction_delay = get_processing_delay();
            EV << "reduction delay is " << extra_reduction_delay << endl;
            simtime_t now = simTime();
            cMessage *timeoutMsg = new cMessage("custom_delay");
            timeoutMsg->setKind(PASS);
            timeoutMsg->addPar("query_id") = m_query_id;
            timeoutMsg->addPar("seq_num") = m_seq_num;
            scheduleAt(now + extra_reduction_delay, timeoutMsg);

        }
        return;
    }

    //TODO revise next "if"s: 2nd drops all packets for me if not forwarding port; 3rd sends up when dest==STP_MULTICAST_ADDRESS; etc.
    // reordering, merge 1st and 3rd, ...

    // BPDU Handling
    if (isStpAware
            && (frame->getDest() == MacAddress::STP_MULTICAST_ADDRESS || frame->getDest() == bridgeAddress)
            && arrivalPortData->getRole() != Ieee8021dInterfaceData::DISABLED
            && isBpdu(packet, frame)) {
        EV_DETAIL << "Deliver BPDU to the STP/RSTP module" << endl;
        sendUp(packet);    // deliver to the STP/RSTP module
    }
    else if (isStpAware && !arrivalPortData->isForwarding()) {
        EV_INFO << "The arrival port is not forwarding! Discarding it!" << endl;
        numDroppedFrames++;
        delete packet;
    }
    else if (in_range(registeredMacAddresses, frame->getDest())) {
        // destination MAC address is registered, send it up
        sendUp(packet);
    }
    else if (frame->getDest().isBroadcast()) {    // broadcast address
        broadcast(packet, arrivalInterfaceId);
    }
    else {
        std::string packet_name = packet->getName();
        bool using_ace = (packet_has_host_instructions(packet) &&
                isPacketAppropriateForMulticast(packet_name));
        std::list<int> outputInterfaceId = macTable->getInterfaceIdForAddress(frame->getDest());
        EV << "Found " << outputInterfaceId.size() << " potential next hops for " << frame->getDest().str() << endl;
        // not ace and Not known -> broadcast
        if (!using_ina && !using_ace && outputInterfaceId.size() == 0 && random_fail_prob <= 0) {

            for (int start_idx = 0; start_idx < frame->getPorts_to_dest_idxArraySize(); start_idx++) {
                EV << "start_from_idx is " << start_idx << endl;
                if (start_idx >= frame->getPorts_to_dest_idxArraySize())
                    throw cRuntimeError("start_from_idx >= frame->getPath_to_dest_idxArraySize(), overflow!!");
                auto output_port_array = frame->getPorts_to_dest_idx(start_idx);
                auto next_start_from_array = frame->getJump_to_idx(start_idx);
                EV << "Ports: [";
                for (int k = 0; k < output_port_array.getInnerArrayArraySize(); k++) {
                    EV << output_port_array.getInnerArray(k) << ", ";
                }
                EV << "]" << endl;
                EV << "Next packets' index: [";
                for (int k = 0; k < next_start_from_array.getInnerArrayArraySize(); k++) {
                    EV << next_start_from_array.getInnerArray(k) << ", ";
                }
                EV << "]" << endl;
                EV << "--------------------------" << endl;
            }


            EV_DETAIL << "Destination address = " << frame->getDest() << " unknown, broadcasting frame " << frame << endl;
            throw cRuntimeError("1)Destination address not known. Broadcasting the frame. For DCs based on you're setting this shouldn't happen.");
            broadcast(packet, arrivalInterfaceId);
        }
        else {
            //for (std::list<int>::iterator it=outputInterfaceId.begin(); it != outputInterfaceId.end(); ++it){
            //NOTE: HERE I USED the first path
            if (using_ace || using_ina) {
                chooseDispatchType(packet, nullptr);
            } else {
                if (outputInterfaceId.size() != 0 || random_fail_prob <= 0) {
                    InterfaceEntry *outputInterface = ifTable->getInterfaceById(*outputInterfaceId.begin());
                    if (isForwardingInterface(outputInterface))
                        chooseDispatchType(packet, outputInterface);
                    else {
                        EV_INFO << "Output interface " << *outputInterface->getFullName() << " is not forwarding. Discarding!" << endl;
                        numDroppedFrames++;
                        delete packet;
                    }
                } else {
                    EV << "No interfaces detected for the packet (probably due to failure...)" << endl;
                    chooseDispatchType(packet, nullptr);
                }
            }
        }
    }
}

unsigned int BouncingIeee8021dRelay::Compute_CRC16_Simple(std::list<int> bytes, int bytes_size){
    const unsigned int generator = 0x1021;
    unsigned int crc = 0;
    EV << "SEPEHR: The generator is: " << generator << endl;

    for (std::list<int>::iterator it=bytes.begin(); it != bytes.end(); ++it){
        int byte = *it;
        crc ^= ((unsigned int)(byte << 8));
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x8000) != 0) {
                crc = ((unsigned int)((crc << 1) ^ generator));
            }
            else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

std::list<int> BouncingIeee8021dRelay::getInfoFlowByteArray(std::string srcAddr, std::string destAddr, int srcPort, int destPort) {
    std::list<int> bytes;
    std::string token;
    std::stringstream src(srcAddr);
    std::stringstream dest(srcAddr);
    std::string item;
    while (std::getline(src, item, '.'))
    {
       bytes.push_back(std::stoi(item));
    }
    while (std::getline(dest, item, '.'))
    {
       bytes.push_back(std::stoi(item));
    }

    bytes.push_back(srcPort);
    bytes.push_back(destPort);

    return bytes;
}

bool BouncingIeee8021dRelay::packet_has_host_instructions(Packet *packet) {
    EV << "BouncingIeee8021dRelay::packet_has_host_instructions" << endl;
    auto ethHeader = packet->peekAtFront<EthernetMacHeader>();
    return (ethHeader->getPorts_to_dest_idxArraySize() > 0);
}

double BouncingIeee8021dRelay::get_processing_delay() {

    if (reduction_delay <= 0)
        throw cRuntimeError("Invalid reduction dleay!!");

    return reduction_delay;
}

int BouncingIeee8021dRelay::get_port_dir(std::string switch_path, int port_idx) {
    if (port_idx < 0)
        throw cRuntimeError("Invalid port idx");

    if (use_fattree) {
        // fattree
        if (switch_path.find("core[") != std::string::npos) {
            return PORTDIR_DOWNSTREAM;
        }
        if (switch_path.find("agg[") != std::string::npos) {
            if (port_idx < int(fattree_k / 2.0))
                return PORTDIR_DOWNSTREAM;
            else
                return PORTDIR_UPSTREAM;
        }
        if (switch_path.find("edge[") != std::string::npos) {
            if (port_idx < num_servers_under_each_leaf * num_gpus)
                throw cRuntimeError("Failed/Recovered link is toward a source!! How?");
            return PORTDIR_UPSTREAM;
        }
    } else {
        // leaf spine
        if (switch_path.find("spine[") != std::string::npos) {
            return PORTDIR_DOWNSTREAM;
        }
        if (switch_path.find("agg[") != std::string::npos) {
            if (port_idx < num_servers_under_each_leaf * num_gpus)
                throw cRuntimeError("Failed/Recovered link is toward a source!! How?");
            return PORTDIR_UPSTREAM;
        }
    }

    throw cRuntimeError("No port dir returned!");
    return -1;
}

void BouncingIeee8021dRelay::fail_or_recover_message_received(Packet* pkt, int port_idx,
        int link_incident_type) {
    // this is only called when the switch IS NOT directly connected to the failed link
    EV << getFullPath() << ": fail_or_recover_message_received --> port: " << port_idx <<
            ", incident: " << link_incident_type << endl;

    auto eth_header = pkt->peekAtFront<EthernetMacHeader>();

    int affected_port_interface_id = ifTable->getInterface(port_idx)->getInterfaceId();
    bool should_notif_neighbors = false;
    std::set<MacAddress> unreachable_mac_addresses;
    for (int affected_address_idx = 0;
            affected_address_idx < eth_header->getAffected_addressesArraySize();
            affected_address_idx++) {
        auto affected_address = eth_header->getAffected_addresses(affected_address_idx);
        EV << affected_address.str() << " is affected!" << endl;
        std::list<int> interfaceIds;
        switch (link_incident_type) {
            case NOTIF_LINK_FAILURE:
                // we should notif if we have no routes after deleting the failed links
                macTable->removeInterfaceForAddress(affected_address, affected_port_interface_id);
                EV << affected_address.str() << " is not longer reachable via port " << port_idx << endl;
                interfaceIds = macTable->getInterfaceIdForAddress(affected_address);
                if (interfaceIds.size() == 0)
                    unreachable_mac_addresses.insert(affected_address);
                break;
            case NOTIF_LINK_RECOVERY:
                // we should notif if we have no routes before adding the recovered link
                interfaceIds = macTable->getInterfaceIdForAddress(affected_address);
                if (interfaceIds.size() == 0)
                    unreachable_mac_addresses.insert(affected_address);
                macTable->ensureInterfaceForAddress(affected_address, affected_port_interface_id);
                EV << affected_address.str() << " is now reachable via port " << port_idx << endl;
                break;
            default:
                throw cRuntimeError("Unknown link_incident_type");
        }
    }


    should_notif_neighbors = (unreachable_mac_addresses.size() != 0);

    if (should_notif_neighbors) {
        EV << "No more routes remaining... Notif neighbors..." << endl;

        // now we should get all the required ports and send one copy on each!
        std::string switch_path = switch_module->getFullPath();
        int port_dir = get_port_dir(switch_path, port_idx); // upstream or downstream
        EV << getFullPath() << "port " << port_idx << " had dir " << port_dir << endl;
        std::list<int> neighbor_port_idx_list;

        //only disable the unrachable mac addresses rather than all in the notif
        b packetPosition = pkt->getFrontOffset();
        pkt->setFrontIteratorPosition(b(0));
        auto phy_header = pkt->removeAtFront<EthernetPhyHeader>();
        auto eth_header = pkt->removeAtFront<EthernetMacHeader>();
        // first empty it
        eth_header->setAffected_addressesArraySize(0);
        for (auto affected_address : unreachable_mac_addresses) {
            eth_header->insertAffected_addresses(affected_address);
        }
        pkt->insertAtFront(eth_header);
        pkt->insertAtFront(phy_header);
        pkt->setFrontIteratorPosition(packetPosition);

        if (use_fattree) {
            // fattree
            if (switch_path.find("core[") != std::string::npos) {
                // the failed/recovered port is definitely downstream
                for (int i = 0; i < fattree_k; i++) {
                    if (i == port_idx)
                        continue;
                    neighbor_port_idx_list.push_back(i);
                }
            }
            if (switch_path.find("agg[") != std::string::npos) {
                if (port_dir == PORTDIR_UPSTREAM) {
                    // all the upstream has failed/recovered so we're sending notif to all downstream!
                    for (int i = 0; i < int(fattree_k / 2.0); i++) {
                        if (i == port_idx)
                            throw cRuntimeError("2) Upstream has failed, why is port idx in downstream range!");
                        neighbor_port_idx_list.push_back(i);
                    }
                } else {
                    // a downstream has failed/recovered, we should notify all neighbors
                    std::cout << port_dir << endl;
                    std::cout << port_idx << endl;
                    throw cRuntimeError("Notif packet received from agg downstread? how?");
                }
            }
            if (switch_path.find("edge[") != std::string::npos) {
                throw cRuntimeError("2) Edge has become disconnected from the DC!");
            }
        } else {
            // leaf spine
            if (switch_path.find("spine[") != std::string::npos) {
                // the failed/recovered port is definitely downstream
                for (int i = 0; i < num_leafs; i++) {
                    if (i == port_idx)
                        continue;
                    neighbor_port_idx_list.push_back(i);
                }
            }
            if (switch_path.find("agg[") != std::string::npos) {
                throw cRuntimeError("Leaf has become disconnected from the DC!");
            }
        }

        for (auto neighbor_port_idx : neighbor_port_idx_list) {
            EV << "Sending notif packet to port " << neighbor_port_idx << endl;
            InterfaceEntry *ie = ifTable->getInterface(neighbor_port_idx);
            dispatch(pkt->dup(), ie);
        }
    }
}

void BouncingIeee8021dRelay::notif_link_failure_or_recovery_to_neighbors(int port_idx,
        int link_incident_type) {
    Enter_Method("notif_link_failure_or_recovery_to_neighbors");
    // this is only called when we are directly connected to the failed link
    EV << "notif_link_failure_or_recovery_to_neighbors --> port: " << port_idx <<
            ", incident: " << link_incident_type << endl;

    auto port_itr = port_to_mac_address_map.find(port_idx);
    if (port_itr == port_to_mac_address_map.end())
        throw cRuntimeError("Why is port not found!");

//    int route_num = -1;
    int affected_port_interface_id = ifTable->getInterface(port_idx)->getInterfaceId();
    bool should_notif_neighbors = false;
    std::set<MacAddress> unreachable_mac_addresses;
    for (auto affected_address : port_itr->second) {
//        EV << affected_address.str() << " is affected!" << endl;
        std::list<int> interfaceIds;
        switch (link_incident_type) {
            case NOTIF_LINK_FAILURE:
                // we should notif if we have no routes after deleting the failed links
                macTable->removeInterfaceForAddress(affected_address, affected_port_interface_id);
//                EV << affected_address.str() << " is no longer reachable via port " << port_idx << endl;
                interfaceIds = macTable->getInterfaceIdForAddress(affected_address);
                if (interfaceIds.size() == 0)
                    unreachable_mac_addresses.insert(affected_address);
                break;
            case NOTIF_LINK_RECOVERY:
                // we should notif if we have no routes before adding the recovered link
                interfaceIds = macTable->getInterfaceIdForAddress(affected_address);
                if (interfaceIds.size() == 0)
                    unreachable_mac_addresses.insert(affected_address);
                macTable->ensureInterfaceForAddress(affected_address, affected_port_interface_id);
                EV << affected_address.str() << " is now reachable via port " << port_idx << endl;
                break;
            default:
                throw cRuntimeError("Unknown link_incident_type");
        }
    }

    should_notif_neighbors = (unreachable_mac_addresses.size() != 0);

    if (should_notif_neighbors) {
        EV << "2) No more routes remaining for " << unreachable_mac_addresses.size() << " addresses. Notif neighbors..." << endl;
        std::string notif_pkt_name = "";
        switch (link_incident_type) {
            case NOTIF_LINK_RECOVERY:
                notif_pkt_name = "recover";
                break;
            case NOTIF_LINK_FAILURE:
                notif_pkt_name = "fail";
                break;
            default:
                throw cRuntimeError("Unsupported incident!");
        }

        if (failure_or_recovery_notif_pkt == nullptr)
            throw cRuntimeError("how is failure_or_recovery_notif_pkt still nullptr!");

        auto notif_pkt = failure_or_recovery_notif_pkt->dup();
        notif_pkt->setName(notif_pkt_name.c_str());

        b packetPosition = notif_pkt->getFrontOffset();
        notif_pkt->setFrontIteratorPosition(b(0));
        auto phy_header = notif_pkt->removeAtFront<EthernetPhyHeader>();
        auto eth_header = notif_pkt->removeAtFront<EthernetMacHeader>();

        // first empty it
        eth_header->setAffected_addressesArraySize(0);
        for (auto affected_address : unreachable_mac_addresses) {
            eth_header->insertAffected_addresses(affected_address);
        }

        notif_pkt->insertAtFront(eth_header);
        notif_pkt->insertAtFront(phy_header);
        notif_pkt->setFrontIteratorPosition(packetPosition);

        // now we should get all the required ports and send one copy on each!
        std::string switch_path = switch_module->getFullPath();
        int port_dir = get_port_dir(switch_path, port_idx); // upstream or downstream
        EV << "port " << port_idx << " had dir " << port_dir << endl;
        std::list<int> neighbor_port_idx_list;

        if (use_fattree) {
            // fattree
            if (switch_path.find("core[") != std::string::npos) {
                // the failed/recovered port is definitely downstream
                for (int i = 0; i < fattree_k; i++) {
                    if (i == port_idx)
                        continue;
                    neighbor_port_idx_list.push_back(i);
                }
            }
            if (switch_path.find("agg[") != std::string::npos) {
                if (port_dir == PORTDIR_UPSTREAM) {
                    // all the upstream has failed/recovered so we're sending notif to all downstream!
                    for (int i = 0; i < int(fattree_k / 2.0); i++) {
                        if (i == port_idx)
                            throw cRuntimeError("Upstream has failed, why is port idx in downstream range!");
                        neighbor_port_idx_list.push_back(i);
                    }
                } else {
                    // a downstream has failed/recovered, we should notify all neighbors
                    for (int i = 0; i < fattree_k; i++) {
                        if (i == port_idx)
                            continue;
                        neighbor_port_idx_list.push_back(i);
                    }
                }
            }
            if (switch_path.find("edge[") != std::string::npos) {
                std::cout << switch_path << endl;
                throw cRuntimeError("Edge has become disconnected from the DC!");
            }
        } else {
            // leaf spine
            if (switch_path.find("spine[") != std::string::npos) {
                // the failed/recovered port is definitely downstream
                for (int i = 0; i < num_leafs; i++) {
                    if (i == port_idx)
                        continue;
                    neighbor_port_idx_list.push_back(i);
                }
            }
            if (switch_path.find("agg[") != std::string::npos) {
                throw cRuntimeError("Leaf has become disconnected from the DC!");
            }
        }

        for (auto neighbor_port_idx : neighbor_port_idx_list) {
            EV << "2) Sending notif packet to port " << neighbor_port_idx << endl;
            InterfaceEntry *ie = ifTable->getInterface(neighbor_port_idx);
            if (ie == nullptr)
                throw cRuntimeError("For notif packet ie should never be nullptr");
            dispatch(notif_pkt->dup(), ie);
        }

        delete notif_pkt;
    } else
        EV << "Not sending any notifs" << endl;
}

void BouncingIeee8021dRelay::handleSelfMessage(cMessage *message) {
    // this is over-written to support INA
    EV << "BouncingIeee8021dRelay::handleSelfMessage" << endl;
    if (message->getKind() == PASS) {
        unsigned long query_id = message->par("query_id").longValue();
        unsigned long seq_num = message->par("seq_num").longValue();
        forward_packet_based_on_ina(nullptr, query_id, seq_num);
        delete message;
        return;
    } else if (message->getKind() == RECOVER_LINK) {
        int port_idx = message->par("port_idx").longValue();
        EV << getFullPath() << ": Recovering port " << port_idx << endl;

        auto port_itr = port_to_recovery_time.find(port_idx);
        if (port_itr == port_to_recovery_time.end()) {
            std::cout << port_idx << endl;
            throw cRuntimeError("How is port not stored for recovery!");
        }
        port_to_recovery_time.erase(port_itr);
        failed_ports.erase(port_idx);
        notif_link_failure_or_recovery_to_neighbors(port_idx, NOTIF_LINK_RECOVERY);


        // find the neighbor's relay unit and correct port to fail or recover if needed
        auto neighbor_path_itr = port_idx_to_neighboring_switch_path.find(port_idx);
        if (neighbor_path_itr == port_idx_to_neighboring_switch_path.end()) {
            throw cRuntimeError("If the neighbor is not a switch, how did we fail a link to it!");
        }
        cModule* neighbor_module = getModuleByPath(neighbor_path_itr->second.c_str())->getParentModule()->getParentModule();
        std::string neighbor_relay_path = neighbor_module->getFullPath() + ".relayUnit";
        BouncingIeee8021dRelay* neighbor_relay = check_and_cast<BouncingIeee8021dRelay*>(getModuleByPath(neighbor_relay_path.c_str()));
        // now get the neighbor port
        std::regex ethPattern(R"(\beth\[(\d+)\])");  // matches eth[number]
        std::smatch match;
        int neighbor_port = -1;
        if (std::regex_search(neighbor_path_itr->second, match, ethPattern)) {
            neighbor_port = std::stoi(match[1]);  // extract the captured number
        }
        if (neighbor_port < 0)
            throw cRuntimeError("Neighbor port not found!");

        EV << neighbor_relay_path << ": Recovering neighbor port " << neighbor_port << endl;
        auto neighbor_port_itr = neighbor_relay->port_to_recovery_time.find(neighbor_port);
        if (neighbor_port_itr == neighbor_relay->port_to_recovery_time.end()) {
            std::cout << neighbor_relay->getFullPath() << endl;
            std::cout << neighbor_port << endl;
            throw cRuntimeError("Neighbor: How is port not stored for recovery!");
        }
        neighbor_relay->port_to_recovery_time.erase(neighbor_port_itr);
        neighbor_relay->failed_ports.erase(neighbor_port);
        neighbor_relay->notif_link_failure_or_recovery_to_neighbors(neighbor_port, NOTIF_LINK_RECOVERY);

        delete message;
        return;
    }
    LayeredProtocolBase::handleSelfMessage(message);
}

BouncingIeee8021dRelay* BouncingIeee8021dRelay::get_upstream_switch_realy(unsigned long query_id,
        unsigned long seq_num,
        int leaf_aggregation_num,
        int spine_aggregation_num,
        int core_aggregation_num) {

    Enter_Method("get_upstream_switch_realy");

    std::string query_seqnum_str;
    if (use_ecmp) {
        // we have one aggregation tree per query
        query_seqnum_str = std::to_string(query_id);
    } else if (use_per_pkt_lb) {
        // we have multiple aggregation tree --> different packets of a flow might traverse different aggregation trees
        query_seqnum_str = std::to_string(query_id) + "_" + std::to_string(seq_num);
    } else
        throw cRuntimeError("invalid LB for ina!");

    std::string topo_path = getParentModule()->getParentModule()->getFullPath();
    std::string relay_module_path;
    if (isSwitchToR()) {
        query_seqnum_str += "_tor";
        if (spine_aggregation_num <= 1 && core_aggregation_num <= 1)
            // not going to upstream
            return nullptr;
        unsigned long query_seqnum_hash = header_hash(query_seqnum_str);
        if (use_fattree) {
            int port_num = int(fattree_k / 2.0);
            int aggregate_index = query_seqnum_hash % port_num;
            relay_module_path = topo_path + ".agg[" +
                    std::to_string(aggregate_index) + "].relayUnit";
        } else {
            int port_num = num_spines;
            int spine_index = query_seqnum_hash % port_num;
            relay_module_path = topo_path + ".spine[" +
                    std::to_string(spine_index) + "].relayUnit";
        }
    } else if (isSwitchAggregate()) {
        query_seqnum_str += "_spine";
        if (use_fattree) {
            if (core_aggregation_num <= 1)
                // not going to upstream
                return nullptr;

            int port_num = int(fattree_k / 2.0);
            unsigned long query_seqnum_hash = header_hash(query_seqnum_str);
            int core_index = query_seqnum_hash % port_num;

            relay_module_path = topo_path + ".core[" +
                    std::to_string(core_index) + "].relayUnit";
        } else
            // no upstream
            return nullptr;
    } else if (isSwitchCore())
        // no upstream
        return nullptr;

    auto upstream_realy = check_and_cast<BouncingIeee8021dRelay*>(getModuleByPath(relay_module_path.c_str()));
    EV << "Upstream for " << getFullPath() << " is " << upstream_realy->getFullPath() << endl;
    return upstream_realy;
}

bool BouncingIeee8021dRelay::ina_has_free_aggregation_pool_spots(unsigned long query_id,
        unsigned long seq_num,
        int leaf_aggregation_num,
        int spine_aggregation_num,
        int core_aggregation_num) {

    Enter_Method("ina_has_free_aggregation_pool_spots");
    EV << "BouncingIeee8021dRelay::ina_has_free_aggregation_pool_spots" << endl;

    // we cannot check ina_aggregation_pool_spots_remaining because it changes through time
    if (ina_aggregation_pool_spots_max <= 0) {
        std::cout << ina_aggregation_pool_spots_max << endl;
        throw cRuntimeError("Invalid ina_aggregation_pool_spots_max");
    }

    std::string queryid_seqnum_str = std::to_string(query_id) + "_" + std::to_string(seq_num);
    if (ina_aggregation_pool_allocations.find(queryid_seqnum_str) !=
            ina_aggregation_pool_allocations.end()) {
        EV << "Already allocated an spot to this" << endl;
        return true;
    }

    if (ina_aggregation_pool_spots_remaining <= 0)
        return false;

    BouncingIeee8021dRelay* upstream_relay = get_upstream_switch_realy(query_id, seq_num,
            leaf_aggregation_num,
            spine_aggregation_num,
            core_aggregation_num);
    if (upstream_relay != nullptr) {
        return upstream_relay->ina_has_free_aggregation_pool_spots(query_id, seq_num,
                leaf_aggregation_num,
                spine_aggregation_num,
                core_aggregation_num);
    }

    return true;

}

void BouncingIeee8021dRelay::ina_allocate_free_aggregation_pool_spots(unsigned long query_id,
        unsigned long seq_num,
        int leaf_aggregation_num,
        int spine_aggregation_num,
        int core_aggregation_num) {

    // allready checked we can allocate... now allocate

    Enter_Method("ina_allocate_free_aggregation_pool_spots");
    EV << "BouncingIeee8021dRelay::ina_allocate_free_aggregation_pool_spots" << endl;

    std::string queryid_seqnum_str = std::to_string(query_id) + "_" + std::to_string(seq_num);
    if (ina_aggregation_pool_allocations.find(queryid_seqnum_str) !=
            ina_aggregation_pool_allocations.end()) {
        EV << "2) Already allocated an spot to this" << endl;
        return;
    }

    if (ina_aggregation_pool_spots_remaining <= 0)
        throw cRuntimeError("No spot is left to be allocated!");

    BouncingIeee8021dRelay* upstream_relay = get_upstream_switch_realy(query_id, seq_num,
            leaf_aggregation_num,
            spine_aggregation_num,
            core_aggregation_num);
    if (upstream_relay != nullptr) {
        // allocate in upstreams
        upstream_relay->ina_allocate_free_aggregation_pool_spots(query_id, seq_num,
                leaf_aggregation_num,
                spine_aggregation_num,
                core_aggregation_num);
    }

    // allocate in us
    ina_aggregation_pool_allocations.insert(queryid_seqnum_str);
    ina_aggregation_pool_spots_remaining--;

}

void BouncingIeee8021dRelay::forward_packet_based_on_ina(Packet *packet,
        unsigned long query_id,
        unsigned long seq_num) {
    EV << "BouncingIeee8021dRelay::forward_packet_based_on_ina" << endl;

    // set tree direction accordingly
    int tree_direction;
    if (packet == nullptr) {
        // packet is traversing up after reduction
        tree_direction = TREE_UP;
        std::pair<unsigned long, unsigned long> query_seqnum_pair =
                        std::make_pair(query_id, seq_num);
        auto query_seqnum_found_itr = query_seqnum_to_port_pkt_map.find(query_seqnum_pair);
        if (query_seqnum_found_itr == query_seqnum_to_port_pkt_map.end()) {
            std::cout << query_id << endl;
            std::cout << seq_num << endl;
            throw cRuntimeError("INA: query id and seq num information does not exist!");
        }

        // get a copy of one of the packets in the aggregation group as a candidate packet
        packet = query_seqnum_found_itr->second.begin()->second->dup();

        auto frame = packet->peekAtFront<EthernetMacHeader>();
        if (frame->getQuery_id() != query_id ||
                frame->getSeq_num() != seq_num)
            throw cRuntimeError("frame->getQuery_id() != query_id || frame->getSeq_num() != seq_num");
    } else {
        // read info from packet
        // reduction result is traversing down
        auto frame = packet->peekAtFront<EthernetMacHeader>();
        tree_direction = frame->getTree_direction();
        if (tree_direction != TREE_DOWN)
            throw cRuntimeError("The packet must be on tree down path by now!");
        query_id = frame->getQuery_id();
        seq_num = frame->getSeq_num();
    }

    if (packet == nullptr)
        throw cRuntimeError("packet should not be nullptr at this point!");

    auto frame = packet->peekAtFront<EthernetMacHeader>();


    // if tree_direction == TREE_UP, check if we reached the highest tree layer
    int base_port_idx = -1;
    int port_num = -1;


    std::string query_seqnum_str;
    if (use_ecmp) {
        // we have one aggregation tree per query
        query_seqnum_str = std::to_string(query_id);
    } else if (use_per_pkt_lb) {
        // we have multiple aggregation tree --> different packets of a flow might traverse different aggregation trees
        query_seqnum_str = std::to_string(query_id) + "_" + std::to_string(seq_num);
    } else
        throw cRuntimeError("invalid LB for ina!");

    if (tree_direction == TREE_UP) {

        if (isSwitchToR()) {

            query_seqnum_str += "_tor";
            if (frame->getIna_spine_aggregation_num() <= 1 &&
                    frame->getIna_core_aggregation_num() <= 1) {

                EV << "Packet has reached the highest node in the reduction tree" << endl;
                b packetPosition = packet->getFrontOffset();
                packet->setFrontIteratorPosition(b(0));
                auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
                auto ethHeader = packet->removeAtFront<EthernetMacHeader>();

                ethHeader->setTree_direction(TREE_DOWN);
                tree_direction = TREE_DOWN;

                packet->insertAtFront(ethHeader);
                packet->insertAtFront(phyHeader);
                packet->setFrontIteratorPosition(packetPosition);

            } else {

                // the first num_servers_under_each_leaf * num_gpus ports are connected to downstream gpus
                base_port_idx = num_servers_under_each_leaf * num_gpus;
                if (use_fattree) {
                    port_num = int(fattree_k / 2.0);
                } else {
                    port_num = num_spines;
                }

            }
        } else if (isSwitchAggregate()) {

            query_seqnum_str += "_spine";
            if (frame->getIna_core_aggregation_num() <= 1) {

                EV << "2) Packet has reached the highest node in the reduction tree" << endl;
                b packetPosition = packet->getFrontOffset();
                packet->setFrontIteratorPosition(b(0));
                auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
                auto ethHeader = packet->removeAtFront<EthernetMacHeader>();

                ethHeader->setTree_direction(TREE_DOWN);
                tree_direction = TREE_DOWN;

                packet->insertAtFront(ethHeader);
                packet->insertAtFront(phyHeader);
                packet->setFrontIteratorPosition(packetPosition);

            } else {

                if (use_fattree) {
                    // the first k/2 to downstream tors
                    base_port_idx = int(fattree_k / 2.0);
                    port_num = int(fattree_k / 2.0);
                } else
                    throw cRuntimeError("How are we going up in spine layer in two-tier leaf-spine!");

            }
        } else if (isSwitchCore()) {

            query_seqnum_str += "_core";
            EV << "3) Packet has reached the highest node in the reduction tree" << endl;
            b packetPosition = packet->getFrontOffset();
            packet->setFrontIteratorPosition(b(0));
            auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
            auto ethHeader = packet->removeAtFront<EthernetMacHeader>();

            ethHeader->setTree_direction(TREE_DOWN);
            tree_direction = TREE_DOWN;

            packet->insertAtFront(ethHeader);
            packet->insertAtFront(phyHeader);
            packet->setFrontIteratorPosition(packetPosition);

        }
    }

    // now forward packet

    if (tree_direction == TREE_UP) {

        EV << "Forwarding packet up" << endl;
        if (port_num <= 0 || base_port_idx < 0)
            throw cRuntimeError("port_num <= 0 || base_port_idx <= 0");
        unsigned long query_seqnum_hash = header_hash(query_seqnum_str);
        int port_offset = query_seqnum_hash % port_num;
        int port_idx = base_port_idx + port_offset;
        EV << "Forwarding to port " << port_idx << endl;
        InterfaceEntry *ie = ifTable->getInterface(port_idx);
        dispatch(packet, ie);

    } else if (tree_direction == TREE_DOWN) {

        EV << "Forwarding original packets down" << endl;

        // delete the original packet, we always set a duplicate up so coming down, we already have
        // originals stored in the switch
        delete packet;

        std::pair<unsigned long, unsigned long> query_seqnum_pair =
                        std::make_pair(query_id, seq_num);
        auto query_seqnum_found_itr = query_seqnum_to_port_pkt_map.find(query_seqnum_pair);
        if (query_seqnum_found_itr == query_seqnum_to_port_pkt_map.end()) {
            std::cout << query_id << endl;
            std::cout << seq_num << endl;
            throw cRuntimeError("2) INA: query id and seq num information does not exist!");
        }

        for (auto port_pkt_itr : query_seqnum_found_itr->second) {
            EV << "Forwarding a copy down to port " << port_pkt_itr.first->getIndex() << endl;

            // set the tree direction for original packets to tree_down
            auto packet_to_send_down = port_pkt_itr.second;
            b packetPosition = packet_to_send_down->getFrontOffset();
            packet_to_send_down->setFrontIteratorPosition(b(0));
            auto phyHeader = packet_to_send_down->removeAtFront<EthernetPhyHeader>();
            auto ethHeader = packet_to_send_down->removeAtFront<EthernetMacHeader>();

            ethHeader->setTree_direction(TREE_DOWN);

            packet_to_send_down->insertAtFront(ethHeader);
            packet_to_send_down->insertAtFront(phyHeader);
            packet_to_send_down->setFrontIteratorPosition(packetPosition);

            dispatch(packet_to_send_down, port_pkt_itr.first);
        }
        query_seqnum_found_itr->second.clear();
        query_seqnum_to_port_pkt_map.erase(query_seqnum_found_itr);

        // free up allocation
        if (ina_aggregation_pool_allocations.find(std::to_string(query_id) + "_" + std::to_string(seq_num)) !=
                ina_aggregation_pool_allocations.end()) {
            // for the first packet, this is not in the list
            ina_aggregation_pool_allocations.erase(std::to_string(query_id) + "_" + std::to_string(seq_num));
            ina_aggregation_pool_spots_remaining++;
        }
        if (ina_aggregation_pool_spots_remaining > ina_aggregation_pool_spots_max)
            throw cRuntimeError("How is ina_aggregation_pool_spots_remaining > ina_aggregation_pool_spots_max");

    }
}

bool BouncingIeee8021dRelay::orca_should_send_to_agent(Packet *packet) {
    auto ethHeader = packet->peekAtFront<EthernetMacHeader>();
    std::string pkt_name = packet->getName();

    if (!ethHeader->getUsing_orca())
        return false;

    // pkt is not data
    if (!isPacketAppropriateForMulticast(pkt_name))
        return false;

    // data is not sent to agent when tree is already initiated
    if (initiate_tree && pkt_name.compare("data") == 0)
        return false;

    if (ethHeader->getOrca_pkt_seen_agent())
        return false;

    if (ethHeader->getHop_count() <= 2)
        return false;

    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        // do not react if not in edge
        if (switch_path.find("edge[") == std::string::npos)
            return false;
    } else {
        // do not react if not in leaf
        if (switch_path.find("agg[") == std::string::npos)
            return false;
    }

    return true;
}

void BouncingIeee8021dRelay::filter_rsbf_header(Packet *packet) {

    std::string pkt_name = packet->getName();
    if (!isPacketAppropriateForMulticast(pkt_name))
        return;

    // do not filter data packets when tree is already initiated
    if (initiate_tree && pkt_name.compare("data") == 0)
        return;

    EV << "BouncingIeee8021dRelay::filter_rsbf_header" << endl;
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
    auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
    auto ipHeader = packet->removeAtFront<Ipv4Header>();
    auto udpHeader = packet->removeAtFront<UdpHeader>();
    auto chunk = packet->removeAtFront<GenericAppMsg>();

    int bitstream_info_idx = -1;
    if (use_fattree) {
        // fattree
        switch(ethHeader->getHop_count()) {
            case 1:
            case 2:
                bitstream_info_idx = 0;
                break;
            case 3:
                bitstream_info_idx = 1;
                break;
            case 4:
            case 5:
                bitstream_info_idx = 2;
                break;
            default:
                std::cout << "Hop count: " << ethHeader->getHop_count() << endl;
                throw cRuntimeError("FAT-TREE: Hop count not supported for RSBF!");
        }
    } else {
        // leaf-spine
        switch(ethHeader->getHop_count()) {
            case 1:
                bitstream_info_idx = 0;
                break;
            case 2:
                bitstream_info_idx = 1;
                break;
            case 3:
                bitstream_info_idx = 2;
                break;
            default:
                std::cout << "Hop count: " << ethHeader->getHop_count() << endl;
                throw cRuntimeError("LEAF-SPINE: Hop count not supported for RSBF!");
        }
    }

    if (bitstream_info_idx < 0 || bitstream_info_idx > chunk->getRsbf_bitstream_listArraySize()) {
        std::cout << "bitstream_info_idx: " << bitstream_info_idx << endl;
        std::cout << "chunk->getRsbf_bitstream_listArraySize(): " << chunk->getRsbf_bitstream_listArraySize() << endl;
        throw cRuntimeError("bitstream_info_idx surpassed Rsbf_bitstream_listArraySize()");
    }

    std::string bitstream = chunk->getRsbf_bitstream_list(bitstream_info_idx);
    unsigned int bitstream_bit_size = bitstream.size();
    if (bitstream_bit_size % 8 != 0)
        throw cRuntimeError("bitstream_bit_size % 8 != 0");
    B bytes_to_be_removed = B(bitstream_bit_size / 8);

    EV << "hop count: " << ethHeader->getHop_count() << ", Bytes to be removed: " << bytes_to_be_removed << endl;

    // don't let packet length fall below 200 B
    if (chunk->getChunkLength() > bytes_to_be_removed + B(200)) {
        chunk->setChunkLength(chunk->getChunkLength() - bytes_to_be_removed);
        chunk->setRsbf_removed_bytes(chunk->getRsbf_removed_bytes() + bytes_to_be_removed.get());
        udpHeader->setTotalLengthField(udpHeader->getTotalLengthField() - bytes_to_be_removed);
        ipHeader->setTotalLengthField(ipHeader->getTotalLengthField() - bytes_to_be_removed);
        ethHeader->setPayload_length(ethHeader->getPayload_length() - bytes_to_be_removed);
        ethHeader->setTotal_length(ethHeader->getTotal_length() - bytes_to_be_removed);
    }

    packet->insertAtFront(chunk);
    packet->insertAtFront(udpHeader);
    packet->insertAtFront(ipHeader);
    packet->insertAtFront(ethHeader);
    packet->insertAtFront(phyHeader);
    packet->setFrontIteratorPosition(packetPosition);

}

void BouncingIeee8021dRelay::fw_rsbf_based(Packet *packet,
        std::set<unsigned long> fw_port_idx_set,
        int port_info_idx,
        long candidate_start_from_idx) {

    EV << "BouncingIeee8021dRelay::fw_rsbf_based called!" << endl;

    std::string pkt_name = packet->getName();
    if (!isPacketAppropriateForMulticast(pkt_name))
        return;

    if (candidate_start_from_idx < 0) {
        std::cout << candidate_start_from_idx << endl;
        throw cRuntimeError("candidate_start_from_idx < 0");
    }

    if (!use_fattree && (num_spines < 0 || num_leafs < 0 ||
            num_servers_under_each_leaf < 0 || num_gpus < 0))
        throw cRuntimeError("Invalid Leaf-Spine parameters...!");

    if (use_fattree && fattree_k < 0)
        throw cRuntimeError("Invalid Fattree parameters");

    EV << "BouncingIeee8021dRelay::fw_rsbf_based" << endl;
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
    auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
    unsigned int hop_count = ethHeader->getHop_count();
    auto ipHeader = packet->removeAtFront<Ipv4Header>();
    auto udpHeader = packet->removeAtFront<UdpHeader>();
    auto chunk = packet->removeAtFront<GenericAppMsg>();

    std::list<unsigned long> potential_port_idx_list;
    int base_port_idx = -1; // base port idx
    int iteration_num = -1; // number of ports to iterate
    int bitstream_info_idx = -1;    // index of bitstream to read

    if (use_fattree) {
        throw cRuntimeError("This part of code should be re-written after we changed shape of fattree to have 2 servers per edge");
        int num_aggs_per_pod = int(fattree_k/2);
        int num_servers_per_tor = int(fattree_k/2);
        int num_cores_per_agg = int(fattree_k/2);
        int num_tors_per_pod = int(fattree_k/2);
        int num_pods = fattree_k;
        switch (hop_count) {
            case 1:
                // ToR upstream
                base_port_idx = num_servers_per_tor * num_gpus;
                iteration_num = num_aggs_per_pod;
                bitstream_info_idx = 0;
                break;
            case 2:
                // Aggregate upstream
                base_port_idx = num_tors_per_pod;
                iteration_num = num_cores_per_agg;
                bitstream_info_idx = 0;
                break;
            case 3:
                // Core downstream
                base_port_idx = 0;
                iteration_num = num_pods;
                bitstream_info_idx = 1;
                break;
            case 4:
                // Aggregate downstream
                base_port_idx = 0;
                iteration_num = num_tors_per_pod;
                bitstream_info_idx = 2;
                break;
            case 5:
                // ToR downstream
                base_port_idx = 0;
                iteration_num = num_servers_per_tor * num_gpus;
                bitstream_info_idx = 2;
                break;
            default:
                throw cRuntimeError("FT RSBF: Unsupported hop count!");
        }
    } else {
        switch (hop_count) {
            case 1:
                // Leaf upstream
                base_port_idx = num_servers_under_each_leaf * num_gpus;
                iteration_num = num_spines;
                bitstream_info_idx = 0;
                break;
            case 2:
                // spine downstream
                base_port_idx = 0;
                iteration_num = num_leafs;
                bitstream_info_idx = 1;
                break;
            case 3:
                // leaf downstream
                base_port_idx = 0;
                iteration_num = num_servers_under_each_leaf * num_gpus;
                bitstream_info_idx = 2;
                break;
            default:
                throw cRuntimeError("LS RSBF: Unsupported hop count!");
        }
    }
    for (int port_idx = 0;
            port_idx < iteration_num;
            port_idx++) {
        auto port_found_itr = fw_port_idx_set.find(base_port_idx + port_idx);
        // leaf upstream ports start from num_servers_under_each_leaf * num_gpus
        if (port_found_itr == fw_port_idx_set.end())
            potential_port_idx_list.push_back(base_port_idx + port_idx);
    }

    if (bitstream_info_idx < 0 || bitstream_info_idx > chunk->getRsbf_bitstream_listArraySize()) {
        std::cout << "2) bitstream_info_idx: " << bitstream_info_idx << endl;
        std::cout << "2) chunk->getRsbf_bitstream_listArraySize(): " << chunk->getRsbf_bitstream_listArraySize() << endl;
        throw cRuntimeError("bitstream_info_idx surpassed Rsbf_bitstream_listArraySize()");
    }

    std::string bitstream = chunk->getRsbf_bitstream_list(bitstream_info_idx);
    std::list<unsigned long> seed_list;
    for (int i = 0; i < chunk->getRsbf_seed_listArraySize(); i++) {
        seed_list.push_back(chunk->getRsbf_seed_list(i));
    }


    packet->insertAtFront(chunk);
    packet->insertAtFront(udpHeader);
    packet->insertAtFront(ipHeader);
    packet->insertAtFront(ethHeader);
    packet->insertAtFront(phyHeader);
    packet->setFrontIteratorPosition(packetPosition);

    for (auto& potential_port_idx : potential_port_idx_list) {
        bool matched = true;
        std::string port_identification = std::to_string(port_info_idx) +
                "_" + std::to_string(potential_port_idx);
        for (auto& seed : seed_list) {
            std::size_t m_hash = hashWithSeed(port_identification, seed);
            m_hash %= bitstream.size();
            if (bitstream[m_hash] != '1') {
                matched = false;
                break;
            }
        }
        if (!matched)
            continue;


        EV << "Redundant send to index: " << potential_port_idx << endl;
        auto packet_to_send = packet->dup();
        packet_to_send->setFrontIteratorPosition(b(0));
        auto phyHeader = packet_to_send->removeAtFront<EthernetPhyHeader>();
        auto ethHeader = packet_to_send->removeAtFront<EthernetMacHeader>();
        ethHeader->setStart_from_idx(candidate_start_from_idx);
        ethHeader->setRsbf_redundant_send(true);
        packet_to_send->insertAtFront(ethHeader);
        packet_to_send->insertAtFront(phyHeader);
        packet_to_send->setFrontIteratorPosition(packetPosition);
        InterfaceEntry *ie = ifTable->getInterface(potential_port_idx);
        dispatch(packet_to_send, ie);
    }

}

unsigned int BouncingIeee8021dRelay::chunk_data_get_random_dst_gpu(unsigned int out_port_idx) {
    int relative_server_idx = int(out_port_idx * 1.0 / num_gpus);
    unsigned int random_idx = getRandomIdx();
    random_idx %= num_gpus;
    unsigned int new_idx = (relative_server_idx * num_gpus) + random_idx;
    EV << "Output port idx changed from " << out_port_idx << " to " << new_idx << endl;
    return new_idx;
}

void BouncingIeee8021dRelay::elmo_pop_pkt_header_overhead(Packet *packet) {

    EV << "BouncingIeee8021dRelay::elmo_pop_pkt_header_overhead" << endl;

    std::string pkt_name = packet->getName();

    // does not apply to non-data packets!
    if (pkt_name.compare("data") != 0)
        return;

    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
    auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
    auto ipHeader = packet->removeAtFront<Ipv4Header>();
    auto udpHeader = packet->removeAtFront<UdpHeader>();
    auto chunk = packet->removeAtFront<GenericAppMsg>();

    if (chunk->getElmo_overhead_pop_bytes() == 0)
        throw cRuntimeError("chunk->getElmo_overhead_pop_bytes() == 0");

    B bytes_to_be_removed = B(chunk->getElmo_overhead_pop_bytes());

    EV << "Elmo Bytes to be removed: " << bytes_to_be_removed << endl;

    // don't let packet length fall below 200 B
    if (chunk->getChunkLength() > bytes_to_be_removed + B(200)) {
        chunk->setChunkLength(chunk->getChunkLength() - bytes_to_be_removed);
        udpHeader->setTotalLengthField(udpHeader->getTotalLengthField() - bytes_to_be_removed);
        ipHeader->setTotalLengthField(ipHeader->getTotalLengthField() - bytes_to_be_removed);
        ethHeader->setPayload_length(ethHeader->getPayload_length() - bytes_to_be_removed);
        ethHeader->setTotal_length(ethHeader->getTotal_length() - bytes_to_be_removed);
    }

    packet->insertAtFront(chunk);
    packet->insertAtFront(udpHeader);
    packet->insertAtFront(ipHeader);
    packet->insertAtFront(ethHeader);
    packet->insertAtFront(phyHeader);
    packet->setFrontIteratorPosition(packetPosition);
}

void BouncingIeee8021dRelay::forward_packet_based_on_host_instructions(Packet *packet) {

    EV << "BouncingIeee8021dRelay::forward_packet_based_on_host_instructions" << endl;

    if (random_fail_prob > 0 && recovery_type != DROP_NOTIF)
        throw cRuntimeError("forward_packet_based_on_host_instructions only works with RECOVERY_NOTIF");

    auto ethHeader = packet->peekAtFront<EthernetMacHeader>();

    if (ethHeader->getUsing_elmo()) {
        elmo_pop_pkt_header_overhead(packet);
    }

    std::string pkt_name = packet->getName();
    bool rsbf_is_pkt_redundant = (use_rsbf && ethHeader->getRsbf_redundant_send());

    if (!initiate_tree &&
            (pkt_name.compare("treeInit") == 0 ||
                    pkt_name.compare("treeFin") == 0)) {
        std::cout << pkt_name << endl;
        throw cRuntimeError("Invalid packet name!");
    }

    if (initiate_tree) {
        unsigned long flow_id = ethHeader->getFlow_id();
        if (pkt_name.compare("treeInit") == 0) {
            EV << "Initiating tree for flow id: " << flow_id << endl;
            tree_flow_id_set.insert(flow_id);
        } else if (pkt_name.compare("treeFin") == 0) {
            EV << "Removing tree info for flow id: " << flow_id << endl;
            tree_flow_id_set.erase(flow_id);
        } else if (pkt_name.compare("data") == 0) {
//            if (tree_flow_id_set.find(flow_id) == tree_flow_id_set.end()) {
//                std::cout << "Flow id is: " << flow_id << endl;
//                throw cRuntimeError("No treeInit packet has passed this switch");
//            }
        } else
            throw cRuntimeError("Not supported packet type!");
        max_simultaneous_trees = std::max(max_simultaneous_trees, tree_flow_id_set.size());
    }

    std::set<int> potential_next_interface_ids;
    if (use_cepheus && pkt_name.compare("treeInit") == 0) {
        // filter out the servers
        auto pkt_dup = packet->dup();
        pkt_dup->setFrontIteratorPosition(b(0));
        pkt_dup->removeAtFront<EthernetPhyHeader>();
        pkt_dup->removeAtFront<EthernetMacHeader>();
        pkt_dup->removeAtFront<Ipv4Header>();
        pkt_dup->removeAtFront<UdpHeader>();
        auto payload = pkt_dup->removeAtFront<GenericAppMsg>();
        int init_tree_pkt_idx = payload->getInit_tree_pkt_idx();
        if (init_tree_pkt_idx < 0)
            throw cRuntimeError("Invalid init_tree_pkt_idx...");
        int start = init_tree_pkt_idx * ceil(183.0/num_gpus);
        int end = (init_tree_pkt_idx+1) * ceil(183.0/num_gpus);
        end = std::min(end, int(payload->getDst_idx_listArraySize()));
        for (int k = start; k < end; k++) {
//            std::cout << "dst is " << payload->getDst_idx_list(k) << endl;
            std::string switch_path = getFullPath();
            size_t pos = switch_path.find('.'); // Find the first dot
            std::string topology_name;
            if (pos != std::string::npos) {
                topology_name = switch_path.substr(0, pos); // Extract substring from 0 to position of '.'
            } else
                throw cRuntimeError("topology name not found!");

            for (int m_gpu_idx = 0; m_gpu_idx < num_gpus; m_gpu_idx++) {
                std::string server_name = topology_name +
                        ".server[" + std::to_string(payload->getDst_idx_list(k)) +
                        "].eth[" + std::to_string(m_gpu_idx) + "].mac";
                auto server_mac = check_and_cast<AugmentedEtherMacFullDuplex *>(getModuleByPath(server_name.c_str()));

                std::list<int> destInterfaceIds =
                        macTable->getInterfaceIdForAddress(server_mac->getMacAddress());

                for (auto dst_ie_it = destInterfaceIds.begin();
                        dst_ie_it != destInterfaceIds.end();
                        dst_ie_it++) {
                    potential_next_interface_ids.insert(*dst_ie_it);
                }
            }
        }
        delete pkt_dup;
    }

//    auto ipHeader = packet->removeAtFront<Ipv4Header>();
    unsigned int start_from_idx = ethHeader->getStart_from_idx();
    EV << "start_from_idx is " << start_from_idx << endl;
    if (start_from_idx >= ethHeader->getPorts_to_dest_idxArraySize())
        throw cRuntimeError("start_from_idx >= ethHeader->getPath_to_dest_idxArraySize(), overflow!!");
    auto output_port_array = ethHeader->getPorts_to_dest_idx(start_from_idx);
    auto next_start_from_array = ethHeader->getJump_to_idx(start_from_idx);

    // used to identify distinct trees
    unsigned long tree_identification = ethHeader->getFlow_id();
    tree_ids_set.insert(tree_identification);

    b packetPosition = packet->getFrontOffset();

    if (orca_should_send_to_agent(packet)) {
        // this happens on the tor and we should not consider failure!
        EV << "Sending packet to agent..." << endl;
        // randomly choose one of the agents
        std::random_device rd;                          // Non-deterministic seed
        std::mt19937 gen(rd());                         // Mersenne Twister engine
        std::uniform_int_distribution<> dist(0, output_port_array.getInnerArrayArraySize()-1);    // Range: [0, 10]
        int random_number = dist(gen);
        packet->setFrontIteratorPosition(b(0));
        auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
        auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
        ethHeader->setOrca_agent_port_index(output_port_array.getInnerArray(random_number));
        packet->insertAtFront(ethHeader);
        packet->insertAtFront(phyHeader);
        packet->setFrontIteratorPosition(packetPosition);
        InterfaceEntry *ie = ifTable->getInterface(output_port_array.getInnerArray(random_number));
        dispatch(packet->dup(), ie);
    } else {
        // Multicast
        EV << "Sending to all " << output_port_array.getInnerArrayArraySize() << " output ports" << endl;

        if (use_rsbf)
            filter_rsbf_header(packet);

        int orca_agent_port_idx = -1;
        if (ethHeader->getUsing_orca() &&
                ((!initiate_tree && pkt_name.compare("data") == 0) ||
                        (initiate_tree && pkt_name.compare("treeInit")) ||
                        (initiate_tree && pkt_name.compare("treeFin")))) {
            auto eth_header = packet->peekAtFront<EthernetMacHeader>();
            orca_agent_port_idx = eth_header->getOrca_agent_port_index();
            if (eth_header->getOrca_pkt_seen_agent() && orca_agent_port_idx < 0)
                throw cRuntimeError("How is packet seen by agent but agent port index is < 0");
        }

//        std::cout << start_from_idx << endl;
//        std::cout << output_port_array.getInnerArrayArraySize() << endl;

        bool drop_notif_already_sent = false;

        if (output_port_array.getInnerArrayArraySize() > 0) {
            std::set<unsigned long> fw_port_idx_set;
            long candidate_start_from_idx = -1;
            // use correct info only if the packet is not RSBF redundant!
            for (int k = 0; k < output_port_array.getInnerArrayArraySize(); k++) {
                candidate_start_from_idx = next_start_from_array.getInnerArray(k);

                if (rsbf_is_pkt_redundant)
                    break;

                if (ethHeader->getUsing_orca() && orca_agent_port_idx == output_port_array.getInnerArray(k)) {
                    EV << "Agent port index found. skipping..." << endl;
                    continue;
                }

                if (use_cepheus && pkt_name.compare("treeInit") == 0) {
                    auto potential_id_itr = potential_next_interface_ids.find(
                            ifTable->getInterface(output_port_array.getInnerArray(k))->getInterfaceId());
                    if (potential_id_itr == potential_next_interface_ids.end()) {
                        EV << output_port_array.getInnerArray(k) << " with ID " <<
                                ifTable->getInterface(output_port_array.getInnerArray(k))->getInterfaceId() <<
                                " is not part of the transmission!" << endl;
                        continue;
                    }
                }

                EV << "Sending to index: " << output_port_array.getInnerArray(k) << endl;
                EV << "Setting next packet's start from idx to " << next_start_from_array.getInnerArray(k) << endl;
                auto packet_to_send = packet->dup();
                packet_to_send->setFrontIteratorPosition(b(0));
                auto phyHeader = packet_to_send->removeAtFront<EthernetPhyHeader>();
                auto ethHeader = packet_to_send->removeAtFront<EthernetMacHeader>();
                ethHeader->setStart_from_idx(next_start_from_array.getInnerArray(k));

                // this is the last hop if the next start from array is empty
                if (ethHeader->getJump_to_idxArraySize() <= ethHeader->getStart_from_idx()) {
                    std::cout << ethHeader->getJump_to_idxArraySize() <<endl;
                    std::cout << ethHeader->getStart_from_idx() << endl;
                    throw cRuntimeError("ethHeader->getStart_from_idx() index out of range");
                }
                auto tmp_start_from_array = ethHeader->getJump_to_idx(
                        ethHeader->getStart_from_idx());
                bool is_last_hop =
                        (tmp_start_from_array.getInnerArrayArraySize()  == 0);

                packet_to_send->insertAtFront(ethHeader);
                packet_to_send->insertAtFront(phyHeader);
                packet_to_send->setFrontIteratorPosition(packetPosition);
                fw_port_idx_set.insert(output_port_array.getInnerArray(k));

                unsigned int out_port_idx = output_port_array.getInnerArray(k);

                // apply spraying...
                if (is_last_hop && ethHeader->getIn_src_sharding()) {
                    out_port_idx = chunk_data_get_random_dst_gpu(out_port_idx);
                }

                InterfaceEntry *ie = ifTable->getInterface(out_port_idx);

                bool dropped_due_to_failure = false;
                // for every packet, just send drop notif once!
                // however, still check if we can send the packet on other links
                if (drop_type == DROP_FAIL &&
                            !ethHeader->getIs_redundant_cidr_pkt())
                    dropped_due_to_failure = failure_check_and_reroute(packet_to_send,
                            ie,
                            drop_notif_already_sent);

                if (dropped_due_to_failure)
                    drop_notif_already_sent = true;

                if (dropped_due_to_failure)
                    continue;

                dispatch(packet_to_send, ie);
            }

            if (use_rsbf)
                fw_rsbf_based(packet, fw_port_idx_set, start_from_idx, candidate_start_from_idx);
            fw_port_idx_set.clear();
        }
    }
    delete packet;

}

void BouncingIeee8021dRelay::schedule_recovery(simtime_t recovery_time, int port_idx) {
    Enter_Method("schedule_recovery");
    EV << "Recovery time for port " << port_idx << " is " << recovery_time << endl;
    cMessage *timeoutMsg = new cMessage("link_recovery");
    timeoutMsg->setKind(RECOVER_LINK);
    timeoutMsg->addPar("port_idx") = port_idx;
    scheduleAt(recovery_time, timeoutMsg);
}

bool BouncingIeee8021dRelay::is_link_failed(int port_idx, Packet *packet) {
    EV << "BouncingIeee8021dRelay::is_link_failed called for " <<
            port_idx << endl;

    // find the neighbor's relay unit and correct port to fail or recover if needed
    auto neighbor_path_itr = port_idx_to_neighboring_switch_path.find(port_idx);
    if (neighbor_path_itr == port_idx_to_neighboring_switch_path.end()) {
        EV << ": Neighbor for port " << port_idx << " is not a switch. Don't fail!" << endl;
        return false;
    }

    cModule* neighbor_module = getModuleByPath(neighbor_path_itr->second.c_str())->getParentModule()->getParentModule();
    std::string neighbor_relay_path = neighbor_module->getFullPath() + ".relayUnit";
    BouncingIeee8021dRelay* neighbor_relay = check_and_cast<BouncingIeee8021dRelay*>(getModuleByPath(neighbor_relay_path.c_str()));
    // now get the neighbor port
    std::regex ethPattern(R"(\beth\[(\d+)\])");  // matches eth[number]
    std::smatch match;
    int neighbor_port = -1;
    if (std::regex_search(neighbor_path_itr->second, match, ethPattern)) {
        neighbor_port = std::stoi(match[1]);  // extract the captured number
    }
    if (neighbor_port < 0)
        throw cRuntimeError("Neighbor port not found!");

    auto failed_port_itr = failed_ports.find(port_idx);
    bool port_failed = (failed_port_itr != failed_ports.end());
    simtime_t now = simTime();
    if (!port_failed) {
        EV << "Port is not failed yet!" << endl;

        simtime_t failure_end_time = get_failure_end_time(port_idx);

        bool should_fail_link = (failure_end_time > 0);

        if (should_fail_link) {

            port_failed = true;

            if (failure_or_recovery_notif_pkt == nullptr)
                throw cRuntimeError("There is no way failure_or_recovery_notif_pkt is nullptr here!");
            if (neighbor_relay->failure_or_recovery_notif_pkt == nullptr)
                neighbor_relay->failure_or_recovery_notif_pkt = failure_or_recovery_notif_pkt->dup();
            if (neighbor_relay->failure_or_recovery_notif_pkt == nullptr)
                throw cRuntimeError("neighbor_relay->failure_or_recovery_notif_pkt == nullptr");

            EV << getFullPath() << ": Failing port " << port_idx << endl;
            EV << "Recovery time: " << failure_end_time << endl;
            failed_ports.insert(port_idx);
            auto port_itr = port_to_recovery_time.find(port_idx);
            if (port_itr != port_to_recovery_time.end()) {
                std::cout << port_idx << endl;
                throw cRuntimeError("How do we already have a recovery time for the port");
            }
            port_to_recovery_time.insert(std::make_pair(port_idx, failure_end_time));
            schedule_recovery(failure_end_time, port_idx);
            notif_link_failure_or_recovery_to_neighbors(port_idx, NOTIF_LINK_FAILURE);


            EV << neighbor_relay_path << ": Failing the neighbor: " << neighbor_path_itr->second << endl;
            auto neighbor_port_itr = neighbor_relay->failed_ports.find(neighbor_port);
            if (neighbor_port_itr != neighbor_relay->failed_ports.end())
                throw cRuntimeError("How is neighbor port already failed!");
            neighbor_relay->failed_ports.insert(neighbor_port);
            auto neighbor_recovery_itr = neighbor_relay->port_to_recovery_time.find(neighbor_port);
            if (neighbor_recovery_itr != neighbor_relay->port_to_recovery_time.end()) {
                std::cout << neighbor_port << endl;
                throw cRuntimeError("Neighbor: How do we already have a recovery time for the port");
            }
            neighbor_relay->port_to_recovery_time.insert(std::make_pair(neighbor_port, failure_end_time));

            // no need to schedule recovery for neighbor as recovering from one side, recovers the other side two
//            neighbor_relay->schedule_recovery(now + failure_period, neighbor_port);
            neighbor_relay->notif_link_failure_or_recovery_to_neighbors(neighbor_port, NOTIF_LINK_FAILURE);
//            std::cout << "--------------------" << endl;
        }
    }

    return port_failed;
}

bool BouncingIeee8021dRelay::isPacketAppropriateForMulticast(std::string pkt_name) {
    if (pkt_name.compare("data") == 0)
        return true;

    if (pkt_name.compare("treeInit") == 0)
        return true;

    if (pkt_name.compare("treeFin") == 0)
        return true;

    return false;
}

unsigned int BouncingIeee8021dRelay::getRandomIdx() {
    static std::random_device rd;  // Obtain a random number from hardware
    static std::mt19937 gen(seed); // Seed the generator
    static std::uniform_int_distribution<int> distrib(0, 1000000000); // Define range
    return distrib(gen);
}

void BouncingIeee8021dRelay::create_notif_header(Packet *packet, std::string notif_name) {

    EV << "Creating notif with name " << notif_name << endl;

    // change the input packet to the notif
    packet->setName(notif_name.c_str());
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();
    auto ip_header = packet->removeAtFront<Ipv4Header>();
    auto udp_header = packet->removeAtFront<UdpHeader>();
    auto chunk = packet->removeAtFront<GenericAppMsg>();
    auto fcs_header = packet->peekAtBack<EthernetFcs>(ETHER_FCS_BYTES);

    b fake_payload_length = B(64);
//    fake_payload_length -= phy_header->getChunkLength();
    fake_payload_length -= eth_header->getChunkLength();
    fake_payload_length -= ip_header->getChunkLength();
    fake_payload_length -= udp_header->getChunkLength();
    fake_payload_length -= fcs_header->getChunkLength();

    chunk->setChunkLength(fake_payload_length);
    packet->insertAtFront(chunk);

    EV << "Creating UDP header" << endl;
    unsigned int org_udp_src_port = udp_header->getSourcePort();
    unsigned int org_udp_dst_port = udp_header->getDestinationPort();
    udp_header->setSourcePort(org_udp_dst_port);
    udp_header->setDestinationPort(org_udp_src_port);
    udp_header->setCrcMode(CRC_DISABLED);
    udp_header->setCrc(0x0000);
    udp_header->setTotalLengthField(udp_header->getChunkLength());
    packet->insertAtFront(udp_header);

    EV << "Creating IP header" << endl;
    auto org_src_ip_addr = ip_header->getSrcAddress();
    auto org_dst_ip_addr = ip_header->getDestAddress();
    ip_header->setSrcAddress(org_dst_ip_addr);
    ip_header->setDestAddress(org_src_ip_addr);
    ip_header->setTotalLengthField(ip_header->getChunkLength() + udp_header->getChunkLength());
    ip_header->setCrcMode(CRC_COMPUTED);
    ip_header->setCrc(0x0000);
    packet->insertAtFront(ip_header);

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
    packet->insertAtFront(eth_header);

    packet->insertAtFront(phy_header);

    packet->setFrontIteratorPosition(packetPosition);

    if (packet->getByteLength() < 64)
        throw cRuntimeError("packet->getBitLength() < B(64)");
}

void BouncingIeee8021dRelay::send_drop_notif(Packet *packet, simtime_t recovery_time) {
    EV << "Sending drop notif" << endl;

    // reset ttl and hopcount for original packet
    auto original_pkt = packet->dup();
    b originalPacketPosition = original_pkt->getFrontOffset();
    original_pkt->setFrontIteratorPosition(b(0));
    auto original_phy_header =
            original_pkt->removeAtFront<EthernetPhyHeader>();
    auto original_eth_header =
            original_pkt->removeAtFront<EthernetMacHeader>();
    auto original_ip_header =
            original_pkt->removeAtFront<Ipv4Header>();
    original_eth_header->setHop_count(0);
    original_ip_header->setTimeToLive(250);
    original_eth_header->setOriginal_pkt_name(packet->getName());
    original_pkt->insertAtFront(original_ip_header);
    original_pkt->insertAtFront(original_eth_header);
    original_pkt->insertAtFront(original_phy_header);
    original_pkt->setFrontIteratorPosition(originalPacketPosition);

    create_notif_header(packet, "drop_notif");

    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();
    eth_header->setOriginal_pkt(original_pkt);
    eth_header->setIs_drop_notif(true);
    eth_header->setRecovery_time(recovery_time);
    packet->insertAtFront(eth_header);
    packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(packetPosition);
}

void BouncingIeee8021dRelay::deflect_on_drop(Packet *packet) {
    EV << "Sending deflect_on_drop" << endl;

    // reset ttl and hopcount for original packet
    b originalPacketPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto original_phy_header =
            packet->removeAtFront<EthernetPhyHeader>();
    auto original_eth_header =
            packet->removeAtFront<EthernetMacHeader>();
    auto original_ip_header =
            packet->removeAtFront<Ipv4Header>();
//    original_eth_header->setHop_count(0);
    original_ip_header->setTimeToLive(250);
    original_eth_header->setOriginal_pkt_name(packet->getName());
    packet->insertAtFront(original_ip_header);
    packet->insertAtFront(original_eth_header);
    packet->insertAtFront(original_phy_header);
    packet->setFrontIteratorPosition(originalPacketPosition);

    packet->setName("drop_notif");
}

int BouncingIeee8021dRelay::get_pod_idx_for_tor(int tor_idx) {
    if (use_fattree) {
        if (fattree_k < 0)
            throw cRuntimeError("Invalid fattree K");
        return int(tor_idx / (fattree_k/2.0));
    } else {
        // for leaf spine all ToRs are in one pod
        return 0;
    }
}

bool BouncingIeee8021dRelay::isAggSwitchInDstPod(Packet *packet) {
    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        // this is not aggregate
        if (switch_path.find("agg[") == std::string::npos)
            return false;
    } else {
        // in leaf spine, spines are like aggs in fattree
        if (switch_path.find("spine[") == std::string::npos)
            return false;
        else
            return true; // every ToR is in the same pod so if we are in spine, we are in dst pod
    }
    auto frame = packet->peekAtFront<EthernetMacHeader>();
    int dst_tor_idx = frame->getDst_tor_idx();
    int dst_pod_idx = get_pod_idx_for_tor(dst_tor_idx);
    int switch_index = getParentModule()->getIndex();
    int switch_pod_idx = get_pod_idx_for_tor(switch_index);
    EV << "Switch pod: " << switch_pod_idx << ", dst pod: " << dst_pod_idx << endl;
    return (switch_pod_idx == dst_pod_idx);
}

bool BouncingIeee8021dRelay::isSwitchToR() {
    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        // is this core?
        if (switch_path.find("edge[") != std::string::npos)
            return true;
    } else {
        if (switch_path.find("agg[") != std::string::npos)
            return true;
    }
    return false;

}

bool BouncingIeee8021dRelay::isSwitchDstToR(Packet *packet) {

    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        // this is not edge (ToR)
        if (switch_path.find("edge[") == std::string::npos)
            return false;
    } else {
        // this is not edge (Leaf)
        if (switch_path.find("agg[") == std::string::npos)
            return false;
    }

    auto frame = packet->peekAtFront<EthernetMacHeader>();
    int dst_tor_idx = frame->getDst_tor_idx();
    int switch_index = getParentModule()->getIndex();

    EV << "Dst tor idx is " << dst_tor_idx << " and switch index is " << switch_index << endl;

    return (dst_tor_idx == switch_index);
}

bool BouncingIeee8021dRelay::isSwitchCore() {

    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        // is this core?
        if (switch_path.find("core[") != std::string::npos)
            return true;
    } else {
        throw cRuntimeError("There is no Core in leaf-spine!");
    }
    return false;
}

bool BouncingIeee8021dRelay::isSwitchAggregate() {

    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        // is this core?
        if (switch_path.find("agg[") != std::string::npos)
            return true;
    } else {
        if (switch_path.find("spine[") != std::string::npos)
            return true;
    }
    return false;
}

void BouncingIeee8021dRelay::multicast_downstream(Packet *packet, int input_port_idx) {
    EV << "BouncingIeee8021dRelay::multicast_downstream. Input port: " << input_port_idx << endl;

    auto ethHeader = packet->peekAtFront<EthernetMacHeader>();
    std::set<unsigned int> dst_server_idx_set;

//    std::cout << ethHeader->getDst_idx_listArraySize() << endl;

    for (int i = 0; i < ethHeader->getDst_idx_listArraySize(); i++) {
        dst_server_idx_set.insert(ethHeader->getDst_idx_list(i));
//        std::cout << ethHeader->getDst_idx_list(i) << endl;
    }

    bool dst_server_found = false;
    for (auto server_idx_itr = server_idx_to_port_map.begin();
            server_idx_itr != server_idx_to_port_map.end();
            server_idx_itr++) {

        if (server_idx_itr->second.find(input_port_idx) != server_idx_itr->second.end()) {
            EV << "Skipping server " << server_idx_itr->first << " since it is the sender!" << endl;
            continue;
        }

        auto dst_idx_itr = dst_server_idx_set.find(server_idx_itr->first);
        if (dst_idx_itr == dst_server_idx_set.end()) {
            EV << "Server " << server_idx_itr->first << "not in list of dst servers... skipping!" << endl;
            continue;
        }

        int port_list_idx = -1;
        if (use_ecmp) {
            std::string switch_name = getParentModule()->getFullPath();
            std::string header_info = switch_name +
                    std::to_string(server_idx_itr->first) +
                    std::to_string(ethHeader->getFlow_id());
            unsigned long header_info_hash = header_hash(header_info);
            EV << "Header hash is " << header_info_hash << endl;
            port_list_idx = header_info_hash % server_idx_itr->second.size();
        } else if (use_per_pkt_lb) {
            unsigned int random_idx = getRandomIdx();
            random_idx %= server_idx_itr->second.size();
            port_list_idx = random_idx;
        }

        if (port_list_idx < 0)
            throw cRuntimeError("Invalid port_list_idx");

        dst_server_found = true;

        auto port_num_it = server_idx_itr->second.begin();
        std::advance(port_num_it, port_list_idx);
        EV << "SEPEHR: output port number is: " << (*port_num_it) << endl;

//        int switch_index = getParentModule()->getIndex();
//        if (switch_index == 9)
//            std::cout << getFullPath() << " , " << server_idx_itr->first
//                    << ": " << ethHeader->getSrc_gpu_idx() <<
//                    "---" << ethHeader->getFlow_id() <<
//                    "---" << port_list_idx <<
//                    "---" << server_idx_itr->second.size() << endl;

        auto packet_to_send = packet->dup();
        InterfaceEntry *ie = ifTable->getInterface(*port_num_it);
        dispatch(packet_to_send, ie);
    }

    if (!dst_server_found)
        throw cRuntimeError("No dst server found for this ToR! How so?");
}

void BouncingIeee8021dRelay::mcast_programmable_core(Packet *packet) {
    EV << "BouncingIeee8021dRelay::mcast_programmable_core called!" << endl;
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();

    if (eth_header->getCidr_base_flow_id() < 0) {
        EV << "We have not stored the base info yet... storing it..." << endl;
        eth_header->setCidr_base_flow_id(eth_header->getFlow_id());
        eth_header->setCidr_base_src_gpu_idx(eth_header->getSrc_gpu_idx());
    }
    unsigned long cidr_base_flow_id = eth_header->getCidr_base_flow_id();
    unsigned int cidr_base_src_gpu_idx = eth_header->getCidr_base_src_gpu_idx();
    if (cidr_base_flow_id < 0 || cidr_base_src_gpu_idx < 0)
        throw cRuntimeError("Invalid cidr_base_flow_id or cidr_base_src_gpu_idx");

    int input_port_idx = packet->getTag<InputPortTag>()->getInput_port_idx();

    for (int i = 0;
            i < eth_header->getPartial_mcast_core_pktArraySize();
            i++) {
        auto pkt = eth_header->getPartial_mcast_core_pkt(i);
        Packet* packet_to_send = pkt->dup();
        delete pkt;
        EthernetPhyHeader* new_ph_header = phy_header->dup();
        packet_to_send->insertAtFront(makeShared<EthernetPhyHeader>(*new_ph_header));
        packet->setFrontIteratorPosition(b(0));
        auto m_phy_header = packet_to_send->removeAtFront<EthernetPhyHeader>();
        auto m_eth_header = packet_to_send->removeAtFront<EthernetMacHeader>();
        m_eth_header->setHop_count(eth_header->getHop_count());

        m_eth_header->setCidr_base_flow_id(cidr_base_flow_id);
        m_eth_header->setCidr_base_src_gpu_idx(cidr_base_src_gpu_idx);
        m_eth_header->setSwift_send_time(eth_header->getSwift_send_time());

        if (m_eth_header->getDst_tor_idx() < 0) {
            std::cout << m_eth_header->getDst_tor_idx() << endl;
            std::cout << m_eth_header->getSeq_num() << endl;
            throw cRuntimeError("Unknown dst tor idx!");
        }

        m_eth_header->setMulticast_pkt_in_programmable_core(false);

        if (eth_header->getInput_ports_passedArraySize() == 0)
            throw cRuntimeError("1) How is Input_ports_passedArraySize() == 0");
        while (m_eth_header->getInput_ports_passedArraySize() > 0)
            m_eth_header->eraseInput_ports_passed(0);
        if (m_eth_header->getInput_ports_passedArraySize() != 0)
            throw cRuntimeError("1) How is Input_ports_passedArraySize() != 0");
        for (int input_port_idx = 0;
                input_port_idx < eth_header->getInput_ports_passedArraySize();
                input_port_idx++) {
            m_eth_header->insertInput_ports_passed(eth_header->getInput_ports_passed(input_port_idx));
        }

        packet_to_send->insertAtFront(m_eth_header);
        packet_to_send->insertAtFront(m_phy_header);
        packet_to_send->setFrontIteratorPosition(packetPosition);
        auto frame = packet_to_send->peekAtFront<EthernetMacHeader>();
        std::list<int> destInterfaceIds = macTable->getInterfaceIdForAddress(frame->getDest());
        if (destInterfaceIds.size() > 1)
            throw cRuntimeError("We must at most only have one path from the core toward each destination!");
        EV << "Sending a copy of the message toward a ToR: " << packet_to_send->str() << endl;
        packet_to_send->addTag<InputPortTag>()->setInput_port_idx(input_port_idx);
        InterfaceEntry *ie = nullptr;

        if (destInterfaceIds.size() > 0)
            ie = ifTable->getInterfaceById(*destInterfaceIds.begin());

        bool dropped_due_to_failure = false;
        if (drop_type == DROP_FAIL &&
                    !frame->getIs_redundant_cidr_pkt())
            dropped_due_to_failure = failure_check_and_reroute(packet_to_send, ie);

        if (dropped_due_to_failure)
            continue;

        if (ie == nullptr)
            throw cRuntimeError("ie shouldn't be nullptr at this point!");

//        packet_to_send->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
        dispatch(packet_to_send, ie);
    }
    packet->insertAtFront(eth_header);
    packet->insertAtFront(phy_header);
}

bool BouncingIeee8021dRelay::is_port_covered_by_cidr(
        unsigned int port_idx, std::string cidr_group_id) {

    // cidr_group_id is in form of X_Y, here we only care about Y
    size_t pos = cidr_group_id.find('_');
    std::string tmp_id;
    if (pos != std::string::npos && pos + 1 < cidr_group_id.length()) {
        tmp_id = cidr_group_id.substr(pos + 1);
    } else
        throw cRuntimeError("cidr_group_id is not in form of X_Y");
    cidr_group_id = tmp_id;

    // Convert port_idx to binary string with leading zeros to match cidr_group_id length
    size_t len = cidr_group_id.length();
    const unsigned int bitset_size = 512;

    if (len > bitset_size)
        throw cRuntimeError("bitset_size should always be > len");

    std::string port_bin = std::bitset<bitset_size>(port_idx).to_string();

    EV << port_idx << " converted to " << port_bin << endl;

    // Compare only the relevant bits
    for (size_t i = 0; i < len; ++i) {
        if (cidr_group_id[i] != '*' && cidr_group_id[i] != port_bin[bitset_size - len + i]) {
            EV << port_idx << " is NOT part of " << cidr_group_id << endl;
            return false;
        }
    }
    EV << port_idx << " IS part of " << cidr_group_id << endl;
    return true;
}

void BouncingIeee8021dRelay::mcast_cidr_core(Packet *packet) {
    EV << "BouncingIeee8021dRelay::mcast_cidr_core called!" << endl;
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();

    std::string cidr_group_id = eth_header->getCore_cidr_group_id();
    if (cidr_group_id.compare("") == 0 ||
            eth_header->getCore_cidr_group_member_count() == 0)
        throw cRuntimeError("No CIDR Core info appended to the packet!");

    if (eth_header->getCidr_base_flow_id() < 0) {
        EV << "We have not stored the base info yet... storing it..." << endl;
        eth_header->setCidr_base_flow_id(eth_header->getFlow_id());
        eth_header->setCidr_base_src_gpu_idx(eth_header->getSrc_gpu_idx());
    }
    unsigned long cidr_base_flow_id = eth_header->getCidr_base_flow_id();
    unsigned int cidr_base_src_gpu_idx = eth_header->getCidr_base_src_gpu_idx();
    if (cidr_base_flow_id < 0 || cidr_base_src_gpu_idx < 0)
        throw cRuntimeError("Invalid cidr_base_flow_id or cidr_base_src_gpu_idx");

    int input_port_idx = packet->getTag<InputPortTag>()->getInput_port_idx();

    std::set<unsigned int> correct_cidr_ports;

    for (int i = 0;
            i < eth_header->getCIDR_core_pktArraySize();
            i++) {
        auto pkt = eth_header->getCIDR_core_pkt(i);
        Packet* packet_to_send = pkt->dup();
        delete pkt;
        EthernetPhyHeader* new_ph_header = phy_header->dup();
        packet_to_send->insertAtFront(makeShared<EthernetPhyHeader>(*new_ph_header));
        packet->setFrontIteratorPosition(b(0));
        auto m_phy_header = packet_to_send->removeAtFront<EthernetPhyHeader>();
        auto m_eth_header = packet_to_send->removeAtFront<EthernetMacHeader>();
        m_eth_header->setHop_count(eth_header->getHop_count());

        m_eth_header->setCidr_base_flow_id(cidr_base_flow_id);
        m_eth_header->setCidr_base_src_gpu_idx(cidr_base_src_gpu_idx);
        m_eth_header->setSwift_send_time(eth_header->getSwift_send_time());

        if (m_eth_header->getDst_tor_idx() < 0) {
            std::cout << m_eth_header->getDst_tor_idx() << endl;
            std::cout << m_eth_header->getSeq_num() << endl;
            throw cRuntimeError("Unknown dst tor idx!");
        }

        m_eth_header->setMulticast_pkt_in_CIDR_core(false);

        if (eth_header->getInput_ports_passedArraySize() == 0)
            throw cRuntimeError("2) How is Input_ports_passedArraySize() == 0");
        while (m_eth_header->getInput_ports_passedArraySize() > 0)
            m_eth_header->eraseInput_ports_passed(0);
        if (m_eth_header->getInput_ports_passedArraySize() != 0)
            throw cRuntimeError("2) How is Input_ports_passedArraySize() != 0");
        for (int input_port_idx = 0;
                input_port_idx < eth_header->getInput_ports_passedArraySize();
                input_port_idx++) {
            m_eth_header->insertInput_ports_passed(eth_header->getInput_ports_passed(input_port_idx));
        }

        packet_to_send->insertAtFront(m_eth_header);
        packet_to_send->insertAtFront(m_phy_header);
        packet_to_send->setFrontIteratorPosition(packetPosition);
        auto frame = packet_to_send->peekAtFront<EthernetMacHeader>();
        std::list<int> destInterfaceIds = macTable->getInterfaceIdForAddress(frame->getDest());
        if (destInterfaceIds.size() > 1)
            throw cRuntimeError("We must at most have one path from the core toward each destination!");
        EV << "Sending a copy of the message toward an aggregate switch: " << packet_to_send->str() << endl;
        packet_to_send->addTag<InputPortTag>()->setInput_port_idx(input_port_idx);
        InterfaceEntry *ie = nullptr;

        if (destInterfaceIds.size() > 0)
            ie = ifTable->getInterfaceById(*destInterfaceIds.begin());

        bool dropped_due_to_failure = false;
        if (drop_type == DROP_FAIL &&
                    !frame->getIs_redundant_cidr_pkt())
            dropped_due_to_failure = failure_check_and_reroute(packet_to_send, ie);

        if (dropped_due_to_failure)
            continue;

        if (ie == nullptr)
            throw cRuntimeError("mcast_cidr_core: ie should not be nullptr at this point.");

        EV << "CIDR to port " << ie->getIndex() << endl;
        correct_cidr_ports.insert(ie->getIndex());
//        packet_to_send->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
        if (!is_port_covered_by_cidr(ie->getIndex(), cidr_group_id)) {
            std::cout << "Port " << ie->getIndex() << endl;
            std::cout << "CIDR group id: " << cidr_group_id << endl;
            throw cRuntimeError("How is not port covered by cidr_group_id");
        }
        dispatch(packet_to_send, ie);
    }

    // if any other packet is sent, it should be dropped later
    eth_header->setIs_redundant_cidr_pkt(true);
    while (eth_header->getCIDR_agg_pktArraySize() != 0)
        eth_header->eraseCIDR_agg_pkt(0);
    while (eth_header->getCIDR_core_pktArraySize() != 0)
        eth_header->eraseCIDR_core_pkt(0);
    packet->insertAtFront(eth_header);
    packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(packetPosition);

    std::set<unsigned int> potential_cidr_downsteams;
    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        if (switch_path.find("core[") != std::string::npos) {
            for (int downstream_port_idx = 0;
                    downstream_port_idx < fattree_k;
                    downstream_port_idx++)
                potential_cidr_downsteams.insert(downstream_port_idx);
        } else
            throw cRuntimeError("Not in a core switch...!");
    } else
        throw cRuntimeError("Leaf-spine does not have core switches!");

    for (auto potential_downstream : potential_cidr_downsteams) {
        // no need to send to input port again
        if (potential_downstream == input_port_idx)
            continue;
        if (correct_cidr_ports.find(potential_downstream) ==
                correct_cidr_ports.end() &&
                failed_ports.find(potential_downstream) ==
                        failed_ports.end()) {
            EV << "Packet has not yet been forwarded to port " <<
                    potential_downstream << endl;
            if (is_port_covered_by_cidr(potential_downstream, cidr_group_id)) {
                EV << "CIDR redundantly sending to port " << potential_downstream << endl;
                auto ie = ifTable->getInterface(potential_downstream);
                dispatch(packet->dup(), ie);
            }
        }
    }

}

void BouncingIeee8021dRelay::mcast_cidr_agg(Packet *packet) {
    EV << "BouncingIeee8021dRelay::mcast_cidr_agg called!" << endl;
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();

    std::string cidr_group_id = eth_header->getAgg_cidr_group_id();
    if (cidr_group_id.compare("") == 0 ||
            eth_header->getAgg_cidr_group_member_count() == 0)
        throw cRuntimeError("No CIDR info appended to the packet!");

    if (eth_header->getCidr_base_flow_id() < 0) {
        EV << "We have not stored the base info yet... storing it..." << endl;
        eth_header->setCidr_base_flow_id(eth_header->getFlow_id());
        eth_header->setCidr_base_src_gpu_idx(eth_header->getSrc_gpu_idx());
    }
    unsigned long cidr_base_flow_id = eth_header->getCidr_base_flow_id();
    unsigned int cidr_base_src_gpu_idx = eth_header->getCidr_base_src_gpu_idx();
    if (cidr_base_flow_id < 0 || cidr_base_src_gpu_idx < 0)
        throw cRuntimeError("Invalid cidr_base_flow_id or cidr_base_src_gpu_idx");

    int input_port_idx = packet->getTag<InputPortTag>()->getInput_port_idx();

    std::set<unsigned int> correct_cidr_ports;

    for (int i = 0;
            i < eth_header->getCIDR_agg_pktArraySize();
            i++) {
        auto pkt = eth_header->getCIDR_agg_pkt(i);
        Packet* packet_to_send = pkt->dup();
        delete pkt;
        EthernetPhyHeader* new_ph_header = phy_header->dup();
        packet_to_send->insertAtFront(makeShared<EthernetPhyHeader>(*new_ph_header));
        packet->setFrontIteratorPosition(b(0));
        auto m_phy_header = packet_to_send->removeAtFront<EthernetPhyHeader>();
        auto m_eth_header = packet_to_send->removeAtFront<EthernetMacHeader>();
        m_eth_header->setHop_count(eth_header->getHop_count());

        m_eth_header->setCidr_base_flow_id(cidr_base_flow_id);
        m_eth_header->setCidr_base_src_gpu_idx(cidr_base_src_gpu_idx);
        m_eth_header->setSwift_send_time(eth_header->getSwift_send_time());

        if (m_eth_header->getDst_tor_idx() < 0) {
            std::cout << m_eth_header->getDst_tor_idx() << endl;
            std::cout << m_eth_header->getSeq_num() << endl;
            throw cRuntimeError("Unknown dst tor idx!");
        }

        m_eth_header->setMulticast_pkt_in_CIDR_agg(false);
        // also turn core to false since it has already passed core
        m_eth_header->setMulticast_pkt_in_CIDR_core(false);

        if (eth_header->getInput_ports_passedArraySize() == 0)
            throw cRuntimeError("3) How is Input_ports_passedArraySize() == 0");
        while (m_eth_header->getInput_ports_passedArraySize() > 0)
            m_eth_header->eraseInput_ports_passed(0);
        if (m_eth_header->getInput_ports_passedArraySize() != 0)
            throw cRuntimeError("3) How is Input_ports_passedArraySize() != 0");
        for (int input_port_idx = 0;
                input_port_idx < eth_header->getInput_ports_passedArraySize();
                input_port_idx++) {
            m_eth_header->insertInput_ports_passed(eth_header->getInput_ports_passed(input_port_idx));
        }

        packet_to_send->insertAtFront(m_eth_header);
        packet_to_send->insertAtFront(m_phy_header);
        packet_to_send->setFrontIteratorPosition(packetPosition);
        auto frame = packet_to_send->peekAtFront<EthernetMacHeader>();
        std::list<int> destInterfaceIds = macTable->getInterfaceIdForAddress(frame->getDest());
        if (destInterfaceIds.size() > 1)
            throw cRuntimeError("We must at most have one path from the core toward each destination!");
        EV << "Sending a copy of the message toward a ToR: " << packet_to_send->str() << endl;
        packet_to_send->addTag<InputPortTag>()->setInput_port_idx(input_port_idx);
        InterfaceEntry *ie = nullptr;
        if (destInterfaceIds.size() > 0)
            ie = ifTable->getInterfaceById(*destInterfaceIds.begin());

        bool dropped_due_to_failure = false;
        if (drop_type == DROP_FAIL &&
                    !frame->getIs_redundant_cidr_pkt())
            dropped_due_to_failure = failure_check_and_reroute(packet_to_send, ie);

        if (dropped_due_to_failure)
            continue;

        if (ie == nullptr)
            throw cRuntimeError("mcast_cidr_agg: ie should not be nullptr at this point.");

        EV << "CIDR to port " << ie->getIndex() << endl;
        correct_cidr_ports.insert(ie->getIndex());
//        packet_to_send->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
        if (!is_port_covered_by_cidr(ie->getIndex(), cidr_group_id)) {
            std::cout << "Port " << ie->getIndex() << endl;
            std::cout << "CIDR group id: " << cidr_group_id << endl;
            throw cRuntimeError("How is not port covered by cidr_group_id");
        }
        dispatch(packet_to_send, ie);
    }

    // if any other packet is sent, it should be dropped later
    eth_header->setIs_redundant_cidr_pkt(true);
    while (eth_header->getCIDR_agg_pktArraySize() != 0)
        eth_header->eraseCIDR_agg_pkt(0);
    packet->insertAtFront(eth_header);
    packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(packetPosition);

    std::set<unsigned int> potential_cidr_downsteams;
    std::string switch_path = switch_module->getFullPath();
    if (use_fattree) {
        if (switch_path.find("agg[") != std::string::npos) {
            for (int downstream_port_idx = 0;
                    downstream_port_idx < int(fattree_k / 2.0);
                    downstream_port_idx++)
                potential_cidr_downsteams.insert(downstream_port_idx);
        } else
            throw cRuntimeError("Not in an aggregate switch!!!");
    } else {
        if (switch_path.find("spine[") != std::string::npos) {
            for (int downstream_port_idx = 0;
                    downstream_port_idx < num_leafs;
                    downstream_port_idx++)
                potential_cidr_downsteams.insert(downstream_port_idx);
        }
    }

    for (auto potential_downstream : potential_cidr_downsteams) {
        // no need to send to input port again
        if (potential_downstream == input_port_idx)
            continue;
        if (correct_cidr_ports.find(potential_downstream) ==
                correct_cidr_ports.end() &&
                failed_ports.find(potential_downstream) ==
                        failed_ports.end()) {
            EV << "Packet has not yet been forwarded to port " <<
                    potential_downstream << endl;
            if (is_port_covered_by_cidr(potential_downstream, cidr_group_id)) {
                EV << "CIDR redundantly sending to port " << potential_downstream << endl;
                auto ie = ifTable->getInterface(potential_downstream);
                dispatch(packet->dup(), ie);
            }
        }
    }

}

void BouncingIeee8021dRelay::chooseDispatchType(Packet *packet, InterfaceEntry *ie) {
    auto frame = packet->peekAtFront<EthernetMacHeader>();

    if (frame->getIs_redundant_cidr_pkt()) {
        EV << "Packet is redundant CIDR packet, dropping....!" << endl;
        delete packet;
        return;
    }

    std::string packet_name = packet->getName();

    std::list<int> destInterfaceIds = macTable->getInterfaceIdForAddress(frame->getDest());
//    macTable->printState();
    int portNum = destInterfaceIds.size();
    Chunk::enableImplicitChunkSerialization = true;
    std::string protocol = packet->getName();

    bool is_packet_arp_or_broadcast = (protocol.find("arp") != std::string::npos) ||
            (frame->getDest().isBroadcast());
//    std::cout << "***************************************************" << endl;
    if (!is_packet_arp_or_broadcast){
        EV << "SEPEHR: Should reduce packet's ttl." << endl;
        b packetPosition = packet->getFrontOffset();
        packet->setFrontIteratorPosition(b(0));
        auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
        auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
        auto ipHeader = packet->removeAtFront<Ipv4Header>();
        short ttl = ipHeader->getTimeToLive() - 1;
        if (ttl <= 0) {
            EV << "ttl is " << ttl << ". dropping the packet!" << endl;
//            emit(bounceLimitPassedSignal, packet->getId());
            light_in_relay_packet_drop_counter++;
            delete packet;
            return;
        }
        EV << "SEPEHR: packet's old ttl is: " << ipHeader->getTimeToLive() << " and it's new ttl is: " << ttl << endl;
        ipHeader->setTimeToLive(ttl);

        packet->insertAtFront(ipHeader);
        packet->insertAtFront(ethHeader);
        packet->insertAtFront(phyHeader);
        packet->setFrontIteratorPosition(packetPosition);
    }

    EV << "SOUGOL: This is the Packet Name: " << protocol << endl;
    EV << "SEPEHR: The number of available ports for this packet is " << portNum << endl;
    EV << "SEPEHR: source mac address: " << frame->getSrc().str() << " and dest mac address: " << frame->getDest().str() << endl;
    EV << "Hop count: " << frame->getHop_count() << endl;

    bool using_ina = (packet_name.compare("data") == 0 && frame->getCollective_alg() == BCAST_INA);

    if (!is_packet_arp_or_broadcast) {
        EV << "packet is not ARP or broadcast" << endl;
        InterfaceEntry *ie2;
        bool dropped_due_to_failure = false;

        if (use_power_of_n_lb) {
            //forward the packet towards destination using power of n choices
            // Considering ports towards servers as well

            EV << "Finding random interface for packet " << packet->str() << endl;
            ie2 = find_interface_to_fw_randomly_power_of_n(packet, true);
            if (ie2 == nullptr)
                ie2 = ie;
        }
        else if (packet_has_host_instructions(packet) &&
                isPacketAppropriateForMulticast(packet_name)) {
            forward_packet_based_on_host_instructions(packet);
            return;
        } else if (using_ina) {
            if (random_fail_prob > 0)
                throw cRuntimeError("failure/drop is not implemented for INA!!");
            forward_packet_based_on_ina(packet);
            return;

        } else if (isSwitchDstToR(packet) &&
                isPacketAppropriateForMulticast(packet_name)) {
            // no need to consider failure at this layer
            // here, we assume that all servers under a Tor are part of the collective
            EV << "Dst tor has reached, broadcast to all servers below except the incoming port" << endl;
            multicast_downstream(packet, packet->getTag<InputPortTag>()->getInput_port_idx());
            delete packet;
            return;
        } else if (partial_mcast_programmable_cores &&
                isSwitchCore() &&
                isPacketAppropriateForMulticast(packet_name) &&
                frame->getMulticast_pkt_in_programmable_core()) {
            EV << "Reached programmable core... Perform multicast" << endl;
            mcast_programmable_core(packet);
            delete packet;
            return;
        } else if (partial_mcast_use_cidr_agg &&
                isSwitchAggregate() &&
                isPacketAppropriateForMulticast(packet_name) &&
                frame->getMulticast_pkt_in_CIDR_agg() &&
                isAggSwitchInDstPod(packet)) {
            EV << "Applying CIDR at aggregate switches" << endl;
            mcast_cidr_agg(packet);
            delete packet;
            return;
        } else if (partial_mcast_use_cidr_core &&
                isSwitchCore() &&
                isPacketAppropriateForMulticast(packet_name) &&
                frame->getMulticast_pkt_in_CIDR_core()) {
            EV << "Applying CIDR at core switches" << endl;
            mcast_cidr_core(packet);
            delete packet;
            return;
        } else if (use_ecmp && portNum > 1) {
            EV << "using ECMP" << endl;
            if (destInterfaceIds.size() == 0)
                ie2 = nullptr;
            else {
                destInterfaceIds.sort();
                b packetPosition = packet->getFrontOffset();
                packet->setFrontIteratorPosition(b(0));
                auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
                auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
                auto ipHeader = packet->removeAtFront<Ipv4Header>();
                unsigned int src_port, dst_port;
                if (use_udp) {
                    // fragmentation probably has happened use ip header
                    src_port = ipHeader->getOriginal_src_port();
                    dst_port = ipHeader->getOriginal_dst_port();

                } else {
                    auto tcpHeader = packet->peekAtFront<tcp::TcpHeader>();
                    src_port = tcpHeader->getSourcePort();
                    dst_port = tcpHeader->getDestinationPort();
                }

                EV << "S&S: The flow info is: ( src_ip: " << ipHeader->getSourceAddress() << ", dest_ip: " << ipHeader->getDestinationAddress() << ", src_port: " << src_port << ", dest_port: " << dst_port << " )" << endl;
                EV << "SEPEHR: Switch IS using ECMP for this packet!" << endl;
                std::string switch_name = getParentModule()->getFullPath();
                std::string header_info = ipHeader->getSourceAddress().str() + ipHeader->getDestinationAddress().str() +
                        std::to_string(src_port) + std::to_string(dst_port) + switch_name;
                unsigned long header_info_hash = header_hash(header_info);
                EV << "There are " << portNum << " ports and header hash is " << header_info_hash << endl;
                int outputPortNum;
                outputPortNum = header_info_hash % portNum;
                EV << "SEPEHR: output port number is: " << outputPortNum << endl;
                std::list<int>::iterator it = destInterfaceIds.begin();
                std::advance(it, outputPortNum);
                EV << "SEPEHR: output interface ID is: " << *it << endl;
                packet->insertAtFront(ipHeader);
                packet->insertAtFront(ethHeader);
                packet->insertAtFront(phyHeader);
                packet->setFrontIteratorPosition(packetPosition);
                ie2 = ifTable->getInterfaceById(*it);
            }
        } else if (use_per_pkt_lb && portNum > 1){
            if (destInterfaceIds.size() == 0)
                ie2 = nullptr;
            else {
                EV << "using packet spraying" << endl;
                unsigned int random_idx = getRandomIdx();
                EV << "port num: " << portNum << ", random idx is " << random_idx << endl;
                int outputPortNum = random_idx % portNum;
                EV << "SEPEHR: output port number is: " << outputPortNum << endl;
                std::list<int>::iterator it = destInterfaceIds.begin();
                std::advance(it, outputPortNum);
                ie2 = ifTable->getInterfaceById(*it);
            }
        } else {
            ie2 = ie;
        }

        if (drop_type == DROP_FAIL &&
                    isPacketAppropriateForMulticast(packet_name) &&
                    !frame->getIs_redundant_cidr_pkt())
            dropped_due_to_failure = failure_check_and_reroute(packet, ie2);
        if (dropped_due_to_failure)
            return;

        if (ie2 == nullptr && packet_name.compare("data") != 0) {
            auto potential_output_port_itr = mac_address_to_port_map.find(frame->getDest().str());
            if (potential_output_port_itr == mac_address_to_port_map.end()) {
                std::cout << frame->getDest().str() << endl;
                throw cRuntimeError("2) potential_output_port_itr == mac_address_to_port_map.end()");
            }

            unsigned int random_idx = getRandomIdx();
            random_idx %= potential_output_port_itr->second.size();
            auto it = potential_output_port_itr->second.begin();
            std::advance(it, random_idx);
            EV << "ie2 is still null but we're sending the packet to port " << (*it) << endl;
            ie2 = ifTable->getInterface(*it);
        }

        if (ie2 == nullptr) {
            std::cout << packet->str() << endl;
            throw cRuntimeError("ie2 should not be still nullptr");
        }

        dispatch(packet, ie2);
    }
    else {
        EV << "SEPEHR: Switch IS NOT using ECMP for this packet!" << endl;
        dispatch(packet, ie);
    }
}

InterfaceEntry* BouncingIeee8021dRelay::find_interface_to_bounce_randomly(Packet *packet) {
    if (!can_deflect)
        throw cRuntimeError("find_interface_to_bounce_randomly called! can deflect is false!");
    // This is how its done in DIBS
    InterfaceEntry *ie = nullptr;
    std::list<int> port_idx_connected_to_switch_neioghbors_copy = port_idx_connected_to_switch_neioghbors;
    int port_idx_connected_to_switch_neioghbors_copy_size;
    int random_index;
    std::string module_path_string;
    std::string switch_name = getParentModule()->getFullName();
    b packet_length = b(packet->getBitLength());

    // choose randomly from the list without replacement
    port_idx_connected_to_switch_neioghbors_copy_size = port_idx_connected_to_switch_neioghbors_copy.size();
    random_index = rand() % port_idx_connected_to_switch_neioghbors_copy_size;
    std::list<int>::iterator it = port_idx_connected_to_switch_neioghbors_copy.begin();
    std::advance(it, random_index);

    int num_ports_checked = 0;

    while (can_deflect){
        if (approximate_random_deflection) {
            if (num_ports_checked >=
                    port_idx_connected_to_switch_neioghbors_copy_size)
                break;
        } else {
            // if not choose a random port that is connected to a switch with available buffer space
            port_idx_connected_to_switch_neioghbors_copy_size = port_idx_connected_to_switch_neioghbors_copy.size();
            if (port_idx_connected_to_switch_neioghbors_copy_size == 0) {
                break;
            }
        }

        // This is actual dibs that we keep picking randomly until
        // every port is full
        module_path_string = switch_name + ".eth[" + std::to_string(*it) + "].mac";

        EV << "SEPEHR: Extracting info for " << module_path_string << endl;
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        std::string queue_full_path = module_path_string + ".queue";
        if (send_header_of_dropped_packet_to_receiver) {
            queue_full_path = module_path_string + ".queue.mainQueue";
        }

        if (!filter_out_full_ports || !mac->is_queue_full(packet_length, queue_full_path)) {
            ie = ifTable->getInterface(*it);
            EV << "The packet is randomly bounced to id: " << ie->getInterfaceId() << endl;

            std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ ie->getIndex())->getPathEndGate()->getFullPath();
            bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;
            if (is_other_side_input_module_path_server) {
                throw cRuntimeError("The chosen bouncing port is towards a host!");
            }

            return ie;
        }

        if (approximate_random_deflection) {
            // This is for our approximation of random deflection
            // where we choose one random number and circularly send it to
            // the first non-congested port
            num_ports_checked++;
            it++;
            if (it == port_idx_connected_to_switch_neioghbors_copy.end())
                it = port_idx_connected_to_switch_neioghbors_copy.begin();
        } else {
            port_idx_connected_to_switch_neioghbors_copy.erase(it);
            port_idx_connected_to_switch_neioghbors_copy_size = port_idx_connected_to_switch_neioghbors_copy.size();
            if (port_idx_connected_to_switch_neioghbors_copy_size == 0)
                break;
            if (port_idx_connected_to_switch_neioghbors_copy_size == port_idx_connected_to_switch_neioghbors.size())
                throw cRuntimeError("You're erasing from the base array two. Probably its because of calling by reference!");
            random_index = rand() % port_idx_connected_to_switch_neioghbors_copy_size;
            it = port_idx_connected_to_switch_neioghbors_copy.begin();
            std::advance(it, random_index);
        }
    }
    EV << "Dropping the packet!" << endl;
    return nullptr;
}

InterfaceEntry* BouncingIeee8021dRelay::find_interface_to_bounce_naively() {
    if (!can_deflect)
        throw cRuntimeError("find_interface_to_bounce_naively called! can deflect is false!");
    if (port_idx_connected_to_switch_neioghbors.size() != 1)
        throw cRuntimeError("port_idx_connected_to_switch_neioghbors.size() != 1");
    // port_idx_connected_to_switch_neioghbors only has one port which is the port
    // to which we should naively deflecting packets.
    InterfaceEntry *ie = nullptr;
    std::list<int>::iterator it = port_idx_connected_to_switch_neioghbors.begin();
    ie = ifTable->getInterface(*it);
    return ie;
}

double BouncingIeee8021dRelay::get_port_utilization(int port, Packet *packet)
{
    EV << "Extracting port utilization" << endl;
    auto frame = packet->peekAtFront<EthernetMacHeader>();
    double queue_util;
    std::string switch_name = getParentModule()->getFullName();

    std::string module_path_string;
//    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
    if (frame->getBouncedHop() == 0)
    {
        // packet has never been bounced.
        module_path_string = switch_name + ".eth[" + std::to_string(port) + "].mac.queue.dataQueue";
        EV << "Extracting utilization of " << module_path_string << " for port " << port << endl;

    }
    else
    {
        // packet has been bounced at least once
        module_path_string = switch_name + ".eth[" + std::to_string(port) + "].mac.queue.bounceBackQueue";
        EV << "Extracting utilization of " << module_path_string << " for port " << port << endl;
    }
    queueing::PacketQueue *queue = check_and_cast<queueing::PacketQueue *>(getModuleByPath(module_path_string.c_str()));
    if (queue->getMaxNumPackets() != -1) {
        // Packet capacity
        queue_util = (1.0 * queue->getNumPackets()) / queue->getMaxNumPackets();
        EV << "Capacity: " << queue->getMaxNumPackets() << ", occupancy: " << queue->getNumPackets() << ", util: " << queue_util << endl;
    } else {
        // Data capacity
        queue_util = (1.0 * queue->getTotalLength().get()) / queue->getMaxTotalLength().get();
        EV << "Capacity: " << queue->getMaxTotalLength() << ", occupancy: " << queue->getTotalLength() << ", util: " << queue_util << endl;
    }

    return queue_util;
}

InterfaceEntry* BouncingIeee8021dRelay::find_a_port_for_packet_towards_source(Packet *packet) {
    InterfaceEntry *ie = nullptr;

    const auto& frame = packet->peekAtFront<EthernetMacHeader>();
    std::list<int> srcInterfaceIds = macTable->getInterfaceIdForAddress(frame->getSrc());
    srcInterfaceIds.sort();
    int portNum = srcInterfaceIds.size();
    Chunk::enableImplicitChunkSerialization = true;
    std::string protocol = packet->getName();
    bool is_packet_arp_or_broadcast = (protocol.find("arp") != std::string::npos) || (frame->getDest().isBroadcast());
    if ((!is_packet_arp_or_broadcast) && (portNum > 1)){

        if (use_power_of_n_lb) {
            std::string module_path_string;
            std::string switch_name = getParentModule()->getFullName();
            long min_queue_occupancy = -1;
            int chosen_source_index = -1;
            std::list<int> chosen_interface_ids;
            int chosen_interface_counter = 0;

            //Try finding an available port that goes towards the source of the packet. Not consider ports going towards sources
            while(chosen_interface_counter < random_power_factor && srcInterfaceIds.size() != 0) {
                int random_idx = rand() % srcInterfaceIds.size();
                std::list<int>::iterator it = srcInterfaceIds.begin();
                std::advance(it, random_idx);
                chosen_interface_counter++;
                chosen_interface_ids.push_back(*it);
                srcInterfaceIds.erase(it);
                if (srcInterfaceIds.size() == (macTable->getInterfaceIdForAddress(frame->getSrc())).size())
                    throw cRuntimeError("You have also changed the size of base array of source_interface_ids.");
            }

            if (chosen_interface_ids.size() == 0)
                throw cRuntimeError("No path towards the source. Not right...!");

            // Apply power of n: Find least congested port and forward the packet to that port
            chosen_interface_ids.sort();
            for (std::list<int>::iterator it=chosen_interface_ids.begin(); it != chosen_interface_ids.end(); it++){
                long queue_occupancy;
                int interface_index = ifTable->getInterfaceById(*it)->getIndex();
                if(bounce_probabilistically) {
                    module_path_string = switch_name + ".eth[" + std::to_string(interface_index) + "].mac.queue.bounceBackQueue";
                    queueing::PacketQueue *queue = check_and_cast<queueing::PacketQueue *>(getModuleByPath(module_path_string.c_str()));
                    if (queue->getMaxNumPackets() != -1) {
                        // Packet capacity
                        queue_occupancy = queue->getNumPackets();
                    } else {
                        // Data capacity
                        queue_occupancy = queue->getTotalLength().get();
                    }
                } else if (bounce_on_same_path) {
                    module_path_string = switch_name + ".eth[" + std::to_string(interface_index) + "].mac";
                    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                    std::string queue_full_path = module_path_string + ".queue";
                    if (send_header_of_dropped_packet_to_receiver) {
                        queue_full_path = module_path_string + ".queue.mainQueue";
                    }
                    queue_occupancy = mac->get_queue_occupancy(queue_full_path);
                } else
                    throw cRuntimeError("Function called with a bouncing technique that doesn't require this function!");
                EV << "considering " << module_path_string << " with occupancy: " << queue_occupancy << endl;
                if (min_queue_occupancy == -1 || queue_occupancy < min_queue_occupancy) {
                    min_queue_occupancy = queue_occupancy;
                    chosen_source_index = interface_index;
                }
            }
            ie = ifTable->getInterface(chosen_source_index);
        }

        else if (use_ecmp) {
            packet->setFrontIteratorPosition(b(0));
            packet->removeAtFront<EthernetMacHeader>();
            const auto& ipv4Header = packet->removeAtFront<Ipv4Header>();
            const auto& tcpHeader = packet->removeAtFront<tcp::TcpHeader>();
            EV << "Sepehr: The flow info is: ( src_ip: " << ipv4Header->getSourceAddress() << ", dest_ip: " << ipv4Header->getDestinationAddress() << ", src_port: " << tcpHeader->getSourcePort() << ", dest_port: " << tcpHeader->getDestinationPort() << " )" << endl;
            std::string header_info = ipv4Header->getSourceAddress().str() + ipv4Header->getDestinationAddress().str() +
                                std::to_string(tcpHeader->getSourcePort()) + std::to_string(tcpHeader->getDestinationPort());
            unsigned long header_info_hash = header_hash(header_info);
            int outputPortNum;
            outputPortNum = header_info_hash % portNum;
            std::list<int>::iterator it = srcInterfaceIds.begin();
            std::advance(it, outputPortNum);
            ie = ifTable->getInterfaceById(*it);
        }
    }
    else {
        ie = ifTable->getInterfaceById(srcInterfaceIds.front());
    }
    return ie;
}

InterfaceEntry* BouncingIeee8021dRelay::find_interface_to_bounce_probabilistically(Packet *packet, InterfaceEntry *original_output_if) {
    if (!can_deflect)
            throw cRuntimeError("find_interface_to_bounce_probabilistically! can deflect is false!");
    // This is how its done in PABO
    EV << "BouncingIeee8021dRelay::find_interface_to_bounce_probabilistically called." << endl;
    std::string switch_name = getParentModule()->getFullName();
    InterfaceEntry *ie = nullptr;
    double utilization = get_port_utilization(original_output_if->getIndex(), packet);
    Packet* packet_dup = packet->dup();
    auto frame = packet->removeAtFront<EthernetMacHeader>();
    int total_hop_num = frame->getTotalHopNum();
    int bounced_distance = frame->getBouncedDistance();
    int bounced_hop = frame->getBouncedHop();
    int max_bounced_distance = frame->getMaxBouncedDistance();

    if (bounced_distance < 0 || bounced_hop < 0 || bounced_distance < 0 || total_hop_num < 0)
        throw cRuntimeError("Something is wrong with your way of calculating bouncing variables.");

    EV << "Packet's bounce distance is: " << frame->getBouncedDistance() << endl;
    if (utilization > utilization_thresh) {
        double probability_numerator = std::exp(bounce_probability_lambda * (utilization_thresh - utilization) / (bounced_hop + 1)) - 1;
        double probability_denuminator = std::exp(bounce_probability_lambda * (utilization_thresh - 1) / (bounced_hop + 1)) - 1;
        double probability = probability_numerator / probability_denuminator;

        if (probability > 1)
            throw cRuntimeError("probability > 1? Doesn't make sense.");

        double dice = dblrand();

        EV << "switch: " << switch_name << " ,port: " << original_output_if->getInterfaceId() <<
                " passing back packet " << packet->str() << "(passBackNum= " << bounced_hop <<
                " ) ,with probability = " << probability << " , dice = " << dice << endl;
        bool should_bounce = dice <= probability;
        if (should_bounce) {
            // bounce back
            bounced_hop++;
            frame->setBouncedHop(bounced_hop);
            bounced_distance++;
            frame->setBouncedDistance(bounced_distance);

            if (max_bounced_distance < bounced_distance) {
                max_bounced_distance = bounced_distance;
                frame->setMaxBouncedDistance(max_bounced_distance);
            }

            total_hop_num++;
            frame->setTotalHopNum(total_hop_num);

            // Bounce back the packet on the same path
            int arrivalInterfaceId = packet->getTag<InterfaceInd>()->getInterfaceId();
            if (arrivalInterfaceId != original_output_if->getInterfaceId())
                ie = ifTable->getInterfaceById(arrivalInterfaceId);
            else {
                ie = find_a_port_for_packet_towards_source(packet_dup);
            }

            EV << "Bouncing back frame " << frame << " with src address " << frame->getSrc() <<
                    " at the " << frame->getBouncedDistance() << " hop " << " to port " << ie->getInterfaceId() << endl;


        } else {
            // Forward the packet normally
            if(bounced_distance >= 1) {
                bounced_distance--;
                frame->setBouncedDistance(bounced_distance);
            }
            total_hop_num++;
            frame->setTotalHopNum(total_hop_num);

            ie = original_output_if;
            EV << "1) Forwarding the packet towards its original destination." << endl;
        }

    } else {
        // Forward the packet normally
        if(bounced_distance >= 1) {
            bounced_distance--;
            frame->setBouncedDistance(bounced_distance);
        }
        total_hop_num++;
        frame->setTotalHopNum(total_hop_num);

        ie = original_output_if;
        EV << "2) Forwarding the packet towards its original destination." << endl;
    }

    packet->insertAtFront(frame);
    delete packet_dup;
    return ie;

}

InterfaceEntry* BouncingIeee8021dRelay::find_interface_to_bounce_on_the_same_path(Packet *packet, InterfaceEntry *original_output_if) {
    if (!can_deflect)
        throw cRuntimeError("find_interface_to_bounce_on_the_same_path! can deflect is false!");
    // This is the main version of Vertigo which bounces everything on the same path
    std::string module_path_string;
    std::string switch_name = getParentModule()->getFullName();
    b packet_length = b(packet->getBitLength());
    int arrivalInterfaceId = packet->getTag<InterfaceInd>()->getInterfaceId();
    InterfaceEntry *ie;
    if (arrivalInterfaceId != original_output_if->getInterfaceId())
        ie = ifTable->getInterfaceById(arrivalInterfaceId);
    else {
        Packet *packet_dup = packet->dup();
        ie = find_a_port_for_packet_towards_source(packet_dup);
        delete packet_dup;
    }
    module_path_string = switch_name + ".eth[" + std::to_string(ie->getIndex()) + "].mac";
    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
    EV << "Arrival interface id is " << arrivalInterfaceId << " and it's position is " << ie->getIndex() << endl;
    std::string queue_full_path = module_path_string + ".queue";
    if (send_header_of_dropped_packet_to_receiver) {
        queue_full_path = module_path_string + ".queue.mainQueue";
    }
    if (mac->is_queue_full(packet_length, queue_full_path)) {
        EV << "Arrival interface is full as well, dropping the packet!";
        return nullptr;
    }
    EV << "I am " << getFullPath() << endl;
    EV << "The packet is bounced on the same path to id: " << ie->getInterfaceId() << endl;

    std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ ie->getIndex())->getPathEndGate()->getFullPath();
    bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;
    if (is_other_side_input_module_path_server) {
        EV << "The chosen bouncing port is towards a host! Reversing packet mac" << endl;
        b packetPosition = packet->getFrontOffset();
        EV << "SEPEHR: This packet is a feedback packet: " << packet << endl;
        EV << "SEPEHR: Change src and dest address before going on." << endl;
        packet->setFrontIteratorPosition(b(0));
        auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
        auto frame = packet->removeAtFront<EthernetMacHeader>();
        const MacAddress dest = frame->getDest();
        const MacAddress src = frame->getSrc();
        EV << "SEPEHR: packet's current addresses are src: " << src.str() << " and dest: " << dest.str() << endl;
        frame->setSrc(dest);
        frame->setDest(src);
        packet->insertAtFront(frame);
        auto frame2 = packet->peekAtFront<EthernetMacHeader>();
        EV << "SEPEHR: packet's updated addresses are src: " << frame2->getSrc().str() << " and dest: " << frame2->getDest().str() << endl;
        packet->insertAtFront(phyHeader);
        packet->setFrontIteratorPosition(packetPosition);
        EV << "SEPEHR: new packet is: " << packet << endl;
    }

    return ie;
}

unsigned int BouncingIeee8021dRelay::update_memory_hash_maps(MacAddress mac_address, std::list<int> interface_ids) {
    EV << "BouncingIeee8021dRelay::update_memory_hash_maps called" << endl;
    uint64 address = mac_address.getInt();
    auto address_it = mac_addr_to_prefix_map.find(address);

    if (address_it != mac_addr_to_prefix_map.end()) {
        // we have the group id for this address
        EV << "Address " << address << " is already recorded in mac_addr_to_prefix_map."
                " returning " << address_it->second << endl;
        return address_it->second;
    }

    std::string group_id_str = "";
    for (auto interface_id_it = interface_ids.begin();
            interface_id_it != interface_ids.end();
            interface_id_it++) {
        group_id_str += std::to_string(*interface_id_it);
        group_id_str += ",";
    }

    auto group_id_str_it = group_to_prefix_map.find(group_id_str);
    if (group_id_str_it != group_to_prefix_map.end()) {
        // the ecmp group was seen before just not for this address
        mac_addr_to_prefix_map.insert(
                            std::pair<uint64,
                            unsigned int>(address,
                                    group_id_str_it->second));
        EV << "Group " << group_id_str << " is already recorded in group_to_prefix_map."
                        " returning " << group_id_str_it->second << endl;
        return group_id_str_it->second;
    }

    // never seen this group for any address before
    unsigned int group_id = prefix_id_counter;
    prefix_id_counter++;
    group_to_prefix_map.insert(
                    std::pair<std::string, unsigned int>(group_id_str, group_id));
    mac_addr_to_prefix_map.insert( std::pair<uint64, unsigned int>(address,
                                        group_id));
    std::list<int> empty_memory_list;
    for (int i = 0; i < random_power_memory_size; i++)
        empty_memory_list.push_back(-1);
    prefix_to_memory_map.insert(std::pair<unsigned int,
            std::list<int>>(group_id, empty_memory_list));
    EV << "Created new group, returning " << group_id << endl;
    return group_id;
}

InterfaceEntry* BouncingIeee8021dRelay::find_interface_to_fw_randomly_power_of_n(Packet *packet, bool consider_servers) {
    // This forwards or bounces the packet randomly to ports with free space using power of two choices
    InterfaceEntry *ie;
    std::string module_path_string;
    std::string switch_name = getParentModule()->getFullName();
    b packet_length = b(packet->getBitLength());
    const auto& frame = packet->peekAtFront<EthernetMacHeader>();
    long min_queue_occupancy = -1, max_queue_occupancy = -1;
    int chosen_source_index = -1;
    std::list<int> chosen_interface_indexes;
    int chosen_interface_counter = 0;
    const MacAddress address = frame->getDest();
    EV << "Finding port for " << address << endl;
    std::list<int> interface_ids = macTable->getInterfaceIdForAddress(address);
    interface_ids.sort();
    bool have_more_than_one_fw_ports = (interface_ids.size() > 1);

    unsigned int group_id;
    // this makes sure that we do not consider a port multiple times
    // to avoid messing with 50-50 chance in tie breaking
    std::unordered_set<int> chosen_interfaces_set;
    if (use_memory && have_more_than_one_fw_ports) {
        group_id = update_memory_hash_maps(address, interface_ids);
    }

    //Try finding an available port that goes towards the source of the packet.
    // Consider ports towards sources for forwarding but not for bouncing
    while(chosen_interface_counter < random_power_factor && interface_ids.size() != 0) {
        int random_idx = rand() % interface_ids.size();
        EV << "randomly choosing one of the ports. random_idx is " << random_idx << endl;
        std::list<int>::iterator it = interface_ids.begin();
        std::advance(it, random_idx);
        int interface_idx = ifTable->getInterfaceById(*it)->getIndex();
        module_path_string = switch_name + ".eth[" + std::to_string(interface_idx) + "].mac";
        EV << "finding main ports: " << module_path_string << endl;
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ interface_idx)->getPathEndGate()->getFullPath();
        bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;

        if (consider_servers || !is_other_side_input_module_path_server) {
            // I count the chosen ports but only add those with free
            // ports because those that are full do not matter.
            chosen_interface_counter++;
            std::string queue_full_path = module_path_string + ".queue";
            if (send_header_of_dropped_packet_to_receiver) {
                queue_full_path = module_path_string + ".queue.mainQueue";
            }
            if (use_vertigo_prio_queue || use_pfabric || use_v2_pifo || !mac->is_queue_full(packet_length, queue_full_path)) {
                chosen_interface_indexes.push_back(interface_idx);
                chosen_interfaces_set.insert(interface_idx);
                EV << "Adding port: " << module_path_string << endl;
            }
        }
        interface_ids.erase(it);
        if (interface_ids.size() == (macTable->getInterfaceIdForAddress(address)).size())
            throw cRuntimeError("You have also changed the size of base array of source_interface_ids.");
    }

    // add the memory: the last element in the memory is the latest used
    std::unordered_map<unsigned int, std::list<int>>::iterator memory_map_it;
    if (use_memory && have_more_than_one_fw_ports) {
        EV << "Adding memory" << endl;
        int memory_counter = 0;
        memory_map_it = prefix_to_memory_map.find(group_id);
        auto memory_it = memory_map_it->second.end();
        while (memory_counter < random_power_memory_size) {
            if (memory_it == memory_map_it->second.begin()) {
                throw cRuntimeError("How is -- memory_it == memory_list.begin() -- possible?");
            }
            memory_it--;
            int interface_idx = (*memory_it);
            EV << "interface_idx is " << interface_idx << endl;
            memory_counter++;
            if (interface_idx >= 0) {
                // a memory is saved
                module_path_string = switch_name + ".eth[" + std::to_string(interface_idx) + "].mac";
                EV << "finding main ports: " << module_path_string << endl;
                AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ interface_idx)->getPathEndGate()->getFullPath();
                bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;

                if (consider_servers || !is_other_side_input_module_path_server) {
                    // I count the chosen ports but only add those with free
                    // ports because those that are full do not matter.
                    std::string queue_full_path = module_path_string + ".queue";
                    if (send_header_of_dropped_packet_to_receiver) {
                        queue_full_path = module_path_string + ".queue.mainQueue";
                    }
                    if (use_vertigo_prio_queue || use_pfabric || use_v2_pifo || !mac->is_queue_full(packet_length, queue_full_path)) {
                        if (chosen_interfaces_set.find(interface_idx) == chosen_interfaces_set.end()) {
                            chosen_interface_indexes.push_back(interface_idx);
                            chosen_interfaces_set.insert(interface_idx);
                            EV << "Adding memory port: " << module_path_string << endl;
                        }
                    }
                }
            }
        }
    }


    if (chosen_interface_indexes.size() == 0) {
        EV << "No available ports was found to forward the packet normally" << endl;
        return nullptr;
    }

    // Apply power of n: Find least congested port and forward the packet to that port
    chosen_interface_indexes.sort();
    for (std::list<int>::iterator it=chosen_interface_indexes.begin(); it != chosen_interface_indexes.end(); it++){
        module_path_string = switch_name + ".eth[" + std::to_string(*it) + "].mac";
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        std::string queue_full_path = module_path_string + ".queue";
        if (send_header_of_dropped_packet_to_receiver) {
            queue_full_path = module_path_string + ".queue.mainQueue";
        }
        long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
        EV << "considering " << module_path_string << " with occupancy: " << queue_occupancy << endl;
        if (min_queue_occupancy == -1 || queue_occupancy < min_queue_occupancy) {
            min_queue_occupancy = queue_occupancy;
            chosen_source_index = *it;
        } else if (queue_occupancy == min_queue_occupancy) {
            // two equally full buffers, break the tie randomly
            double dice = dblrand();
            EV << "Two ports with occupancy of " << min_queue_occupancy << ". Breaking tie: dice is " << dice << endl;
            if (dice >= 0.5) {
                // 50% update the chosen source idx
                chosen_source_index = *it;
            }
        }
    }

    EV << "Chosen port: " << chosen_source_index << endl;


    if (use_memory && have_more_than_one_fw_ports) {
        // if the chosen source index is already in the list we don't need to update the memory
        bool found = false;
        // front is written last, so pop_front would return -1 if there is space in memory
        int last_element_in_mem = memory_map_it->second.front();
        std::list<int>::iterator max_occupancy_port_in_memory_idx;
        for (auto list_it = memory_map_it->second.begin();
                list_it != memory_map_it->second.end();
                list_it++) {
            if ((*list_it) == chosen_source_index) {
                EV << "chosen_source_index is already in the memory!" << endl;
                found = true;
                break;
            }
            if (last_element_in_mem != -1) {
                // if max_occupancy_port_in_memory_idx has -1 in it which is the initial value in
                // memory, then we have enough space in memory for a new record and do not need
                // to remove one, make sure to always push_back
                // entering this if means that all the initial -1s in memory are over-written
                module_path_string = switch_name + ".eth[" + std::to_string(*list_it) + "].mac";
                AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                std::string queue_full_path = module_path_string + ".queue";
                if (send_header_of_dropped_packet_to_receiver) {
                    queue_full_path = module_path_string + ".queue.mainQueue";
                }
                long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
                if (max_queue_occupancy == -1 || queue_occupancy > max_queue_occupancy) {
                    max_queue_occupancy = queue_occupancy;
                    max_occupancy_port_in_memory_idx = list_it;
                } else if (queue_occupancy == max_queue_occupancy) {
                    // two equally full buffers, break the tie randomly
                    double dice = dblrand();
                    EV << "Two ports in memory with occupancy of " << max_queue_occupancy << ". Breaking tie: dice is " << dice << endl;
                    if (dice >= 0.5) {
                        // 50% update the chosen source idx
                        max_occupancy_port_in_memory_idx = list_it;
                    }
                }
                EV << "max_occupancy_port_in_memory_idx: " << (*max_occupancy_port_in_memory_idx) << endl;
            }
        }

        if (!found) {
            if (last_element_in_mem == -1) {
                // enough space in memory (it still has -1)
                EV << "Enough space in memory... no need to remove any record!" << endl;
                memory_map_it->second.pop_front();
                memory_map_it->second.push_back(chosen_source_index);
            } else {
                // should replace
                module_path_string = switch_name + ".eth[" + std::to_string(chosen_source_index) + "].mac";
                AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                std::string queue_full_path = module_path_string + ".queue";
                if (send_header_of_dropped_packet_to_receiver) {
                    queue_full_path = module_path_string + ".queue.mainQueue";
                }
                long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
                if (queue_occupancy < max_queue_occupancy) {
                    // replace it
                    EV << "Replacing " << (*max_occupancy_port_in_memory_idx) << " with " << chosen_source_index << endl;
                    auto memory_replace_it = memory_map_it->second.erase(max_occupancy_port_in_memory_idx);
                    memory_map_it->second.push_back(chosen_source_index);
                }
            }
        }
        if (memory_map_it->second.size() != random_power_memory_size)
            throw cRuntimeError("memory_list.size() != random_power_memory_size");
    }

    module_path_string = switch_name + ".eth[" + std::to_string(chosen_source_index) + "].mac";
    EV << "Chosen port: " << module_path_string << endl;
    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
    ie = ifTable->getInterface(chosen_source_index);

    if (!consider_servers) {
        std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ ie->getIndex())->getPathEndGate()->getFullPath();
        bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;
        if (is_other_side_input_module_path_server) {
            throw cRuntimeError("The chosen bouncing port is towards a host!");
        }
    }

    return ie;
}

void BouncingIeee8021dRelay::apply_early_deflection(Packet *packet, bool consider_servers, InterfaceEntry *ie2) {

    // bounce the packet

    EV << "BouncingIeee8021dRelay::apply_early_deflection called." << endl;
//    std::cout << simTime() << ": applying early deflection!" << endl;

    std::string module_path_string;
    std::string switch_name = getParentModule()->getFullName();
    module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac";

    // This forwards or bounces the packet randomly to ports using power of two choices
    std::string queue_full_path;
    InterfaceEntry *ie;
    EV << "Bouncing the ejected packet " << packet->str() << endl;
    b packet_length = b(packet->getBitLength());
    const auto& frame = packet->peekAtFront<EthernetMacHeader>();
    long min_queue_occupancy = -1;
    int chosen_source_index = -1;
    std::list<int> chosen_interface_indexes;
    int chosen_interface_counter = 0;

    // we choose two random ports and drop if both of them are full
    chosen_interface_counter = 0;
    std::list<int> port_idx_connected_to_switch_neioghbors_copy = port_idx_connected_to_switch_neioghbors;

    while(chosen_interface_counter < random_power_bounce_factor && port_idx_connected_to_switch_neioghbors_copy.size() != 0) {
        int random_idx = rand() % port_idx_connected_to_switch_neioghbors_copy.size();
        EV << "randomly choosing a port for bounce towards neighbor switches. random_idx is " << random_idx << endl;
        std::list<int>::iterator it = port_idx_connected_to_switch_neioghbors_copy.begin();
        std::advance(it, random_idx);
        module_path_string = switch_name + ".eth[" + std::to_string(*it) + "].mac";
        EV << "finding additional ports: " << module_path_string << endl;
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        queue_full_path = module_path_string + ".queue";
        if (!mac->is_queue_over_v2_threshold(packet_length, queue_full_path, packet) ||
                !mac->is_packet_tag_larger_than_last_packet(queue_full_path, packet)) {
            // We don't want to add the port only if we are using v2_pifo and we are not planning
            // to use NDP, accordingly, we leave the packet to get dropped in the queue
            chosen_interface_indexes.push_back(*it);
            EV << "Adding port: " << module_path_string << endl;
        }

        chosen_interface_counter++;
        port_idx_connected_to_switch_neioghbors_copy.erase(it);
        if (port_idx_connected_to_switch_neioghbors.size() == port_idx_connected_to_switch_neioghbors_copy.size())
            throw cRuntimeError("2)You have also changed the size of base array of port_idx_connected_to_switch_neioghbors.");
    }

    if (chosen_interface_indexes.size() == 0) {
        // Cannot bounce this packet, drop it and get to the next packet
        EV << "No available ports was found to bounce the packet, drop the bounced packet." << endl;
//            emit(feedBackPacketDroppedSignal, int(frame->getIs_bursty()));
//            emit(feedBackPacketDroppedPortSignal, ie2->getIndex());
        light_in_relay_packet_drop_counter++;
        delete packet;
        return;
    }

    // Apply power of n: Find least congested port and forward the packet to that port
    chosen_interface_indexes.sort();
    for (std::list<int>::iterator it=chosen_interface_indexes.begin(); it != chosen_interface_indexes.end(); it++){
        module_path_string = switch_name + ".eth[" + std::to_string(*it) + "].mac";
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        std::string queue_full_path = module_path_string + ".queue";
        if (send_header_of_dropped_packet_to_receiver) {
            queue_full_path = module_path_string + ".queue.mainQueue";
        }
        long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
        EV << "considering " << module_path_string << " with occupancy: " << queue_occupancy << endl;
        if (min_queue_occupancy == -1 || queue_occupancy < min_queue_occupancy) {
            min_queue_occupancy = queue_occupancy;
            chosen_source_index = *it;
        } else if (queue_occupancy == min_queue_occupancy) {
            // two equally full buffers, break the tie randomly
            double dice = dblrand();
            EV << "Two ports with occupancy of " << min_queue_occupancy << ". Breaking tie: dice is " << dice << endl;
            if (dice >= 0.5) {
                // 50% update the chosen source idx
                chosen_source_index = *it;
            }
        }
    }
    if (send_header_of_dropped_packet_to_receiver) {
        module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac.queue.mainQueue";
    } else {
        module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac.queue";
    }

    module_path_string = switch_name + ".eth[" + std::to_string(chosen_source_index) + "].mac";
    EV << "Chosen port: " << module_path_string << endl;
    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
    ie = ifTable->getInterface(chosen_source_index);

    queue_full_path =  module_path_string + ".queue";
    if (send_header_of_dropped_packet_to_receiver) {
        queue_full_path = module_path_string + ".queue.mainQueue";
    }

    // See if you should push the packet or drop it
    if (mac->is_queue_full(packet_length, queue_full_path) ||
                        (mac->is_queue_over_v2_threshold(packet_length, queue_full_path, packet) &&
                        mac->is_packet_tag_larger_than_last_packet(queue_full_path, packet))) {
        EV << "Dropping deflected packet packet to " << queue_full_path << endl;
//        std::cout << simTime() << ": Dropping deflected packet packet to " << queue_full_path << endl;
        light_in_relay_packet_drop_counter++;
        delete packet;
        return;
    }

    // Send packet
    auto mac_header = packet->peekAtFront<EthernetMacHeader>();
    mac->add_on_the_way_packet(b(packet->getBitLength()), mac_header->getIs_v2_dropped_packet_header());
    EV << module_path_string << ": ";
    if (!consider_servers) {
        std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ ie->getIndex())->getPathEndGate()->getFullPath();
        bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;
        if (is_other_side_input_module_path_server) {
            throw cRuntimeError("The chosen bouncing port is towards a host!");
        }
    }

    emit(feedBackPacketGeneratedSignal, packet->getId());
    EV << "Sending frame " << packet << " on output interface " << ie->getFullName() << " with destination = " << frame->getDest() << endl;
    numDispatchedNonBPDUFrames++;
    auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
    packet->clearTags();
    auto newPacketProtocolTag = packet->addTag<PacketProtocolTag>();
    *newPacketProtocolTag = *oldPacketProtocolTag;
    delete oldPacketProtocolTag;
    packet->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
    packet->trim();
    emit(packetSentToLowerSignal, packet);
    send(packet, "ifOut");
}

void BouncingIeee8021dRelay::find_interface_to_bounce_randomly_v2(Packet *packet, bool consider_servers, InterfaceEntry *ie2) {
    if (!can_deflect)
        throw cRuntimeError("find_interface_to_bounce_randomly_v2! can deflect is false!");
    // eject the packets to create room for packet
    EV << "find_interface_to_bounce_randomly_v2 called!" << endl;
    std::string module_path_string;
    std::string switch_name = getParentModule()->getFullName();
    b packetPosition = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    auto phyHeader = packet->removeAtFront<EthernetPhyHeader>();
    auto ethHeader = packet->removeAtFront<EthernetMacHeader>();
    auto ipHeader = packet->removeAtFront<Ipv4Header>();

    unsigned long seq, ret_count;
    for (unsigned int i = 0; i < ipHeader->getOptionArraySize(); i++) {
        const TlvOptionBase *option = &ipHeader->getOption(i);
        if (option->getType() == IPOPTION_V2_MARKING) {
            auto opt = check_and_cast<const Ipv4OptionV2Marking*>(option);
            seq = opt->getSeq();
            ret_count = opt->getRet_num();
            break;
        }
        // Check if something is returned
        if (i == ipHeader->getOptionArraySize() - 1)
            throw cRuntimeError("Marking cannot be off at this position!");
    }

    EV << "Packet's seq = " << seq << " and ret_count = " << ret_count << endl;

    packet->insertAtFront(ipHeader);
    packet->insertAtFront(ethHeader);
    packet->insertAtFront(phyHeader);
    packet->setFrontIteratorPosition(packetPosition);
    const auto& frame = packet->peekAtFront<EthernetMacHeader>();
    PacketQueue* queue;
    if (send_header_of_dropped_packet_to_receiver) {
        module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac.queue.mainQueue";
    } else {
        module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac.queue";
    }
    if (use_v2_pifo) {
        queue = check_and_cast<V2PIFO *>(getModuleByPath(module_path_string.c_str()));
    } else if (use_pfabric)
        queue = check_and_cast<pFabric *>(getModuleByPath(module_path_string.c_str()));
    module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac";

    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
    std::string queue_full_path = module_path_string + ".queue";
    if (send_header_of_dropped_packet_to_receiver) {
        queue_full_path = module_path_string + ".queue.mainQueue";
    }
    int extraction_port_interface_id = mac->interfaceEntry->getInterfaceId();
    EV << "Trying to inject packet" << packet->str() << " with seq=" << seq << endl;
    EV << module_path_string << ": ";
    int num_packets_to_eject = queue->getNumPacketsToEject(b(packet->getBitLength()), seq, ret_count,
            mac->on_the_way_packet_num, mac->on_the_way_packet_length);
    EV << "Number of packets to eject is " << num_packets_to_eject << endl;
    std::list<Packet*> ejected_packets;
    bool deflecting_original_packet = false;
    if (num_packets_to_eject < 0) {
        // bounce packet itself
        // we have already generated an SRC if the type is org or org_def
        // so here just check if type is def and generate src in that case because we are deflecting
        // the packet itself
        // we send the SRC here if we're deflecting original packet and if we're not, we send it
        // in the while loop
        deflecting_original_packet = true;
        EV << "Adding the main packets to the list to be bounced" << endl;
        if (src_enabled_packet_type == BOLT_VERTIGO_DEF_PKT)
            bolt_evaluate_if_src_packet_should_be_generated(packet, ie2, ignore_cc_thresh_for_deflected_packets);
        if (dctcp_mark_deflected_packets_only)
            dctcp_mark_ecn_for_deflected_packets(packet);
        ejected_packets.push_back(packet);
    } else {
        // eject packets to make room for the main packet
        EV << "Ejecting packets from the queue to be bounced." << endl;
        module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac";
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        auto frame = packet->peekAtFront<EthernetMacHeader>();
        mac->add_on_the_way_packet(b(packet->getBitLength()), frame->getIs_v2_dropped_packet_header());

        ejected_packets = queue->eject_and_push(num_packets_to_eject);

        // send the main packet to the output queue
        EV << "Sending the main packet " << packet << " on output interface " << ie2->getFullName() << " with destination = " << frame->getDest() << endl;
        numDispatchedNonBPDUFrames++;
        auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
        packet->clearTags();
        auto newPacketProtocolTag = packet->addTag<PacketProtocolTag>();
        *newPacketProtocolTag = *oldPacketProtocolTag;
        delete oldPacketProtocolTag;
        packet->addTag<InterfaceReq>()->setInterfaceId(ie2->getInterfaceId());
        packet->addTag<PacketInsertionControlTag>()->setInsert_packet_without_checking_the_queue(true);
        packet->trim();
        emit(packetSentToLowerSignal, packet);
        send(packet, "ifOut");
    }

    // bounce the remaining packets
    while (ejected_packets.size() > 0){

        // This forwards or bounces the packet randomly to ports with free space using power of two choices
        InterfaceEntry *ie;
        auto packet = ejected_packets.front();

        if (!deflecting_original_packet &&
                (src_enabled_packet_type == BOLT_VERTIGO_DEF_PKT ||
                        src_enabled_packet_type == BOLT_VERTIGO_ORG_DEF_PKT)) {
            bolt_evaluate_if_src_packet_should_be_generated(packet, ie2, ignore_cc_thresh_for_deflected_packets,
                    false, extraction_port_interface_id);
        }

        if (!deflecting_original_packet && dctcp_mark_deflected_packets_only)
            dctcp_mark_ecn_for_deflected_packets(packet, false);

        mark_packet_deflection_tag(packet, deflecting_original_packet);

        if (add_queue_idx_to_deflected)
            append_queue_idx_to_packet_header(packet, deflecting_original_packet);

        EV << "Bouncing the ejected packet " << packet->str() << endl;
        ejected_packets.pop_front();
        b packet_length = b(packet->getBitLength());
        const auto& frame = packet->peekAtFront<EthernetMacHeader>();
        long min_queue_occupancy = -1, max_queue_occupancy = -1;
        int chosen_source_index = -1;
        std::list<int> chosen_interface_indexes;
        int chosen_interface_counter = 0;

        // we choose two random ports and drop if both of them are full
        chosen_interface_counter = 0;
        std::list<int> port_idx_connected_to_switch_neioghbors_copy = port_idx_connected_to_switch_neioghbors;
        std::unordered_set<int> chosen_interfaces_set;
        while(chosen_interface_counter < random_power_bounce_factor && port_idx_connected_to_switch_neioghbors_copy.size() != 0) {
            int random_idx = rand() % port_idx_connected_to_switch_neioghbors_copy.size();
            EV << "randomly choosing a port for bounce towards neighbor switches. random_idx is " << random_idx << endl;
            std::list<int>::iterator it = port_idx_connected_to_switch_neioghbors_copy.begin();
            std::advance(it, random_idx);
            module_path_string = switch_name + ".eth[" + std::to_string(*it) + "].mac";
            EV << "finding additional ports: " << module_path_string << endl;
            mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
            queue_full_path = module_path_string + ".queue";
            if (((use_pfabric || use_v2_pifo) && !drop_bounced_in_relay) || !mac->is_queue_full(packet_length, queue_full_path)) {
                // We don't want to add the port only if we are using v2_pifo and we are not planning
                // to use NDP, accordingly, we leave the packet to get dropped in the queue
                chosen_interface_indexes.push_back(*it);
                chosen_interfaces_set.insert(*it);
                EV << "Adding port: " << module_path_string << endl;
            }

            chosen_interface_counter++;
            port_idx_connected_to_switch_neioghbors_copy.erase(it);
            if (port_idx_connected_to_switch_neioghbors.size() == port_idx_connected_to_switch_neioghbors_copy.size())
                throw cRuntimeError("2)You have also changed the size of base array of port_idx_connected_to_switch_neioghbors.");
        }

        // add the memory: the last element in the memory is the latest used
        if (use_memory) {
            EV << "Adding memory" << endl;
            int memory_counter = 0;
            auto memory_it = deflection_memory.end();
            while (memory_counter < random_power_bounce_memory_size) {
                if (memory_it == deflection_memory.begin())
                    throw cRuntimeError("Deflection: How is -- memory_it == memory_list.begin() -- possible?");
                memory_it--;
                memory_counter++;
                int interface_idx = (*memory_it);
                EV << "Interface_idx for deflection is " << interface_idx << endl;
                if (interface_idx >= 0) {
                    // a memory is saved
                    module_path_string = switch_name + ".eth[" + std::to_string(interface_idx) + "].mac";
                    EV << "finding main ports: " << module_path_string << endl;
                    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                    std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ interface_idx)->getPathEndGate()->getFullPath();
                    bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;
                    if (is_other_side_input_module_path_server)
                        throw cRuntimeError("Why do we have deflection toward server in memory");
                    // I count the chosen ports but only add those with free
                    // ports because those that are full do not matter.
                    std::string queue_full_path = module_path_string + ".queue";
                    if (send_header_of_dropped_packet_to_receiver) {
                        queue_full_path = module_path_string + ".queue.mainQueue";
                    }
                    if (((use_pfabric || use_v2_pifo) && !drop_bounced_in_relay) || !mac->is_queue_full(packet_length, queue_full_path)) {
                        if (chosen_interfaces_set.find(interface_idx) == chosen_interfaces_set.end()) {
                            chosen_interface_indexes.push_back(interface_idx);
                            chosen_interfaces_set.insert(interface_idx);
                            EV << "Adding memory port: " << module_path_string << endl;
                        }
                    }
                }
            }
        }

        if (chosen_interface_indexes.size() == 0) {
            // Cannot bounce this packet, drop it and get to the next packet
            EV << "No available ports was found to bounce the packet, drop the bounced packet." << endl;
//            emit(feedBackPacketDroppedSignal, int(frame->getIs_bursty()));
//            emit(feedBackPacketDroppedPortSignal, ie2->getIndex());
            light_in_relay_packet_drop_counter++;
            delete packet;
            continue;
        }

        // Apply power of n: Find least congested port and forward the packet to that port
        chosen_interface_indexes.sort();
        for (std::list<int>::iterator it=chosen_interface_indexes.begin(); it != chosen_interface_indexes.end(); it++){
            module_path_string = switch_name + ".eth[" + std::to_string(*it) + "].mac";
            AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
            std::string queue_full_path = module_path_string + ".queue";
            if (send_header_of_dropped_packet_to_receiver) {
                queue_full_path = module_path_string + ".queue.mainQueue";
            }
            long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
            EV << "considering " << module_path_string << " with occupancy: " << queue_occupancy << endl;
            if (min_queue_occupancy == -1 || queue_occupancy < min_queue_occupancy) {
                min_queue_occupancy = queue_occupancy;
                chosen_source_index = *it;
            } else if (queue_occupancy == min_queue_occupancy) {
                // two equally full buffers, break the tie randomly
                double dice = dblrand();
                EV << "Two ports with occupancy of " << min_queue_occupancy << ". Breaking tie: dice is " << dice << endl;
                if (dice >= 0.5) {
                    // 50% update the chosen source idx
                    chosen_source_index = *it;
                }
            }
        }
        if (send_header_of_dropped_packet_to_receiver) {
            module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac.queue.mainQueue";
        } else {
            module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac.queue";
        }

        if (use_memory) {
            // if the chosen source index is already in the list we don't need to update the memory
            bool found = false;
            // front is written last, so pop_front would return -1 if there is space in memory
            int last_element_in_mem = deflection_memory.front();
            std::list<int>::iterator max_occupancy_port_in_memory_idx;
            for (auto list_it = deflection_memory.begin();
                    list_it != deflection_memory.end();
                    list_it++) {
                if ((*list_it) == chosen_source_index) {
                    EV << "chosen_source_index for deflection is already in the memory!" << endl;
                    found = true;
                    break;
                }
                if (last_element_in_mem != -1) {
                    // if max_occupancy_port_in_memory_idx has -1 in it which is the initial value in
                    // memory, then we have enough space in memory for a new record and do not need
                    // to remove one, make sure to always push_back
                    // entering this if means that all the initial -1s in memory are over-written
                    module_path_string = switch_name + ".eth[" + std::to_string(*list_it) + "].mac";
                    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                    std::string queue_full_path = module_path_string + ".queue";
                    if (send_header_of_dropped_packet_to_receiver) {
                        queue_full_path = module_path_string + ".queue.mainQueue";
                    }
                    long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
                    if (max_queue_occupancy == -1 || queue_occupancy > max_queue_occupancy) {
                        max_queue_occupancy = queue_occupancy;
                        max_occupancy_port_in_memory_idx = list_it;
                    } else if (queue_occupancy == max_queue_occupancy) {
                        // two equally full buffers, break the tie randomly
                        double dice = dblrand();
                        EV << "Two ports in memory with occupancy of " << max_queue_occupancy << ". Breaking tie: dice is " << dice << endl;
                        if (dice >= 0.5) {
                            // 50% update the chosen source idx
                            max_occupancy_port_in_memory_idx = list_it;
                        }
                    }
                    EV << "max_occupancy_port_in_memory_idx: " << (*max_occupancy_port_in_memory_idx) << endl;
                }
            }
            if (!found) {
                if (last_element_in_mem == -1) {
                    // enough space in memory (it still has -1)
                    EV << "Enough space in memory... no need to remove any record!" << endl;
                    deflection_memory.pop_front();
                    deflection_memory.push_back(chosen_source_index);
                } else {
                    // should replace
                    module_path_string = switch_name + ".eth[" + std::to_string(chosen_source_index) + "].mac";
                    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
                    std::string queue_full_path = module_path_string + ".queue";
                    if (send_header_of_dropped_packet_to_receiver) {
                        queue_full_path = module_path_string + ".queue.mainQueue";
                    }
                    long queue_occupancy = mac->get_queue_occupancy(queue_full_path);
                    if (queue_occupancy < max_queue_occupancy) {
                        // replace it
                        EV << "Replacing " << (*max_occupancy_port_in_memory_idx) << " with " << chosen_source_index << endl;
                        auto memory_replace_it = deflection_memory.erase(max_occupancy_port_in_memory_idx);
                        deflection_memory.push_back(chosen_source_index);
                    }
                }
            }
            if (deflection_memory.size() != random_power_bounce_memory_size)
                throw cRuntimeError("deflection_memory.size() != random_power_bounce_memory_size");
        }

        module_path_string = switch_name + ".eth[" + std::to_string(chosen_source_index) + "].mac";
        EV << "Chosen port: " << module_path_string << endl;
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        ie = ifTable->getInterface(chosen_source_index);

        queue_full_path =  module_path_string + ".queue";
        if (send_header_of_dropped_packet_to_receiver) {
            queue_full_path = module_path_string + ".queue.mainQueue";
        }

        if (send_header_of_dropped_packet_to_receiver && mac->is_queue_full(packet_length, queue_full_path)) {
            std::list<Packet*> ejected_packets;
            // Eject from the queue you are bouncing to
            EV << "Ejecting packets from the queue to to make room for the bounced packet." << endl;
            module_path_string = switch_name + ".eth[" + std::to_string(chosen_source_index) + "].mac";
            AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
            module_path_string += ".queue.mainQueue";
            V2PIFO *queue = check_and_cast<V2PIFO *>(getModuleByPath(module_path_string.c_str()));

            auto packet_dup = packet->dup();
            packet_dup->removeAtFront<EthernetMacHeader>();
            auto ipHeader = packet_dup->removeAtFront<Ipv4Header>();
            delete packet_dup;

            unsigned long seq, ret_count;
            for (unsigned int i = 0; i < ipHeader->getOptionArraySize(); i++) {
                const TlvOptionBase *option = &ipHeader->getOption(i);
                if (option->getType() == IPOPTION_V2_MARKING) {
                    auto opt = check_and_cast<const Ipv4OptionV2Marking*>(option);
                    seq = opt->getSeq();
                    ret_count = opt->getRet_num();
                    break;
                }
                // Check if something is returned
                if (i == ipHeader->getOptionArraySize() - 1)
                    throw cRuntimeError("Marking cannot be off at this position!");
            }
            int num_packets_to_eject_for_bounced_packet = queue->getNumPacketsToEject(b(packet->getBitLength()), seq, ret_count,
                        mac->on_the_way_packet_num, mac->on_the_way_packet_length);
            EV << "Number of packets to eject for bounced packet is " << num_packets_to_eject_for_bounced_packet << endl;

            if (num_packets_to_eject_for_bounced_packet < 0)
                ejected_packets.push_back(packet);
            else {
                ejected_packets = queue->eject_and_push(num_packets_to_eject_for_bounced_packet);

                auto frame = packet->peekAtFront<EthernetMacHeader>();
                if (frame->getIs_v2_dropped_packet_header())
                    throw cRuntimeError("How is the packet and ndp packet but we are bouncing it!");

                mac->add_on_the_way_packet(b(packet->getBitLength()), frame->getIs_v2_dropped_packet_header());
                // send the main packet to the output queue
                EV << "Sending the main packet " << packet << " on output interface " << ie->getFullName() << " with destination = " << frame->getDest() << endl;
                numDispatchedNonBPDUFrames++;
                auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
                packet->clearTags();
                auto newPacketProtocolTag = packet->addTag<PacketProtocolTag>();
                *newPacketProtocolTag = *oldPacketProtocolTag;
                delete oldPacketProtocolTag;
                packet->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
                packet->trim();
                send(packet, "ifOut");
            }

            while (ejected_packets.size() > 0) {
                // remove the payload
                // mark the header
                // Send the packet to the original queue
                auto packet = ejected_packets.front();
                ejected_packets.pop_front();
                auto frame = packet->peekAtFront<EthernetMacHeader>();
                if (frame->getOriginal_interface_id() < 0)
                    throw cRuntimeError("frame->getOriginal_interface_id()");
                auto ie = ifTable->getInterfaceById(frame->getOriginal_interface_id());
                EV << "Packet should be dropped. Sending the header to the receiver." << endl;
//                emit(feedBackPacketDroppedSignal, int(frame->getIs_bursty()));
//                emit(feedBackPacketDroppedPortSignal, ie->getIndex());
                light_in_relay_packet_drop_counter++;
                EV << "main packet is " << packet << endl;
                auto packet_dup = packet->dup();
                b position = packet_dup->getFrontOffset();
                packet_dup->setFrontIteratorPosition(b(0));
                Packet* new_packet = new Packet();
                if (num_packets_to_eject < 0 && num_packets_to_eject_for_bounced_packet < 0) {
                    // The main packet has phy header. If the packet is inserted in a queue and ejected, it doesn't
                    // have phy header
                    new_packet->insertAtBack(packet_dup->removeAtFront<EthernetPhyHeader>());
                }
                auto mac_header = packet_dup->removeAtFront<EthernetMacHeader>();
                mac_header->setIs_v2_dropped_packet_header(true);
                new_packet->insertAtBack(mac_header);
                auto ipv4_header = packet_dup->removeAtFront<Ipv4Header>();
                auto tcp_header = packet_dup->removeAtFront<tcp::TcpHeader>();
                auto ether_fcs = packet_dup->popAtBack<EthernetFcs>(ETHER_FCS_BYTES);
                b new_length = mac_header->getChunkLength() + ipv4_header->getChunkLength() +
                        tcp_header->getChunkLength() + ether_fcs->getChunkLength();
                b old_length = (b)packet->getBitLength();
                b data_length = old_length - new_length;
                ipv4_header->setTotalLengthField(ipv4_header->getTotalLengthField() - data_length);
                new_packet->insertAtBack(ipv4_header);
                new_packet->insertAtBack(tcp_header);
                new_packet->insertAtBack(ether_fcs);
                delete packet_dup;
                new_packet->setFrontIteratorPosition(position);
                EV << "Header of packet is " << new_packet << endl;
                EV << "Sending frame " << new_packet << " on output interface " << ie->getFullName() << " with destination = " << frame->getDest() << endl;
                numDispatchedNonBPDUFrames++;
                auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
                packet->clearTags();
                new_packet->clearTags();
                auto newPacketProtocolTag = new_packet->addTag<PacketProtocolTag>();
                *newPacketProtocolTag = *oldPacketProtocolTag;
                delete oldPacketProtocolTag;
                new_packet->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
                new_packet->trim();
                new_packet->setName(packet->getName());
                EV << "ipv4Header->getTotalLengthField: " << ipv4_header->getTotalLengthField() << endl;
                EV << "new_packet->getDataLength(): " << new_packet->getDataLength() << endl;
                delete packet;
                send(new_packet, "ifOut");
            }
            continue;
        }

        // Send packet
        auto mac_header = packet->peekAtFront<EthernetMacHeader>();
        mac->add_on_the_way_packet(b(packet->getBitLength()), mac_header->getIs_v2_dropped_packet_header());
        EV << module_path_string << ": ";
        if (!consider_servers) {
            std::string other_side_input_module_path = getParentModule()->gate(getParentModule()->gateBaseId("ethg$o")+ ie->getIndex())->getPathEndGate()->getFullPath();
            bool is_other_side_input_module_path_server = other_side_input_module_path.find("server") != std::string::npos;
            if (is_other_side_input_module_path_server) {
                throw cRuntimeError("The chosen bouncing port is towards a host!");
            }
        }

        emit(feedBackPacketGeneratedSignal, packet->getId());
        EV << "Sending frame " << packet << " on output interface " << ie->getFullName() << " with destination = " << frame->getDest() << endl;
        numDispatchedNonBPDUFrames++;
        auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
        packet->clearTags();
        auto newPacketProtocolTag = packet->addTag<PacketProtocolTag>();
        *newPacketProtocolTag = *oldPacketProtocolTag;
        delete oldPacketProtocolTag;
        packet->addTag<InterfaceReq>()->setInterfaceId(ie->getInterfaceId());
        packet->trim();
        emit(packetSentToLowerSignal, packet);
        send(packet, "ifOut");
    }
}

void BouncingIeee8021dRelay::mark_packet_deflection_tag(Packet *packet, bool has_phy_header) {
    EV << "mark_packet_deflection_tag called for " << packet->str() << endl;
    b position = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    Ptr<EthernetPhyHeader> phy_header;
    // if a packet is extracted from queue to be deflected it doesn't have a
    // physical header
    if (has_phy_header)
        phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();

    eth_header->setIs_deflected(true);

    packet->insertAtFront(eth_header);
    if (has_phy_header)
        packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(position);
}

void BouncingIeee8021dRelay::append_queue_idx_to_packet_header(Packet *packet, bool has_phy_header) {
    EV << "append_queue_idx_to_packet_header " << packet->str() << endl;
    b position = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    Ptr<EthernetPhyHeader> phy_header;
    // if a packet is extracted from queue to be deflected it doesn't have a
    // physical header
    if (has_phy_header)
        phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();

    if (deflected_queue_idx_association_paradigm == PAR_DEP_BURSTY_VS_NONBURSTY) {
        // prioritizing bursty packets
        std::string packet_name = packet->getName();
        if (packet_name.find("tcpseg") != std::string::npos) {
            if (eth_header->getIs_bursty()) {
                eth_header->setPrio_queue_idx(0);
            } else {
                eth_header->setPrio_queue_idx(2);
            }
        } else {
            // prioritize control packets
            eth_header->setPrio_queue_idx(0);
        }
    }

    packet->insertAtFront(eth_header);
    if (has_phy_header)
        packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(position);
}

void BouncingIeee8021dRelay::dctcp_mark_ecn_for_deflected_packets(Packet *packet, bool has_phy_header) {
    EV << "marking the ecn for the deflected packet!" << endl;
    b position = packet->getFrontOffset();
    packet->setFrontIteratorPosition(b(0));
    Ptr<EthernetPhyHeader> phy_header;
    if (has_phy_header)
        phy_header = packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet->removeAtFront<EthernetMacHeader>();
    auto ip_header = packet->removeAtFront<Ipv4Header>();
    ip_header->setEcn(IP_ECN_CE);
    packet->insertAtFront(ip_header);
    packet->insertAtFront(eth_header);
    if (has_phy_header)
        packet->insertAtFront(phy_header);
    packet->setFrontIteratorPosition(position);
}

void BouncingIeee8021dRelay::bolt_pifo_evaluate_if_src_packet_should_be_generated(Packet *packet, InterfaceEntry *ie, bool ignore_cc_thresh) {
    // This function uses Bolt+PIFO in which we react to the packet that is pushed from before CC_thresh to after it
    // this function is just called when a packet is not being deflected
    EV << "bolt_pifo_evaluate_if_src_packet_should_be_generated for " << packet->str() << endl;
    if (use_bolt && use_pifo_bolt_reaction) {
        std::string switch_name = getParentModule()->getFullName();
        std::string module_path_string = switch_name + ".eth[" + std::to_string(ie->getIndex()) + "].mac";
        AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        std::string queue_full_path = module_path_string + ".queue";
        // in this case we only use V2PIFO queue
        V2PIFO * queue = check_and_cast<V2PIFO *>(getModuleByPath(queue_full_path.c_str()));
        b queue_len;
        if (queue->update_qlen_periodically) {
            queue_len = queue->periodic_qlen_bytes;
        } else {
            queue_len = queue->getTotalLength();
        }
        int queue_len_pkt_num = queue->getNumPackets();
        b ccThresh = queue->CCThresh;
        if (queue_len >= ccThresh) {
            EV << "queue_len >= ccThresh" << endl;
            std::list<Packet*> packets_to_react = queue->get_list_of_packets_to_react(packet);
            EV << "packet_to_react size is " << packets_to_react.size() << endl;
            while (packets_to_react.size() != 0) {
                auto front_packet = packets_to_react.front();
                std::string packet_name = packet->getName();
                bool has_phy_header = false;
                int extraction_port_interface_id = -1;
                if (packet_name.find("tcpseg") != std::string::npos) {
                    // packets that are already in the queue does not have phy header
                    if (front_packet == packet) {
                        EV << "reacting to the original packet!" << endl;
                        if (packets_to_react.size() != 1)
                            throw cRuntimeError("we are reacting to the original packet and the size of the list is not 1");
                        has_phy_header = true;
                    } else {
                        EV << "reacting to a packet that was inserted in the queue previously!" << endl;
                        extraction_port_interface_id = mac->interfaceEntry->getInterfaceId();
                    }
                    b position = front_packet->getFrontOffset();
                    front_packet->setFrontIteratorPosition(b(0));
                    Ptr<EthernetPhyHeader> phy_header;
                    // if a packet is extracted from queue to be deflected it doesn't have a
                    // physical header
                    if (has_phy_header)
                        phy_header = front_packet->removeAtFront<EthernetPhyHeader>();

                    auto eth_header = front_packet->removeAtFront<EthernetMacHeader>();
                    auto ip_header = front_packet->removeAtFront<Ipv4Header>();
                    auto tcp_header = front_packet->removeAtFront<tcp::TcpHeader>();
                    bool should_generate_src = (!tcp_header->getBolt_dec());

                    tcp_header->setBolt_dec(true);
                    tcp_header->setBolt_inc(false);

                    front_packet->insertAtFront(tcp_header);
                    front_packet->insertAtFront(ip_header);
                    front_packet->insertAtFront(eth_header);
                    if (has_phy_header)
                        front_packet->insertAtFront(phy_header);
                    front_packet->setFrontIteratorPosition(position);

                    if (should_generate_src) {
                        auto packet_dup = front_packet->dup();
                        if(!has_phy_header) {
                            auto phy_header = makeShared<EthernetPhyHeader>();
                            packet_dup->insertAtFront(phy_header);
                            packet_dup->setFrontIteratorPosition(B(8));
    //                        throw cRuntimeError("fuck");
                        }
                        generate_and_send_bolt_src_packet(packet_dup, queue_len_pkt_num, mac->get_link_util(), extraction_port_interface_id);
                        delete packet_dup;
                    }
                }
                packets_to_react.pop_front();
            }
        }
    }
}

void BouncingIeee8021dRelay::bolt_evaluate_if_src_packet_should_be_generated(Packet *packet, InterfaceEntry *ie, bool ignore_cc_thresh, bool has_phy_header, int extraction_port_interface_id) {
    // This function uses Bolt's original technique, reacting to newly arrived packets/deflected packets for us
    EV << "bolt_evaluate_if_src_packet_should_be_generated for " << packet->str() << endl;
    if (use_bolt) {
        std::string packet_name = packet->getName();
        if (packet_name.find("tcpseg") != std::string::npos) {
            // the src should be generated only if the packet is datapacket
            std::string switch_name = getParentModule()->getFullName();
            std::string module_path_string = switch_name + ".eth[" + std::to_string(ie->getIndex()) + "].mac";
            AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
            std::string queue_full_path = module_path_string + ".queue";
            b queue_len;
            b queue_cap_bytes;
            int queue_len_pkt_num;
            b ccThresh;
            b sel_reaction_ccThresh_max;
            double relative_priority;
            if (use_bolt_queue) {
                V2PIFOBoltQueue * queue = check_and_cast<V2PIFOBoltQueue *>(getModuleByPath(queue_full_path.c_str()));
                queue_len = queue->getTotalLength();
                queue_cap_bytes = queue->getMaxTotalLength();
                queue_len_pkt_num = queue->getNumPackets();
                ccThresh = queue->CCThresh;
                if (apply_selective_net_reaction && selective_net_reaction_type == QUANTILE_SEL_REACTION)
                    throw cRuntimeError("selective_net_reaction_type == QUANTILE_SEL_REACTION: Selective reaction is not yet implemented for bolt queue!");
                EV << "the length of " << queue->getFullPath();
            } else if (use_bolt_with_vertigo_queue) {
                V2PIFO * queue = check_and_cast<V2PIFO *>(getModuleByPath(queue_full_path.c_str()));
                if (queue->update_qlen_periodically) {
                    queue_len = queue->periodic_qlen_bytes;
                    queue_len_pkt_num = queue->periodic_qlen_num_packets;
                } else {
                    queue_len = queue->getTotalLength();
                    queue_len_pkt_num = queue->getNumPackets();
                }
                queue_cap_bytes = queue->getMaxTotalLength();
                ccThresh = queue->CCThresh;
                if (apply_selective_net_reaction && queue_len >= ccThresh) {
                    EV << "qlen is " << queue_len << endl;
                    sel_reaction_ccThresh_max = queue->bolt_CCThresh_max_selective_reaction;
                    if (sel_reaction_ccThresh_max <= ccThresh)
                        throw cRuntimeError("sel_reaction_ccThresh_max <= ccThresh");
                    if (ignore_cc_thresh)
                        throw cRuntimeError("ignore_cc_thresh and selective reaction cannot be applied together!");
                    if (selective_net_reaction_type == QUANTILE_SEL_REACTION)
                        relative_priority = queue->sel_reaction_calc_quantile(packet);
                }
                EV << "the length of " << queue->getFullPath();
            }
            bool should_src_be_sent = ignore_cc_thresh;
            if (apply_selective_net_reaction && queue_len >= ccThresh) {
                EV << "Applying selective feedback" << endl;
                if (selective_net_reaction_type == QUANTILE_SEL_REACTION) {
                    should_src_be_sent = should_src_be_sent || (queue_len >= sel_reaction_ccThresh_max);
                    if (queue_len < sel_reaction_ccThresh_max) {
                        // we use the condition which is set based on the min and max ccThresh
                        // so we calculate the queue cap and queue len relative to min and max ccThresh
                        EV << "queue_len >= ccThresh && queue_len < sel_reaction_ccThresh_max" << endl;
                        EV << "relative priority is " << relative_priority << endl;
                        b relative_queue_len = queue_len - ccThresh;
                        if (queue_cap_bytes < ccThresh)
                            throw cRuntimeError("queue_cap_bytes < ccThresh");
                        b relative_queue_cap;
                        if (queue_cap_bytes < sel_reaction_ccThresh_max)
                            relative_queue_cap = queue_cap_bytes - ccThresh;
                        else
                            relative_queue_cap = sel_reaction_ccThresh_max - ccThresh;
                        should_src_be_sent = should_src_be_sent || (relative_queue_len > (relative_queue_cap * (1 - sel_reaction_alpha * relative_priority)));
                    }
                } else if (selective_net_reaction_type == NONBURSTY_ONLY_SEL_REACTION) {
                    auto eth_header = packet->peekAtFront<EthernetMacHeader>();
                    should_src_be_sent = should_src_be_sent || (!eth_header->getIs_bursty());
                }
            } else {
                should_src_be_sent = should_src_be_sent || (queue_len >= ccThresh);
            }
            if (should_src_be_sent) {
                EV  << " is " << queue_len << "which is larger than " << ccThresh << endl;
                b position = packet->getFrontOffset();
                packet->setFrontIteratorPosition(b(0));
                Ptr<EthernetPhyHeader> phy_header;
                // if a packet is extracted from queue to be deflected it doesn't have a
                // physical header
                if (has_phy_header)
                    phy_header = packet->removeAtFront<EthernetPhyHeader>();

                auto eth_header = packet->removeAtFront<EthernetMacHeader>();
                auto ip_header = packet->removeAtFront<Ipv4Header>();
                auto tcp_header = packet->removeAtFront<tcp::TcpHeader>();
                bool should_generate_src = (!tcp_header->getBolt_dec());

                tcp_header->setBolt_dec(true);
                tcp_header->setBolt_inc(false);

                packet->insertAtFront(tcp_header);
                packet->insertAtFront(ip_header);
                packet->insertAtFront(eth_header);
                if (has_phy_header)
                    packet->insertAtFront(phy_header);
                packet->setFrontIteratorPosition(position);

                if (should_generate_src) {
                    auto packet_dup = packet->dup();
                    if(!has_phy_header) {
                        auto phy_header = makeShared<EthernetPhyHeader>();
                        packet_dup->insertAtFront(phy_header);
                        packet_dup->setFrontIteratorPosition(B(8));
//                        throw cRuntimeError("fuck");
                    }
                    generate_and_send_bolt_src_packet(packet_dup, queue_len_pkt_num, mac->get_link_util(), extraction_port_interface_id);
                    delete packet_dup;
                }
            }
        }
    }
}

bool BouncingIeee8021dRelay::bolt_is_packet_src(Packet *packet) {
    std::string packet_name = packet->getName();
    return (packet_name.find("SRC") != std::string::npos);
}

void BouncingIeee8021dRelay::generate_and_send_bolt_src_packet(Packet *packet, int queue_occupancy_packet_num, long link_util, int extraction_port_interface_id) {
    // based on bolt's code we use queue occupancy in number of packets not bytes
    EV << "generate_and_send_bolt_src_packet called for " <<
            packet->str() << endl;
    b src_ip_header_len = b(0);
    auto packet_dup = packet->dup();
    b position = packet_dup->getFrontOffset();
    packet_dup->setFrontIteratorPosition(b(0));
    Ptr<EthernetPhyHeader> phy_header;
    // if a packet is extracted from queue to be deflected it doesn't have a
    // physical header
    phy_header = packet_dup->removeAtFront<EthernetPhyHeader>();
    auto eth_header = packet_dup->removeAtFront<EthernetMacHeader>();
    auto ip_header = packet_dup->removeAtFront<Ipv4Header>();
    auto tcp_header = packet_dup->removeAtFront<tcp::TcpHeader>();
    auto ether_fcs = packet_dup->removeAtBack<EthernetFcs>(ETHER_FCS_BYTES);

//    std::cout << simTime() << ", Original packet info: " << endl <<
//            "TCP: src: " << tcp_header->getSrcPort() << ", dst: " << tcp_header->getDestPort() << endl <<
//            "IP: src: " << ip_header->getSrcAddress() << ", dst: " << ip_header->getDestAddress() << endl <<
//            "ETH: src: " << eth_header->getSrc() << ", dst: " << eth_header->getDest() << endl;

    Packet *src_packet = new Packet("SRC");

    unsigned short int src_port = tcp_header->getSrcPort();
    tcp_header->setSrcPort(tcp_header->getDestPort());
    tcp_header->setDestPort(src_port);
    tcp_header->setBolt_tx_time(eth_header->getTime_packet_sent_from_src());
    tcp_header->setSrcbit(true);
    tcp_header->setAckBit(false);
    tcp_header->setLink_util(link_util);
    tcp_header->setQueue_occupancy_packet_num(queue_occupancy_packet_num);
    tcp_header->setCrcMode(inet::CRC_COMPUTED);
    tcp_header->setCrc(0);
    src_ip_header_len += tcp_header->getChunkLength();

    auto src_addr = ip_header->getSrcAddress();
    ip_header->setSrcAddress(ip_header->getDestAddress());
    ip_header->setDestAddress(src_addr);
    // I don't know where this is right
    ip_header->setCrcMode(inet::CRC_COMPUTED);
    ip_header->setCrc(0);
    ip_header->getOptionsForUpdate().deleteOptionByType(IPOPTION_V2_MARKING);
    ip_header->setTimeToLive(TTL);

    unsigned short numOptions = ip_header->getOptionArraySize();
    if (numOptions > 0) {    // options present?
        throw cRuntimeError("Bolt: numOptions for SRC packet IP header should be 0!");
    }
    src_ip_header_len += ip_header->getChunkLength();
    ip_header->setTotalLengthField(B(src_ip_header_len));

    auto src_mac = eth_header->getSrc();
    eth_header->setSrc(eth_header->getDest());
    eth_header->setDest(src_mac);


//    std::cout << "SRC packet info: " << endl <<
//            "TCP: src: " << tcp_header->getSrcPort() << ", dst: " << tcp_header->getDestPort() << endl <<
//            "IP: src: " << ip_header->getSrcAddress() << ", dst: " << ip_header->getDestAddress() << endl <<
//            "ETH: src: " << eth_header->getSrc() << ", dst: " << eth_header->getDest() << endl;
//    std::cout << "-------------------------------" << endl;

    src_packet->insertAtFront(tcp_header);
    src_packet->insertAtFront(ip_header);
    src_packet->insertAtFront(eth_header);
    src_packet->insertAtFront(phy_header);
    src_packet->insertAtBack(ether_fcs);
    src_packet->setFrontOffset(position);

    if (packet_dup->findTag<InterfaceInd>()) {
        auto interface_ind_tag = src_packet->addTag<InterfaceInd>();
        *interface_ind_tag = *packet_dup->getTag<InterfaceInd>();
    } else if (extraction_port_interface_id != -1)
        src_packet->addTagIfAbsent<InterfaceInd>()->setInterfaceId(extraction_port_interface_id);
    else
        throw cRuntimeError("Don't have any interface ind!!!");

    auto packet_protocol_tag = src_packet->addTag<PacketProtocolTag>();
    *packet_protocol_tag = *packet_dup->getTag<PacketProtocolTag>();

    delete packet_dup;

    EV << "src packet is " << src_packet->str() << endl;

    handleAndDispatchFrame(src_packet);
}

bool BouncingIeee8021dRelay::failure_check_and_reroute(Packet *packet, InterfaceEntry *ie, bool skip_notif) {

    if (random_fail_prob <= 0)
        return false;

    if (failure_or_recovery_notif_pkt == nullptr) {
        failure_or_recovery_notif_pkt = packet->dup();
        create_notif_header(failure_or_recovery_notif_pkt, "failure_or_recovery_notif");
    }

    auto frame = packet->peekAtFront<EthernetMacHeader>();
    if (drop_type == DROP_FAIL) {
        if (ie == nullptr || is_link_failed(ie->getIndex(), packet)) {
            EV << "Dropping packet due to failure" << endl;

            simtime_t earliest_recovery_time;    // just a big simtime
            bool recovery_time_loaded = false;

            if (ie == nullptr) {
                // find the minimum recovery time
                EV << "ie is null" << endl;
                auto potential_output_port_itr = mac_address_to_port_map.find(frame->getDest().str());
                if (potential_output_port_itr == mac_address_to_port_map.end()) {
                    std::cout << frame->getDest().str() << endl;
                    throw cRuntimeError("potential_output_port_itr == mac_address_to_port_map.end()");
                }
                for (auto potential_port : potential_output_port_itr->second) {
                    EV << "Looking for potential port " << potential_port << endl;
                    auto port_itr = port_to_recovery_time.find(potential_port);
                    if (port_itr == port_to_recovery_time.end())
                        continue;
                    if (recovery_time_loaded)
                        earliest_recovery_time = std::min(earliest_recovery_time, port_itr->second);
                    else {
                        recovery_time_loaded = true;
                        earliest_recovery_time = port_itr->second;
                    }
                }
            } else {
                EV << "Looking for port " << ie->getIndex() << endl;
                auto port_itr = port_to_recovery_time.find(ie->getIndex());
                if (port_itr == port_to_recovery_time.end())
                    throw cRuntimeError("2) No recovery time found!");
                recovery_time_loaded = true;
                earliest_recovery_time = port_itr->second;
            }

            if (!recovery_time_loaded) {
                EV << getFullPath() << ": Recovery time not found showing that this port is not directly failed just entries where removed as a part of another incident" << endl;
                earliest_recovery_time = 0;
            }

            EV << "earliest_recovery_time: " << earliest_recovery_time << endl;


            if (skip_notif) {
                EV << "Skipping drop notification" << endl;
                delete packet;
            } else {
                switch (recovery_type) {
                    case DROP_NOTIF:
                        send_drop_notif(packet, earliest_recovery_time);
                        break;
                    case DEFLECT_ON_DROP:
                        deflect_on_drop(packet);
                        break;
                    default:
                        throw cRuntimeError("No failure recovery method indicated!");
                }
                EV << "Notif pkt is: " << packet->str() << endl;
                b pkt_porition = packet->getFrontOffset();
                packet->setFrontIteratorPosition(b(0));
                auto m_phy_header =
                        packet->removeAtFront<EthernetPhyHeader>();
                auto m_eth_header =
                        packet->removeAtFront<EthernetMacHeader>();
                if (m_eth_header->getInput_ports_passedArraySize() == 0)
                    throw cRuntimeError("5) How is getInput_ports_passedArraySize() == 0");
                unsigned int output_port = m_eth_header->getInput_ports_passed(
                        m_eth_header->getInput_ports_passedArraySize() - 1);
                m_eth_header->eraseInput_ports_passed(
                        m_eth_header->getInput_ports_passedArraySize() - 1);
                packet->insertAtFront(m_eth_header);
                packet->insertAtFront(m_phy_header);
                packet->setFrontIteratorPosition(pkt_porition);
                EV << "Notif packet is being sent to port " << output_port << endl;
                ie = ifTable->getInterface(output_port);
                dispatch(packet, ie);
            }
            return true;
        }
    }
    return false;
}

void BouncingIeee8021dRelay::dispatch(Packet *packet, InterfaceEntry *ie)
{
    EV << "BouncingIeee8021dRelay::dispatch" << endl;

//    std::cout << simTime() << endl;

    // picky deflection
    bool is_packet_picked_for_deflection = true;
    if (apply_picky_deflection) {
        if (picky_deflection_type == BURSTY_ONLY_PICKY_DEF) {
            auto packet_dup = packet->dup();
            auto eth_header = packet->peekAtFront<EthernetMacHeader>();
            is_packet_picked_for_deflection = eth_header->getIs_bursty();
            delete packet_dup;
        }
    }

    if (ie != nullptr) {
        b position = packet->getFrontOffset();
        packet->setFrontIteratorPosition(b(0));
        auto phy_header_temp = packet->removeAtFront<EthernetPhyHeader>();
        auto mac_header_temp = packet->removeAtFront<EthernetMacHeader>();
        mac_header_temp->setOriginal_interface_id(ie->getInterfaceId());
        packet->insertAtFront(mac_header_temp);
        packet->insertAtFront(phy_header_temp);
        packet->setFrontIteratorPosition(position);
    }

    const auto& frame = packet->peekAtFront<EthernetMacHeader>();
    std::string switch_name = getParentModule()->getFullName();
    b packet_length = b(packet->getBitLength());

    std::string module_path_string;
    InterfaceEntry *ie2 = nullptr;

    if (frame->getIs_v2_dropped_packet_header())
        EV << "Packet is a dropped packet header. No need to bounce it." << endl;

    if (!frame->getIs_v2_dropped_packet_header() && !frame->getDest().isBroadcast()) {
        // If there is enough space on the chosen port simply forward it
        module_path_string = switch_name + ".eth[" + std::to_string(ie->getIndex()) + "].mac";
        EV << "The chosen port path is " << module_path_string << endl;
        AugmentedEtherMac *mac_temp = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
        std::string queue_full_path = module_path_string + ".queue";
        if (send_header_of_dropped_packet_to_receiver) {
            queue_full_path = module_path_string + ".queue.mainQueue";
        }



        if (can_deflect && bounce_probabilistically && is_packet_picked_for_deflection) {
            // Using PABO
            ie2 = find_interface_to_bounce_probabilistically(packet, ie);
        }
        else if (use_vertigo_prio_queue && bounce_randomly_v2) {
            // for early deflection we drop the packet in relay unit so we
            // check can deflect inside this if to support incremental deployment
            bolt_evaluate_if_src_packet_should_be_generated(packet, ie, ignore_cc_thresh_for_deflected_packets);
            if (mac_temp->is_queue_full(packet_length, queue_full_path) ||
                    (mac_temp->is_queue_over_v2_threshold(packet_length, queue_full_path, packet) &&
                    mac_temp->is_packet_tag_larger_than_last_packet(queue_full_path, packet))) {
//                std::cout << simTime() << ": Applying early deflection" << endl;
                // in early deflection we should drop non-bursty packets if we are only
                // deflecting bursty packets
                if (bolt_is_packet_src(packet)) {
                    delete packet;
                } else if (is_packet_picked_for_deflection && can_deflect) {
                    // try to deflect the packet
                    if (dctcp_mark_deflected_packets_only)
                        dctcp_mark_ecn_for_deflected_packets(packet);
                    mark_packet_deflection_tag(packet);
                    apply_early_deflection(packet, false, ie);
                } else {
                    // drop the packet
                    // this part is added for incremental deployment
                    light_in_relay_packet_drop_counter++;
                    delete packet;
                }
                return;
            } else {
                ie2 = ie;
            }
        } else if (can_deflect && mac_temp->is_queue_full(packet_length, queue_full_path)) {
            if (bounce_randomly && is_packet_picked_for_deflection) {
                if (bolt_is_packet_src(packet)) {
                    // do not deflect SRC packets
                    ie2 = ie;
                } else {
                    // Using DIBS --> Randomly bouncing to a not full switch port
                    // Bolt: ignoring cc_thresh for deflected packets.
                    bolt_evaluate_if_src_packet_should_be_generated(packet, ie, ignore_cc_thresh_for_deflected_packets);
                    if (dctcp_mark_deflected_packets_only)
                        dctcp_mark_ecn_for_deflected_packets(packet);
                    mark_packet_deflection_tag(packet);
                    ie2 = find_interface_to_bounce_randomly(packet);
                }
            } else if (bounce_naively && is_packet_picked_for_deflection) {
                if (bolt_is_packet_src(packet)) {
                    // do not deflect SRC packets
                    ie2 = ie;
                } else {
                    // Bolt: ignoring cc_thresh for deflected packets.
                    bolt_evaluate_if_src_packet_should_be_generated(packet, ie, ignore_cc_thresh_for_deflected_packets);
                    if (dctcp_mark_deflected_packets_only)
                        dctcp_mark_ecn_for_deflected_packets(packet);
                    mark_packet_deflection_tag(packet);
                    ie2 = find_interface_to_bounce_naively();
                }
            } else if (bounce_on_same_path && is_packet_picked_for_deflection){
                // Using main version of Vertigo
                ie2 = find_interface_to_bounce_on_the_same_path(packet, ie);
            } else if (bounce_randomly_v2 && is_packet_picked_for_deflection) {
                if (bolt_is_packet_src(packet)) {
                    // do not deflect SRC packets
                    ie2 = ie;
                } else {
                    // Bounce the packet to source using power of n choices.
                    // Not considering ports towards servers
                    EV << "Frames src is " << frame->getSrc() << " and frame's dst is " << frame->getDest() << endl;
                    if (src_enabled_packet_type == BOLT_VERTIGO_ORG_PKT ||
                            src_enabled_packet_type == BOLT_VERTIGO_ORG_DEF_PKT) {
                        // Bolt: ignoring cc_thresh for deflected packets.
                        bolt_evaluate_if_src_packet_should_be_generated(packet, ie, ignore_cc_thresh_for_deflected_packets);
                    }
                    find_interface_to_bounce_randomly_v2(packet, false, ie);
                    return;
                }
            } else {
                ie2 = nullptr;
                EV << "No bouncing method chosen! Normally drop the packet!" << endl;
            }
            if (ie2 == nullptr && (use_vertigo_prio_queue || use_v2_pifo || use_pfabric)) {
                // if this is true, we handle the drop in the mac layer and not the relay unit
                ie2 = ie;
            }
            if (ie2 == nullptr) {
//                emit(feedBackPacketDroppedSignal, int(frame->getIs_bursty()));
//                emit(feedBackPacketDroppedPortSignal, ie->getIndex());
                light_in_relay_packet_drop_counter++;
                delete packet;
                return;
            }
        } else {
            // This is when you normally sending the packet to the output queue
            // without any deflection or something...
            // here we implement the src process for bolt
            // Bolt: For normally forwarding packets we should not ignore CC_thresh
            // if you don't want SRC to be generated for normal packets, just set the CC thresh high
            // so that it's never hit
            if (use_pifo_bolt_reaction) {
                bolt_pifo_evaluate_if_src_packet_should_be_generated(packet, ie, false);
            } else {
                // original bolt reaction
                bolt_evaluate_if_src_packet_should_be_generated(packet, ie, false);
            }
            ie2 = ie;
        }
    } else {
        ie2 = ie;
    }

    if (ie2->getInterfaceId() != ie->getInterfaceId()) {
        EV << "The output interface has changed as a result of bouncing." << endl;
        emit(feedBackPacketGeneratedSignal, packet->getId());
    }

    module_path_string = switch_name + ".eth[" + std::to_string(ie2->getIndex()) + "].mac";
    AugmentedEtherMac *mac = check_and_cast<AugmentedEtherMac *>(getModuleByPath(module_path_string.c_str()));
    auto mac_header = packet->peekAtFront<EthernetMacHeader>();
    mac->add_on_the_way_packet(packet_length, mac_header->getIs_v2_dropped_packet_header());

//    std::cout << getFullPath() << ", adding packet length is: " << packet_length << endl;

    EV << "2) Sending frame " << packet << " on output interface " << ie2->getFullName() << " with destination = " << frame->getDest() << endl;

    numDispatchedNonBPDUFrames++;
    auto oldPacketProtocolTag = packet->removeTag<PacketProtocolTag>();
    int oldPacketInputPortIdx = packet->getTag<InputPortTag>()->getInput_port_idx();
    packet->clearTags();
    auto newPacketProtocolTag = packet->addTag<PacketProtocolTag>();
    *newPacketProtocolTag = *oldPacketProtocolTag;
    delete oldPacketProtocolTag;
    packet->addTag<InterfaceReq>()->setInterfaceId(ie2->getInterfaceId());
    packet->addTag<InputPortTag>()->setInput_port_idx(oldPacketInputPortIdx);
    packet->trim();
    emit(packetSentToLowerSignal, packet);

    send(packet, "ifOut");
}

void BouncingIeee8021dRelay::learn(MacAddress srcAddr, int arrivalInterfaceId)
{
    Ieee8021dInterfaceData *port = getPortInterfaceData(arrivalInterfaceId);

    EV << "SEPEHR: Is learning." << endl;

    if (!isStpAware || port->isLearning())
        macTable->updateTableWithAddress(arrivalInterfaceId, srcAddr);
}

void BouncingIeee8021dRelay::sendUp(Packet *packet)
{
    EV_INFO << "Sending frame " << packet << " to the upper layer" << endl;
    send(packet, "upperLayerOut");
}

Ieee8021dInterfaceData *BouncingIeee8021dRelay::getPortInterfaceData(unsigned int interfaceId)
{
    if (isStpAware) {
        InterfaceEntry *gateIfEntry = ifTable->getInterfaceById(interfaceId);
        Ieee8021dInterfaceData *portData = gateIfEntry ? gateIfEntry->getProtocolData<Ieee8021dInterfaceData>() : nullptr;

        if (!portData)
            throw cRuntimeError("Ieee8021dInterfaceData not found for port = %d", interfaceId);

        return portData;
    }
    return nullptr;
}

void BouncingIeee8021dRelay::start()
{
    ie = chooseInterface();
    if (ie) {
        bridgeAddress = ie->getMacAddress(); // get the bridge's MAC address
        registerAddress(bridgeAddress); // register bridge's MAC address
    }
    else
        throw cRuntimeError("No non-loopback interface found!");
}

void BouncingIeee8021dRelay::stop()
{
    ie = nullptr;
}

InterfaceEntry *BouncingIeee8021dRelay::chooseInterface()
{
    // TODO: Currently, we assume that the first non-loopback interface is an Ethernet interface
    //       since relays work on EtherSwitches.
    //       NOTE that, we don't check if the returning interface is an Ethernet interface!
    for (int i = 0; i < ifTable->getNumInterfaces(); i++) {
        InterfaceEntry *current = ifTable->getInterface(i);
        if (!current->isLoopback())
            return current;
    }

    return nullptr;
}

void BouncingIeee8021dRelay::finish()
{
    recordScalar("number of received BPDUs from STP module", numReceivedBPDUsFromSTP);
    recordScalar("number of received frames from network (including BPDUs)", numReceivedNetworkFrames);
    recordScalar("number of dropped frames (including BPDUs)", numDroppedFrames);
    recordScalar("number of delivered BPDUs to the STP module", numDeliveredBDPUsToSTP);
    recordScalar("number of dispatched BPDU frames to the network", numDispatchedBDPUFrames);
    recordScalar("number of dispatched non-BDPU frames to the network", numDispatchedNonBPDUFrames);
}


