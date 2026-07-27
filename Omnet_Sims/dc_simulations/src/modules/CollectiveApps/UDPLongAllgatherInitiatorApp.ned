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

package dc_simulations.modules.CollectiveApps;

import inet.applications.contract.IApp;

//
// Client for a generic request-response style protocol over TCP.
// May be used as a rough model of HTTP or FTP users.
// Compatible with both IPv4 (~Ipv4) and IPv6 (~Ipv6).
//
// The model communicates with the server in sessions. During a session,
// the client opens a single TCP connection to the server, sends several
// requests (always waiting for the complete reply to arrive before
// sending a new request), and closes the connection.
//
// The server app should be ~TcpGenericServerApp; the model sends ~GenericAppMsg
// messages.
//
// Example settings:
//
// FTP:
// <pre>
//    numRequestsPerSession = exponential(3)
//    requestLength = 1B*int(truncnormal(20,5))
//    replyLength = 1B*int(exponential(1000000))
// </pre>
//
// Note that this module doesn't open separate TCP connections for commands
// and data transfer as the FTP protocol.
//
// HTTP:
// <pre>
//    numRequestsPerSession = 1 <i>(HTTP 1.0)</i>
//    numRequestsPerSession = exponential(5) <i>(HTTP 1.1, with keepalive)</i>
//    requestLength = 1B*int(truncnormal(350,20))
//    replyLength = 1B*int(exponential(2000))
// </pre>
//
// Note that since most web pages contain images and may contain frames,
// applets etc, possibly from various servers, and browsers usually download
// these items in parallel to the main HTML document, this module cannot
// serve as a realistic web client.
//
// Also, with HTTP 1.0 it is the server that closes the connection after
// sending the response, while in this model it is the client.
//
// @see ~TcpGenericServerApp, ~GenericAppMsg, ~TelnetApp
//
simple UDPLongAllgatherInitiatorApp like IApp
{
    parameters:
        string localAddress = default(""); // may be left empty ("")
        int localPort = default(-1); // port number to listen on
        string connectAddress = default("");  // server address (may be symbolic)
        int connectPort = default(1000); // port number to connect to
        volatile int msgLength @unit(B) = default(200B); // length of a request
        int timeToLive = default(-1); // if not -1, set the TTL (IPv4) or Hop Limit (IPv6) field of sent packets to this value

        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);    // extra time after lifecycle stop operation finished
        double stopOperationTimeout @unit(s) = default(2s);    // timeout value for lifecycle stop operation

		int dscp = default(-1); // if not -1, set the DSCP field (on IPv4/IPv6) of sent packets to this value
        string multicastInterface = default("");  // if not empty, set the multicast output interface option on the socket (interface name expected)
        bool receiveBroadcast = default(false); // if true, makes the socket receive broadcast packets
        bool joinLocalMulticastGroups = default(false); // if true, makes the socket receive packets from all multicast groups set on local interfaces
		
        string distibutions_base_root = default("./");
        string collective_alg_type_str = default("");	// setting the collective alg
        string collective_type_str = default("");	// setting the collective
        string application_category = default("");
        int num_flows_per_ml_query = default(-1);
        volatile int flow_size @unit(B);
        double flow_size_multiplier = default(1);
        double inter_arrival_time_multiplier = default(1);        
        double frag_ratio = default(-1);
        
        int mss = default(-1); // in bytes
        int packet_creation_batch_size = default(-1);
        
        int num_gpus = default(1);
        bool mcast_in_host = default(false);
                
        double initiation_delay_mean = default(0);
        double initiation_delay_stddev = default(0);
        
        // failure for ace
        int failure_perc = default(-1);
        int lookahead = default(-1);
        
        // should we reduce in source before sending with ace
        bool ace_apply_src_reduce = default(false);
        
        // for the case of combinational collectives
        bool using_ace = default(false);
        
        // LB
        string lb_granularity_str = default("flow");	// flow, packet
        int lb_tree_num = default(1);
        string lb_tree_select_type_str = default("random"); // random, roundrobin
        
        int splitted_subflow_num = default(-1);
        
        int num_servers = default(-1);	// num servers under each leaf!
        
        // optireduce
        int optireduce_incast_factor = default(-1);
        bool use_becca_optireduce = default(false);
        
        // orca
        bool use_orca = default(false);
        double orca_controller_delay_mean = default(0);
        double orca_controller_delay_stddev = default(0);
        
        // rsbf
        bool use_rsbf = default(false);
        string rsbf_bytes = default("0,0,0");
    	int rsbf_hash_num = default(0);
        
        // elmo
        bool use_elmo = default(false);
        
        bool use_fattree = default(false);
        int k = default(-1);
        
        bool use_hpn = default(false);
        
        // num leafs in leaf spine
        int num_spines = default(-1);
        int num_aggs = default(-1);
                
        // use tree initiation/termination packet
        bool initiate_tree = default(false);
        int tree_id_overhead_bytes = default(8);
        
        // Cepheus
        bool use_cepheus = default(false);
        
        int nvlink_speed_gbps = default(7200); // 900 GBps
        
        bool partial_mcast_chunk_data = default(false);
        bool partial_mcast_chunk_tors = default(false);
        
        bool partial_mcast_programmable_cores = default(false);
        bool partial_mcast_divide_tors_among_cores = default(false);
        
        // CIDR
        bool partial_mcast_use_cidr_agg = default(false);
        bool partial_mcast_use_cidr_core = default(false);
        string cidr_type_str = default("full_tree");

        double random_fail_prob = default(-1);
        
        // autoccl
        bool use_autoccl = default(false);
    	bool autoccl_allreduce_only = default(false);
        
        @signal[requestSent]();
        @signal[flowNum]();
        @signal[replyLengths]();
        @signal[flowStartedQueryID]();
        @signal[controllerDelay]();
        @signal[flowStartedCollectiveType]();
        @signal[flowStartedCollectiveAlg]();
        @signal[autocclChosenCollectiveAlg]();
        @signal[autocclChosenCollective]();
        
        @statistic[requestSentRequesterID](title="request sent requester id"; source="requestSent"; record=vector; interpolationmode=none);
        @statistic[flowNumPerQuery](title="Number of flows per ml query"; source="flowNum"; record=vector; interpolationmode=none);
        @statistic[replyLengthAsked](title="reply length of the packets"; source="replyLengths"; record=vector; interpolationmode=none);
        @statistic[flowStartQueryID](title="flow started query id"; source="flowStartedQueryID"; record=vector; interpolationmode=none);
        @statistic[flowStartCollectiveType](title="flow started collective type"; source="flowStartedCollectiveType"; record=vector; interpolationmode=none);
        @statistic[flowStartCollectiveAlg](title="flow started collective algorithm"; source="flowStartedCollectiveAlg"; record=vector; interpolationmode=none);
        @statistic[controllerDelay](title="Orca's controller delay"; source="controllerDelay"; record=vector; interpolationmode=none);
        @statistic[autocclChosenCollectiveAlg](title="AutoCCL's chosen collective algorithms"; source="autocclChosenCollectiveAlg"; record=vector; interpolationmode=none);
        @statistic[autocclChosenCollective](title="AutoCCL's chosen collective"; source="autocclChosenCollective"; record=vector; interpolationmode=none);
    gates:
        input socketIn @labels(TcpCommand/up);
        output socketOut @labels(TcpCommand/down);
}

