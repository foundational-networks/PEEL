#include "inet/common/INETDefs.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "unordered_map"

namespace inet {

class AllgatherCustomCollectiveMsgInitiatorInfo
{
    public:
        unsigned long flow_size;
        unsigned long remaining_bytes;
        unsigned long query_id;
        unsigned long flow_id;
        unsigned long num_dst_servers_per_query;
        std::list<int> responsibility;
        std::list<int> responsibility_gpu;
        std::list<unsigned long> responsiblity_flow_ids;

        // for ring all reduce
        int ring_end_server_idx = -1;
        int ring_end_gpu_idx = -1;

        // for binary tree
        int tree_search_type = -1;
        int tree_traversal_dir = -1;

        int m_collective_type = -1;
        int m_collective_alg = -1;

        // for packet batching
        simtime_t initiated_time;
        UdpSocket* out_socket;
        int dest_port;
        int src_gpu, dst_gpu;
        L3Address destination;
        std::list<unsigned long> network_assisted_flow_ids_list;
        // in net assisted, there is no need to convey dst gpu in the msg
        std::list<int> network_assisted_destination_idx_list;
        std::list<int> network_assisted_dst_gpu_idx_list;
//        typedef std::list<unsigned int> jump_idx_list; // one jump_idx_list is for one switch
//        typedef std::list<unsigned int> port_list;  // one port_list is for one switch
//        std::list<port_list> list_of_ports_to_dsts;
//        std::list<jump_idx_list> list_of_jumps_to_idxs;

        // optireduce
        int optireduce_dst_server = -1;
        int optireduce_dst_gpu = -1;
        // for optireduce: true by default since it starts with reduction phase
        bool optireduce_in_reduction_phase = true;

        // rsbf
        unsigned long rsbf_original_flow_size = 0;

        // treeInit: Torch / Cepheus
        B tree_init_overhead = B(0);

        int init_tree_idx = -1;

        double extra_delay = 0;

        unsigned long partial_mcast_original_flow_size_bytes = 0;

        // added for core-only programmability
        int candidate_server_idx = -1;
        int candidate_gpu_idx = -1;
        int num_msgs_in_tor_group = -1;
        int tor_group_idx = -1;
        simtime_t controller_finish_time = 0;

        // CIDR
        std::string agg_cidr_group_id = "";
        unsigned long agg_cidr_group_member_count = 0;
        std::string core_cidr_group_id = "";
        unsigned long core_cidr_group_member_count = 0;

        int dst_tor_idx = -1;

        bool in_src_sharding = false;

        bool using_orca = false;

        bool using_elmo = false;

        // ina
        int leaf_aggregation_num = -1;
        int spine_aggregation_num = -1;
        int core_aggregation_num = -1;

        AllgatherCustomCollectiveMsgInitiatorInfo(unsigned long flow_id, unsigned long flow_size, unsigned long query_id, unsigned long num_dst_servers_per_query) {
            this->flow_size = flow_size;
            this->query_id = query_id;
            this->flow_id = flow_id;
            this->num_dst_servers_per_query = num_dst_servers_per_query;
            this->remaining_bytes = 0;
            this->src_gpu = -1;
            this->dst_gpu = -1;
        }

        void check_metrics() {
            EV << "Info: num_dst_servers_per_query: " << this->num_dst_servers_per_query <<
                    ", responsibility.size: " << this->responsibility.size() <<
                    ", responsiblity_flow_ids.size: " << this->responsiblity_flow_ids.size() <<
                    ", network_assisted_flow_ids_list.size: " << this->network_assisted_flow_ids_list.size() <<
                    ", network_assisted_destination_idx_list.size: " << this->network_assisted_destination_idx_list.size() <<
                    ", m_collective_type: " << this->m_collective_type <<
                    ", m_collective_type: " << this->m_collective_alg << endl;
            if (this->responsibility.size() > this->num_dst_servers_per_query+1)
                throw cRuntimeError("responsibility.size() > num_dst_servers_per_query+1");
            if (this->responsiblity_flow_ids.size() > this->num_dst_servers_per_query+1)
                throw cRuntimeError("responsiblity_flow_ids.size() > num_dst_servers_per_query+1");
            if (this->responsibility.size() != this->responsiblity_flow_ids.size())
                throw cRuntimeError("responsibility.size() != responsiblity_flow_ids.size()");
            if (this->responsibility.size() != this->responsibility_gpu.size())
                throw cRuntimeError("this->responsibility.size() != this->responsibility_gpu.size()");
            if (this->network_assisted_flow_ids_list.size() > this->num_dst_servers_per_query)
                throw cRuntimeError("network_assisted_flow_ids_list.size() > num_dst_servers_per_query");
            if (this->network_assisted_destination_idx_list.size() > this->num_dst_servers_per_query)
                throw cRuntimeError("network_assisted_destination_idx_list.size() > num_dst_servers_per_query");
            if (this->m_collective_type <= 0)
                throw cRuntimeError("Invalid collective_type");
            if (this->m_collective_alg <= 0)
                throw cRuntimeError("Invalid collective algorithm");
        }
};

}
