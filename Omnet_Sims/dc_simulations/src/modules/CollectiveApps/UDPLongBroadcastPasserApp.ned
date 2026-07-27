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
simple UDPLongBroadcastPasserApp like IApp
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
		
		bool is_message_splitted = default(false);
//        string collective_alg_type_str = default("");
        int splitted_chunk_size_bytes = default(-1);
        int splitted_subflow_num = default(-1);
        
        int mss = default(-1); // in bytes
        int packet_creation_batch_size = default(-1);
        
        int nvlink_speed_gbps = default(7200); // 900 GBps
        bool mcast_in_host = default(false);
        int num_gpus = default(1);
                
        double processing_delay_mean = default(0);
        double processing_delay_stddev = default(0);
        
        int mtu = default(1452);
        
        // optireduce
        int optireduce_incast_factor = default(-1);
        bool use_becca_optireduce = default(false);
        bool use_cidr_optireduce = default(false);
        
        // orca
        bool use_orca = default(false);
        
        // rsbf
        bool use_rsbf = default(false);
        
        bool use_fattree = default(false);
        
        // autoccl
        bool use_autoccl = default(false);
    	bool autoccl_allreduce_only = default(false);
        int num_flows_per_ml_query = default(-1);
        
        // fattree_k
        int k = default(-1);
        // num leafs in leaf spine
        int num_spines = default(-1);
        int num_aggs = default(-1);
        int num_servers = default(-1);	// num servers under each leaf!
        
                        
        @signal[flowEnded]();
        @signal[flowEndedQueryID]();
        @signal[flowPassed]();
        
        @statistic[flowEndedRequesterID](title="flow ended requester id"; source="flowEnded"; record=vector; interpolationmode=none);
        @statistic[flowEndQueryID](title="flow ended query id"; source="flowEndedQueryID"; record=vector; interpolationmode=none);
        @statistic[flowPassedRequesterID](title="flow passed flow/requester id"; source="flowPassed"; record=vector; interpolationmode=none);
        
        
    gates:
        input socketIn @labels(TcpCommand/up);
        output socketOut @labels(TcpCommand/down);
}

