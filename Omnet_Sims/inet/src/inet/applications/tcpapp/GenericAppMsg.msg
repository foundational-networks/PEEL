//
// Copyright (C) 2004 Andras Varga
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


import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.chunk.Chunk;
import inet.common.packet.Packet;

namespace inet;

//
// Message class for generic request-reply style applications.
// The client sends a ~GenericAppMsg which contains the number of
// bytes the client wants the server to send back as reply.
//
// This way intelligence (behaviour specific to the modelled application,
// e.g. HTTP, SMB, database protocol) needs only to be present in
// the client, and the server model can be kept simple and dumb.
//
// @see ~TcpGenericServerApp, TCPGenericCliAppBase (C++ only)
//

message InnerList {
    unsigned int innerArray[];
}

message TwoDInnerList {
    InnerList innerArray[];
}

class GenericAppMsg extends FieldsChunk
{
    B expectedReplyLength;   // in bytes
    double replyDelay;       // reply after this many seconds
    bool serverClose;        // with TCP: if true, server should close the
                             // connection after sending the reply
    unsigned long requesterID = 0;
    bool is_micro_burst_flow = false;
    unsigned long query_id; // only for bursty flows
    simtime_t requested_time = 0;
	b total_flow_size = b(0);
	int responsibility_list[];
	int responsibility_gpu_list[];
	unsigned long responsibility_flow_id_list[];
//	unsigned int test[unsigned int []]
	InnerList ports_to_dest_idx[];
	InnerList jump_to_idx[];
	// there is a mapping between dst_idx_list and flow_ids_list
	// dst_idx_list[i] <---> flow_ids_list[i]
	int dst_idx_list[];
	unsigned long flow_ids_list[];
	string app_name = "";
	string app_full_path = "";
	int src_gpu_idx = -1;
	int dst_gpu_idx = -1;
	
	// for failure handling
	TwoDInnerList all_ports_to_dst[];
	TwoDInnerList all_jump_to_idx[];
	int current_tree_idx = -1;
	
	// for ring allreduce
    int ring_end_server_idx = -1;
    int ring_end_gpu_idx = -1;
    
    bool optireduce_in_reduction_phase = false;
    
    // for binary tree allreduce/allgather
    int tree_search_type = -1;
    int tree_traversal_dir = -1;
    
    int collective_type = -1;
    int collective_alg_type = -1;
    
    // ina
    int ina_leaf_aggregation_num = -1;
    int ina_spine_aggregation_num = -1;
    int ina_core_aggregation_num = -1;
    
    unsigned long seq_num = 0; // seq num starts from 1
    unsigned int collective_scale = 0;
    
    // rsbf
    unsigned int rsbf_seed_list[];
    string rsbf_bitstream_list[];
    unsigned long rsbf_removed_bytes = 0;
    
    int init_tree_pkt_idx = -1;

    int dst_tor_idx = -1;
    
    unsigned long partial_mcast_original_flow_size_bytes = 0;
    simtime_t controller_setup_finish_time = 0;
    
    int num_msgs_in_tor_group = -1;
    int tor_group_idx = -1;
    
    // CIDR
    string agg_cidr_group_id = "";
    unsigned long agg_cidr_group_member_count = 0;
    string core_cidr_group_id = "";
    unsigned long core_cidr_group_member_count = 0;
    
    bool in_src_sharding = false;	// partial mcast chunk data
    
    bool using_orca = false;
    
    bool using_elmo = false;
   	int useful_data_bytes = 0;	// only used for specific techniques such as elmo!
   	int elmo_overhead_pop_bytes = 0;	// how much to pop from overhead per multicast tree layer!
	
}



