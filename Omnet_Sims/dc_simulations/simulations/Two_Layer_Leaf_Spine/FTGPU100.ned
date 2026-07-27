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

package dc_simulations.simulations.Two_Layer_Leaf_Spine;

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
network FTGPU100
{
    parameters:
        @display("bgb=643,481");
        int k;
        int num_servers @prompt("Number of servers per edge switch");
        int num_bursty_apps = default(0);
        int num_mice_background_flow_apps_in_each_server = default(0);
        int num_ml_apps_in_each_server = default(0);
        int num_gpus @prompt("Number of gpus per server") = default(1);
        int num_gpus_per_edge = num_servers * num_gpus;
        int num_aggs_per_pod = int(k/2);
    types:
        channel C extends DatarateChannel
        {
            datarate = 100Gbps;
            delay = 100ns;
        }

    submodules:
        core[int(k/2)*int(k/2)]: LSEtherSwitch {
            parameters:
            	@display("p=100,100,r,50");
            gates:
                ethg[k];
        }
        agg[k * int(k/2)]: LSEtherSwitch {
            parameters:
                @display("p=100,200,r,50");
            gates:
                ethg[k];
        }
        edge[k * int(k/2)]: LSEtherSwitch {
            parameters:
                @display("p=100,300,r,50");
            gates:
                ethg[num_gpus_per_edge + num_aggs_per_pod];
        }
        server[k * int(k/2) * num_servers]: PortPaddingStandardHost {
            // k pods * k/2 edges per pod * k/2 servers per edge
            parameters:
            	@display("p=100,400,r,50");
            gates:
                ethg[num_gpus];
        }
        configurator: Ipv4NetworkConfigurator;
//    connections allowunconnected:
    connections:
        // iterate over the servers
        for i=0..(k * int(k/2) * num_servers)-1, for j=0.. (num_gpus)-1 {
            server[i].ethg[j] <--> C <--> edge[int(i/num_servers)].ethg[int(i%num_servers)*num_gpus + j];
        }
        // iterate over edges
        for i=0..(int(k/2)*k)-1, for j=(num_servers*num_gpus)..(num_servers*num_gpus)+int(k/2)-1 {
            edge[i].ethg[j] <--> C <--> agg[(int(i/(k/2)) * int(k/2)) + (j - num_servers*num_gpus)].ethg[i%(int(k/2))];
        }
        // iterate over aggs
        for i=0..(int(k/2)*k)-1, for j=int(k/2)..k-1 {
            agg[i].ethg[j] <--> C <--> core[(i%int(k/2)) * int(k/2) + (j - int(k/2))].ethg[int(i / (k / 2))];
        }
}
