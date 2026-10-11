#include "inet/common/INETDefs.h"
#include <algorithm>
#include <vector>
#include <unordered_map>
#include "inet/transportlayer/contract/udp/UdpSocket.h"

namespace inet {

class RDMCScheduleInfo
{
    public:
        std::vector<int> dst_servers_list;
        std::vector<int> dst_gpus_list;
        std::vector<unsigned long> flow_ids_list;

        bool hierarchical;
        std::unordered_map<int, std::vector<int>> intra_server_gpus;

        int m_collective_type;

        std::vector<bool> ready_to_receive;
        std::vector<bool> received_block;

        size_t send_step;
        size_t receive_step;

        bool sending;

        size_t num_nodes;
        size_t log2_num_nodes;
        size_t rdmc_node_id;
        size_t num_blocks;

        unsigned long flow_size;
        unsigned long block_flow_size;

        struct Transfer {
            int target_node;
            int target_block_num;

            Transfer(int tnode, int tblnum) {
                target_node = tnode;
                target_block_num = tblnum;
            }
        };

        Transfer *send_schedule;
        Transfer *receive_schedule;

        RDMCScheduleInfo(size_t total_nodes, size_t total_blocks, unsigned long fsize, size_t nid, size_t ctype) {
            EV << "In RDMCScheduleInfo Constructor" << endl;
            num_nodes = total_nodes;
            num_blocks = total_blocks;
            log2_num_nodes = log2(num_nodes);
            EV<<"num_nodes: " << num_nodes;
            EV<<" log2_num_nodes: " << log2_num_nodes;
            EV<<" num_blocks: " << num_blocks << endl;
            rdmc_node_id = nid;
            block_flow_size = fsize / num_blocks;
            flow_size = fsize;
            m_collective_type = ctype;
            send_step = 0;
            receive_step = 0;
            sending = false;
            send_schedule = nullptr;
            receive_schedule = nullptr;
            dst_servers_list.resize(num_nodes);
            dst_gpus_list.resize(num_nodes);
            flow_ids_list.resize(num_nodes);
            hierarchical = false;


            // at initialization, we set ready to receive to true for each nodes first active/send dimension.
            ready_to_receive.resize(num_nodes, false);
            received_block.resize(num_blocks, false);

            // load the first block to receive
            load_first_block();
        }

        ~RDMCScheduleInfo() {
            delete send_schedule;
            delete receive_schedule;
        }

        size_t get_total_steps() {
            return log2_num_nodes + num_blocks - 1;
        }

        simtime_t get_intra_host_pass_delay(int nvlink_speed_gbps) {
            double nvlink_speed_bps = nvlink_speed_gbps * std::pow(10, 9);
            int total_intra_host_steps = log2(intra_server_gpus[dst_servers_list[rdmc_node_id]].size()) + num_blocks - 1;
            simtime_t nvlink_delay = ((block_flow_size * 8.0) / (nvlink_speed_bps)) * total_intra_host_steps;
            return nvlink_delay;
        }

        Transfer* get_outgoing_transfer(int sender, size_t sender_send_step) {
            EV << "in get_outgoing_transfer" << endl;
            size_t total_steps = get_total_steps();
            size_t rank_mask = (~((size_t)0)) >> (sizeof(size_t) * CHAR_BIT - log2_num_nodes);
            size_t step_index = sender_send_step % log2_num_nodes;
            uint32_t neighbor = sender ^ (1 << step_index);

            if (sender_send_step >= total_steps || neighbor == 0) {
                return nullptr;
            }

            size_t rotated_rank = ((neighbor | (neighbor << log2_num_nodes)) >> step_index) & rank_mask;

            if ((rotated_rank & 1) == 0) {
                if (sender_send_step < log2_num_nodes) return nullptr;
                return new Transfer(neighbor, sender_send_step - log2_num_nodes);
            }

            for (unsigned int index = 1; index < log2_num_nodes; index++) {
                if (rotated_rank & (1 << index)) {
                    if(sender_send_step + index < log2_num_nodes) return nullptr;
                    size_t block_number = std::min(sender_send_step + index - log2_num_nodes, num_blocks - 1);
                    return new Transfer(neighbor, block_number);
                }
            }

            size_t block_number = std::min(sender_send_step, num_blocks - 1);
            Transfer* tmptrans = new Transfer(neighbor, block_number);
            return tmptrans;
        }

        void load_outgoing_schedule() {
            EV << "in load_outgoing_schedule" << endl;
            send_schedule = get_outgoing_transfer(rdmc_node_id, send_step);

            EV << "node " << rdmc_node_id << " - send step: " << send_step;
            if (send_schedule != nullptr) EV << " , send to: " << send_schedule->target_node << " , block num: " << send_schedule->target_block_num << endl;
            else EV << " no send" << endl;
        }

        void load_incoming_schedule () {
            EV << "in load_incoming_schedule" << endl;
            size_t step_index = receive_step % log2_num_nodes;
            int neighbor = rdmc_node_id ^ (1 << step_index);

            receive_schedule = get_outgoing_transfer(neighbor, receive_step);

            EV << "node " << rdmc_node_id << " - receive step: " << receive_step;
            if (receive_schedule != nullptr) {
                receive_schedule->target_node = neighbor;
                EV << " , receive from: " << receive_schedule->target_node << " , block num: " << receive_schedule->target_block_num << endl;
            }
            else EV << " no receive" << endl;
        }

        void load_first_block() {
            EV << "in load_first_block" << endl;
            load_incoming_schedule();
            while(receive_schedule == nullptr && receive_step < get_total_steps()) {
                receive_step++;
                load_incoming_schedule();
            }
        }
};

}
