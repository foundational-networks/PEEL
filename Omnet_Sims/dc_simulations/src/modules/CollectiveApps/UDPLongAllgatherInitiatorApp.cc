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
#include "./UDPLongAllgatherInitiatorApp.h"
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
#include <sstream>
#include <random>
#include <chrono>


#define INIT_PORT 80
#define MAX_PORT_NUM 65450      // 65535 - 85

using namespace inet;

Define_Module(UDPLongAllgatherInitiatorApp);


simsignal_t UDPLongAllgatherInitiatorApp::flowStartedQueryIDSignal = registerSignal("flowStartedQueryID");
simsignal_t UDPLongAllgatherInitiatorApp::controllerDelaySignal = registerSignal("controllerDelay");
simsignal_t UDPLongAllgatherInitiatorApp::flowStartedCollectiveTypeSignal = registerSignal("flowStartedCollectiveType");
simsignal_t UDPLongAllgatherInitiatorApp::flowStartedCollectiveAlgSignal = registerSignal("flowStartedCollectiveAlg");
simsignal_t UDPLongAllgatherInitiatorApp::requestSentSignal = registerSignal("requestSent");
simsignal_t UDPLongAllgatherInitiatorApp::flowNumSignal = registerSignal("flowNum");
// reply length is same as flow size
simsignal_t UDPLongAllgatherInitiatorApp::replyLengthsSignal = registerSignal("replyLengths");

static std::string generateZeroBitStream(int length) {
    return std::string(length, '0');
}

UDPLongAllgatherInitiatorApp::~UDPLongAllgatherInitiatorApp()
{
    cancelAndDelete(selfMsg);
    for (auto itr = flowid_to_msginfo_mapper.begin(); itr != flowid_to_msginfo_mapper.end(); itr++) {
//        std::cout << getFullPath() << ": Initiator still has dangling flows..." <<
//                "flow_id: " << itr->second->flow_id <<
//                ", query_id: " << itr->second->flow_id <<
//                ", remaining bytes: " << itr->second->remaining_bytes << endl;
        delete itr->second;
    }
    flowid_to_msginfo_mapper.clear();
    query_gpu_map_to_lists_of_list_of_jumps_to_idxs.clear();
    query_gpu_map_to_lists_of_list_of_ports_to_dsts.clear();
    flow_id_map_to_lists_of_list_of_jumps_to_idxs.clear();
    flow_id_map_to_lists_of_list_of_ports_to_dsts.clear();
}

static std::list<std::list<unsigned int>> parseStringToListOfLists(const std::string& str) {
    std::list<std::list<unsigned int>> result;

    // Remove the outer brackets
    std::string cleaned_str = str.substr(1, str.size() - 2);

    // Remove spaces
    cleaned_str.erase(std::remove(cleaned_str.begin(), cleaned_str.end(), ' '), cleaned_str.end());

    std::stringstream ss(cleaned_str);
    std::string sublist;

    // Iterate over sublists
    while (std::getline(ss, sublist, ']')) {
        size_t start = sublist.find('[');
        if (start != std::string::npos) {
            std::string sublist_content = sublist.substr(start + 1); // Extract contents between brackets
            std::stringstream sublist_ss(sublist_content);
            std::string item;
            std::list<unsigned int> inner_list;

            // Parse individual integers in the sublist
            while (std::getline(sublist_ss, item, ',')) {
                inner_list.push_back(static_cast<unsigned int>(std::stoul(item)));
            }

            result.push_back(inner_list); // Add parsed sublist to the result
        }
    }

    return result;
}

static int callback_inter_arrival(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->inter_arrival_times.push_back(std::stod(argv[i]));
      }
   }

   return 0;
}

static int callback_flow_size(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->flow_sizes.push_back(std::stoul(argv[i]));
      }
   }
   return 0;
}

static int callback_dst_server_num(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->dst_server_nums.push_back(std::stoul(argv[i]));
      }
   }
   return 0;
}

static int callback_dst_server_idx(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->dst_server_idx.push_back(std::stoi(argv[i]));
      }
   }
   return 0;
}

static int callback_dst_gpu_idx(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->dst_gpu_idx.push_back(std::stoi(argv[i]));
      }
   }
   return 0;
}

static int callback_flow_ids(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->flow_ids.push_back(std::stoul(argv[i]));
      }
   }
   return 0;
}

static int callback_collective_types(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->fill_out_query_type_info(argv[i]);
      }
   }
   return 0;
}

static int callback_query_ids(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->query_ids.push_back(std::stoul(argv[i]));
      }
   }
   return 0;
}

static int callback_ports_to_dest(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->ports_to_dst_list_of_lists.push_back(parseStringToListOfLists(argv[i]));
      }
   }
   return 0;
}

static int callback_jump_to_idx(void *data, int argc, char **argv, char **azColName){
   int i;
   UDPLongAllgatherInitiatorApp *app_object = (UDPLongAllgatherInitiatorApp*) data;

   for(i = 0; i<argc; i++){
      if (std::strcmp(argv[i], "") != 0) {
          app_object->jump_to_idx_list_of_lists.push_back(parseStringToListOfLists(argv[i]));
      }
   }
   return 0;
}

void UDPLongAllgatherInitiatorApp::fill_out_query_type_info(std::string query_type_info_str) {

    char delimiter_c = '-';
    int count = 0;
    for (char c : query_type_info_str) {
        if (c == delimiter_c) {
            count++;
        }
    }
    if (count != 1)
        throw cRuntimeError("There must necessarily be only one - in the query_type_info_str");

    std::string delimiter = "-";
    size_t pos = query_type_info_str.find(delimiter);
    std::string collective_str = query_type_info_str.substr(0, pos);
    std::string collective_alg_str = query_type_info_str.substr(pos + delimiter.length());
    int m_collective_type = collective_str_to_type(collective_str);
    int m_collective_alg_type = collective_alg_str_to_type(collective_alg_str);

    EV << "query_type_info_str: " << query_type_info_str << endl;
    EV << "collective: " << collective_str << ", collective_alg: " << collective_alg_str << endl;

    if (m_collective_type < 0 || m_collective_alg_type < 0)
        throw cRuntimeError("At this point both the collective and the algorithm should be known!");
    if (m_collective_alg_type == BCAST_NETWORK_ASSISTED)
        throw cRuntimeError("in combinational, no collective alg should be network assisted by default");

    if (using_ace) {
        // here we should decide on the algorithm for ace
        if (!ace_apply_src_reduce)
            throw cRuntimeError("Why is ace's apply_src_reduce off in combinational case!");

        switch (m_collective_type) {
            case BROADCAST:
            case ALL_REDUCE:
            case ALL_GATHER:
                if (partial_mcast_use_cidr_agg)
                    m_collective_alg_type = BCAST_PARTIAL_MULTICAST;
                else
                    m_collective_alg_type = BCAST_NETWORK_ASSISTED;
                break;
            case ALL_TO_ALL_V:
            case REDUCE:
            case REDUCE_SCATTER:
                // do not change the type
                break;
            default:
                std::cout << m_collective_type << endl;
                throw cRuntimeError("Unexpected collective type!");
        }
    }

    collective_types.push_back(m_collective_type);
    collective_algs.push_back(m_collective_alg_type);
}


int UDPLongAllgatherInitiatorApp::collective_alg_str_to_type(std::string collective_alg_type_str) {
    if (collective_alg_type_str.compare("singlesender") == 0) {
        return BCAST_SINGLE_SENDER;
    } else if (collective_alg_type_str.compare("binomialtree") == 0) {
        return BCAST_BINOMIAL_TREE;
    } else if (collective_alg_type_str.compare("binarytree") == 0) {
        return BCAST_BINARY_TREE;
    } else if (collective_alg_type_str.compare("ring") == 0) {
        return BCAST_RING;
    } else if (collective_alg_type_str.compare("optireduce") == 0) {
        return BCAST_OPTIREDUCE;
    } else if (collective_alg_type_str.compare("network_assisted") == 0) {
        return BCAST_NETWORK_ASSISTED;
    } else if (collective_alg_type_str.compare("partial_mcast") == 0) {
        return BCAST_PARTIAL_MULTICAST;
    } else if (collective_alg_type_str.compare("ina") == 0) {
        return BCAST_INA;
    } else if (collective_alg_type_str.compare("rdmc") == 0) {
        return BCAST_RDMC;
    }
    return -1;
}


int UDPLongAllgatherInitiatorApp::collective_str_to_type(std::string collective_type_str) {
    if (collective_type_str.compare("broadcast") == 0) {
        return BROADCAST;
    } else if (collective_type_str.compare("alltoallv") == 0) {
        return ALL_TO_ALL_V;
    } else if (collective_type_str.compare("allreduce") == 0) {
        return ALL_REDUCE;
    } else if (collective_type_str.compare("reduce") == 0) {
        return REDUCE;
    } else if (collective_type_str.compare("allgather") == 0) {
        return ALL_GATHER;
    } else if (collective_type_str.compare("reducescatter") == 0) {
        return REDUCE_SCATTER;
    }
    return -1;
}

