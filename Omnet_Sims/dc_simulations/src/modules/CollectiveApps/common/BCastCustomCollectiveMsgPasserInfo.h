#include "inet/common/INETDefs.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

namespace inet {

class BCastCustomCollectiveMsgPasserInfo
{
    public:
        unsigned long msg_size;
        unsigned long total_flow_size = 0;
        unsigned long query_id;
        unsigned long flow_id;
        std::list<int> responsibility;
        std::list<int> responsibility_gpu;
        std::list<unsigned long> responsiblity_flow_ids;
        bool should_pass;
        bool is_new_flow;
        bool original_flow_terminated;
        unsigned long num_dsts = 0;

        // for packet batching
        simtime_t initiated_time;
        UdpSocket* out_socket;
        int dest_port = -1;
        int src_gpu, dst_gpu;
        L3Address destination;

        // for all reduce ring
        int ring_end_server_idx = -1;
        int ring_end_gpu_idx = -1;

        // for binary tree
        int tree_search_type = -1;
        int tree_traversal_dir = -1;

        int m_collective_type = -1;
        int m_collective_alg = -1;

        // optireduce
        int optireduce_dst_server = -1;
        int optireduce_dst_gpu = -1;
        // for optireduce: true by default since it starts with reduction phase
        bool optireduce_in_reduction_phase = true;
        std::string initiator_app_path;

        //optireduce becca
        std::list<unsigned long> network_assisted_flow_ids_list;
        // in net assisted, there is no need to convey dst gpu in the msg
        std::list<int> network_assisted_destination_idx_list;
        std::list<int> network_assisted_dst_gpu_idx_list;
        std::list<std::list<std::list<unsigned int>>> lists_of_lists_of_jumps;
        std::list<std::list<std::list<unsigned int>>> lists_of_lists_of_ports;

        // CIDR
        std::string agg_cidr_group_id = "";
        unsigned long agg_cidr_group_member_count = 0;
        std::string core_cidr_group_id = "";
        unsigned long core_cidr_group_member_count = 0;
        int dst_tor_idx = -1;

        BCastCustomCollectiveMsgPasserInfo() {
            this->total_flow_size = 0;
            this->flow_id = 0;
            this->msg_size = 0;
            this->query_id = 0;
            this->should_pass = false;
            this->is_new_flow = false;
            this->original_flow_terminated = false;
            this->src_gpu = -1;
            this->dst_gpu = -1;
        }

        BCastCustomCollectiveMsgPasserInfo(unsigned long flow_id, unsigned long msg_size,
                unsigned long total_flow_size, unsigned long query_id, int src_gpu) {
            this->msg_size = msg_size;
            this->total_flow_size = total_flow_size;
            this->query_id = query_id;
            this->flow_id = flow_id;
            this->should_pass = false;
            this->is_new_flow = false;
            this->original_flow_terminated = false;
            this->src_gpu = src_gpu;
            this->dst_gpu = -1;
        }

        void verify_info() {
            if (this->total_flow_size == 0 || this->flow_id == 0 || this->msg_size == 0 ||
                    this->query_id == 0) {
                std::cout << "total_flow_size, flow_id, msg_size, "
                        "query_id, ring_end_server_idx, ring_end_gpu_idx" << endl;
                std::cout << this->total_flow_size << ", " <<  this->flow_id << ", " <<
                        this->msg_size << ", " << this->query_id << this->ring_end_server_idx <<
                        this->ring_end_gpu_idx << endl;
                throw cRuntimeError("BCastCustomCollectiveMsgPasserInfo not verified!");
            }
        }

        void check_metrics() {
            EV << "Info: num_dsts: " << this->num_dsts <<
                    ", responsibility.size: " << this->responsibility.size() <<
                    ", responsiblity_flow_ids.size: " << this->responsiblity_flow_ids.size() <<
                    ", ring_end_server_idx: " << this->ring_end_server_idx <<
                    ", ring_end_gpu_idx: " << this->ring_end_gpu_idx <<
                    ", m_collective_type: " << this->m_collective_type <<
                    ", m_collective_alg: " << this->m_collective_alg <<
                    ", optireduce_in_reduction_phase: " << this->optireduce_in_reduction_phase << endl;
            if (this->responsibility.size() > this->num_dsts+1)
                throw cRuntimeError("responsibility.size() > num_dsts+1");
//            if (this->responsiblity_flow_ids.size() > this->num_dsts+1)
//                throw cRuntimeError("responsiblity_flow_ids.size() > num_dsts+1");
            if (this->responsibility_gpu.size() > this->num_dsts+1)
                throw cRuntimeError("responsibility_gpu.size() > this->num_dsts+1");
            if (this->m_collective_type <= 0)
                throw cRuntimeError("this->m_collective_type <= 0");
            if (this->m_collective_alg <= 0)
                throw cRuntimeError("this->m_collective_alg <= 0");
        }
};

}
