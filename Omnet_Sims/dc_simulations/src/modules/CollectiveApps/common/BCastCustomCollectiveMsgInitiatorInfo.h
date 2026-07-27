#include "inet/common/INETDefs.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

namespace inet {

class BCastCustomCollectiveMsgInitiatorInfo
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

        // for packet batching
        simtime_t initiated_time;
        UdpSocket* out_socket;
        int dest_port;
        int src_gpu, dst_gpu;
        L3Address destination;
        std::list<unsigned long> network_assisted_flow_ids_list;
        std::list<unsigned long> network_assisted_dst_server_idx_list;

        BCastCustomCollectiveMsgInitiatorInfo(unsigned long flow_id, unsigned long flow_size,
                unsigned long query_id, unsigned long num_dst_servers_per_query) {
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
                    ", network_assisted_dst_server_idx_list.size: " << this->network_assisted_dst_server_idx_list.size() << endl;
            if (this->responsibility.size() > this->num_dst_servers_per_query)
                throw cRuntimeError("responsibility.size() > num_dst_servers_per_query");
            if (this->responsiblity_flow_ids.size() > this->num_dst_servers_per_query)
                throw cRuntimeError("responsiblity_flow_ids.size() > num_dst_servers_per_query");
            if (this->responsibility.size() != this->responsiblity_flow_ids.size())
                throw cRuntimeError("responsibility.size() != responsiblity_flow_ids.size()");
            if (this->responsibility.size() != this->responsibility_gpu.size())
                throw cRuntimeError("this->responsibility.size() != this->responsibility_gpu.size()");
            if (this->network_assisted_flow_ids_list.size() > this->num_dst_servers_per_query)
                throw cRuntimeError("network_assisted_flow_ids_list.size() > num_dst_servers_per_query");
            if (this->network_assisted_dst_server_idx_list.size() > this->num_dst_servers_per_query)
                throw cRuntimeError("network_assisted_dst_server_idx_list.size() > num_dst_servers_per_query");
        }
};

}