void UDPLongAllgatherInitiatorApp::read_value_from_db(std::string inter_arrival_db_path,
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
        std::string jump_to_idx_table_name)
{
    std::string column_name = "server" + std::to_string(parent_index) + "app" + std::to_string(app_index);

    sqlite3* DB;
    char *zErrMsg = 0;
    int rc;
    std::string sql;
    int exit = 0;

    exit = sqlite3_open(inter_arrival_db_path.c_str(), &DB);
    if (exit) {
        throw cRuntimeError(sqlite3_errmsg(DB));
        return;
    }
    sql = "SELECT " + column_name + " from " + inter_arrival_table_name;
    rc = sqlite3_exec(DB, sql.c_str(), callback_inter_arrival, (void*)this, &zErrMsg);
    if( rc != SQLITE_OK ) {
      throw cRuntimeError("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(DB);


    if (collective_type <= 0 || collective_type == ALL_TO_ALL_V) {
        exit = sqlite3_open(flow_size_db_path.c_str(), &DB);
        if (exit) {
            throw cRuntimeError(sqlite3_errmsg(DB));
            return;
        }
        sql = "SELECT " + column_name + " from " + flow_size_table_name;
        rc = sqlite3_exec(DB, sql.c_str(), callback_flow_size, (void*)this, &zErrMsg);
        if( rc != SQLITE_OK ) {
          throw cRuntimeError("SQL error: %s\n", zErrMsg);
        }
        sqlite3_close(DB);
    }
//
//
//    exit = sqlite3_open(dst_server_num_db_path.c_str(), &DB);
//    if (exit) {
//        throw cRuntimeError(sqlite3_errmsg(DB));
//        return;
//    }
//    sql = "SELECT " + column_name + " from " + dst_server_num_table_name;
//    rc = sqlite3_exec(DB, sql.c_str(), callback_dst_server_num, (void*)this, &zErrMsg);
//    if( rc != SQLITE_OK ) {
//      throw cRuntimeError("SQL error: %s\n", zErrMsg);
//    }
//    sqlite3_close(DB);


    exit = sqlite3_open(dst_server_idx_db_path.c_str(), &DB);
    if (exit) {
        throw cRuntimeError(sqlite3_errmsg(DB));
        return;
    }
    sql = "SELECT " + column_name + " from " + dst_server_idx_table_name;
    rc = sqlite3_exec(DB, sql.c_str(), callback_dst_server_idx, (void*)this, &zErrMsg);
    if( rc != SQLITE_OK ) {
      throw cRuntimeError("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(DB);


    exit = sqlite3_open(dst_gpu_idx_db_path.c_str(), &DB);
    if (exit) {
        throw cRuntimeError(sqlite3_errmsg(DB));
        return;
    }
    sql = "SELECT " + column_name + " from " + dst_gpu_idx_table_name;
    rc = sqlite3_exec(DB, sql.c_str(), callback_dst_gpu_idx, (void*)this, &zErrMsg);
    if( rc != SQLITE_OK ) {
      throw cRuntimeError("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(DB);


    exit = sqlite3_open(flow_ids_db_path.c_str(), &DB);
    if (exit) {
        throw cRuntimeError(sqlite3_errmsg(DB));
        return;
    }
    sql = "SELECT " + column_name + " from " + flow_ids_table_name;
    rc = sqlite3_exec(DB, sql.c_str(), callback_flow_ids, (void*)this, &zErrMsg);
    if( rc != SQLITE_OK ) {
      throw cRuntimeError("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(DB);


    exit = sqlite3_open(query_ids_db_path.c_str(), &DB);
    if (exit) {
        throw cRuntimeError(sqlite3_errmsg(DB));
        return;
    }
    sql = "SELECT " + column_name + " from " + query_ids_table_name;
    rc = sqlite3_exec(DB, sql.c_str(), callback_query_ids, (void*)this, &zErrMsg);
    if( rc != SQLITE_OK ) {
      throw cRuntimeError("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(DB);

    if (collective_type <= 0) {
        // combinational queries
        exit = sqlite3_open(query_types_db_path.c_str(), &DB);
        if (exit) {
            throw cRuntimeError(sqlite3_errmsg(DB));
            return;
        }
        sql = "SELECT " + column_name + " from " + query_types_table_name;
        rc = sqlite3_exec(DB, sql.c_str(), callback_collective_types, (void*)this, &zErrMsg);
        if( rc != SQLITE_OK ) {
          throw cRuntimeError("SQL error: %s\n", zErrMsg);
        }
        sqlite3_close(DB);
    }

    if (collective_alg_type <= 0 ||
            collective_alg_type == BCAST_NETWORK_ASSISTED ||
            (collective_alg_type == BCAST_OPTIREDUCE && use_becca_optireduce)) {

        exit = sqlite3_open(ports_to_dest_db_path.c_str(), &DB);
        if (exit) {
            throw cRuntimeError(sqlite3_errmsg(DB));
            return;
        }
        sql = "SELECT " + column_name + " from " + ports_to_dest_table_name;
        rc = sqlite3_exec(DB, sql.c_str(), callback_ports_to_dest, (void*)this, &zErrMsg);
        if( rc != SQLITE_OK ) {
          throw cRuntimeError("SQL error: %s\n", zErrMsg);
        }
        sqlite3_close(DB);

        exit = sqlite3_open(jump_to_idx_db_path.c_str(), &DB);
        if (exit) {
            throw cRuntimeError(sqlite3_errmsg(DB));
            return;
        }
        sql = "SELECT " + column_name + " from " + jump_to_idx_table_name;
        rc = sqlite3_exec(DB, sql.c_str(), callback_jump_to_idx, (void*)this, &zErrMsg);
        if( rc != SQLITE_OK ) {
          throw cRuntimeError("SQL error: %s\n", zErrMsg);
        }
        sqlite3_close(DB);

    }

}

int UDPLongAllgatherInitiatorApp::get_dst_server_num_per_query(int m_collective_alg) {
    // In all gather, the participant itself is part of the num_flows_per_ml_query
    // for binary tree we include the sender server in the reference as well
    if (m_collective_alg == BCAST_BINARY_TREE ||
            m_collective_alg == BCAST_RING ||
            m_collective_alg == BCAST_SINGLE_SENDER ||
            m_collective_alg == BCAST_INA ||
            m_collective_alg == BCAST_OPTIREDUCE ||
            m_collective_alg == BCAST_RDMC)
        return num_flows_per_ml_query;
    return num_flows_per_ml_query - num_gpus;
}

int UDPLongAllgatherInitiatorApp::get_dst_server_idx() {
    int server_idx;
    if (dst_server_idx.empty()) {
        throw cRuntimeError("Trying to read from dst_server_idx but it's empty!");
    }
    server_idx = dst_server_idx.front();
    dst_server_idx.pop_front();

    return server_idx;
}

int UDPLongAllgatherInitiatorApp::get_dst_gpu_idx() {
    int gpu_idx;
    if (dst_gpu_idx.empty()) {
        throw cRuntimeError("Trying to read from dst_gpu_idx but it's empty!");
    }
    gpu_idx = dst_gpu_idx.front();
    dst_gpu_idx.pop_front();

    if (gpu_idx < 0 || gpu_idx >= num_gpus)
        throw cRuntimeError("gpu_idx < 0 || gpu_idx >= num_gpus");

    return gpu_idx;
}

simtime_t UDPLongAllgatherInitiatorApp::get_inter_arrival_time() {

//    if (parent_index != 14 || ignore >= num_gpus)
//        return 100000;
//    ignore++;

    // Because every app sends background flows independently, the inter_arrival_times for background
    // flows is relative to the time previous flow was sent --> Small values, so we add now to it
    // For incsat flows, because the inter_arrival times are between different apps, the values are actually
    // the time that the flow should be sent --> large values, so we return it independently
    if (inter_arrival_times.empty()) {
        throw cRuntimeError("You're trying to read from inter_arrival_times while it's empty");
    }
    simtime_t inter_arrival_time = inter_arrival_times.front();
    simtime_t now = simTime();
    inter_arrival_times.pop_front();
    // For ML the inter-arrival times presents the start time not the gap
    if (now > inter_arrival_time)
        throw cRuntimeError("UDPLongAllgatherInitiatorApp: now > inter_arrival_time");
    EV << "inter_arrival_time is " << inter_arrival_time << endl;
    return inter_arrival_time;
}

std::list<std::list<unsigned int>> UDPLongAllgatherInitiatorApp::get_ports_to_dst_list() {
    if (ports_to_dst_list_of_lists.empty())
        throw cRuntimeError("Trying to read from ports_to_dst_list_of_lists when it's empty!");
    auto output = ports_to_dst_list_of_lists.front();
    ports_to_dst_list_of_lists.pop_front();
    return output;
}

std::list<std::list<unsigned int>> UDPLongAllgatherInitiatorApp::get_jump_to_idx_list() {
    if (jump_to_idx_list_of_lists.empty())
        throw cRuntimeError("Trying to read from jump_to_idx_list_of_lists when it's empty!");
    auto output = jump_to_idx_list_of_lists.front();
    jump_to_idx_list_of_lists.pop_front();
    return output;
}

int UDPLongAllgatherInitiatorApp::get_collective_type() {
    if (collective_type > 0)
        return collective_type;
    if (collective_types.size() == 0)
        throw cRuntimeError("collective types list is empty!");
    int m_collective_type = collective_types.front();
    collective_types.pop_front();
    if (m_collective_type <= 0)
        throw cRuntimeError("Invalid collective type");
    return m_collective_type;
}

int UDPLongAllgatherInitiatorApp::get_collective_alg() {
    if (collective_alg_type > 0)
        return collective_alg_type;
    if (collective_algs.size() == 0)
        throw cRuntimeError("collective algs list is empty!");
    int m_collective_alg = collective_algs.front();
    collective_algs.pop_front();
    if (m_collective_alg <= 0)
        throw cRuntimeError("Invalid collective algorithm");
    return m_collective_alg;

}

int UDPLongAllgatherInitiatorApp::get_flow_size(int m_collective_type, int m_collective_alg) {

    if (m_collective_type <= 0 || m_collective_alg <= 0)
        throw cRuntimeError("Invalid collective type or alg!");

    int fsize;
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
        fsize = flow_size / num_flows_per_ml_query;
    } else if (m_collective_type == REDUCE_SCATTER) {
        // every chunk is sent to one person
        fsize = flow_size / num_flows_per_ml_query;
    } else if (m_collective_type == ALL_TO_ALL_V){
        if (flow_sizes.size() == 0)
            throw cRuntimeError("How is flow sizes empty!!!");
        fsize = flow_sizes.front();
        flow_sizes.pop_front();
    } else {
        fsize = flow_size;
    }
//    std::cout << getFullPath() << ": fsize is " << fsize << endl;

    return fsize;
}

unsigned long UDPLongAllgatherInitiatorApp::get_flow_id() {
    if (flow_ids.empty()) {
        throw cRuntimeError("You're trying to read from flow_ids while it's empty");
    }
    unsigned long flow_id = flow_ids.front();
    flow_ids.pop_front();
//    EV << "Remaining flow id size" << flow_ids.size() << endl;
    return flow_id;
}

unsigned long UDPLongAllgatherInitiatorApp::get_query_id() {
    if (query_ids.empty()) {
        throw cRuntimeError("You're trying to read from bursty_query_ids while it's empty. How is that possible?");
    }
    unsigned long query_id = query_ids.front();
    query_ids.pop_front();
    return query_id;
}

void UDPLongAllgatherInitiatorApp::initialize_rsbf_info() {

    if (rsbf_overhead > 0)
        return;

    std::string rsbf_bytes_str = par("rsbf_bytes");
    std::stringstream ss(rsbf_bytes_str);
    std::string token;
    while (std::getline(ss, token, ','))
        rsbf_bytes.push_back(static_cast<unsigned int>(std::stoul(token)));

    for (auto it = rsbf_bytes.begin(); it != rsbf_bytes.end(); it++) {
        rsbf_overhead += (*it);
    }

    rsbf_hash_num = int(par("rsbf_hash_num"));
    if (rsbf_hash_num <= 0)
        throw cRuntimeError("rsbf_hash_num should be > 0");

    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(seed); // Seed the generator
    std::uniform_int_distribution<unsigned int> dist;
    for (int gpu_idx = 0; gpu_idx < num_gpus; gpu_idx++) {
        std::list<unsigned int> hash_seeds;
        for (int i = 0; i < rsbf_hash_num; i++) {
            hash_seeds.push_back(dist(gen));
        }
        hash_seeds_map.insert(std::make_pair(gpu_idx, hash_seeds));
    }
}

std::list<std::string> UDPLongAllgatherInitiatorApp::generate_rsbf_bitstreams(std::list<port_list> port_info, int src_gpu_idx) {
    EV << "UDPLongAllgatherInitiatorApp::generate_rsbf_bitstreams called!" << endl;

    if (port_info.size() == 0)
        throw cRuntimeError("Port info must not be empty!!");

    std::list<std::string> bitstream_list;

    std::string bitstreams[rsbf_bytes.size()];
    int idx = 0;
    for (auto it = rsbf_bytes.begin(); it != rsbf_bytes.end(); it++) {
        bitstreams[idx] = generateZeroBitStream(*it * 8);
        idx += 1;
    }

    auto hash_seeds_itr = hash_seeds_map.find(src_gpu_idx);
    if (hash_seeds_itr == hash_seeds_map.end())
        throw cRuntimeError("Hash seeds must exist for all GPUs");

    std::list<int> downstream_leafidx_list;

    if (use_fattree) {
        throw cRuntimeError("This section of code should be re-written after the shape of the fattree for collectives changed to 2 servers per edge!");
        if (fattree_k < 0)
            throw cRuntimeError("Invalid fattree K");
        // this is just for fat-tree without failure, we assume that tree is sorted based on forwarding ports
        // k^2/4 for servers src pod, K for tor switches in the src pod, one for the src agg
        int core_info_idx = int(fattree_k/2)*int(fattree_k/2) + fattree_k + 1;

        auto port_info_itr = port_info.begin();
        int port_info_idx = -1;

        for (auto port_info_itr = port_info.begin();
                port_info_itr != port_info.end();
                port_info_itr++) {
            port_info_idx++;

            if ((*port_info_itr).size() == 0)
                continue;
            // skip src server info
            if (port_info_idx == 0)
                continue;

            int bitstream_info_idx = -1;
            if (port_info_idx < core_info_idx)
                // level 1
                bitstream_info_idx = 0;
            else if (port_info_idx == core_info_idx)
                // core level, level 2
                bitstream_info_idx = 1;
            else
                // core level, level 3
                bitstream_info_idx = 2;
            if (bitstream_info_idx >= rsbf_bytes.size())
                throw cRuntimeError("Invalid bitstream_info_idx");

            for (auto& val : (*port_info_itr)) {
                std::string port_identification = std::to_string(port_info_idx) + "_" + std::to_string(val);
                for (auto& hash_seed : hash_seeds_itr->second) {
                    std::size_t m_hash = hashWithSeed(port_identification, hash_seed);
                    m_hash %= bitstreams[bitstream_info_idx].size();
                    bitstreams[bitstream_info_idx][m_hash] = '1';
                }
            }
        }

    } else {
        // this is just for leaf spine without failure

        auto port_info_itr = port_info.begin();
        int port_info_idx = -1;

        for (auto port_info_itr = port_info.begin();
                port_info_itr != port_info.end();
                port_info_itr++) {
            port_info_idx++;

            if ((*port_info_itr).size() == 0)
                continue;
            // skip src server info
            if (port_info_idx == 0)
                continue;

            int bitstream_info_idx = -1;
            switch (port_info_idx) {
                case 1:
                    // first leaf
                    bitstream_info_idx = 0;
                    break;
                case 2:
                    bitstream_info_idx = 1;
                    // spine
                    break;
                default:
                    bitstream_info_idx = 2;
            }

            if (bitstream_info_idx >= rsbf_bytes.size())
                throw cRuntimeError("Invalid bitstream_info_idx");

            for (auto& val : (*port_info_itr)) {
                std::string port_identification = std::to_string(port_info_idx) + "_" + std::to_string(val);
                for (auto& hash_seed : hash_seeds_itr->second) {
                    std::size_t m_hash = hashWithSeed(port_identification, hash_seed);
                    m_hash %= bitstreams[bitstream_info_idx].size();
                    bitstreams[bitstream_info_idx][m_hash] = '1';
                }
            }
        }
    }

    for (int i = 0; i < rsbf_bytes.size(); i++)
        bitstream_list.push_back(bitstreams[i]);
    return bitstream_list;
}

void UDPLongAllgatherInitiatorApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {

        app_index = getIndex();
        parent_index = getParentModule()->getIndex();
        selfMsg = new cMessage("sendTimer");
        mss = par("mss");
        packet_creation_batch_size = par("packet_creation_batch_size");

        m_network = getSystemModule();

        num_gpus = int(par("num_gpus"));

        using_ace = bool(par("using_ace"));

        nvlink_speed_gbps = int(par("nvlink_speed_gbps"));

        if (using_ace && parent_index == 0)
            std::cout << "Using ace in combinational case" << endl;

        std::string lb_granularity_str = par("lb_granularity_str");
        if (lb_granularity_str.compare("flow") == 0) {
            EV << "Performing LB per flow!" << endl;
            lb_granularity = LB_FLOW;
        } else if (lb_granularity_str.compare("packet") == 0) {
            EV << "Performing LB per packet!" << endl;
            lb_granularity = LB_PKT;
        } else
            throw cRuntimeError("Unknown LB for ace");

        lb_tree_num = int(par("lb_tree_num"));

        if (lb_granularity != LB_FLOW && lb_tree_num == 1)
            throw cRuntimeError("LB granularity and tree num mismatch!");

        if (lb_granularity == LB_FLOW)
            lb_tree_num = 1;

        std::string lb_tree_select_type_str = par("lb_tree_select_type_str");
        if (lb_tree_select_type_str.compare("random") == 0) {
            lb_tree_select_type = LB_Random;
        } else if (lb_tree_select_type_str.compare("roundrobin") == 0) {
            lb_tree_select_type = LB_RR;
        } else
            throw cRuntimeError("Unknown lb_tree_select_type_str");

        // generate a list of random gpu idx
        std::string seed_str = std::to_string(parent_index) + std::to_string(app_index);
        seed = std::atoi(seed_str.c_str());
        seed += repetition_num;
        srand(seed);

        num_servers_under_each_leaf = int(getAncestorPar("num_servers"));

        initiation_delay_mean = par("initiation_delay_mean");
        initiation_delay_stddev = par("initiation_delay_stddev");
        if (initiation_delay_mean > 0 && initiation_delay_stddev > 0) {
            // Create a random device and a generator
            std::mt19937 generator(seed);

            // Create a normal distribution with the given mean and standard deviation
            std::normal_distribution<double> distribution(initiation_delay_mean, initiation_delay_stddev);
            for (int i = 0; i < num_gpus; i++) {
                double delay = distribution(generator);
                initiation_delay_per_gpu.push_back(delay);
            }
        }

        failure_perc = int(par("failure_perc"));
        lookahead = int(par("lookahead"));
        if (failure_perc >= 0 && lookahead < 0)
            throw cRuntimeError("failure_perc >= 0 && lookahead < 0");
        if (failure_perc < 0 && lookahead >= 0)
            throw cRuntimeError("failure_perc < 0 && lookahead >= 0");

        frag_ratio = double(par("frag_ratio"));

        ace_apply_src_reduce = bool(par("ace_apply_src_reduce"));

        splitted_subflow_num = par("splitted_subflow_num");

        optireduce_incast_factor = int(par("optireduce_incast_factor"));
        use_becca_optireduce = bool(par("use_becca_optireduce"));

        if (optireduce_incast_factor <= 0) {
            optireduce_incast_factor = INT_MAX - 1;
            EV << "Changing incast factor " << optireduce_incast_factor << endl;
        }

        // consider orca overhead
        use_orca = (bool)par("use_orca");
        orca_controller_delay_mean = par("orca_controller_delay_mean");
        orca_controller_delay_stddev = par("orca_controller_delay_stddev");

        // use rsbf
        use_rsbf = bool(par("use_rsbf"));
        if (use_orca && use_rsbf)
            throw cRuntimeError("Orca and RSBF cannot be enabled together");

        /*
         * mss zero means do not fragment data in the application layer (This causes
         * problems with pipelined binomial tree because the ip layer waits for all
         * the fragments before passing them up)
         * */
        if (mss < 0)
            throw cRuntimeError("mss < 0");

        use_fattree = bool(par("use_fattree"));
        fattree_k = int(par("k"));

        num_spines = int(par("num_spines"));
        num_leafs = int(par("num_aggs"));

        use_hpn = bool(par("use_hpn"));

        if (use_fattree) {
            if (fattree_k <= 0)
                throw cRuntimeError("fattree_k <= 0");
        } else if (use_hpn) {
            throw cRuntimeError("Under construction...");
        } else {
            if (num_leafs < 0 || num_spines < 0)
                throw cRuntimeError("num_leafs < 0 || num_spines < 0");
        }

        initiate_tree = bool(par("initiate_tree"));
        tree_id_overhead_bytes = B(par("tree_id_overhead_bytes"));

        use_cepheus = bool(par("use_cepheus"));

        if (use_cepheus && !initiate_tree)
            throw cRuntimeError("How is cepheus one without tree initiations!");

        if (use_rsbf || initiate_tree)
            initialize_rsbf_info();

        partial_mcast_chunk_data = bool(par("partial_mcast_chunk_data"));
        partial_mcast_chunk_tors = bool(par("partial_mcast_chunk_tors"));
        partial_mcast_programmable_cores = bool(par("partial_mcast_programmable_cores"));
        partial_mcast_divide_tors_among_cores = bool(par("partial_mcast_divide_tors_among_cores"));
        partial_mcast_use_cidr_agg = bool(par("partial_mcast_use_cidr_agg"));
        partial_mcast_use_cidr_core = bool(par("partial_mcast_use_cidr_core"));

        if (partial_mcast_chunk_tors && (partial_mcast_use_cidr_agg || partial_mcast_use_cidr_core))
            throw cRuntimeError("partial_mcast_chunk_tors is not programmed to work with partial_mcast_use_cidr_agg");

        if (!use_fattree && (partial_mcast_programmable_cores || partial_mcast_use_cidr_core))
            throw cRuntimeError("partial_mcast_programmable_cores/partial_mcast_use_cidr_core just makes sense in fattree");

        if (partial_mcast_programmable_cores && partial_mcast_use_cidr_core)
            throw cRuntimeError("partial_mcast_programmable_cores and partial_mcast_use_cidr_core cannot be enabled together!");

        if (partial_mcast_chunk_data && partial_mcast_chunk_tors)
            throw cRuntimeError("You can either chunk data or tors!");

        if (partial_mcast_divide_tors_among_cores && !partial_mcast_programmable_cores)
            throw cRuntimeError("partial_mcast_divide_tors_among_cores && !partial_mcast_programmable_cores");

        if (partial_mcast_use_cidr_agg || partial_mcast_use_cidr_core) {
            set_cidr_max_bit_size();
            std::string cidr_type_str = par("cidr_type_str");
            if (cidr_type_str.compare("full_tree") == 0)
                cidr_type = CIDR_FULL_TREE;
            else if (cidr_type_str.compare("bw_gain") == 0)
                cidr_type = CIDR_BW_GAIN;
            else
                throw cRuntimeError("Unknown cidr type!");
        }

        random_fail_prob = double(par("random_fail_prob"));

        rdmc_num_blocks = int(par("rdmc_num_blocks"));
        rdmc_hierarchical = bool(par("rdmc_hierarchical"));
    }
}

double UDPLongAllgatherInitiatorApp::get_processing_delay(
        int collective_type,
        int collective_alg) {
    Enter_Method("get_processing_delay");

    if (collective_alg <= 0 || collective_type <= 0)
        throw cRuntimeError("collective_alg <= 0 || collective_type <= 0");

    // we only consider extra latency for reduce
    if (collective_type != REDUCE && collective_type != REDUCE_SCATTER &&
            collective_type != ALL_REDUCE)
        return 0;

    // broadcast phase of optireduce has no processing
    if (collective_alg != BCAST_NETWORK_ASSISTED &&
            collective_alg != BCAST_PARTIAL_MULTICAST)
        throw cRuntimeError("The entered algorithm has no processing delay on the initializer....");

    if (initiation_delay_mean <= 0 || initiation_delay_stddev <= 0)
        return 0;

    std::mt19937 generator(seed);
    std::normal_distribution<double> distribution(initiation_delay_mean, initiation_delay_stddev);
    double delay = distribution(generator);
    if (delay < 0)
        delay = 0;
    return delay;
}

// for CIDR agg

void UDPLongAllgatherInitiatorApp::set_cidr_max_bit_size() {
    int tor_per_pod = 0;
    if (use_fattree) {
        tor_per_pod = fattree_k / 2.0;

        int pod_num = fattree_k;
        core_cidr_max_bit_size = ceil(log2(pod_num));
    } else {
        tor_per_pod = num_leafs;
    }

    if (tor_per_pod <= 0)
        throw cRuntimeError("Invalid tor_per_pod");

    agg_cidr_max_bit_size = ceil(log2(tor_per_pod));
    EV << "agg_cidr_max_bit_size = " << agg_cidr_max_bit_size <<
           "core_cidr_max_bit_size = " << core_cidr_max_bit_size << endl;
}

void UDPLongAllgatherInitiatorApp::CIDR_trie_insert(TrieNode* root,
        unsigned long node_idx,
        int max_bit_size,
        int trie_type) {

    TrieNode* node = root;
    std::string id(max_bit_size, '*');

    unsigned long relative_node_idx;
    switch (trie_type) {
        case CIDR_AGG:
            // we should get tor idx relative to pod
            relative_node_idx = get_relative_tor_idx(node_idx);
            break;
        case CIDR_CORE:
            // pod's relative idx is equal to pod idx
            relative_node_idx = node_idx;
            break;
        default:
            throw cRuntimeError("Unknown trie type!");
    }

    if (relative_node_idx < 0 || relative_node_idx >= (1 << max_bit_size)) {
        std::cout << "Invalid relative_node_idx: " << relative_node_idx << endl;
        std::cout << "max_bit_size: " << max_bit_size << endl;
        throw cRuntimeError("Invalid relative_node_idx");
    }

    EV << "CIDR_trie_insert called! node_idx: " << node_idx <<
            ", relative_node_idx: " << relative_node_idx <<
            ", max_bit_size: " << max_bit_size <<
            ", trie type: " << trie_type << endl;

    for (int i = max_bit_size - 1; i >= 0; --i) {
        int bit = (relative_node_idx >> i) & 1;
        int id_idx = max_bit_size - 1 - i;
        id[id_idx] = '0' + bit;
        if (!node->child[bit]) {
            node->child[bit] = new TrieNode();
            node->child[bit]->id = id;
        }
        node = node->child[bit];
    }
    node->isLeaf = true;
    node->actual_node_idx = node_idx;
}

void UDPLongAllgatherInitiatorApp::CIDR_update_leaf_info(TrieNode* node) {
    EV << "CIDR_update_leaf_info called for node " << node->id << endl;
    for (int bit = 0; bit <= 1; ++bit) {
        if (node->child[bit]) {
            if (node->child[bit]->isLeaf) {
                node->leaf_ids.push_back(node->child[bit]->id);
                if (node->child[bit]->actual_node_idx < 0)
                    throw cRuntimeError("Invalid actual_node_idx for tree leaf node!");
                node->leaf_actual_node_indexes.push_back(node->child[bit]->actual_node_idx);
                node->height = std::max(node->child[bit]->height + 1, node->height);
            } else {
                CIDR_update_leaf_info(node->child[bit]);
                for (std::string leaf_id : node->child[bit]->leaf_ids) {
                    node->leaf_ids.push_back(leaf_id);
                }
                for (unsigned long actual_node_idx : node->child[bit]->leaf_actual_node_indexes) {
                    if (actual_node_idx < 0)
                        throw cRuntimeError("Invalid actual tor idx!");
                    node->leaf_actual_node_indexes.push_back(actual_node_idx);
                }
                node->height = std::max(node->child[bit]->height + 1, node->height);
            }
        }
    }
    EV << "Node " << node->id << ": height: " << node->height <<
            ", node->leaf_ids.size: " << node->leaf_ids.size() << endl;
}

void UDPLongAllgatherInitiatorApp::trie_CIRD_compress_full_tree(TrieNode* node,
            std::unordered_map<unsigned long, std::string>& node_idx_to_prefix_map){
    if (node->isLeaf) {
        // we didn't find full tree up until the leaf!
        EV << "Leaf reached!" << endl;
        EV << "Leaf id " << node->id << " refers to Tor/Pod " << node->actual_node_idx << endl;
        node_idx_to_prefix_map.insert(std::make_pair(node->actual_node_idx, node->id));
        return;
    }

    bool iterate_children = true;
    EV << "trie_CIRD_compress_full_tree called for node " << node->id << endl;
    if (node->child[0] && node->child[1]) {
        if (node->child[0]->height != node->child[1]->height)
            throw cRuntimeError("How is the height of children different!");

        EV << "id: " << node->id <<
                ", child[0] leafs: " << node->child[0]->leaf_ids.size() <<
                ", child[1] leafs: " << node->child[1]->leaf_ids.size() <<
                ", node height: " << node->height << endl;

        if (!node->child[0]->isLeaf &&
                !node->child[1]->isLeaf &&
                (node->leaf_ids.size() !=
                node->child[0]->leaf_ids.size() +
                node->child[1]->leaf_ids.size()))
            throw cRuntimeError("How are not leaf ids aggregated in the parent?");

        if (node->leaf_ids.size() ==
                std::pow(2, node->height)) {
            EV << "Sub tree is complete!" << endl;
            iterate_children = false;
            auto actual_node_idx_itr = node->leaf_actual_node_indexes.begin();
            for (std::string leaf_id : node->leaf_ids) {
                EV << "Leaf id " << leaf_id << " refers to Tor/Pod " << (*actual_node_idx_itr) << endl;
                node_idx_to_prefix_map.insert(std::make_pair((*actual_node_idx_itr), node->id));
                actual_node_idx_itr++;
            }
        }
    }

    if (iterate_children) {
        for (int bit = 0; bit <= 1; ++bit) {
            if (node->child[bit])
                trie_CIRD_compress_full_tree(node->child[bit],
                        node_idx_to_prefix_map);
        }
    }
}

void UDPLongAllgatherInitiatorApp::trie_CIRD_compress_bw_gain(TrieNode* node,
            std::unordered_map<unsigned long, std::string>& node_idx_to_prefix_map){


    EV << "trie_CIRD_compress_bw_gain called for node " << node->id << endl;

    if (node->isLeaf) {
        // we didn't group anything up until the leaf!
        EV << "Leaf reached!" << endl;
        EV << "Leaf id " << node->id << " refers to Tor/Pod " << node->actual_node_idx << endl;
        node_idx_to_prefix_map.insert(std::make_pair(node->actual_node_idx, node->id));
        return;
    }

    bool iterate_children = true;
    unsigned int expected_leafs = std::pow(2, node->height);
    unsigned int included_leafs = node->leaf_ids.size();

    if (included_leafs > expected_leafs) {
        std::cout << included_leafs << endl;
        std::cout << expected_leafs << endl;
        throw cRuntimeError("How is included_leafs > expected_leafs");
    }

    // sending one instead of everyone
    unsigned int bandwidth_gain = (included_leafs - 1);
    unsigned int bandwidth_loss = (expected_leafs - included_leafs);

    EV << "Bandwidth gain: " << bandwidth_gain << ", Bandwidth loss: " << bandwidth_loss << endl;
    if (bandwidth_gain > bandwidth_loss) {
        EV << "bandwidth_gain > bandwidth_loss!" << endl;
        iterate_children = false;
        auto actual_node_idx_itr = node->leaf_actual_node_indexes.begin();
        for (std::string leaf_id : node->leaf_ids) {
            EV << "Leaf id " << leaf_id << " refers to Tor/Pod " << (*actual_node_idx_itr) << endl;
            node_idx_to_prefix_map.insert(std::make_pair((*actual_node_idx_itr), node->id));
            actual_node_idx_itr++;
        }
    }

    if (iterate_children) {
        for (int bit = 0; bit <= 1; ++bit) {
            if (node->child[bit])
                trie_CIRD_compress_bw_gain(node->child[bit],
                        node_idx_to_prefix_map);
        }
    }
}


std::unordered_map<unsigned long, std::string>
    UDPLongAllgatherInitiatorApp::CIRD_minimal_prefix_cover(
        std::unordered_set<unsigned long> node_idx_list,
        int max_bit_size,
        int trie_type) {
    Enter_Method("CIRD_minimal_prefix_cover");

    TrieNode* root = new TrieNode();
    std::string root_id(max_bit_size, '*');
    root->id = root_id;

    for (unsigned long node_idx : node_idx_list) {
        CIDR_trie_insert(root, node_idx, max_bit_size, trie_type);
    }

    CIDR_update_leaf_info(root);

    std::unordered_map<unsigned long, std::string> node_idx_to_prefix_map;
    switch (cidr_type) {
        case CIDR_FULL_TREE:
            trie_CIRD_compress_full_tree(root,
                    node_idx_to_prefix_map);
            break;
        case CIDR_BW_GAIN:
            trie_CIRD_compress_bw_gain(root,
                    node_idx_to_prefix_map);
            break;
        default:
            throw cRuntimeError("CIDR type not known!!!");
    }

    EV << "Deleting root!" << endl;
    delete root;
    return node_idx_to_prefix_map;
}

double UDPLongAllgatherInitiatorApp::get_controller_delay(bool using_orca,
        bool is_onetime_generation) {
    if (!using_orca && !is_onetime_generation)
        return 0;

    if (orca_controller_delay_mean <= 0 ||
            orca_controller_delay_stddev <= 0)
        return 0;


    // Create a random seed using current time (for randomness)
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    // Create random engine with the seed
    std::default_random_engine generator(seed);
    // Define a normal distribution
    std::normal_distribution<double> distribution(orca_controller_delay_mean, orca_controller_delay_stddev);
    double delay = distribution(generator);

    double controller_delay = std::max(delay, 0.0);

    return controller_delay;

}

void UDPLongAllgatherInitiatorApp::add_initiation_delays_and_sort() {
    if (collective_type > 0 || collective_alg_type > 0) {
        for (auto itr = inter_arrival_times.begin();
                itr != inter_arrival_times.end(); itr++) {
            simtime_t delay = initiation_delay_per_gpu.front();
            initiation_delay_per_gpu.pop_front();
            initiation_delay_per_gpu.push_back(delay);
            if (delay > 0)
                (*itr) = (*itr) + delay;
        }
    }
    inter_arrival_times.sort();
}

void UDPLongAllgatherInitiatorApp::handleStartOperation(LifecycleOperation *operation)
{
    EV << "SEPEHR: handleStartOperation called." << endl;



    std::string distributions_base_root = par("distibutions_base_root");
    std::string application_category = par("application_category");

    std::string collective_alg_type_str = par("collective_alg_type_str");
    collective_alg_type = collective_alg_str_to_type(collective_alg_type_str);
    if (collective_alg_type < 0 and parent_index == 0)
        std::cout << "No algorithm dictated for the collectives" << endl;

    std::string collective_type_str = par("collective_type_str");
    collective_type = collective_str_to_type(collective_type_str);
    if (collective_type < 0 && parent_index == 0)
        std::cout << "No collective dictated. Will use combinational!" << endl;

    if ((collective_type > 0 && collective_alg_type <= 0) ||
            (collective_type <= 0 && collective_alg_type > 0))
        throw cRuntimeError("We should either set both collective and alg or none of them!");


    EV << "Getting passer app's path on the same server" << endl;
    std::string full_app_path = getFullPath();
    size_t pos = full_app_path.find("app[1]");
    if (pos == std::string::npos)
        throw cRuntimeError("app[1] must exist in app name");
    full_app_path[pos + 4] = '0';
    passer_app = check_and_cast<UDPLongBroadcastPasserApp*>(getModuleByPath(full_app_path.c_str()));

    flow_size_multiplier = par("flow_size_multiplier");
    inter_arrival_time_multiplier = par("inter_arrival_time_multiplier");
    num_flows_per_ml_query = par("num_flows_per_ml_query");
    flow_size = par("flow_size");
    repetition_num = atoi(getEnvir()->getConfigEx()->getVariable(CFGVAR_REPETITION));
//    repetition_num = 0;

    std::string inter_arrival_db_name;
    std::string flow_size_db_name;
    std::string dst_server_num_db_name;
    std::string dst_server_db_name;
    std::string dst_gpu_db_name;
    std::string flow_ids_db_name;
    std::string query_ids_db_name;
    std::string query_types_db_name;
    std::string ports_to_dest_db_name;
    std::string jump_to_idx_db_name;

    std::string flow_size_multiplier_str = std::to_string(flow_size_multiplier);
    std::string inter_multiplier_str = std::to_string(inter_arrival_time_multiplier);
    std::string rep_num_string = std::to_string(repetition_num);
    std::string num_flows_per_query_string = std::to_string(num_flows_per_ml_query);
    std::string flow_size_string = std::to_string(flow_size);
    std::string num_gpus_str = std::to_string(num_gpus);

    inter_arrival_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_inter_arrival_time_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    flow_size_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_flow_size_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    dst_server_num_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_dst_server_num_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    dst_server_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_dst_server_idx_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    dst_gpu_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_dst_gpu_idx_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    flow_ids_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_flow_ids_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    query_ids_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_query_ids_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    query_types_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_query_types_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    ports_to_dest_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_ports_to_dst_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";
    jump_to_idx_db_name = distributions_base_root + "distributions/" + application_category + "_allgather_jump_to_idx_" + flow_size_multiplier_str + "_flowmult_" + inter_multiplier_str + "_intermult_" + num_flows_per_query_string + "_flows_per_ml_query_" + flow_size_string + "_flow_size_" + num_gpus_str + "_gpus_";

    if ((collective_alg_type < 0 || collective_alg_type == BCAST_NETWORK_ASSISTED) &&
            failure_perc >= 0) {
        // for failure cases
        std::string fail_perc_str = std::to_string(failure_perc);
        std::string lookahead_str = std::to_string(lookahead);
        ports_to_dest_db_name += fail_perc_str + "_failperc_" + lookahead_str + "_lookahead_";
        jump_to_idx_db_name += fail_perc_str + "_failperc_" + lookahead_str + "_lookahead_";
    }
    if ((collective_alg_type < 0 || collective_alg_type == BCAST_NETWORK_ASSISTED) &&
            (lb_granularity != LB_FLOW && lb_tree_num != 1)) {
        // for performing LB
        std::string lb_tree_num_str = std::to_string(lb_tree_num);
        ports_to_dest_db_name += lb_tree_num_str + "_lb_";
        jump_to_idx_db_name += lb_tree_num_str + "_lb_";
    }

    if (frag_ratio > 0) {
        // for when we have fragmented load assignment
        std::string frag_ratio_str = std::to_string(frag_ratio);
        inter_arrival_db_name += frag_ratio_str + "_frag_";
        flow_size_db_name += frag_ratio_str + "_frag_";
        dst_server_num_db_name += frag_ratio_str + "_frag_";
        dst_server_db_name += frag_ratio_str + "_frag_";
        dst_gpu_db_name += frag_ratio_str + "_frag_";
        flow_ids_db_name += frag_ratio_str + "_frag_";
        query_ids_db_name += frag_ratio_str + "_frag_";
        query_types_db_name += frag_ratio_str + "_frag_";
        ports_to_dest_db_name += frag_ratio_str + "_frag_";
        jump_to_idx_db_name += frag_ratio_str + "_frag_";
    }

    inter_arrival_db_name += rep_num_string + ".db";
    flow_size_db_name += rep_num_string + ".db";
    dst_server_num_db_name += rep_num_string + ".db";
    dst_server_db_name += rep_num_string + ".db";
    dst_gpu_db_name += rep_num_string + ".db";
    flow_ids_db_name += rep_num_string + ".db";
    query_ids_db_name += rep_num_string + ".db";
    query_types_db_name += rep_num_string + ".db";
    ports_to_dest_db_name += rep_num_string + ".db";
    jump_to_idx_db_name += rep_num_string + ".db";

    read_value_from_db(inter_arrival_db_name, flow_size_db_name, dst_server_num_db_name,
                dst_server_db_name, dst_gpu_db_name, flow_ids_db_name,
                query_ids_db_name, query_types_db_name, ports_to_dest_db_name,
                jump_to_idx_db_name,
                "inter_arrival", "flow_size",
                "dst_server_num", "dst_server_idx", "dst_gpu_idx",
                "flow_ids", "query_ids", "query_types",
                "ports_to_dst", "jump_to_idx");
    if (dst_server_idx.size() % num_flows_per_ml_query != 0)
        throw cRuntimeError("dst_server_idx.size() % num_flows_per_ml_query must be 0");
    if (dst_server_idx.size() != dst_gpu_idx.size()) {
        std::cout << "dst_server_idx.size: " << dst_server_idx.size() << endl;
        std::cout << "dst_gpu_idx.size: " << dst_gpu_idx.size() << endl;
        throw cRuntimeError("Dst servers and dst gpus are not in the same size!");
    }

    // There is nothing to do in this app
    if (inter_arrival_times.empty())
        return;
    if (initiation_delay_mean > 0 && initiation_delay_stddev > 0)
        add_initiation_delays_and_sort();
    initiation_delay_per_gpu.clear();

    if (selfMsg == nullptr)
        throw cRuntimeError("selfMsg should not be nullptr!");

    selfMsg->setKind(SEND);
    scheduleAt(get_inter_arrival_time(), selfMsg);
}

void UDPLongAllgatherInitiatorApp::finish()
{
    ApplicationBase::finish();
}

unsigned int UDPLongAllgatherInitiatorApp::getLbTreeIdx(unsigned long query_id, int src_gpu_idx) {
    // this is for optireduce becca
    if (lb_tree_select_type == LB_Random) {
        // similar to normal random idx generator
        return getLbTreeIdx(0);
    } else if (lb_tree_select_type == LB_RR) {
        std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
        auto rr_tree_idx_found_itr = query_gpu_map_to_round_robin_idx.find(query_src_gpu_pair);
        if (rr_tree_idx_found_itr == query_gpu_map_to_round_robin_idx.end()) {
            query_gpu_map_to_round_robin_idx.insert(std::make_pair(query_src_gpu_pair, 0));
            rr_tree_idx_found_itr = query_gpu_map_to_round_robin_idx.find(query_src_gpu_pair);
        }
        int rr_index = rr_tree_idx_found_itr->second;
        rr_tree_idx_found_itr->second++;
//        std::cout << "Returning " << rr_index << endl;
        return rr_index;
    }
    throw cRuntimeError("Uknown lb_tree_select_type2");
}

unsigned int UDPLongAllgatherInitiatorApp::getLbTreeIdx(unsigned long flow_id) {
    if (lb_tree_select_type == LB_Random) {
        static std::random_device rd;  // Obtain a random number from hardware
        static std::mt19937 gen(seed); // Seed the generator
        static std::uniform_int_distribution<int> distrib(0, 1000000000); // Define range
        return distrib(gen);
    } else if (lb_tree_select_type == LB_RR) {
        auto rr_tree_idx_found_itr = flow_id_map_to_round_robin_idx.find(flow_id);
        if (rr_tree_idx_found_itr == flow_id_map_to_round_robin_idx.end()) {
            flow_id_map_to_round_robin_idx.insert(std::make_pair(flow_id, 0));
            rr_tree_idx_found_itr = flow_id_map_to_round_robin_idx.find(flow_id);
        }
        int rr_index = rr_tree_idx_found_itr->second;
        rr_tree_idx_found_itr->second++;
//        std::cout << "Returning " << rr_index << endl;
        return rr_index;
    }
    throw cRuntimeError("Uknown lb_tree_select_type");
}

int UDPLongAllgatherInitiatorApp::generate_next_batch(unsigned long flow_id, int gpu_idx) {
    Enter_Method("generate_next_batch");
    EV << "Generate next batch called for flow id: " << flow_id << " and gpu: " << gpu_idx << endl;
    auto msg_info_found_itr = flowid_to_msginfo_mapper.find(flow_id);
    if (msg_info_found_itr == flowid_to_msginfo_mapper.end()) {
        EV << "Initiator: Flow with flow id " << flow_id << " deleted!" << endl;
        return BATCH_TERMINATE;
    }
    if (msg_info_found_itr->second->remaining_bytes <= 0)
        throw cRuntimeError("The remaining msg info has 0 remaining bytes! How?");
    EV << "Generating next batch for flow id: " << flow_id << endl;

    if (packet_creation_batch_size <= 0)
        throw cRuntimeError("generate_next_batch must not be called for packet_creation_batch_size <= 0");

    if (msg_info_found_itr->second->flow_id != flow_id)
        throw cRuntimeError("How is the input flow id != msg_info_found_itr->second->flow_id");

    if (mss == 0)
        throw cRuntimeError("mss cannot be 0 when this function is called!");
    int remaining_packet_num = packet_creation_batch_size;
    // remaining_packet_num = -1 means that send all the packets right away
    while (msg_info_found_itr->second->remaining_bytes > 0 && remaining_packet_num != 0) {
        EV << "msg_info_found_itr->second->remaining_bytes is " << msg_info_found_itr->second->remaining_bytes << endl;

        unsigned long original_send_bytes = msg_info_found_itr->second->remaining_bytes;

        B m_chunk_length;
        if (msg_info_found_itr->second->remaining_bytes > mss) {
            m_chunk_length = B(mss);
            msg_info_found_itr->second->remaining_bytes -= mss;
        } else {
            m_chunk_length = B(msg_info_found_itr->second->remaining_bytes);
            msg_info_found_itr->second->remaining_bytes = 0;
        }

        auto seq_num_found_itr = flow_id_to_seq_num.find(msg_info_found_itr->second->flow_id);
        if (seq_num_found_itr == flow_id_to_seq_num.end()) {
            std::cout << "Flow id: " << msg_info_found_itr->second->flow_id << endl;
            throw cRuntimeError("sequence number not found!");
        }
        seq_num_found_itr->second += 1;

        if (msg_info_found_itr->second->src_gpu != gpu_idx && !msg_info_found_itr->second->in_src_sharding) {
            std::cout << "gpu_idx = " << gpu_idx << endl;
            std::cout << "msg_info_found_itr->second->src_gpu = " << msg_info_found_itr->second->src_gpu << endl;
            throw cRuntimeError("How is msg_info_found_itr->second->src_gpu != gpu_idx");
        }

        // in case of chunk data, we don't need to
        auto packet = create_packet("data",
                m_chunk_length,
                seq_num_found_itr->second,
                msg_info_found_itr->second,
                msg_info_found_itr->second->src_gpu,
                msg_info_found_itr->second->dst_gpu,
                -1,
                original_send_bytes);


        msg_info_found_itr->second->out_socket->sendTo(packet, msg_info_found_itr->second->destination, msg_info_found_itr->second->dest_port);
        remaining_packet_num--;
        if (remaining_packet_num == 0)
            EV << "More packets are remaining but batch limit has been reached 2" << endl;
    }

    if (initiate_tree && !use_cepheus && msg_info_found_itr->second->remaining_bytes <= 0) {
        // cepheus does not present any tree initiation mechanism
        EV << "Sending a tree termination packet" << endl;
        auto packet = create_packet("treeFin",
                        msg_info_found_itr->second->tree_init_overhead + tree_id_overhead_bytes,
                        0,
                        msg_info_found_itr->second,
                        msg_info_found_itr->second->src_gpu,
                        msg_info_found_itr->second->dst_gpu);
        msg_info_found_itr->second->out_socket->sendTo(packet, msg_info_found_itr->second->destination, msg_info_found_itr->second->dest_port);
    }

    if (msg_info_found_itr->second->remaining_bytes <= 0) {
        EV << "Initiator: Flow with flow id " << flow_id << " is completed!" << endl;
        msg_info_found_itr->second->network_assisted_flow_ids_list.clear();
        msg_info_found_itr->second->network_assisted_destination_idx_list.clear();
        flow_id_map_to_lists_of_list_of_ports_to_dsts.erase(flow_id);
        flow_id_map_to_lists_of_list_of_jumps_to_idxs.erase(flow_id);
        flow_id_map_to_round_robin_idx.erase(flow_id);
//        msg_info_found_itr->second->list_of_ports_to_dsts.clear();
//        msg_info_found_itr->second->list_of_jumps_to_idxs.clear();
        msg_info_found_itr->second->responsibility.clear();
        msg_info_found_itr->second->responsibility_gpu.clear();
        msg_info_found_itr->second->responsiblity_flow_ids.clear();

        unsigned long query_id = msg_info_found_itr->second->query_id;
        int collective_alg = msg_info_found_itr->second->m_collective_alg;
        int src_gpu_idx = msg_info_found_itr->second->src_gpu;
        delete msg_info_found_itr->second;
        flowid_to_msginfo_mapper.erase(msg_info_found_itr);
        flow_id_to_seq_num.erase(flow_id);
        if (collective_alg == BCAST_OPTIREDUCE) {
            optireduce_send_next(query_id, src_gpu_idx);
        }
        if (collective_alg == BCAST_RDMC) {
            rdmc_complete_block_send(query_id, parent_index, src_gpu_idx);
        }
        return BATCH_TERMINATE;
    }
    return BATCH_SEND;

}

int UDPLongAllgatherInitiatorApp::get_random_gpu_idx() {
    int gpu_idx = rand() % num_gpus;
    return gpu_idx;
}

simtime_t UDPLongAllgatherInitiatorApp::get_intra_host_pass_delay(unsigned long msg_size_bytes) {
//    Enter_Method("get_intra_host_pass_delay");
    EV << "Adding in-host pass delay" << endl;
    // add nvlink/nvswitch latency to pass time
    double nvlink_speed_bps = nvlink_speed_gbps * std::pow(10, 9);
    simtime_t nvlink_delay = (msg_size_bytes * 8.0) / (nvlink_speed_bps);
    // going to nvswitch and then going to the next GPU
    nvlink_delay *= 2.0;
    EV << "Nv delay is " << nvlink_delay << endl;
    return nvlink_delay;
}

void UDPLongAllgatherInitiatorApp::send_to_self(int src_gpu_idx, int dst_gpu_idx, AllgatherCustomCollectiveMsgInitiatorInfo* msg_info) {


    EV << "UDPLongAllgatherInitiatorApp::send_to_self Called." << endl;
    EV << "src gpu idx, dst gpu idx, flow id, flow size, query id, flow num" << endl;
    EV << src_gpu_idx << ", " << dst_gpu_idx << ", " << msg_info->flow_id << ", " <<
            msg_info->flow_size << ", " << msg_info->query_id << ", " <<
            msg_info->num_dst_servers_per_query << endl;

    EV << "collective type: " << msg_info->m_collective_type << ", algorithm: " << msg_info->m_collective_alg << endl;
    if(msg_info->m_collective_type <= 0 || msg_info->m_collective_alg <= 0)
        throw cRuntimeError("msg_info->m_collective_type <= 0 || msg_info->m_collective_alg <= 0 (2)");

    if (msg_info->m_collective_alg != BCAST_BINARY_TREE &&
            msg_info->m_collective_alg != BCAST_RING &&
            msg_info->m_collective_alg != BCAST_SINGLE_SENDER &&
            msg_info->m_collective_alg != BCAST_OPTIREDUCE &&
            msg_info->m_collective_alg != BCAST_RDMC)
        throw cRuntimeError("send to self not supported by this collective type!");

    unsigned long send_bytes = msg_info->flow_size;

    simtime_t now = simTime();

    unsigned long tmp_mss;
    if (mss == 0) {
        // send all data at once
        tmp_mss = send_bytes;
    } else
        tmp_mss = mss;
    msg_info->check_metrics();
    EV << "src gpu idx: " << src_gpu_idx << ", dst_gpu_idx: " << dst_gpu_idx << endl;
    EV << "Sending " << send_bytes << " B" << endl;

    // add send to self delay
    msg_info->extra_delay = get_intra_host_pass_delay(msg_info->flow_size).dbl();

    while (send_bytes > 0) {

        B m_chunk_length;
        if (send_bytes > tmp_mss) {
            m_chunk_length = B(tmp_mss);
            send_bytes -= tmp_mss;
        } else {
            m_chunk_length = B(send_bytes);
            send_bytes = 0;
        }

        auto seq_num_found_itr = flow_id_to_seq_num.find(msg_info->flow_id);
        if (seq_num_found_itr == flow_id_to_seq_num.end()) {
            flow_id_to_seq_num.insert(std::make_pair(msg_info->flow_id, 0));
            seq_num_found_itr = flow_id_to_seq_num.find(msg_info->flow_id);
        }
        seq_num_found_itr->second += 1;

        // this is never called with ACE
        auto packet = create_packet("data",
                m_chunk_length,
                seq_num_found_itr->second,
                msg_info,
                src_gpu_idx,
                dst_gpu_idx);

        passer_app->passed_from_initiator(packet);
    }

    EV << "sending data with " << msg_info->flow_size << " bytes to self" << endl;
    EV << "SEPEHR: sending data with flow ID: " << msg_info->flow_id << endl;

    for (auto it = msg_info->responsiblity_flow_ids.begin(); it != msg_info->responsiblity_flow_ids.end(); it++) {
        emit(requestSentSignal, (*it));
        emit(replyLengthsSignal, msg_info->flow_size);
        emit(flowStartedQueryIDSignal, msg_info->query_id);
        emit(controllerDelaySignal, get_controller_delay(msg_info->using_orca) + msg_info->extra_delay);
        emit(flowStartedCollectiveTypeSignal, msg_info->m_collective_type);
        emit(flowStartedCollectiveAlgSignal, msg_info->m_collective_alg);
        emit(flowNumSignal, msg_info->num_dst_servers_per_query);
    }

    EV << "Msg is deleted!" << endl;
    msg_info->responsibility.clear();
    msg_info->responsibility_gpu.clear();
    msg_info->responsiblity_flow_ids.clear();
    msg_info->network_assisted_flow_ids_list.clear();
    msg_info->network_assisted_destination_idx_list.clear();
//    msg_info->list_of_ports_to_dsts.clear();
//    msg_info->list_of_jumps_to_idxs.clear();
    flow_id_to_seq_num.erase(msg_info->flow_id);
    delete msg_info;
}

Packet* UDPLongAllgatherInitiatorApp::create_packet(std::string pkt_name,
        B pkt_size,
        unsigned long seq_num,
        AllgatherCustomCollectiveMsgInitiatorInfo* msg_info,
        int src_gpu_idx,
        int dst_gpu_idx,
        int init_tree_pkt_idx,
        unsigned long remaining_bytes) {

    EV << "Create_packet --> Name: " << pkt_name <<
            ", Pkt size: " << pkt_size <<
            ", Seq num: " << seq_num <<
            ", src gpu idx: " << src_gpu_idx <<
            ", dst gpu idx: " << dst_gpu_idx << endl;

    const auto& payload = makeShared<GenericAppMsg>();
    Packet *packet = new Packet(pkt_name.c_str());
//    packet->addTag<SocketInd>()->setSocketId(socket_id);
    unsigned long query_id = msg_info->query_id;
    payload->setQuery_id(query_id);
    payload->setChunkLength(pkt_size); // flow size is in bytes
    payload->setServerClose(false);
    simtime_t now = simTime();
    payload->addTag<CreationTimeTag>()->setCreationTime(now);
    payload->setRequesterID(msg_info->flow_id);
    payload->setRequested_time(now);
    payload->setTotal_flow_size(B(msg_info->flow_size));
    payload->setApp_name("UDPLongAllgatherInitiatorApp");
    payload->setApp_full_path(getFullPath().c_str());
    payload->setSrc_gpu_idx(src_gpu_idx);
    payload->setDst_gpu_idx(dst_gpu_idx);

    payload->setRing_end_server_idx(msg_info->ring_end_server_idx);
    payload->setRing_end_gpu_idx(msg_info->ring_end_gpu_idx);

    payload->setOptireduce_in_reduction_phase(msg_info->optireduce_in_reduction_phase);

    payload->setTree_search_type(msg_info->tree_search_type);
    payload->setTree_traversal_dir(msg_info->tree_traversal_dir);

    payload->setCollective_type(msg_info->m_collective_type);
    payload->setCollective_alg_type(msg_info->m_collective_alg);

    payload->setAgg_cidr_group_id(msg_info->agg_cidr_group_id.c_str());
    payload->setAgg_cidr_group_member_count(msg_info->agg_cidr_group_member_count);
    payload->setCore_cidr_group_id(msg_info->core_cidr_group_id.c_str());
    payload->setCore_cidr_group_member_count(msg_info->core_cidr_group_member_count);
    payload->setDst_tor_idx(msg_info->dst_tor_idx);

    payload->setSeq_num(seq_num);
    payload->setCollective_scale(msg_info->num_dst_servers_per_query);

    payload->setInit_tree_pkt_idx(init_tree_pkt_idx);


//        if (msg_info->m_collective_alg == BCAST_NETWORK_ASSISTED)
//            payload->setUse_host_assisted_routing(true);

    payload->setTor_group_idx(msg_info->tor_group_idx);
    payload->setNum_msgs_in_tor_group(msg_info->num_msgs_in_tor_group);

    // rdmc params
    payload->setRdmc_source_server_id(msg_info->rdmc_source_server_id);
    payload->setRdmc_source_gpu_id(msg_info->rdmc_source_gpu_id);
    payload->setRdmc_block_number(msg_info->rdmc_block_num);

    if (msg_info->responsibility.size() > 0) {
        for (auto it = msg_info->responsibility.begin();
                it != msg_info->responsibility.end(); it++)
            payload->insertResponsibility_list(*it);

        for (auto it = msg_info->responsibility_gpu.begin();
                it != msg_info->responsibility_gpu.end(); it++)
            payload->insertResponsibility_gpu_list(*it);

        for (auto it = msg_info->responsiblity_flow_ids.begin();
                it != msg_info->responsiblity_flow_ids.end(); it++) {
            payload->insertResponsibility_flow_id_list(*it);
        }
    }

    payload->setIn_src_sharding(msg_info->in_src_sharding);

    if (msg_info->in_src_sharding) {
        payload->setPartial_mcast_original_flow_size_bytes(
                        msg_info->partial_mcast_original_flow_size_bytes);
    }

    if (msg_info->m_collective_alg == BCAST_PARTIAL_MULTICAST) {
        if (pkt_name.compare("data") == 0) {
            if (msg_info->controller_finish_time > 0) {
                payload->setController_setup_finish_time(msg_info->controller_finish_time);
                EV <<  " -- controller_finish_time: " << msg_info->controller_finish_time << endl;
            }
        }
    }

    if (msg_info->m_collective_alg == BCAST_NETWORK_ASSISTED ||
            msg_info->m_collective_alg == BCAST_INA ||
            msg_info->m_collective_alg == BCAST_PARTIAL_MULTICAST) {

        for (auto itr = msg_info->network_assisted_destination_idx_list.begin();
                itr != msg_info->network_assisted_destination_idx_list.end(); itr++)
            payload->insertDst_idx_list(*itr);

        for (auto itr = msg_info->network_assisted_flow_ids_list.begin();
                itr != msg_info->network_assisted_flow_ids_list.end(); itr++)
            payload->insertFlow_ids_list(*itr);

    }

    if (msg_info->m_collective_alg == BCAST_NETWORK_ASSISTED) {

        payload->setUsing_orca(msg_info->using_orca);

        // read the appropriate list of jumps and ports for flow_id
        auto list_of_jumps_found_itr = flow_id_map_to_lists_of_list_of_jumps_to_idxs.find(
                msg_info->flow_id);
        if (list_of_jumps_found_itr == flow_id_map_to_lists_of_list_of_jumps_to_idxs.end())
            throw cRuntimeError("No such list of jumps exist in query_id_map_to_lists_of_list_of_jumps_to_idxs");
        auto list_of_ports_found_itr = flow_id_map_to_lists_of_list_of_ports_to_dsts.find(
                msg_info->flow_id);
        if (list_of_ports_found_itr == flow_id_map_to_lists_of_list_of_ports_to_dsts.end())
            throw cRuntimeError("No such list of ports exist in query_id_map_to_lists_of_list_of_ports_to_dsts");
        if (list_of_jumps_found_itr->second.size() != list_of_ports_found_itr->second.size())
            throw cRuntimeError("list_of_jumps_found_itr->second.size() != list_of_ports_found_itr->second.size()");

        unsigned int random_idx = getLbTreeIdx(msg_info->flow_id);
        random_idx %= list_of_jumps_found_itr->second.size();
//            std::cout << list_of_jumps_found_itr->second.size() << endl;
//            std::cout << random_idx << endl;
//            std::cout << "---------------------------" << endl;

        auto _list_of_jumps_itr = list_of_jumps_found_itr->second.begin();
        std::advance(_list_of_jumps_itr, random_idx);
        auto _list_of_ports_itr = list_of_ports_found_itr->second.begin();
        std::advance(_list_of_ports_itr, random_idx);
        if ((*_list_of_jumps_itr).size() != (*_list_of_ports_itr).size())
            throw cRuntimeError("(*_list_of_jumps_itr).size() != (*_list_of_ports_itr).size()");


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

        if (random_fail_prob > 0) {
            // store info for failure recovery
            payload->setCurrent_tree_idx(random_idx);
            for (const auto& twoD : list_of_ports_found_itr->second) {
                TwoDInnerList twoDItem;
                for (const auto& oneD : twoD) {
                    InnerList innerItem;
                    for (auto v : oneD)
                        innerItem.insertInnerArray(v); // assuming similar insert exists
                    twoDItem.insertInnerArray(innerItem);
                }
                payload->insertAll_ports_to_dst(twoDItem);
            }
            for (const auto& twoD : list_of_jumps_found_itr->second) {
                TwoDInnerList twoDItem;
                for (const auto& oneD : twoD) {
                    InnerList innerItem;
                    for (auto v : oneD)
                        innerItem.insertInnerArray(v); // assuming similar insert exists
                    twoDItem.insertInnerArray(innerItem);
                }
                payload->insertAll_jump_to_idx(twoDItem);
            }
        }

        if (use_rsbf) {
            std::list<std::string> bitstream_list =
                    generate_rsbf_bitstreams(*_list_of_ports_itr, src_gpu_idx);
            auto hash_seeds_itr = hash_seeds_map.find(src_gpu_idx);
            for (auto& hash_seed : hash_seeds_itr->second)
                payload->insertRsbf_seed_list(hash_seed);

            for (auto& s : bitstream_list)
                payload->insertRsbf_bitstream_list(s.c_str());
        }
    }
    packet->insertAtBack(payload);
    return packet;

}

void UDPLongAllgatherInitiatorApp::send_msg(int server_idx,
        int gpu_idx,
        AllgatherCustomCollectiveMsgInitiatorInfo* msg_info,
        int src_gpu_idx,
        bool perform_tree_initiation)
{

    EV << "UDPLongAllgatherInitiatorApp::send_msg Called." << endl;
    EV << "src gpu idx, dest idx, dst, gpu idx, flow id, flow size, query id, flow num" << endl;
    EV << src_gpu_idx << ", " << server_idx << ", " << gpu_idx << ", " << msg_info->flow_id << ", " <<
            msg_info->flow_size << ", " << msg_info->query_id << ", " <<
            msg_info->num_dst_servers_per_query << endl;

    if (perform_tree_initiation &&
            msg_info->m_collective_alg != BCAST_NETWORK_ASSISTED)
        throw cRuntimeError("Tree initiation enabled without BECCA");

    std::map<std::tuple<int,int>, UdpSocket*>::iterator socket_found;

    EV << "collective type: " << msg_info->m_collective_type << ", algorithm: " << msg_info->m_collective_alg << endl;
    if(msg_info->m_collective_type <= 0 || msg_info->m_collective_alg <= 0)
        throw cRuntimeError("msg_info->m_collective_type <= 0 || msg_info->m_collective_alg <= 0 (1)");


    socket_found = destination_to_socket_mapper.find(std::make_tuple(server_idx, gpu_idx));

    if (socket_found == destination_to_socket_mapper.end())
        throw cRuntimeError("We're sending the request but cannot find the socket!");

    unsigned long send_bytes = msg_info->flow_size;

    simtime_t now = simTime();
    int dest_port = 80;
    L3Address destination;
    destination = get_destination(server_idx, gpu_idx);

    unsigned long tmp_mss;
    if (mss == 0) {
        // send all data at once
        tmp_mss = send_bytes;
    } else
        tmp_mss = mss;
//    std::cout << "Sending " << msg_info->flow_id << endl;
    int remaining_packet_num = packet_creation_batch_size;
    // remaining_packet_num = -1 means that send all the packets right away
    msg_info->check_metrics();
    if (src_gpu_idx < 0)
        src_gpu_idx = get_random_gpu_idx();
    EV << "src gpu idx: " << src_gpu_idx << ", dst_gpu_idx: " << gpu_idx << endl;

    if (perform_tree_initiation) {
        EV << "Sending a tree initiation packet" << endl;
        if (msg_info->tree_init_overhead > B(0)) {
            int init_pkt_idx = 0;
            while (msg_info->tree_init_overhead > B(0)) {
                EV << "Sending init pkt " << init_pkt_idx << endl;
                B init_pkt_size = std::min(msg_info->tree_init_overhead, B(mss) - tree_id_overhead_bytes);
                auto packet = create_packet("treeInit",
                                init_pkt_size + tree_id_overhead_bytes,
                                0,
                                msg_info,
                                src_gpu_idx,
                                gpu_idx,
                                init_pkt_idx);
                socket_found->second->sendTo(packet, destination, dest_port);
                msg_info->tree_init_overhead -= init_pkt_size;
                init_pkt_idx += 1;
            }
        } else
            throw cRuntimeError("msg_info->tree_init_overhead must be > 0");
    }

    while (send_bytes > 0 && remaining_packet_num != 0) {

        unsigned long original_send_bytes = send_bytes;
        B m_chunk_length;
        if (send_bytes > tmp_mss) {
            m_chunk_length = B(tmp_mss);
            send_bytes -= tmp_mss;
        } else {
            m_chunk_length = B(send_bytes);
            send_bytes = 0;
        }

        auto seq_num_found_itr = flow_id_to_seq_num.find(msg_info->flow_id);
        if (seq_num_found_itr == flow_id_to_seq_num.end()) {
            flow_id_to_seq_num.insert(std::make_pair(msg_info->flow_id, 0));
            seq_num_found_itr = flow_id_to_seq_num.find(msg_info->flow_id);
        }
        seq_num_found_itr->second += 1;

        auto packet = create_packet("data",
                m_chunk_length,
                seq_num_found_itr->second,
                msg_info,
                src_gpu_idx,
                gpu_idx,
                -1,
                original_send_bytes);

        EV << "calling send to from send_msg" << endl;
        socket_found->second->sendTo(packet, destination, dest_port);
        remaining_packet_num--;
        if (remaining_packet_num == 0)
            EV << "More packets are remaining but batch limit has been reached" << endl;
    }

    if (perform_tree_initiation && !use_cepheus && send_bytes <= 0) {
        EV << "Sending a tree termination packet" << endl;
        auto packet = create_packet("treeFin",
                        tree_id_overhead_bytes,
                        0,
                        msg_info,
                        src_gpu_idx,
                        gpu_idx);
        socket_found->second->sendTo(packet, destination, dest_port);
    }

    EV << "sending data with " << msg_info->flow_size << " bytes" << endl;
    EV << "SEPEHR: sending data with flow ID: " << msg_info->flow_id << endl;
    if (msg_info->m_collective_alg == BCAST_NETWORK_ASSISTED ||
            msg_info->m_collective_alg == BCAST_PARTIAL_MULTICAST) {
        for (auto itr = msg_info->network_assisted_flow_ids_list.begin();
                itr != msg_info->network_assisted_flow_ids_list.end(); itr++) {
            emit(requestSentSignal, (*itr));
            emit(replyLengthsSignal, msg_info->flow_size);
            emit(flowStartedQueryIDSignal, msg_info->query_id);
            emit(controllerDelaySignal, get_controller_delay(msg_info->using_orca) + msg_info->extra_delay);
            emit(flowStartedCollectiveTypeSignal, msg_info->m_collective_type);
            emit(flowStartedCollectiveAlgSignal, msg_info->m_collective_alg);
            emit(flowNumSignal, msg_info->num_dst_servers_per_query);
        }
    } else {
        emit(requestSentSignal, msg_info->flow_id);
        emit(replyLengthsSignal, msg_info->flow_size);
        emit(flowStartedQueryIDSignal, msg_info->query_id);
        emit(controllerDelaySignal, get_controller_delay(msg_info->using_orca) + msg_info->extra_delay);
        emit(flowStartedCollectiveTypeSignal, msg_info->m_collective_type);
        emit(flowStartedCollectiveAlgSignal, msg_info->m_collective_alg);
        emit(flowNumSignal, msg_info->num_dst_servers_per_query);
    }


    if (msg_info->responsibility.size() > 0) {
        for (auto it = msg_info->responsiblity_flow_ids.begin(); it != msg_info->responsiblity_flow_ids.end(); it++) {
            emit(requestSentSignal, (*it));
            emit(replyLengthsSignal, msg_info->flow_size);
            emit(flowStartedQueryIDSignal, msg_info->query_id);
            emit(controllerDelaySignal, get_controller_delay(msg_info->using_orca) + msg_info->extra_delay);
            emit(flowStartedCollectiveTypeSignal, msg_info->m_collective_type);
            emit(flowStartedCollectiveAlgSignal, msg_info->m_collective_alg);
            emit(flowNumSignal, msg_info->num_dst_servers_per_query);
        }
    }

    if (send_bytes <= 0) {
        EV << "Msg info deleted!" << endl;
        msg_info->responsibility.clear();
        msg_info->responsibility_gpu.clear();
        msg_info->responsiblity_flow_ids.clear();
        msg_info->network_assisted_flow_ids_list.clear();
        msg_info->network_assisted_destination_idx_list.clear();
        msg_info->network_assisted_dst_gpu_idx_list.clear();
        flow_id_map_to_lists_of_list_of_ports_to_dsts.erase(msg_info->flow_id);
        flow_id_map_to_lists_of_list_of_jumps_to_idxs.erase(msg_info->flow_id);
        flow_id_map_to_round_robin_idx.erase(msg_info->flow_id);
        flow_id_to_seq_num.erase(msg_info->flow_id);
//        msg_info->list_of_ports_to_dsts.clear();
//        msg_info->list_of_jumps_to_idxs.clear();
        unsigned long query_id = msg_info->query_id;
        int collective_alg = msg_info->m_collective_alg;
        delete msg_info;
        if (collective_alg == BCAST_OPTIREDUCE) {
            optireduce_send_next(query_id, src_gpu_idx);
        }
        if (collective_alg == BCAST_RDMC) {
            rdmc_complete_block_send(query_id, parent_index, src_gpu_idx);
        }
    } else {
        msg_info->remaining_bytes = send_bytes;
        msg_info->initiated_time = now;
        msg_info->out_socket = socket_found->second;
        msg_info->dest_port = dest_port;
        msg_info->destination = destination;
        msg_info->src_gpu = src_gpu_idx;
        msg_info->dst_gpu = gpu_idx;
//        for (auto itr = network_assisted_flow_ids_list.begin();
//                itr != network_assisted_flow_ids_list.end(); itr++)
//            msg_info->network_assisted_flow_ids_list.push_back(*itr);
//
//        for (auto itr = network_assisted_destination_idx_list.begin();
//                itr != network_assisted_destination_idx_list.end(); itr++)
//            msg_info->network_assisted_destination_idx_list.push_back(*itr);
//
//        for (auto itr = ports_to_dst_list.begin();
//                itr != ports_to_dst_list.end(); itr++)
//            msg_info->list_of_ports_to_dsts.push_back(*itr);
//
//        for (auto itr = jump_to_idx_list.begin();
//                itr != jump_to_idx_list.end(); itr++)
//            msg_info->list_of_jumps_to_idxs.push_back(*itr);

        flowid_to_msginfo_mapper.insert(std::make_pair(msg_info->flow_id, msg_info));
    }
    EV << "--------------------------" << endl;
}

int UDPLongAllgatherInitiatorApp::get_local_port() {

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

void UDPLongAllgatherInitiatorApp::init_socket(int server_idx, int gpu_idx) {
    EV << "UDPLongAllgatherInitiatorApp::init_socket for " << server_idx <<
            " gpu " << gpu_idx << endl;

    if (server_idx < 0 || gpu_idx < 0)
        throw cRuntimeError("Invalid server or gpu idx!");

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
//
//        bool receiveBroadcast = par("receiveBroadcast");
//        if (receiveBroadcast)
//            socket->setBroadcast(true);
//
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

L3Address UDPLongAllgatherInitiatorApp::get_destination(int server_idx, int gpu_idx) {
    std::string connect_address_str = "server[" + std::to_string(server_idx) +
            "]%eth" + std::to_string(gpu_idx);
    const char* connect_address = connect_address_str.c_str();
    L3Address destination;
    L3AddressResolver().tryResolve(connect_address, destination);
    return destination;
}

// returns -1 if node is not leaf
int UDPLongAllgatherInitiatorApp::find_parent(int size, int search_type, int element_idx_in_list, bool just_leafs) {

    EV << "Finding parent for element_idx=" << element_idx_in_list <<
            ". Just for leafs? " << just_leafs <<endl;

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
//        std::cout << getFullPath() << " " << start << " " << mid
//                << " " << end << " " << search_type << endl;
        double mid_double = double(start + end) / 2.0;
//        std::cout << mid_double << endl;
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
//        std::cout << mid << endl;
//        std::cout << "------------------------" << endl;
        if (element_idx_in_list == mid) {
            if (!just_leafs)
                return previous_mid;
            break;
        }
        if (search_type == TREE_LOW && start == 0 && end == 1) {
            if (element_idx_in_list != 0)
                throw cRuntimeError("element_idx_in_list must be 0 in this scenario");
            break;
        }
        if (search_type == TREE_HIGH && start == size-2 && end == size-1) {
            if (element_idx_in_list != size - 1)
                throw cRuntimeError("element_idx_in_list must be 0 in this scenario");
            break;
        }

        previous_mid = mid;
        if (element_idx_in_list > mid)
            start = mid;
        else
            end = mid;
    }

    EV << "After the while loop, start is " << start <<
            ", mid is " << mid <<
            ", end is " << end <<
            ", and previous_mid is " << previous_mid << endl;

    // This is for just leafs
    if (end - start == 2)
        return previous_mid;
    // handle edge cases
    if (search_type == TREE_LOW) {
        if (element_idx_in_list == size - 1 && start == size - 2 && end == size - 1)
            return start;
    }
    if (search_type == TREE_HIGH)
        if (element_idx_in_list == 0 && start == 0 && end == 1)
            return end;
    return -1;
}

void UDPLongAllgatherInitiatorApp::snd_allgather_binary_tree() {
    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_binary_tree()" << endl;
    unsigned long query_id = get_query_id();
    int m_collective_type = get_collective_type();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_BINARY_TREE);
    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_BINARY_TREE);
    // / 2.0 as we send it through two trees
    unsigned long tmp_flow_size;
    switch (m_collective_type) {
        case BROADCAST:
        case ALL_GATHER:
            tmp_flow_size = flow_size;
            break;
        case ALL_REDUCE:
            tmp_flow_size = flow_size / 2.0;
            break;
        default:
            std::cout << "Collective is " << m_collective_type << endl;
            throw cRuntimeError("Collective not supported by binary tree!");
    }

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    // check if there are any repititive servers in a query
    int* dst_server_idx_list = new int[num_dst_servers_per_query];
    int* dst_gpu_idx_list = new int[num_dst_servers_per_query];
    unsigned long* flow_id_list = new unsigned long[num_dst_servers_per_query];
    for (int i = 0; i < num_dst_servers_per_query; i++){
        dst_server_idx_list[i] = get_dst_server_idx();
        dst_gpu_idx_list[i] = get_dst_gpu_idx();
        flow_id_list[i] = get_flow_id();
    }

    int tmp_source_gpu_idx = source_gpu_round_trip_idx;
    int element_idx_in_list = -1;
    for (int gpu_offset = 0; gpu_offset < num_gpus; gpu_offset++) {
        for (int k = 0; k < num_dst_servers_per_query; k++) {
            if (dst_server_idx_list[k] == parent_index &&
                    dst_gpu_idx_list[k] == tmp_source_gpu_idx) {
                element_idx_in_list = k;
                EV << "element_idx_in_list is " << k << endl;
                break;
            }
        }
        if (element_idx_in_list >= 0)
            break;
        tmp_source_gpu_idx++;
        tmp_source_gpu_idx %= num_gpus;
    }
    if (element_idx_in_list < 0)
        throw cRuntimeError("element was not found in list!");
    source_gpu_round_trip_idx = tmp_source_gpu_idx;

    int parent_low_index = find_parent(num_dst_servers_per_query, TREE_LOW, element_idx_in_list, true);
    int parent_high_index = find_parent(num_dst_servers_per_query, TREE_HIGH, element_idx_in_list, true);
    EV << "parent_low_index: " << parent_low_index << endl;
    EV << "parent_high_index: " << parent_high_index << endl;
    int tree_parent_server_idx, tree_parent_gpu_idx;

//    unsigned long m_flow_id = flow_id_list[element_idx_in_list];
    // use different flow_ids for differetn source gpus
    unsigned long m_flow_id = flow_id_list[source_gpu_round_trip_idx];
//    std::cout << getFullPath() << ": Flow id is " << m_flow_id << ", gpu_idx: " << source_gpu_round_trip_idx << ", flow_size = " << tmp_flow_size << endl;

    int tree_parent_index_list[2] = {parent_low_index, parent_high_index};

    for (int k = 0; k < 2; k++) {
        if (tree_parent_index_list[k] < 0) {
            EV << "Server " << parent_index << " gpu " << source_gpu_round_trip_idx << " is not a leaf in tree " << k << "!" << endl;
            continue;
        }
        EV << "Server " << parent_index << " gpu " << source_gpu_round_trip_idx << " is a leaf in tree " << k << "!" << endl;
        EV << "flow id is " << m_flow_id << endl;
        tree_parent_server_idx = dst_server_idx_list[tree_parent_index_list[k]];
        tree_parent_gpu_idx = dst_gpu_idx_list[tree_parent_index_list[k]];
        EV << "Tree parent server idx: " << tree_parent_server_idx << " and gpu idx: " << tree_parent_gpu_idx << endl;


        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(m_flow_id,
                tmp_flow_size, query_id, num_dst_servers_per_query);

        for (int t = 0; t < num_dst_servers_per_query; t++) {
            new_msg->responsibility.push_back(dst_server_idx_list[t]);
            new_msg->responsibility_gpu.push_back(dst_gpu_idx_list[t]);
            new_msg->responsiblity_flow_ids.push_back(flow_id_list[t]);
        }
        new_msg->dst_gpu = tree_parent_gpu_idx;

        if (k == 0)
            new_msg->tree_search_type = TREE_LOW;
        if (k == 1)
            new_msg->tree_search_type = TREE_HIGH;
        new_msg->tree_traversal_dir = TREE_UP;

        new_msg->m_collective_type = m_collective_type;
        new_msg->m_collective_alg = BCAST_BINARY_TREE;

        if (tree_parent_server_idx == parent_index) {
            EV << "The parent GPU is in the same server" << endl;
            send_to_self(source_gpu_round_trip_idx, tree_parent_gpu_idx, new_msg);
        } else {
            init_socket(tree_parent_server_idx, tree_parent_gpu_idx);
            send_msg(tree_parent_server_idx, tree_parent_gpu_idx, new_msg, source_gpu_round_trip_idx);
        }
    }

    source_gpu_round_trip_idx++;
    source_gpu_round_trip_idx %= num_gpus;

    delete[] dst_server_idx_list;
    dst_server_idx_list = nullptr;
    delete[] dst_gpu_idx_list;
    dst_gpu_idx_list = nullptr;
    delete[] flow_id_list;
    flow_id_list = nullptr;

}


void UDPLongAllgatherInitiatorApp::initiate_rdmc_connection(unsigned long qid, RDMCScheduleInfo* rdmc_schedule) {
    EV << "initiate_rdmc_connection" << endl;
    for (int i=1; i<rdmc_schedule->num_nodes; i++) {
        int node_server_id = rdmc_schedule->dst_servers_list[i];
        EV << "Getting Brdoacast App for server << " << node_server_id << endl;
        UDPLongBroadcastPasserApp* target_app = check_and_cast<UDPLongBroadcastPasserApp*>(m_network->getSubmodule("server", node_server_id)->getSubmodule("app", 0));
        target_app->initiate_rdmc_connection(qid, i, rdmc_schedule);
    }
}

void UDPLongAllgatherInitiatorApp::rdmc_receiver_ready_to_receive(unsigned long qid, int src_server_id, int src_gpu_id, int receiver_node_id) {
    Enter_Method("rdmc_receiver_ready_to_receive");
    EV << "rdmc_receiver_ready_to_receive - receiver: " << receiver_node_id << " for query: " << qid << endl;
    auto rdmc_schedule_it = query_node_map_to_rdmc_schedule.find(std::make_tuple(qid, src_server_id, src_gpu_id));
    if (rdmc_schedule_it == query_node_map_to_rdmc_schedule.end())
        throw cRuntimeError("RDMC context for query not found!");

    rdmc_schedule_it->second->ready_to_receive[receiver_node_id] = true;
    if (rdmc_schedule_it->second->send_schedule != nullptr && !rdmc_schedule_it->second->sending) snd_allgather_rdmc(qid, rdmc_schedule_it->second);
}


void UDPLongAllgatherInitiatorApp::rdmc_complete_block_send(unsigned long qid, int src_server_id, int src_gpu_id) {
    EV << "rdmc_complete_block_send for sender: " << src_gpu_id << endl;
    auto rdmc_schedule_it = query_node_map_to_rdmc_schedule.find(std::make_tuple(qid, src_server_id, src_gpu_id));
    if (rdmc_schedule_it == query_node_map_to_rdmc_schedule.end())
        throw cRuntimeError("RDMC context for query not found!");

    RDMCScheduleInfo* rdmc_schedule = rdmc_schedule_it->second;
//    std::cout << "node "<<rdmc_schedule->rdmc_node_id << " (step "<<rdmc_schedule->send_step<<") - finished sending block " << rdmc_schedule->send_schedule->target_block_num << " to node " << rdmc_schedule->send_schedule->target_node << endl;
    rdmc_schedule->sending = false;
    delete rdmc_schedule->send_schedule;
    rdmc_schedule->send_schedule = nullptr;

    while(rdmc_schedule->send_schedule == nullptr && rdmc_schedule->send_step < rdmc_schedule->get_total_steps()) {
        rdmc_schedule->send_step++;
        rdmc_schedule->load_outgoing_schedule();
    }
    
    if (rdmc_schedule->send_step == rdmc_schedule->get_total_steps()) {
        // we have reached the end of the pipeline, no more data to send
        EV << "Reached the end of binomial pipeline" << endl;
        // cleanup
        delete rdmc_schedule;
        query_node_map_to_rdmc_schedule.erase(rdmc_schedule_it);
        return;
    }

    snd_allgather_rdmc(qid, rdmc_schedule);
}


void UDPLongAllgatherInitiatorApp::snd_allgather_rdmc(unsigned long qid, RDMCScheduleInfo* rsche) {
    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_rdmc()" << endl;

    unsigned long query_id;
    RDMCScheduleInfo* rdmc_schedule;
    int src_gpu_id;

    if (rsche != nullptr) {
        rdmc_schedule = rsche;
        query_id = qid;
        src_gpu_id = rdmc_schedule->dst_gpus_list[rdmc_schedule->rdmc_node_id];
    }
    else {
        query_id = get_query_id();
        int m_collective_type = get_collective_type();
        unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_RDMC);
        unsigned long flow_size = get_flow_size(m_collective_type, BCAST_RDMC);

        if (num_dst_servers_per_query == 0)
            throw cRuntimeError("num_dst_servers_per_query == 0");

        // we only perform rdmc with power of 2 number of nodes
        if ((num_dst_servers_per_query & (num_dst_servers_per_query - 1)) != 0)
            throw cRuntimeError("num_dst_servers_per_query is not power of 2");

        // check if there are any repititive servers in a query
        int* dst_server_idx_list = new int[num_dst_servers_per_query];
        int* dst_gpu_idx_list = new int[num_dst_servers_per_query];
        unsigned long* flow_id_list = new unsigned long[num_dst_servers_per_query];
        std::unordered_set<int> unique_servers;
        for (int i = 0; i < num_dst_servers_per_query; i++){
            dst_server_idx_list[i] = get_dst_server_idx();
            dst_gpu_idx_list[i] = get_dst_gpu_idx();
            flow_id_list[i] = get_flow_id();
            unique_servers.insert(dst_server_idx_list[i]); // Sana: needed for hierarchical RDMC
        }

        int tmp_source_gpu_idx = source_gpu_round_trip_idx;
        int element_idx_in_list = -1;
        for (int gpu_offset = 0; gpu_offset < num_gpus; gpu_offset++) {
            for (int k = 0; k < num_dst_servers_per_query; k++) {
                if (dst_server_idx_list[k] == parent_index &&
                        dst_gpu_idx_list[k] == tmp_source_gpu_idx) {
                    element_idx_in_list = k;
                    EV << "element_idx_in_list is " << k << endl;
                    break;
                }
            }
            if (element_idx_in_list >= 0)
                break;
            tmp_source_gpu_idx++;
            tmp_source_gpu_idx %= num_gpus;
        }
        if (element_idx_in_list < 0)
            throw cRuntimeError("element was not found in list!");
        source_gpu_round_trip_idx = tmp_source_gpu_idx;
        src_gpu_id = source_gpu_round_trip_idx;

        size_t total_rdmc_nodes = num_dst_servers_per_query;
        if (rdmc_hierarchical) total_rdmc_nodes = unique_servers.size();

        // store everything in RDMC Context
        // source/sender node is always id 0
        rdmc_schedule = new RDMCScheduleInfo(total_rdmc_nodes, rdmc_num_blocks, flow_size, 0, m_collective_type);

        if (!rdmc_hierarchical) {
            // for rdmc, the nodes are id'd from 0 to n-1 with sender/source with id 0. So, we shift the lists accordingly
            for (int i = 0; i < num_dst_servers_per_query; i++) {
                rdmc_schedule->dst_servers_list[i] = dst_server_idx_list[element_idx_in_list];
                rdmc_schedule->dst_gpus_list[i] = dst_gpu_idx_list[element_idx_in_list];
                rdmc_schedule->flow_ids_list[i] = flow_id_list[element_idx_in_list];
                element_idx_in_list++;
                element_idx_in_list %= num_dst_servers_per_query;
            }
        }
        else {
            rdmc_schedule->hierarchical = true;
            int write_idx = 0;
            for (int i = 0; i < num_dst_servers_per_query; i++) {
                int curr_server_id = dst_server_idx_list[element_idx_in_list];
                if (rdmc_schedule->intra_server_gpus.find(curr_server_id) == rdmc_schedule->intra_server_gpus.end()) {
                    rdmc_schedule->dst_servers_list[write_idx] = curr_server_id;
                    rdmc_schedule->dst_gpus_list[write_idx] = dst_gpu_idx_list[element_idx_in_list];
                    rdmc_schedule->flow_ids_list[write_idx] = flow_id_list[element_idx_in_list];
                    write_idx++;
                }
                rdmc_schedule->intra_server_gpus[curr_server_id].push_back(dst_gpu_idx_list[element_idx_in_list]);
                element_idx_in_list++;
                element_idx_in_list %= num_dst_servers_per_query;
            }
        }

        // store the rdmc schedule to use in future.
        query_node_map_to_rdmc_schedule.insert(std::make_pair(std::make_tuple(query_id, parent_index, source_gpu_round_trip_idx), rdmc_schedule));

        // the source node initiates connections with all other nodes so they can load relevant state and schedule information
        initiate_rdmc_connection(query_id, rdmc_schedule);

        rdmc_schedule->load_outgoing_schedule();

        source_gpu_round_trip_idx++;
        source_gpu_round_trip_idx %= num_gpus;

        delete[] dst_server_idx_list;
        dst_server_idx_list = nullptr;
        delete[] dst_gpu_idx_list;
        dst_gpu_idx_list = nullptr;
        delete[] flow_id_list;
        flow_id_list = nullptr;
    }

    if (rdmc_schedule->send_schedule != nullptr && !rdmc_schedule->sending && rdmc_schedule->ready_to_receive[rdmc_schedule->send_schedule->target_node]) {
        rdmc_schedule->ready_to_receive[rdmc_schedule->send_schedule->target_node] = false;
        EV << "node 0 - RDMC send conditions met to send block " << rdmc_schedule->send_schedule->target_block_num << " to " << rdmc_schedule->send_schedule->target_node << endl;
        unsigned long m_flow_id = rdmc_schedule->flow_ids_list[0]; // source/sender node is always 0th
        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(m_flow_id,
                rdmc_schedule->block_flow_size, query_id, int(rdmc_schedule->num_nodes));

        for (int t = 0; t < int(rdmc_schedule->num_nodes); t++) {
            new_msg->responsibility.push_back(rdmc_schedule->dst_servers_list[t]);
            new_msg->responsibility_gpu.push_back(rdmc_schedule->dst_gpus_list[t]);
            new_msg->responsiblity_flow_ids.push_back(rdmc_schedule->flow_ids_list[t]);
        }

        int target_dst_server = rdmc_schedule->dst_servers_list[rdmc_schedule->send_schedule->target_node];
        int target_dst_gpu = rdmc_schedule->dst_gpus_list[rdmc_schedule->send_schedule->target_node];
        new_msg->dst_gpu = target_dst_gpu;

        new_msg->m_collective_type = rdmc_schedule->m_collective_type;
        new_msg->m_collective_alg = BCAST_RDMC;

        new_msg->rdmc_source_server_id = rdmc_schedule->dst_servers_list[0];
        new_msg->rdmc_source_gpu_id = rdmc_schedule->dst_gpus_list[0];
        new_msg->rdmc_block_num = rdmc_schedule->send_schedule->target_block_num;

        rdmc_schedule->sending = true;


        if (target_dst_server == parent_index) {
            EV << "The parent GPU is in the same server" << endl;
            send_to_self(src_gpu_id, target_dst_gpu, new_msg);

            // trigger block send completion here [for intra host this is the only place to do so]
            rdmc_complete_block_send(query_id, parent_index, src_gpu_id);
        } else {
            init_socket(target_dst_server, target_dst_gpu);
            send_msg(target_dst_server, target_dst_gpu, new_msg, src_gpu_id);

        }
    }
    else {
        // either nothing to send or currently sending a block or the receiver is not ready to receive, so we wait.
        // initiator never receives anything so we do not need to do anything for receive_schedule
        return;
    }
}


void UDPLongAllgatherInitiatorApp::snd_allgather_binomial_tree() {
    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_binomial_tree()" << endl;
    unsigned long query_id = get_query_id();
    int m_collective_type = get_collective_type();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_BINOMIAL_TREE);
    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_BINOMIAL_TREE);

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    // check if there are any repititive servers in a query
    int dst_server_idx_list[num_dst_servers_per_query];
    int dst_gpu_idx_list[num_dst_servers_per_query];
    unsigned long flow_id_list[num_dst_servers_per_query];
    std::unordered_map<unsigned long, int> flowid_dstidx_mapper;
    std::unordered_map<unsigned long, int> flowid_dstgpu_mapper;
    for (int i = 0; i < num_dst_servers_per_query; i++){
        while (true) {
            // in dst_server_idx there can be this server's index, so filter it out
            int tmp_dst_server_idx = get_dst_server_idx();
            int tmp_gpu_idx = get_dst_gpu_idx();
            if (tmp_dst_server_idx != parent_index) {
                dst_server_idx_list[i] = tmp_dst_server_idx;
                dst_gpu_idx_list[i] = tmp_gpu_idx;
                flow_id_list[i] = get_flow_id();
                break;
            }
        }
        if (dst_server_idx_list[i] == parent_index)
            throw cRuntimeError("A server cannot initialize to itself");
        flowid_dstidx_mapper.insert(std::make_pair(flow_id_list[i], dst_server_idx_list[i]));
        flowid_dstgpu_mapper.insert(std::make_pair(flow_id_list[i], dst_gpu_idx_list[i]));
    }

    unsigned int start = 0;
    unsigned int end = num_dst_servers_per_query;
    std::unordered_map<unsigned long, std::list<int>> flowid_responsibility_list_mapper;
    std::unordered_map<unsigned long, std::list<int>> flowid_responsibility_gpu_list_mapper;
    std::unordered_map<unsigned long, std::list<unsigned long>> flowid_responsibility_flowids_list_mapper;
    while (true) {
        unsigned int mid = int((end-start)/2);
        auto mid_found = flowid_responsibility_list_mapper.find(flow_id_list[mid]);
        if (mid_found != flowid_responsibility_list_mapper.end())
            throw cRuntimeError("Mid should not exist in dstidx_responsibility_list_mapper");
        std::list<int> temp_list;
        std::list<int> temp_gpu_list;
        std::list<unsigned long> temp_flowids_list;
        for (int i = mid+1; i < end; i++) {
            temp_list.push_back(dst_server_idx_list[i]);
            temp_gpu_list.push_back(dst_gpu_idx_list[i]);
            temp_flowids_list.push_back(flow_id_list[i]);
        }
        flowid_responsibility_list_mapper.insert(std::make_pair(flow_id_list[mid], temp_list));
        flowid_responsibility_gpu_list_mapper.insert(std::make_pair(flow_id_list[mid], temp_gpu_list));
        flowid_responsibility_flowids_list_mapper.insert(std::make_pair(flow_id_list[mid], temp_flowids_list));
        end = mid;
        if (end <= start)
            break;
    }

    if (flowid_responsibility_list_mapper.size() !=
            flowid_responsibility_flowids_list_mapper.size() ||
            flowid_responsibility_list_mapper.size() !=
                    flowid_responsibility_gpu_list_mapper.size())
        throw cRuntimeError("flowid_responsibility_list_mapper.size() != flowid_responsibility_flowids_list_mapper.size()");


    for (auto itr = flowid_responsibility_list_mapper.begin();
            itr != flowid_responsibility_list_mapper.end(); itr++){
        auto server_idx_found_itr = flowid_dstidx_mapper.find(itr->first);
        if (server_idx_found_itr == flowid_dstidx_mapper.end())
            throw cRuntimeError("server idx not found in flowid_dstidx_mapper");
        int server_idx = server_idx_found_itr->second;

        auto gpu_idx_found_itr = flowid_dstgpu_mapper.find(itr->first);
        if (gpu_idx_found_itr == flowid_dstgpu_mapper.end())
            throw cRuntimeError("gpu idx not found in flowid_dstgpu_mapper");
        int gpu_idx = gpu_idx_found_itr->second;

        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(itr->first, flow_size, query_id,
                num_dst_servers_per_query);
        new_msg->responsibility = itr->second;

        auto gpu_it_found = flowid_responsibility_gpu_list_mapper.find(itr->first);
        if (gpu_it_found == flowid_responsibility_gpu_list_mapper.end())
            throw cRuntimeError("LongBroadcastInitiatorApp::gpu_it_found == flowid_responsibility_gpu_list_mapper.end()");
        new_msg->responsibility_gpu = gpu_it_found->second;

        auto flowid_it_found = flowid_responsibility_flowids_list_mapper.find(itr->first);
        if (flowid_it_found == flowid_responsibility_flowids_list_mapper.end())
            throw cRuntimeError("LongAllgatherInitiatorApp::flowid_it_found == dstidx_responsibility_flowids_list_mapper.end()");
        new_msg->responsiblity_flow_ids = flowid_it_found->second;

        new_msg->m_collective_type = m_collective_type;
        new_msg->m_collective_alg = BCAST_BINOMIAL_TREE;

        init_socket(server_idx, gpu_idx);

        send_msg(server_idx, gpu_idx, new_msg, source_gpu_round_trip_idx);
    }

    source_gpu_round_trip_idx++;
    source_gpu_round_trip_idx %= num_gpus;
}

std::list<std::list<std::list<unsigned int>>> UDPLongAllgatherInitiatorApp::optireduce_get_port_list(unsigned long query_id, int src_gpu_idx) {
    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
    auto port_list_found_itr = query_gpu_map_to_lists_of_list_of_ports_to_dsts.find(query_src_gpu_pair);
    if (port_list_found_itr == query_gpu_map_to_lists_of_list_of_ports_to_dsts.end())
        throw cRuntimeError("Port list must exist here!");
    return port_list_found_itr->second;
}

void UDPLongAllgatherInitiatorApp::optireduce_remove_port_list(unsigned long query_id, int src_gpu_idx) {
    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
    query_gpu_map_to_lists_of_list_of_ports_to_dsts.erase(query_src_gpu_pair);
    query_gpu_map_to_round_robin_idx.erase(query_src_gpu_pair);
}

std::list<std::list<std::list<unsigned int>>> UDPLongAllgatherInitiatorApp::optireduce_get_jump_list(unsigned long query_id, int src_gpu_idx) {
    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
    auto jump_list_found_itr = query_gpu_map_to_lists_of_list_of_jumps_to_idxs.find(query_src_gpu_pair);
    if (jump_list_found_itr == query_gpu_map_to_lists_of_list_of_jumps_to_idxs.end())
        throw cRuntimeError("Jump list must exist here!");
    return jump_list_found_itr->second;
}

void UDPLongAllgatherInitiatorApp::optireduce_remove_jump_list(unsigned long query_id, int src_gpu_idx) {
    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
    query_gpu_map_to_lists_of_list_of_jumps_to_idxs.erase(query_src_gpu_pair);
    query_gpu_map_to_round_robin_idx.erase(query_src_gpu_pair);
}

void UDPLongAllgatherInitiatorApp::optireduce_send_next(unsigned long query_id, int src_gpu_idx) {
    EV << "UDPLongAllgatherInitiatorApp::optireduce_send_next for query: " << query_id
            << " and source_gpu_idx: " << src_gpu_idx << endl;

    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, src_gpu_idx);
    auto msg_record_found_itr = optireduce_query_gpu_msg_record.find(query_src_gpu_pair);
    if (msg_record_found_itr == optireduce_query_gpu_msg_record.end())
        return;

    int max_snd_num = optireduce_incast_factor;
    while (max_snd_num > 0 && msg_record_found_itr->second.size() > 0) {
        EV << "Sending next message! msg_record_found_itr->second.size(): " <<
                msg_record_found_itr->second.size() << ", max_snd_num: " << max_snd_num <<
                " .......... " << endl;
        auto m_msg = msg_record_found_itr->second.front();
        msg_record_found_itr->second.pop_front();
        if (m_msg->optireduce_dst_server < 0 || m_msg->optireduce_dst_gpu < 0)
            throw cRuntimeError("Invalid optireduce_dst_server or optireduce_dst_gpu");

        if (m_msg->optireduce_dst_server == parent_index) {
            EV << "The destination GPU is in the same server" << endl;
            send_to_self(src_gpu_idx, m_msg->optireduce_dst_gpu, m_msg);
        } else {
            EV << "Sending to someone outside of the host!" << endl;
            init_socket(m_msg->optireduce_dst_server, m_msg->optireduce_dst_gpu);
            send_msg(m_msg->optireduce_dst_server, m_msg->optireduce_dst_gpu, m_msg, src_gpu_idx);
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

void UDPLongAllgatherInitiatorApp::snd_allgather_optireduce() {

    if (initiation_delay_mean != 0)
        throw cRuntimeError("No initiation delay for optireduce!");

    if (optireduce_incast_factor <= 0)
        throw cRuntimeError("invalid optireduce_incast_factor");

    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_optireduce()" << endl;
    unsigned long query_id = get_query_id();
    int m_collective_type = get_collective_type();

    if (m_collective_type != ALL_REDUCE)
        throw cRuntimeError("OptiReduce is only for All-Reduce");

    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_OPTIREDUCE);
    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_OPTIREDUCE);

    EV << "Initiating reduction for query: " << query_id << " among " <<
            num_dst_servers_per_query <<  " nodes with chunk size: " <<
            flow_size << endl;

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    int dst_server_idx_list[num_dst_servers_per_query];
    int gpu_idx_list[num_dst_servers_per_query];
    unsigned long flow_id_list[num_dst_servers_per_query];
    unsigned int element_idx;

    for (int i = 0; i < num_dst_servers_per_query; i++){
        // in dst_server_idx there can be this server's index, so filter it out
        dst_server_idx_list[i] = get_dst_server_idx();
        gpu_idx_list[i] = get_dst_gpu_idx();
        flow_id_list[i] = get_flow_id();

        if (dst_server_idx_list[i] == parent_index &&
                gpu_idx_list[i] == source_gpu_round_trip_idx)
            element_idx = i;
    }

    std::pair<unsigned long, int> query_src_gpu_pair = std::make_pair(query_id, source_gpu_round_trip_idx);

    if (use_becca_optireduce) {
        // store tree info to be used for broadcast
        std::list<std::list<jump_idx_list>> lists_of_lists_of_jump_idx;
        std::list<std::list<jump_idx_list>> lists_of_lists_of_ports_to_dsts;
        for (int m = 0; m < lb_tree_num; m++) {
            lists_of_lists_of_jump_idx.push_back(get_jump_to_idx_list());
            lists_of_lists_of_ports_to_dsts.push_back(get_ports_to_dst_list());
        }
        auto jump_idx_found_itr = query_gpu_map_to_lists_of_list_of_jumps_to_idxs.find(query_src_gpu_pair);
        if (jump_idx_found_itr != query_gpu_map_to_lists_of_list_of_jumps_to_idxs.end())
            throw cRuntimeError("How are we already having jumps and ports mapped to this <query,src_gpu> pair?");
        query_gpu_map_to_lists_of_list_of_jumps_to_idxs.insert(
                std::make_pair(query_src_gpu_pair, lists_of_lists_of_jump_idx));
        query_gpu_map_to_lists_of_list_of_ports_to_dsts.insert(
                std::make_pair(query_src_gpu_pair, lists_of_lists_of_ports_to_dsts));
    }

    // create the round-robin for incast control
    auto msg_record_found_itr = optireduce_query_gpu_msg_record.find(query_src_gpu_pair);
    if (msg_record_found_itr == optireduce_query_gpu_msg_record.end()) {
        std::list<AllgatherCustomCollectiveMsgInitiatorInfo*> tmp_list;
        optireduce_query_gpu_msg_record.insert(std::make_pair(query_src_gpu_pair, tmp_list));
        msg_record_found_itr = optireduce_query_gpu_msg_record.find(query_src_gpu_pair);
    }
    for (int i = element_idx+1; i < num_dst_servers_per_query; i++) {
        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(flow_id_list[i], flow_size, query_id, num_dst_servers_per_query);
        for (int m = 0; m < num_dst_servers_per_query; m++) {
            new_msg->responsibility.push_back(dst_server_idx_list[m]);
            new_msg->responsibility_gpu.push_back(gpu_idx_list[m]);
            new_msg->responsiblity_flow_ids.push_back(flow_id_list[m]);
        }
        new_msg->m_collective_type = m_collective_type;
        new_msg->m_collective_alg = BCAST_OPTIREDUCE;

        new_msg->optireduce_dst_server = dst_server_idx_list[i];
        new_msg->optireduce_dst_gpu = gpu_idx_list[i];
        msg_record_found_itr->second.push_back(new_msg);
    }

    for (int i = 0; i < element_idx; i++) {
        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(flow_id_list[i], flow_size, query_id, num_dst_servers_per_query);
        for (int m = 0; m < num_dst_servers_per_query; m++) {
            new_msg->responsibility.push_back(dst_server_idx_list[m]);
            new_msg->responsibility_gpu.push_back(gpu_idx_list[m]);
            new_msg->responsiblity_flow_ids.push_back(flow_id_list[m]);
        }
        new_msg->m_collective_type = m_collective_type;
        new_msg->m_collective_alg = BCAST_OPTIREDUCE;

        new_msg->optireduce_dst_server = dst_server_idx_list[i];
        new_msg->optireduce_dst_gpu = gpu_idx_list[i];
        msg_record_found_itr->second.push_back(new_msg);
    }

    optireduce_send_next(query_id, source_gpu_round_trip_idx);

    source_gpu_round_trip_idx++;
    source_gpu_round_trip_idx %= num_gpus;
}

void UDPLongAllgatherInitiatorApp::snd_allgather_ring() {
    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_ring()" << endl;
    unsigned long query_id = get_query_id();
    int m_collective_type = get_collective_type();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_RING);
    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_RING);

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    // check if there are any repititive servers in a query
    int dst_server_idx_list[num_dst_servers_per_query];
    int gpu_idx_list[num_dst_servers_per_query];
    unsigned long flow_id_list[num_dst_servers_per_query];
    for (int i = 0; i < num_dst_servers_per_query; i++){
        // in dst_server_idx there can be this server's index, so filter it out
        dst_server_idx_list[i] = get_dst_server_idx();
        gpu_idx_list[i] = get_dst_gpu_idx();
        flow_id_list[i] = get_flow_id();
    }

    if (m_collective_type == REDUCE) {
        // for ring, the only person who should initiate the ring is a gpu in dst_server_idx_list[0]
        // others should just wait and pass
        if (dst_server_idx_list[0] != parent_index) {
            EV << "The reduce initiator is server " << dst_server_idx_list[0] << " which isn't this server!" << endl;
            return;
        }
        auto ring_reduce_query_id_found_itr = ring_reduce_query_id.find(query_id);
        if (ring_reduce_query_id_found_itr == ring_reduce_query_id.end()) {
            EV << "This is the reduce initiator server and this query has not been initiated. Initiating..." << endl;
            ring_reduce_query_id.insert(query_id);
        } else {
            EV << "This is the reduce initiator server but the query is already initiated. Do nothing!" << endl;
            return;
        }
    }

    int tmp_source_gpu_idx = source_gpu_round_trip_idx;
    int element_idx_in_list = -1;
    for (int gpu_offset = 0; gpu_offset < num_gpus; gpu_offset++) {
        for (int k = 0; k < num_dst_servers_per_query; k++) {
            if (dst_server_idx_list[k] == parent_index &&
                    gpu_idx_list[k] == tmp_source_gpu_idx) {
                element_idx_in_list = k;
                EV << "element_idx_in_list is " << k << endl;
                break;
            }
        }
        if (element_idx_in_list >= 0)
            break;
        tmp_source_gpu_idx++;
        tmp_source_gpu_idx %= num_gpus;
    }
    if (element_idx_in_list < 0)
        throw cRuntimeError("element was not found in list!");
    source_gpu_round_trip_idx = tmp_source_gpu_idx;

    // remove the current element from the list
    int ring_dst_server_idx_list[num_dst_servers_per_query - 1];
    int ring_gpu_idx_list[num_dst_servers_per_query - 1];
    unsigned long ring_flow_id_list[num_dst_servers_per_query - 1];
    int idx = 0;
    for (int k = element_idx_in_list + 1; k < num_dst_servers_per_query; k++) {
        ring_dst_server_idx_list[idx] = dst_server_idx_list[k];
        ring_gpu_idx_list[idx] = gpu_idx_list[k];
        ring_flow_id_list[idx] = flow_id_list[k];
        idx++;
    }
    for (int k = 0; k < element_idx_in_list; k++) {
        ring_dst_server_idx_list[idx] = dst_server_idx_list[k];
        ring_gpu_idx_list[idx] = gpu_idx_list[k];
        ring_flow_id_list[idx] = flow_id_list[k];
        idx++;
    }

    int server_idx = ring_dst_server_idx_list[0];
    int gpu_idx = ring_gpu_idx_list[0];
    auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(ring_flow_id_list[0], flow_size, query_id, num_dst_servers_per_query - 1);
    for (int i = 1; i < num_dst_servers_per_query-1; i++) {
        new_msg->responsibility.push_back(ring_dst_server_idx_list[i]);
        new_msg->responsibility_gpu.push_back(ring_gpu_idx_list[i]);
        new_msg->responsiblity_flow_ids.push_back(ring_flow_id_list[i]);
    }

    if (m_collective_type == ALL_REDUCE) {
        new_msg->ring_end_server_idx = ring_dst_server_idx_list[num_dst_servers_per_query - 2];
        new_msg->ring_end_gpu_idx = ring_gpu_idx_list[num_dst_servers_per_query - 2];
        // add the current server and a random GPU from it to the list
        new_msg->responsibility.push_back(parent_index);
        new_msg->responsibility_gpu.push_back(source_gpu_round_trip_idx);
        new_msg->responsiblity_flow_ids.push_back(flow_id_list[element_idx_in_list]);

        // add the first destination to the end of the list
        new_msg->responsibility.push_back(ring_dst_server_idx_list[0]);
        new_msg->responsibility_gpu.push_back(ring_gpu_idx_list[0]);
        new_msg->responsiblity_flow_ids.push_back(ring_flow_id_list[0]);
    }

    if (m_collective_type == REDUCE) {
        new_msg->responsibility.push_back(parent_index);
        new_msg->responsibility_gpu.push_back(source_gpu_round_trip_idx);
        new_msg->responsiblity_flow_ids.push_back(flow_id_list[element_idx_in_list]);
    }

    new_msg->m_collective_type = m_collective_type;
    new_msg->m_collective_alg = BCAST_RING;

    if (server_idx == parent_index) {
        EV << "The destination GPU is in the same server" << endl;
        send_to_self(source_gpu_round_trip_idx, gpu_idx, new_msg);
    } else {
        init_socket(server_idx, gpu_idx);
        send_msg(server_idx, gpu_idx, new_msg, source_gpu_round_trip_idx);
    }

    source_gpu_round_trip_idx++;
    source_gpu_round_trip_idx %= num_gpus;
}

void UDPLongAllgatherInitiatorApp::snd_allgather_single_sender() {
    EV << "UDPLongAllgatherInitiatorApp::initiate_allgather_single_sender" << endl;
    unsigned long query_id = get_query_id();
    int m_collective_type = get_collective_type();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_SINGLE_SENDER);
    unsigned long flow_size = 0;

    // for all-to-allv we should read flow sizes per flow
    if (m_collective_type != ALL_TO_ALL_V)
        flow_size = get_flow_size(m_collective_type, BCAST_SINGLE_SENDER);

    // check if there are any repititive servers in a query
    int dst_server_idx_list[num_dst_servers_per_query-1];
    int gpu_idx_list[num_dst_servers_per_query-1];
    unsigned long flow_id_list[num_dst_servers_per_query-1];
    unsigned long flow_size_list[num_dst_servers_per_query-1];
    int tmp_idx = 0;
    for (int i = 0; i < num_dst_servers_per_query; i++){
        int tmp_dst_server_idx = get_dst_server_idx();
        int tmp_dst_gpu_idx = get_dst_gpu_idx();
        if (tmp_dst_server_idx == parent_index &&
                tmp_dst_gpu_idx == source_gpu_round_trip_idx)
            continue;
        // in dst_server_idx there can be this server's index, so filter it out
        dst_server_idx_list[tmp_idx] = tmp_dst_server_idx;
        gpu_idx_list[tmp_idx] = tmp_dst_gpu_idx;
        flow_id_list[tmp_idx] = get_flow_id();
        if (m_collective_type == ALL_TO_ALL_V)
            flow_size_list[tmp_idx] = get_flow_size(m_collective_type, BCAST_SINGLE_SENDER);
        tmp_idx++;
    }

    // src reduce for REDUCE
    if ((m_collective_type == REDUCE || m_collective_type == REDUCE_SCATTER) && ace_apply_src_reduce) {
        auto query_id_counter_found_itr = query_id_counter.find(query_id);
        if (query_id_counter_found_itr == query_id_counter.end()) {
            query_id_counter.insert(std::make_pair(query_id, 0));
            query_id_counter_found_itr = query_id_counter.find(query_id);
        }
        query_id_counter_found_itr->second++;
        if (query_id_counter_found_itr->second > num_gpus)
            throw cRuntimeError("How can we have query_id_counter_found_itr->second > num_gpus2");
        if (query_id_counter_found_itr->second < num_gpus) {
            EV << "We should wait for all the GPU data to arrive before reducing and sending!" << endl;
            return;
        }
        // query_id_counter_found_itr->second == num_gpus
        query_id_counter.erase(query_id_counter_found_itr);
    }

    if (tmp_idx != num_dst_servers_per_query-1) {
        std::cout << "collective: " << m_collective_type << endl;
        std::cout << "query_id: " << query_id << endl;
        std::cout << getFullPath() << endl;
        std::cout << "src gpu: " << source_gpu_round_trip_idx << endl;
        std::cout << "tmp_idx is: " << tmp_idx << endl;
        std::cout << "num_dst_servers_per_query-1 is: " << num_dst_servers_per_query-1 << endl;
        throw cRuntimeError("tmp_idx should have reached num_dst_servers_per_query-1");
    }

    for (int i = 0; i < num_dst_servers_per_query-1; i++){

        if (m_collective_type == REDUCE) {
            // for reduce we always reduce on the server in dst_server_idx_list[0]
            // the gpu idx on which the reduction is performed is calculated as query_id%num_gpus
            int reduction_gpu_idx = query_id % num_gpus;
            if (dst_server_idx_list[i] != dst_server_idx_list[0] || gpu_idx_list[i] != reduction_gpu_idx)
                continue;
        }

        if (m_collective_type == ALL_TO_ALL_V)
            flow_size = flow_size_list[i];

        if (flow_size <= 0)
            throw cRuntimeError("flow size should be positive!");

        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(flow_id_list[i], flow_size, query_id, num_dst_servers_per_query-1);

        new_msg->m_collective_type = m_collective_type;
        new_msg->m_collective_alg = BCAST_SINGLE_SENDER;

        if (dst_server_idx_list[i] == parent_index) {
            EV << "The destination GPU is in the same server" << endl;
            send_to_self(source_gpu_round_trip_idx, gpu_idx_list[i], new_msg);
        } else {
            // check if we already have a socket to this destination
            init_socket(dst_server_idx_list[i], gpu_idx_list[i]);
            send_msg(dst_server_idx_list[i], gpu_idx_list[i], new_msg, source_gpu_round_trip_idx);
        }

        if (m_collective_type == REDUCE) {
            EV << "Reducing at server: " << dst_server_idx_list[i] << ", gpu: " << gpu_idx_list[i] << endl;
            break;
        }
    }

    source_gpu_round_trip_idx++;
    source_gpu_round_trip_idx %= num_gpus;

}

//void UDPLongAllgatherInitiatorApp::filter_out_self_data() {
//    if (using_ace)
//        throw cRuntimeError("With combinational queries, filter_out_self_data should never be called");
//    auto gpu_it = dst_gpu_idx.begin();
//    for (auto it = dst_server_idx.begin(); it != dst_server_idx.end();) {
//        if (*it == parent_index) {
//            it = dst_server_idx.erase(it);
//            gpu_it = dst_gpu_idx.erase(gpu_it);
//        } else {
//            it++;
//            gpu_it++;
//        }
//    }
//}

void UDPLongAllgatherInitiatorApp::snd_allgather_ina() {
    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_ina" << endl;

    if (use_fattree)
        throw cRuntimeError("snd_allgather_ina is not yet deployed with fattree");

    unsigned long query_id = get_query_id();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_INA);
    int m_collective_type = get_collective_type();

    if (m_collective_type != ALL_REDUCE)
        throw cRuntimeError("In-network Aggregation should only be used for ALL_REDUCE");

    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_INA);

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    // we set the flow id to zero
    auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(get_flow_id(), flow_size, query_id, num_dst_servers_per_query);

    int candidate_server_idx = -1;
    int candidate_gpu_idx = 0; // can always be 0 for ina

    int parent_leaf_idx = int(parent_index / num_servers_under_each_leaf);
    EV << "Parent leaf idx is: " << parent_leaf_idx << endl;

    bool msg_flow_size_used = false;

    for (int i = 0; i < num_dst_servers_per_query; i++) {
        int m_dst_server_idx = get_dst_server_idx();
        int m_gpu_idx = get_dst_gpu_idx();
        int dst_leaf_idx = int(m_dst_server_idx / num_servers_under_each_leaf);
        // choose a candidate from under another leaf
        if (candidate_server_idx == -1 &&
                m_dst_server_idx != parent_index &&
                parent_leaf_idx != dst_leaf_idx) {
            candidate_server_idx = m_dst_server_idx;
            EV << "Candidate server idx is " << candidate_server_idx << endl;
        }
        new_msg->network_assisted_destination_idx_list.push_back(m_dst_server_idx);
        new_msg->network_assisted_dst_gpu_idx_list.push_back(m_gpu_idx);
        // we already fetched from flow id once, re-use it
        if (m_dst_server_idx == parent_index &&
                source_gpu_round_trip_idx == m_gpu_idx &&
                !msg_flow_size_used) {
            msg_flow_size_used = true;
            new_msg->network_assisted_flow_ids_list.push_back(new_msg->flow_id);
        } else
            new_msg->network_assisted_flow_ids_list.push_back(get_flow_id());
    }

    if (flow_ids.size() % num_flows_per_ml_query != 0)
        throw cRuntimeError("flow_ids.size() % num_flows_per_ml_query != 0");

    if (candidate_server_idx == -1)
        throw cRuntimeError("Now candidate was chosen!");

    new_msg->m_collective_type = m_collective_type;
    new_msg->m_collective_alg = BCAST_INA;

    // used to know if we must add reduction latency overhead
    new_msg->tree_traversal_dir = TREE_UP;

    init_socket(candidate_server_idx, candidate_gpu_idx);

    send_msg(candidate_server_idx, candidate_gpu_idx, new_msg, source_gpu_round_trip_idx);

    source_gpu_round_trip_idx++;
    source_gpu_round_trip_idx %= num_gpus;

}

