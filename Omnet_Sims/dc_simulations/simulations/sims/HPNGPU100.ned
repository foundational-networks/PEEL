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

package dc_simulations.simulations.sims;

import inet.networklayer.common.InterfaceTable;
import inet.networklayer.arp.ipv4.GlobalArp;
import inet.networklayer.configurator.ipv4.Ipv4FlatNetworkConfigurator;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import dc_simulations.modules.LSSwitch.LSNode.LSEtherSwitch;
//import inet.node.inet.StandardHost;
import ned.DatarateChannel;
import inet.node.ethernet.EtherSwitch;
//import dc_simulations.modules.RandomHost.RandomStandardHost;
import dc_simulations.modules.Host.HostWithPortPadding.PortPaddingStandardHost;



//
// TODO auto-generated type
//
network HPNGPU100
{
    parameters:
        @display("bgb=643,481");
        int num_servers_per_segment;
        int num_planes_per_pod;
        int num_segments_per_pod;
        int num_aggs_per_plane;	// must be %15==0
        int num_pods;
        int num_bursty_apps = default(0);
        int num_mice_background_flow_apps_in_each_server = default(0);
        int num_ml_apps_in_each_server = default(0);
        // num_gpus is the number of links connected to every host, for HPN paper it's 16
        int num_gpus @prompt("Number of gpus per server") = default(1);
//        int num_gpus_per_edge = int(k/2) * num_gpus;
//        int num_aggs_per_pod = int(k/2);
		// every group of 15 aggs are connected to 8 cores in each plane (15-1 oversub)
		int num_cores_per_plane = int(num_aggs_per_plane / 15) * 8;
		int num_cores = int(num_planes_per_pod * num_cores_per_plane);
		int num_tors_per_segment = num_gpus;
		int num_tors_per_pod = num_segments_per_pod * num_tors_per_segment;
		int num_aggs_per_pod = num_planes_per_pod * num_aggs_per_plane;
		int total_num_aggs = num_pods * num_aggs_per_pod;
		int total_num_tors = num_pods * num_tors_per_pod;
		int total_num_servers = num_pods * num_segments_per_pod * num_servers_per_segment;
    types:
        channel C extends DatarateChannel
        {
            datarate = 100Gbps;
            delay = 100ns;
        }

    submodules:
        core[num_cores]: LSEtherSwitch {
            parameters:
            	@display("p=100,100,r,50");
            gates:
                ethg[num_pods * 15];
        }
        agg[total_num_aggs]: LSEtherSwitch {
            parameters:
                @display("p=100,200,r,50");
            gates:
                ethg[int(num_segments_per_pod * num_tors_per_segment / num_planes_per_pod) + 8]; // to tors + to 8 cores
        }
        edge[total_num_tors]: LSEtherSwitch {
            parameters:
                @display("p=100,300,r,50");
            gates:
                ethg[num_servers_per_segment + num_aggs_per_plane];
        }
        server[total_num_servers]: PortPaddingStandardHost {
            // k pods * k/2 edges per pod * k/2 servers per edge
            parameters:
            	@display("p=100,400,r,50");
            gates:
                ethg[num_gpus];
        }
        configurator: Ipv4NetworkConfigurator;
//    connections allowunconnected:
    connections:
        // iterate over the servers and connect them to edges
        for i=0..total_num_servers-1, for j=0.. (num_gpus)-1 {
            server[i].ethg[j] <--> C <--> edge[int(i/num_servers_per_segment)*num_tors_per_segment+j].ethg[int(i%num_servers_per_segment)];
        }
        // iterate over edges and connect them to aggs
        for i=0..total_num_tors-1, for j=0..num_aggs_per_plane-1 {
            edge[i].ethg[num_servers_per_segment + j] <--> C <--> agg[int(i/num_tors_per_pod) * num_aggs_per_pod + int(i % num_planes_per_pod) * num_aggs_per_plane + j].ethg[int(int(i % num_tors_per_pod)/num_planes_per_pod)];
        }
        // iterate over aggs and connect them to cores
        for i=0..total_num_aggs-1, for j=0..7 {
            agg[i].ethg[int(num_tors_per_pod / num_planes_per_pod) + j] <--> C <--> core[int(int(i % num_aggs_per_pod)/num_aggs_per_plane)*num_cores_per_plane + int((i % num_aggs_per_plane)/15)*8 + j].ethg[int(i/num_aggs_per_pod) * 15 + (i % 15)];
        }
}
