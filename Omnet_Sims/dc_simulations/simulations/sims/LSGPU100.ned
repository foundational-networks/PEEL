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
network LSGPU100
{
    parameters:
        @display("bgb=643,481");
        int num_spines @prompt("Number of spines");
        int num_aggs @prompt("Number of aggregation level switches");
        int num_servers @prompt("Number of servers per switch");
        int num_receiver_servers = num_aggs  * num_servers;
        int num_servers_client_gets_connected_to @prompt("Random Number for choosing the servers to get connected to") = default(-1);
        int num_bursty_apps = default(0);
        int num_mice_background_flow_apps_in_each_server = default(0);
        int num_ml_apps_in_each_server = default(0);
        int num_gpus @prompt("Number of gpus per server") = default(1);
        int num_gpus_per_agg = num_servers * num_gpus;

        
    types:
        channel MChannel extends DatarateChannel
        {
            datarate = 100Gbps;
            delay = 100ns;
        }

    submodules:
        spine[num_spines]: LSEtherSwitch {
            parameters:
            	@display("p=100,200,r,50");
            gates:
                ethg[num_aggs];
        }
        agg[num_aggs]: LSEtherSwitch {
            parameters:
                @display("p=100,314,r,50");
            gates:
                ethg[num_gpus_per_agg+num_spines];
        }
        server[num_receiver_servers]: PortPaddingStandardHost {
            parameters:
            	@display("p=100,400,r,50");
            gates:
                ethg[num_gpus];
        }
//        server[num_receiver_servers]: PortPaddingStandardHost if !test {
//            parameters:
//            	@display("p=100,400,r,50");
//        }
        configurator: Ipv4NetworkConfigurator;
//    connections allowunconnected:
    connections:
        for i=0..num_spines-1, for j=0..num_aggs-1 {
            spine[i].ethg[j] <--> MChannel <--> agg[j].ethg[(num_gpus_per_agg)+i];
        }
        for j=0..(num_receiver_servers)-1, for k=0.. (num_gpus)-1 {
            	server[j].ethg[k] <--> MChannel <--> agg[int(j/num_servers)].ethg[(int(j%num_servers)*num_gpus + k)];
        }
}