int UDPLongAllgatherInitiatorApp::get_tor_idx_for_server(int server_idx) {
    Enter_Method("get_tor_idx_for_server");

    int number_of_servers_under_tor = -1;
    if (use_fattree) {
        // we changed fattree to have 2 servers per tor
        number_of_servers_under_tor = num_servers_under_each_leaf;
    } else {
        number_of_servers_under_tor = num_servers_under_each_leaf;
    }
//    std::cout << "number_of_servers_under_tor: " << number_of_servers_under_tor << endl;
    int tor_idx = int(server_idx / number_of_servers_under_tor);
//    std::cout << "server_idx: " << server_idx << ", tor idx: " << tor_idx << endl;
    return tor_idx;
}

int UDPLongAllgatherInitiatorApp::get_pod_idx_for_tor(int tor_idx) {
    Enter_Method("get_pod_idx_for_tor");
    if (use_fattree) {
        if (fattree_k < 0)
            throw cRuntimeError("Invalid fattree K");
        return int(tor_idx / (fattree_k/2.0));
    } else {
        // everyone is in one pod
        return 0;
    }
}

unsigned long UDPLongAllgatherInitiatorApp::get_relative_tor_idx(unsigned long tor_idx) {
    Enter_Method("get_relative_tor_idx");
    if (use_fattree) {
        return tor_idx % int(fattree_k / 2.0);
    } else
        return tor_idx; // relative tor idx for leaf spine is the tor idx itselfs

}

