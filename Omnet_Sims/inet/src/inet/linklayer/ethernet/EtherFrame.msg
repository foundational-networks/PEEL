//
// Copyright (C) 2003 Andras Varga; CTIE, Monash University, Australia
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
import inet.common.packet.chunk.Chunk;
import inet.common.packet.Packet;
import inet.linklayer.common.EtherType;
import inet.linklayer.common.FcsMode;
import inet.linklayer.common.Ieee802Ctrl;
import inet.linklayer.common.MacAddress;
import inet.linklayer.ieee8021q.Ieee8021qHeader;
import inet.applications.tcpapp.GenericAppMsg;

cplusplus {{
#include "inet/linklayer/ethernet/Ethernet.h"
}}

namespace inet;

enum EthernetControlOpCode
{
    ETHERNET_CONTROL_PAUSE = 1;
}

enum SapCode;

//
// Ethernet MAC header
//
class EthernetMacHeader extends FieldsChunk
{
    chunkLength = B(ETHER_MAC_HEADER_BYTES);
    MacAddress dest;
    MacAddress src;
    Ieee8021qHeader *cTag @owned;
    Ieee8021qHeader *sTag @owned;
    int typeOrLength;
    int hop_count = 0;
    // -1 for prio_queue_id means that we should use default queue
    int prio_queue_idx = -1;
    // For Vertigo
    bool isFB = false;
    bool allow_same_input_output = false;
    int original_interface_id = -1;
    
    // For PABO
    int bouncedDistance;	// passBackHop
    int maxBouncedDistance;		// maxPassBackHop
    int bouncedHop;	// passBackNum
    int totalHopNum;
    
    // For further measurements
    bool is_bursty = false;
    b payload_length = b(0);
    b total_length = b(0); // total lenght that the payload is part of
    b offset = b(0);
    
    // For V2
    bool is_v2_dropped_packet_header = false;
    uint16_t queue_occupancy = -1;
    
    // swift
    simtime_t time_packet_received_at_nic;  // this is added to every packet when it lands in a host
    simtime_t local_nic_rx_delay; // this is the time packet quits the nic rcv queue to move up to other layers
    simtime_t remote_queueing_time;  // This is the time ack packet is sent - the time data packet is received at host
    
    
    // fabric_delay
    simtime_t fabric_delay_time_sent_from_source;
    
    // bolt
    simtime_t time_packet_sent_from_src;
    bool is_deflected = false;
    
    // network assisted routing
    InnerList ports_to_dest_idx[];
    InnerList jump_to_idx[];
    int dst_idx_list[];
    unsigned int start_from_idx;
    
	// for failure handling
	TwoDInnerList all_ports_to_dst[];
	TwoDInnerList all_jump_to_idx[];
	int current_tree_idx = -1;
        
    // ina
    int ina_leaf_aggregation_num = -1;
    int ina_spine_aggregation_num = -1;
    int ina_core_aggregation_num = -1;
    
    int tree_direction = -1;
    unsigned long query_id = 0;
    unsigned long flow_id = 0;
    unsigned int collective_scale = 0;
    
    int collective_alg = -1;
    
    // pfc
    int pfc_signal = -1;
    
    // ecn_mark
    bool is_ecn_marked = false;
    
    // dcqcn cnp
    unsigned long cnp_flow_id;
    int cnp_src_gpu_id;
    
    int src_gpu_idx = -1;
    
    unsigned long seq_num = 0;
    
    bool orca_pkt_seen_agent = false;
    int orca_agent_port_index = -1;
    
    // rsbf
    unsigned int rsbf_seed_list[];
    string rsbf_bitstream_list[];
    bool rsbf_redundant_send = false;

    int dst_tor_idx = -1;
    
    Packet* partial_mcast_core_pkt[];
    bool multicast_pkt_in_programmable_core = false;
    
    // CIDR
    string agg_cidr_group_id = "";
    unsigned long agg_cidr_group_member_count = 0;
    bool is_redundant_cidr_pkt = false;
    Packet* CIDR_agg_pkt[];
    bool multicast_pkt_in_CIDR_agg = false;
    
    string core_cidr_group_id = "";
    unsigned long core_cidr_group_member_count = 0;
    Packet* CIDR_core_pkt[];
    bool multicast_pkt_in_CIDR_core = false;
    
    // drop recovery (notif/deflect on drop)
    Packet* original_pkt = nullptr;
    bool is_drop_notif = false;
    unsigned int input_ports_passed[];
    string original_pkt_name = "";
    simtime_t recovery_time = 0; // only used by ace to know when it can use a tree again!
    
    // failure and recovery
	MacAddress affected_addresses[];
	
	// swift send times
	simtime_t swift_send_time = 0;
	long cidr_base_flow_id = -1;	// this is the flow id that is carried by the first packet when doing cidr
	int cidr_base_src_gpu_idx = -1;	// this is the flow id that is carried by the first packet when doing cidr
    
    bool in_src_sharding = false;	// partial mcast chunk data
    
    bool using_orca = false;
    
    bool using_elmo = false;

}

cplusplus {{
    inline bool isEth2Header(const EthernetMacHeader& hdr)  { return isEth2Type(hdr.getTypeOrLength()); }  // typeOrLength is type
    inline bool isIeee8023Header(const EthernetMacHeader& hdr)  { return isIeee8023Length(hdr.getTypeOrLength()); }  // typeOrLength is length
}}

//
//
// Ethernet control frames used by the PAUSE protocol
//
class EthernetControlFrame extends FieldsChunk
{
//    EthernetHeader.etherType = ETHERTYPE_FLOW_CONTROL;
    int opCode = -1;
}

//
// Ethernet frame used by the PAUSE protocol
//
class EthernetPauseFrame extends EthernetControlFrame
{
    chunkLength = ETHER_PAUSE_COMMAND_BYTES;
    opCode = ETHERNET_CONTROL_PAUSE;
    int pauseTime; // in 512 bit-time units
}

class EthernetPadding extends FieldsChunk
{
}

class EthernetFcs extends FieldsChunk
{
    chunkLength = ETHER_FCS_BYTES;
    uint32_t fcs;
    FcsMode fcsMode = FCS_MODE_UNDEFINED;
}

cplusplus(EthernetFcs) {{
    EthernetFcs(FcsMode _fcsMode) : EthernetFcs() { fcsMode = _fcsMode; }
}}