void UDPLongAllgatherInitiatorApp::snd_allgather_partial_mcast() {
    // sending to ToRs to multicast there

    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_partial_mcast" << endl;

    unsigned long query_id = get_query_id();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_PARTIAL_MULTICAST);
    int m_collective_type = get_collective_type();

    switch (m_collective_type) {
        case ALL_TO_ALL_V:
        case REDUCE:
        case REDUCE_SCATTER:
            throw cRuntimeError("Invalid collective for marc!");
            break;
        default:
            break;
    }

    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_PARTIAL_MULTICAST);

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    int src_tor_idx = get_tor_idx_for_server(parent_index);
    int src_pod_idx = get_pod_idx_for_tor(src_tor_idx);

    int num_src_gpus_to_use = 1;
    double msg_passing_delay = 0;
    bool should_chunk_data = false;
    if (partial_mcast_chunk_data) {
        if (m_collective_type == BROADCAST ||
                (m_collective_type == ALL_REDUCE &&
                        ace_apply_src_reduce)) {
            should_chunk_data = true;
            if (use_fattree) {
                // in fat-tree we are bounded by the transmission rate between ToRs and aggs which is link_bw * K/2
                num_src_gpus_to_use = std::min(num_gpus, int(fattree_k/2.0));
            } else {
                // in leaf spine we typically have more spines than gpus per server
                num_src_gpus_to_use = std::min(num_gpus, num_spines);
            }
            if (flow_size % num_src_gpus_to_use != 0)
                throw cRuntimeError("Flow size cannot be divided by num_src_gpus_to_use");
            flow_size /= num_src_gpus_to_use;
        }
    }

    unsigned long original_flow_size = flow_size * num_src_gpus_to_use;

    if (should_chunk_data || partial_mcast_chunk_tors)
        msg_passing_delay = get_intra_host_pass_delay(original_flow_size).dbl();

    EV << "Using " << num_src_gpus_to_use << " gpus to send the data. Flow size per GPU is " <<
            flow_size << " bytes and total flow size is: " << original_flow_size << " bytes." << endl;

    // maps each ToR idx to [(server idx, gpu idx, flow id)]
    std::map<int, std::list<std::tuple<int, int, unsigned long>>> tor_idx_to_flow_info_map;
    std::unordered_set<int> dst_servers_in_this_query_set;
    dst_servers_in_this_query_set.insert(parent_index);
    // store the CIDR groups per pod -- later used for cidr at core
    std::map<unsigned long, std::set<std::string>> dst_pod_to_tor_cidr_groups;
    std::unordered_map<unsigned long, std::unordered_set<unsigned long>> cidr_pod_idx_to_dst_tor_idx_list;
    std::unordered_set<unsigned long> cidr_dst_pod_idx_set;
    int total_picks = 0;
    std::list<unsigned long> flow_ids_tmp_list;
    for (int i = 0; i < num_dst_servers_per_query + num_gpus; i++) {
        int m_dst_server_idx = get_dst_server_idx();
        int m_gpu_idx = get_dst_gpu_idx();
        // filter_out_self_data
        while (m_dst_server_idx == parent_index) {
            EV << "Skipping self..." << endl;
            total_picks++;
            i++;

            if (i == num_dst_servers_per_query + num_gpus)
                break;

            m_dst_server_idx = get_dst_server_idx();
            m_gpu_idx = get_dst_gpu_idx();
        }
        EV << "Picking " << m_dst_server_idx << " as dst server" << endl;
        if (i > num_dst_servers_per_query + num_gpus)
            throw cRuntimeError("i > num_dst_servers_per_query");
        if (i == num_dst_servers_per_query + num_gpus)
            break;
        total_picks++;

        if (m_dst_server_idx == parent_index)
            throw cRuntimeError("We have already filtered out self data, why are we seeing this!");

        unsigned long m_flow_id = get_flow_id();
        flow_ids_tmp_list.push_back(m_flow_id);

        auto dst_server_idx_found = dst_servers_in_this_query_set.find(m_dst_server_idx);
        if (dst_server_idx_found != dst_servers_in_this_query_set.end())
            continue;

        dst_servers_in_this_query_set.insert(m_dst_server_idx);
        std::tuple<int, int, unsigned long> flow_info_tuple = std::make_tuple(m_dst_server_idx,
                m_gpu_idx,
                m_flow_id);
        int tor_idx = get_tor_idx_for_server(m_dst_server_idx);
        auto tor_idx_itr = tor_idx_to_flow_info_map.find(tor_idx);
        if (tor_idx_itr == tor_idx_to_flow_info_map.end()) {
            std::list<std::tuple<int, int, unsigned long>> tmp_list;
            tor_idx_to_flow_info_map.insert(std::make_pair(tor_idx, tmp_list));
            tor_idx_itr = tor_idx_to_flow_info_map.find(tor_idx);
        }
        tor_idx_itr->second.push_back(flow_info_tuple);

        int pod_idx = get_pod_idx_for_tor(tor_idx);
        if (partial_mcast_use_cidr_agg) {
            // assign dst tors that are not src tors to cidr info
            auto pod_found_itr = cidr_pod_idx_to_dst_tor_idx_list.find(pod_idx);
            if (pod_found_itr == cidr_pod_idx_to_dst_tor_idx_list.end()) {
                std::unordered_set<unsigned long> tmp_set;
                cidr_pod_idx_to_dst_tor_idx_list.insert(std::make_pair(pod_idx,
                        tmp_set));
                pod_found_itr = cidr_pod_idx_to_dst_tor_idx_list.find(pod_idx);
            }
            pod_found_itr->second.insert(tor_idx);
        }

        if (partial_mcast_use_cidr_core) {
            cidr_dst_pod_idx_set.insert(pod_idx);
            std::set<std::string> tmp_set;
            dst_pod_to_tor_cidr_groups.insert(std::make_pair(pod_idx, tmp_set));
        }
    }


    if (m_collective_type == ALL_REDUCE && ace_apply_src_reduce) {
        auto query_id_counter_found_itr = query_id_counter.find(query_id);
        if (query_id_counter_found_itr == query_id_counter.end()) {
            query_id_counter.insert(std::make_pair(query_id, 0));
            query_id_counter_found_itr = query_id_counter.find(query_id);
        }
        query_id_counter_found_itr->second++;
        if (query_id_counter_found_itr->second > num_gpus)
            throw cRuntimeError("How can we have query_id_counter_found_itr->second > num_gpus");
        if (query_id_counter_found_itr->second < num_gpus) {
            EV << "We should wait for all the GPU data to arrive before reducing and sending!" << endl;
            return;
        }
        // query_id_counter_found_itr->second == num_gpus
        query_id_counter.erase(query_id_counter_found_itr);

        // add pass to self reduce delay
        msg_passing_delay += get_processing_delay(m_collective_type, BCAST_PARTIAL_MULTICAST);
        if (!should_chunk_data) {
            // if should_chunk_data is true, we reduce after the shards are sent to each GPU and the delay is already added
            msg_passing_delay += get_intra_host_pass_delay(original_flow_size).dbl();
        }
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
    for (auto pod_idx_itr = cidr_pod_idx_to_dst_tor_idx_list.begin();
            pod_idx_itr != cidr_pod_idx_to_dst_tor_idx_list.end();
            pod_idx_itr++) {
        EV << "CIDR at agg: Processing pod " << pod_idx_itr->first << endl;
        if (agg_cidr_max_bit_size == 0)
            throw cRuntimeError("agg_cidr_max_bit_size should be > 0");
        // std::unordered_map<unsigned long, std::string>
        auto tor_index_to_prefix_info = CIRD_minimal_prefix_cover(
                pod_idx_itr->second,
                agg_cidr_max_bit_size,
                CIDR_AGG);

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

            if (partial_mcast_use_cidr_core) {
                // store unique aggregate cidr groups we have under each pod
                auto dst_pod_itr = dst_pod_to_tor_cidr_groups.find(pod_idx_itr->first);
                if (dst_pod_itr == dst_pod_to_tor_cidr_groups.end())
                    throw cRuntimeError("Dst pod must exist here!");
                dst_pod_itr->second.insert(cidr_group_id);
            }
        }
    }

    // this is empty if !partial_mcast_use_cidr_core
//    std::unordered_map<unsigned long, std::list<std::string>> cidr_pod_group_ids_list;  // pod idx --> id
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
        if (core_cidr_max_bit_size == 0)
            throw cRuntimeError("core_cidr_max_bit_size should be > 0");
        // std::unordered_map<unsigned long, std::string>
        auto pod_index_to_prefix_info = CIRD_minimal_prefix_cover(
                cidr_dst_pod_idx_set,
                core_cidr_max_bit_size,
                CIDR_CORE);

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

    // total picks must always reach num_flows_per_ml_query (num_dst_servers_per_query+num_gpus)
    if (total_picks != num_dst_servers_per_query+num_gpus) {
        std::cout << total_picks << endl;
        std::cout << num_dst_servers_per_query+num_gpus << endl;
        throw cRuntimeError("invalid total picks");
    }

    // now we know how many dst servers we have
    if (flow_ids_tmp_list.size() < num_dst_servers_per_query)
        throw cRuntimeError("How are we choosing the source server more than num_gpus times!");
    EV << "num_dst_servers_per_query update from " << num_dst_servers_per_query;
    num_dst_servers_per_query = flow_ids_tmp_list.size();
    EV << " to " << num_dst_servers_per_query << endl;

    std::map<int, int> tor_to_candidate_server_idx_map;
    std::map<int, int> tor_to_candidate_gpu_idx_map;
    int flow_id_list_idx = -1;
    for (int sender_gpu_idx = 0;
            sender_gpu_idx < num_src_gpus_to_use;
            sender_gpu_idx++) {

        std::map<int, int> pod_to_tor_num;
        std::map<int, std::list<AllgatherCustomCollectiveMsgInitiatorInfo*>> tor_group_to_msg_list;

        for (auto it = tor_idx_to_flow_info_map.begin();
                it != tor_idx_to_flow_info_map.end();
                it++) {
            if (it->second.size() == 0)
                throw cRuntimeError("How is the list empty!!");
            int pod_idx = get_pod_idx_for_tor(it->first);

            // we want to assign different flow ids to chunks of the same flow sent by different gpus
            flow_id_list_idx++;

            if (flow_id_list_idx >= num_dst_servers_per_query)
                throw cRuntimeError("flow_id_list_idx >= num_dst_servers_per_query!");
            int candidate_flow_id = flow_ids_list[flow_id_list_idx];
            auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(candidate_flow_id, flow_size, query_id, num_dst_servers_per_query);
            for (auto flow_info_list_it = it->second.begin(); flow_info_list_it != it->second.end(); flow_info_list_it++) {
                new_msg->network_assisted_destination_idx_list.push_back(std::get<0>(*flow_info_list_it));
                new_msg->network_assisted_dst_gpu_idx_list.push_back(std::get<1>(*flow_info_list_it));
                new_msg->network_assisted_flow_ids_list.push_back(std::get<2>(*flow_info_list_it));
            }
            new_msg->dst_tor_idx = it->first;

            if (partial_mcast_use_cidr_agg) {
                auto cird_tor_info_itr = cidr_tor_group_ids.find(it->first);
                if (cird_tor_info_itr == cidr_tor_group_ids.end() && it->first != src_tor_idx) {
                    std::cout << "src tor: " << src_tor_idx << endl;
                    std::cout << "dst tor: " << it->first << endl;
                    throw cRuntimeError("No cidr group found for Tor and dst_tor != src_tor");
                }
                if (cird_tor_info_itr != cidr_tor_group_ids.end()) {
                    new_msg->agg_cidr_group_id = cird_tor_info_itr->second;
                    auto cidr_group_cnt_itr = agg_cidr_group_member_count_map.find(cird_tor_info_itr->second);
                    if (cidr_group_cnt_itr == agg_cidr_group_member_count_map.end())
                        throw cRuntimeError("No cidr group cnt found for cidr group!");
                    new_msg->agg_cidr_group_member_count = cidr_group_cnt_itr->second;
                    EV << "Tor " << it->first << " is assigned to CIDR group " <<
                            new_msg->agg_cidr_group_id << " and group cnt is " <<
                            new_msg->agg_cidr_group_member_count << endl;
                }
            }

            if (partial_mcast_use_cidr_core) {
                auto cird_pod_info_itr = agg_cidr_id_to_core_cidr_id.find(new_msg->agg_cidr_group_id);
                if (cird_pod_info_itr == agg_cidr_id_to_core_cidr_id.end() &&
                        pod_idx != src_pod_idx) {
                    std::cout << "agg_cidr_group_id: " << new_msg->agg_cidr_group_id << endl;
                    std::cout << "src pod: " << src_pod_idx << endl;
                    std::cout << "dst pod: " << pod_idx << endl;
                    throw cRuntimeError("No cidr group found for pod and dst_pod != src_pod");
                }
                if (cird_pod_info_itr != agg_cidr_id_to_core_cidr_id.end()) {
                    new_msg->core_cidr_group_id = cird_pod_info_itr->second;
                    auto cidr_group_cnt_itr = core_cidr_group_member_count_map.find(cird_pod_info_itr->second);
                    if (cidr_group_cnt_itr == core_cidr_group_member_count_map.end())
                        throw cRuntimeError("No cidr group cnt found for core cidr group!");
                    new_msg->core_cidr_group_member_count = cidr_group_cnt_itr->second;
                    EV << "Pod " << pod_idx << " is assigned to CIDR group " <<
                            new_msg->core_cidr_group_id << " and group cnt is " <<
                            new_msg->core_cidr_group_member_count << endl;
                }
            }

            new_msg->extra_delay = msg_passing_delay;
            new_msg->partial_mcast_original_flow_size_bytes = original_flow_size;
            new_msg->in_src_sharding = should_chunk_data;

            if (new_msg->network_assisted_destination_idx_list.size() == 0 ||
                    new_msg->network_assisted_dst_gpu_idx_list.size() == 0)
                throw cRuntimeError("Invalid network assited list size!");

            // check if we already have a socket to this destination
            int candidate_server_idx = (*new_msg->network_assisted_destination_idx_list.begin());
            int candidate_gpu_idx = (*new_msg->network_assisted_dst_gpu_idx_list.begin());

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

            new_msg->m_collective_type = m_collective_type;
            new_msg->m_collective_alg = BCAST_PARTIAL_MULTICAST;

            EV << "Pod idx: " << pod_idx << ", src pod idx: " << src_pod_idx << endl;
            if (partial_mcast_programmable_cores && (pod_idx != src_pod_idx)) {
                EV << "Grouping ToRs for programmable cores" << endl;
                new_msg->candidate_server_idx = candidate_server_idx;
                new_msg->candidate_gpu_idx = candidate_gpu_idx;
                auto pod_itr = pod_to_tor_num.find(pod_idx);
                if (pod_itr == pod_to_tor_num.end()) {
                    pod_to_tor_num.insert(std::make_pair(pod_idx, 0));
                    pod_itr = pod_to_tor_num.find(pod_idx);
                }
                pod_itr->second++;
                int group_idx = 0;
                if (partial_mcast_divide_tors_among_cores)
                    group_idx = pod_itr->second;
                EV << "Group idx for tor " << it->first <<
                        " and pod " << pod_idx <<
                        " is " << group_idx << endl;
                auto group_itr = tor_group_to_msg_list.find(group_idx);
                if (group_itr == tor_group_to_msg_list.end()) {
                    std::list<AllgatherCustomCollectiveMsgInitiatorInfo*> tmp_list;
                    tor_group_to_msg_list.insert(std::make_pair(group_idx, tmp_list));
                    group_itr = tor_group_to_msg_list.find(group_idx);
                }
                group_itr->second.push_back(new_msg);
            } else {
                EV << "Sending message right away" << endl;

                if (new_msg->candidate_gpu_idx >= 0 ||
                        new_msg->candidate_server_idx >= 0)
                    throw cRuntimeError("candidate server and gpu idx on the message should be -1 here!");

                init_socket(candidate_server_idx, candidate_gpu_idx);

                EV << "sender_gpu_idx: " << sender_gpu_idx <<
                        ", source_gpu_round_trip_idx: " << source_gpu_round_trip_idx << endl;

                EV << "Sending message to ToR " << new_msg->dst_tor_idx << endl;
                send_msg(candidate_server_idx,
                        candidate_gpu_idx,
                        new_msg,
                        source_gpu_round_trip_idx,
                        initiate_tree);

                if (partial_mcast_chunk_tors) {
                    // rotate among src gpus for distinct tors
                    source_gpu_round_trip_idx++;
                    source_gpu_round_trip_idx %= num_gpus;
                    EV << "Moving the gpu one ahead to " << source_gpu_round_trip_idx << endl;
                }
            }
        }

        // we don't record controller delay since it is simultaneously handled with per-tor transmission
        double controller_delay = get_controller_delay(false, true);
        if (tor_group_to_msg_list.size() > 0) {
            if (!partial_mcast_programmable_cores)
                throw cRuntimeError("partial_mcast_programmable_cores must be true here!");
            for (auto group_it = tor_group_to_msg_list.begin();
                    group_it != tor_group_to_msg_list.end();
                    group_it++) {
                if (group_it->second.size() == 0)
                    throw cRuntimeError("List corresponding to no group should be empty!");

                std::map<unsigned int, std::set<std::string>> dst_pod_to_cidr_id_set;
                int num_msgs_expected_to_be_sent_by_core = 0;
                if (partial_mcast_use_cidr_agg) {
                    for (auto m_msg : group_it->second) {
                        if (m_msg->dst_tor_idx < 0)
                            throw cRuntimeError("We need dst tor idx here!");
                        unsigned int dst_pod_idx = get_pod_idx_for_tor(m_msg->dst_tor_idx);
                        auto dst_pod_itr = dst_pod_to_cidr_id_set.find(dst_pod_idx);
                        if (dst_pod_itr == dst_pod_to_cidr_id_set.end()) {
                            std::set<std::string> tmp_set;
                            dst_pod_to_cidr_id_set.insert(std::make_pair(dst_pod_idx, tmp_set));
                            dst_pod_itr = dst_pod_to_cidr_id_set.find(dst_pod_idx);
                        }
                        dst_pod_itr->second.insert(m_msg->agg_cidr_group_id);
                    }
                    for (auto dst_pod_itr : dst_pod_to_cidr_id_set)
                        num_msgs_expected_to_be_sent_by_core += dst_pod_itr.second.size();
                }

                EV << "Sending messages for group " << group_it->first << endl;
                for (auto msg_it = group_it->second.begin();
                        msg_it != group_it->second.end();
                        msg_it++) {

                    auto msg_to_send = (*msg_it);
                    if (msg_to_send == nullptr)
                        throw cRuntimeError("msg_to_send should not be nullptr!");

                    msg_to_send->num_msgs_in_tor_group = group_it->second.size();
                    if (partial_mcast_use_cidr_agg) {
                        if (partial_mcast_divide_tors_among_cores)
                            throw cRuntimeError("It is meaningless to divide dst tors across multiple cores while we have CIDR agg!");
                        if (!use_fattree)
                            throw cRuntimeError("num_msgs_in_tor_group update for CIDR is not yet supported for leaf-spine!");

//                        // with fragmentation, the following condition is not necessarily true...
//                        if (msg_to_send->num_msgs_in_tor_group % int(fattree_k / 2) != 0)
//                            throw cRuntimeError("num_msgs_in_tor_group must be dividable by K/2 (tors per pod)");

//                        if (msg_to_send->dst_tor_idx < 0)
//                            throw cRuntimeError("We need dst tor idx here 2!");
//                        unsigned int dst_pod_idx = get_pod_idx_for_tor(msg_to_send->dst_tor_idx);
//                        auto dst_pod_itr = dst_pod_to_cidr_id_set.find(dst_pod_idx);
//                        if (dst_pod_itr == dst_pod_to_cidr_id_set.end())
//                            throw cRuntimeError("Dst pod itr shoudl exist here!");

                        if (num_msgs_expected_to_be_sent_by_core <= 0)
                            throw cRuntimeError("Invalid num_msgs_expected_to_be_sent_by_core");

                        // now one message is sent to each pod!
                        EV << "num_msgs_in_tor_group updated from " << msg_to_send->num_msgs_in_tor_group;
//                        msg_to_send->num_msgs_in_tor_group /= int(fattree_k / 2);
//                        msg_to_send->num_msgs_in_tor_group = dst_pod_itr->second.size();
                        msg_to_send->num_msgs_in_tor_group = num_msgs_expected_to_be_sent_by_core;
                        EV << " to " << msg_to_send->num_msgs_in_tor_group << endl;
                    }


                    msg_to_send->tor_group_idx = group_it->first;

                    msg_to_send->controller_finish_time = simTime() + controller_delay;

                    int candidate_server_idx = msg_to_send->candidate_server_idx;
                    int candidate_gpu_idx = msg_to_send->candidate_gpu_idx;
                    msg_to_send->candidate_gpu_idx = -1;
                    msg_to_send->candidate_server_idx = -1;

                    init_socket(candidate_server_idx, candidate_gpu_idx);

                    EV << "source_gpu_round_trip_idx: " << source_gpu_round_trip_idx << endl;
                    send_msg(candidate_server_idx,
                            candidate_gpu_idx,
                            msg_to_send,
                            source_gpu_round_trip_idx,
                            initiate_tree);
                }

                if (partial_mcast_chunk_tors) {
                    // rotate among src gpus for distinct tor groups
                    source_gpu_round_trip_idx++;
                    source_gpu_round_trip_idx %= num_gpus;
                    EV << "Moving the gpu one ahead to " << source_gpu_round_trip_idx << endl;
                }

            }
        }

        if (should_chunk_data) {
            // rotate among src gpus for distict shards
            source_gpu_round_trip_idx++;
            source_gpu_round_trip_idx %= num_gpus;
        }
    }

    if (!should_chunk_data && !partial_mcast_chunk_tors) {
        // change the src gpu for the next data
        source_gpu_round_trip_idx++;
        source_gpu_round_trip_idx %= num_gpus;
    }

}

void UDPLongAllgatherInitiatorApp::snd_allgather_network_assisted() {
    EV << "UDPLongAllgatherInitiatorApp::snd_allgather_network_assisted" << endl;

    unsigned long query_id = get_query_id();
    unsigned long num_dst_servers_per_query = get_dst_server_num_per_query(BCAST_NETWORK_ASSISTED);
    int m_collective_type = get_collective_type();

    unsigned long flow_size = get_flow_size(m_collective_type, BCAST_NETWORK_ASSISTED);
    unsigned int pkt_num = std::ceil(flow_size * 1.0 / mss);
    unsigned long rsbf_original_flow_size = 0;
    B tree_init_overhead = B(0);

    int num_src_gpus_to_use = 1;
    double msg_passing_delay = 0;
    bool should_chunk_data = false;
    if (partial_mcast_chunk_data) {
        if (m_collective_type == BROADCAST ||
                (m_collective_type == ALL_REDUCE &&
                        ace_apply_src_reduce)) {
            should_chunk_data = true;
            if (use_fattree) {
                // in fat-tree we are bounded by the transmission rate between ToRs and aggs which is link_bw * K/2
                num_src_gpus_to_use = std::min(num_gpus, int(fattree_k/2.0));
            } else {
                // in leaf spine we typically have more spines than gpus per server
                num_src_gpus_to_use = std::min(num_gpus, num_spines);
            }
            if (flow_size % num_src_gpus_to_use != 0)
                throw cRuntimeError("Flow size cannot be divided by num_src_gpus_to_use");
            flow_size /= num_src_gpus_to_use;
        }
    }

    if (initiate_tree) {
        throw cRuntimeError("initiate tree idea was left out!");
        EV << "Pkt num: " << pkt_num << ". Tree ID overhead per_pkt is: " << tree_id_overhead_bytes << endl;
        unsigned int total_tree_id_overhead_bytes = tree_id_overhead_bytes.get() * pkt_num;
        if (use_orca) {
            tree_init_overhead = B(19);
        } else if (use_cepheus) {
            total_tree_id_overhead_bytes = 0;
            // every 183 nodes is covered by 1 MSS
            // we devide 183 by num_gpus to get number of hosts since we only encode the hosts in multicast
            tree_init_overhead = B(int(num_dst_servers_per_query / 183.0 * mss));
        } else {
            // default is rsbf
            if (rsbf_overhead == 0)
                throw cRuntimeError("rsbf_overhead should be > 0");
            tree_init_overhead = B(rsbf_overhead);
        }
        EV << "Tree init overhead is " << tree_init_overhead << endl;
        flow_size += total_tree_id_overhead_bytes;
        EV << "new flow size is " << flow_size << endl;
    } else {
        // add orca/rsbf overhead
        unsigned int overhead = 0;
        // here it should be either all-gather, all-reduce, or broadcast
        if (use_orca)
            overhead = pkt_num * 19;
        else if (use_rsbf) {
            rsbf_original_flow_size = flow_size;
            overhead = pkt_num * rsbf_overhead;
        }
        flow_size += overhead;
        EV << "Overhead is: " << overhead << " B." << endl;
        EV << "New flow size is " << flow_size << endl;
    }

    unsigned long original_flow_size = flow_size * num_src_gpus_to_use;

    EV << "Using " << num_src_gpus_to_use << " gpus to send the data. Flow size per GPU is " <<
            flow_size << " bytes and total flow size is: " << original_flow_size << " bytes." << endl;

    if (num_dst_servers_per_query == 0)
        throw cRuntimeError("num_dst_servers_per_query == 0");

    if (should_chunk_data)
        msg_passing_delay = get_intra_host_pass_delay(original_flow_size).dbl();

    std::unordered_set<int> dst_servers_in_this_query_set;
    std::list<int> dst_servers_in_this_query_list;
    std::list<int> dst_gpus_in_this_query_list;
    dst_servers_in_this_query_set.insert(parent_index);
    int total_picks = 0;
    std::list<unsigned long> flow_ids_tmp_list;
    for (int i = 0; i < num_dst_servers_per_query + num_gpus; i++) {
        int m_dst_server_idx = get_dst_server_idx();
        int m_gpu_idx = get_dst_gpu_idx();
        // filter_out_self_data
        while (m_dst_server_idx == parent_index) {
            total_picks++;
            i++;

            if (i == num_dst_servers_per_query + num_gpus)
                break;

            m_dst_server_idx = get_dst_server_idx();
            m_gpu_idx = get_dst_gpu_idx();
        }
        if (i > num_dst_servers_per_query + num_gpus)
            throw cRuntimeError("i > num_dst_servers_per_query");
        if (i == num_dst_servers_per_query + num_gpus)
            break;
        total_picks++;

        if (m_dst_server_idx == parent_index)
            throw cRuntimeError("We have already filtered out self data, why are we seeing this!");

        unsigned long m_flow_id = get_flow_id();
        flow_ids_tmp_list.push_back(m_flow_id);

        auto dst_server_idx_found = dst_servers_in_this_query_set.find(m_dst_server_idx);
        if (dst_server_idx_found != dst_servers_in_this_query_set.end())
            continue;
        dst_servers_in_this_query_set.insert(m_dst_server_idx);
        dst_servers_in_this_query_list.push_back(m_dst_server_idx);
        dst_gpus_in_this_query_list.push_back(m_gpu_idx);
    }

    unsigned long flow_ids_list[flow_ids_tmp_list.size()];
    int m_idx = 0;
    for (auto m_flow_ids : flow_ids_tmp_list) {
        flow_ids_list[m_idx] = m_flow_ids;
        m_idx++;
    }

    // total picks must always reach num_flows_per_ml_query (num_dst_servers_per_query+num_gpus)
    if (total_picks != num_dst_servers_per_query+num_gpus) {
        std::cout << total_picks << endl;
        std::cout << num_dst_servers_per_query+num_gpus << endl;
        throw cRuntimeError("invalid total picks");
    }

    // now we know how many dst servers we have
    if (flow_ids_tmp_list.size() < num_dst_servers_per_query)
        throw cRuntimeError("How are we choosing the source server more than num_gpus times!");
    EV << "num_dst_servers_per_query update from " << num_dst_servers_per_query;
    num_dst_servers_per_query = flow_ids_tmp_list.size();
    EV << " to " << num_dst_servers_per_query << endl;

    std::list<std::list<jump_idx_list>> lists_of_lists_of_jump_idx;
    std::list<std::list<jump_idx_list>> lists_of_lists_of_ports_to_dsts;
    for (int m = 0; m < lb_tree_num; m++) {
        lists_of_lists_of_jump_idx.push_back(get_jump_to_idx_list());
        lists_of_lists_of_ports_to_dsts.push_back(get_ports_to_dst_list());
    }

    if (dst_gpus_in_this_query_list.size() != dst_servers_in_this_query_list.size())
        throw cRuntimeError("dst_gpus_in_this_query_list.size() != dst_servers_in_this_query_list.size()");

    if (m_collective_type == ALL_REDUCE && ace_apply_src_reduce) {
        auto query_id_counter_found_itr = query_id_counter.find(query_id);
        if (query_id_counter_found_itr == query_id_counter.end()) {
            query_id_counter.insert(std::make_pair(query_id, 0));
            query_id_counter_found_itr = query_id_counter.find(query_id);
        }
        query_id_counter_found_itr->second++;
        if (query_id_counter_found_itr->second > num_gpus)
            throw cRuntimeError("How can we have query_id_counter_found_itr->second > num_gpus");
        if (query_id_counter_found_itr->second < num_gpus) {
            EV << "We should wait for all the GPU data to arrive before reducing and sending!" << endl;
            return;
        }
        // query_id_counter_found_itr->second == num_gpus
        query_id_counter.erase(query_id_counter_found_itr);

        // add pass to self reduce delay
        msg_passing_delay += get_processing_delay(m_collective_type, BCAST_NETWORK_ASSISTED);
        if (!should_chunk_data) {
            // if should_chunk_data is true, we reduce after the shards are sent to each GPU and the delay is already added
            msg_passing_delay += get_intra_host_pass_delay(original_flow_size).dbl();
        }
    }

    int base_flow_id_list_idx = -1;
    for (int sender_gpu_idx = 0;
            sender_gpu_idx < num_src_gpus_to_use;
            sender_gpu_idx++) {

        EV << "Sending chunk " << sender_gpu_idx << endl;

        base_flow_id_list_idx++;

        // we set the flow id to zero
        auto new_msg = new AllgatherCustomCollectiveMsgInitiatorInfo(
                flow_ids_list[base_flow_id_list_idx],
                flow_size,
                query_id,
                num_dst_servers_per_query);

        new_msg->rsbf_original_flow_size = rsbf_original_flow_size;
        new_msg->tree_init_overhead = tree_init_overhead;

        new_msg->extra_delay = msg_passing_delay;
        new_msg->partial_mcast_original_flow_size_bytes = original_flow_size;
        new_msg->in_src_sharding = should_chunk_data;

        new_msg->using_orca = use_orca;

        auto dst_gpu_idx_itr = dst_gpus_in_this_query_list.begin();
        int flow_id_list_idx = -1;
        for (auto dst_server_idx_itr : dst_servers_in_this_query_list) {
            flow_id_list_idx++;
            if (dst_server_idx_itr == parent_index) {
                throw cRuntimeError("The source should have been filtered out now!");
            }

            EV << "Sending to server: " << dst_server_idx_itr << endl;
            new_msg->network_assisted_destination_idx_list.push_back(dst_server_idx_itr);
            new_msg->network_assisted_dst_gpu_idx_list.push_back(*dst_gpu_idx_itr);
            new_msg->network_assisted_flow_ids_list.push_back(flow_ids_list[flow_id_list_idx]);
        }

        auto jump_idx_found_itr = flow_id_map_to_lists_of_list_of_jumps_to_idxs.find(new_msg->flow_id);
        if (jump_idx_found_itr != flow_id_map_to_lists_of_list_of_jumps_to_idxs.end())
            throw cRuntimeError("How are we already having jumps and ports mapped to this flow id?");
        flow_id_map_to_lists_of_list_of_jumps_to_idxs.insert(
                std::make_pair(new_msg->flow_id, lists_of_lists_of_jump_idx));
        flow_id_map_to_lists_of_list_of_ports_to_dsts.insert(
                std::make_pair(new_msg->flow_id, lists_of_lists_of_ports_to_dsts));

        // check if we already have a socket to this destination
        int candidate_server_idx = (*new_msg->network_assisted_destination_idx_list.begin());
        int candidate_gpu_idx = (*new_msg->network_assisted_dst_gpu_idx_list.begin());

        new_msg->m_collective_type = m_collective_type;
        new_msg->m_collective_alg = BCAST_NETWORK_ASSISTED;

        init_socket(candidate_server_idx, candidate_gpu_idx);

        send_msg(candidate_server_idx,
                candidate_gpu_idx,
                new_msg,
                source_gpu_round_trip_idx,
                initiate_tree);

        source_gpu_round_trip_idx++;
        source_gpu_round_trip_idx %= num_gpus;
    }
}

void UDPLongAllgatherInitiatorApp::handleMessageWhenUp(cMessage *msg)
{
    EV << "UDPLongAllgatherInitiatorApp::handleMessageWhenUp" << endl;
//    EV << "msg is " << msg->getFullName() << endl;
    unsigned long storage_loc;
    AllgatherCustomCollectiveMsgInitiatorInfo* msg_info;
    int m_collective_alg;
    if (msg->isSelfMessage()) {
        ASSERT(msg == selfMsg);
        switch (selfMsg->getKind()) {
            case SEND:
                m_collective_alg = get_collective_alg();
                if (m_collective_alg == BCAST_SINGLE_SENDER) {
                    snd_allgather_single_sender();
                } else if (m_collective_alg == BCAST_BINOMIAL_TREE) {
                    snd_allgather_binomial_tree();
                } else if (m_collective_alg == BCAST_BINARY_TREE) {
                    snd_allgather_binary_tree();
                } else if (m_collective_alg == BCAST_NETWORK_ASSISTED) {
                    snd_allgather_network_assisted();
                } else if (m_collective_alg == BCAST_PARTIAL_MULTICAST) {
                    snd_allgather_partial_mcast();
                } else if (m_collective_alg == BCAST_RING){
                    snd_allgather_ring();
                } else if (m_collective_alg == BCAST_OPTIREDUCE){
                    snd_allgather_optireduce();
                } else if (m_collective_alg == BCAST_INA){
                    snd_allgather_ina();
                } else if (m_collective_alg == BCAST_RDMC){
                    snd_allgather_rdmc();
                } else
                    throw cRuntimeError("Unknown algorithm type!");

                if (inter_arrival_times.empty())
                    return;

                selfMsg->setKind(SEND);
                scheduleAt(get_inter_arrival_time(), selfMsg);

                break;

            default:
                delete msg;
                throw cRuntimeError("Invalid kind %d in self message", (int)selfMsg->getKind());
        }
    }
    else {
        EV << "Wrong message: " << msg->str() << endl;
        delete msg;
        throw cRuntimeError("Received a message that is unknown!!");
    }

    if (dst_server_idx.size() % num_flows_per_ml_query != 0) {
        std::cout << getFullPath() << endl;
        std::cout << dst_server_idx.size() << endl;
        std::cout << num_flows_per_ml_query << endl;
        std::cout << "How is dst_server_idx.size() % num_flows_per_ml_query != 0" << endl;
        throw cRuntimeError("How is st_server_idx.size() % num_flows_per_ml_query != 0");
    }
}

void UDPLongAllgatherInitiatorApp::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    throw cRuntimeError("UDPLongAllgatherInitiatorApp::socketDataArrived");
}

void UDPLongAllgatherInitiatorApp::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void UDPLongAllgatherInitiatorApp::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void UDPLongAllgatherInitiatorApp::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();

    char buf[100];
    getDisplayString().setTagArg("t", 0, buf);
}

void UDPLongAllgatherInitiatorApp::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    for (auto itr = destination_to_socket_mapper.begin(); itr != destination_to_socket_mapper.end(); itr++) {
        itr->second->close();
    }
    destination_to_socket_mapper.clear();
//    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void UDPLongAllgatherInitiatorApp::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    throw cRuntimeError("Operation creashed!");
}


