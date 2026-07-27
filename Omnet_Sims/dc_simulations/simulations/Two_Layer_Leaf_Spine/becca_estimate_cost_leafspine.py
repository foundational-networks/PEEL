import imp


import math

# for Binary Tree
class TreeNode:
    def __init__(self, id):
        self.id = id
        self.children = []

def create_tree(left, right):
    mid = (left + right) // 2
    mid_node = TreeNode(mid)
    if (left < mid):
        mid_node.children.append(create_tree(left, mid-1))
    if (right > mid):
        mid_node.children.append(create_tree(mid+1, right))
    return mid_node

def dfs(parent, path_length, nvlink_path_length, n_servers, n_gpus):
    parent_host_idx = parent.id // n_gpus
    parent_leaf_idx = parent.id // (n_gpus * n_servers)
    for child in parent.children:
        child_host_idx = child.id // n_gpus
        child_leaf_idx = child.id // (n_gpus * n_servers)
        if (child_host_idx == parent_host_idx):
            nvlink_path_length += 2    # nv_link
        else:
            # if the parent and child are on the same host, no bw cost inserts into the network
            if (child_leaf_idx == parent_leaf_idx):
                # communication is under a leaf
                path_length += 2
            else:
                # communication happens through spines
                path_length += 4
        path_length, nvlink_path_length = dfs(child, path_length, nvlink_path_length, n_servers, n_gpus)
    return path_length, nvlink_path_length

class LeafSpine:
    def __init__(self, n_spines, n_leafs, n_servers, n_gpus):
        self.n_spines = n_spines
        self.n_leafs = n_leafs
        self.n_servers = n_servers  # num servers under each leaf
        self.n_gpus = n_gpus        # num gpus in each server
        print(f'Topology info: {self.n_spines} spines, {self.n_leafs} leafs, {self.n_servers} servers per leaf, {self.n_gpus} gpus per server\n')

    def get_becca_cost(self, n_nodes, collective=None):
        if collective is None:
            raise Exception("Collective is None")
        if (n_nodes % self.n_gpus != 0):
            raise Exception("n_nodes do not cover all the gpus in a host")
        if (n_nodes % (self.n_gpus * self.n_servers) != 0):
            raise Exception("n_nodes do not cover all the gpus under a leaf")
        
        # exclude the source's host
        bcast_n_dst_hosts = (n_nodes // self.n_gpus) - 1
        # excluse the source's leaf
        bcast_n_leafs = (n_nodes // (self.n_gpus * self.n_servers)) - 1

        if (collective == 'allreduce'):
            n_trees = (n_nodes // self.n_gpus)
            nv_link_cost_per_host = 2 * (self.n_gpus - 1) + (self.n_gpus * (bcast_n_dst_hosts+1)) # gathering data in one gpu and broadcasting in all other hosts and all other nodes inside the same host
        elif (collective == 'broadcast'):
            n_trees = 1
            nv_link_cost_per_host = (self.n_gpus + (self.n_gpus * bcast_n_dst_hosts)) # broadcasting using becca's BEB once inside source host and once inside every dst host)
        elif (collective == 'allgather'):
            nv_link_cost_per_host = self.n_gpus # broadcasting using becca's BEB
            n_trees = n_nodes
        else:
            raise Exception("Unknown collective!")

        nv_link_cost = n_trees * nv_link_cost_per_host
        
        tree_bw_cost = 0
        if (bcast_n_dst_hosts < self.n_servers):
            # communication is under a leaf
            tree_bw_cost = 1 + \
                bcast_n_dst_hosts
        else:
            tree_bw_cost = 2 + \
                bcast_n_leafs + \
                bcast_n_dst_hosts

        total_bw_cost = n_trees * tree_bw_cost
        print(f'BECCA --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {total_bw_cost+nv_link_cost}')
    
    
    def get_becca_with_local_leaf_reduction_cost(self, n_nodes, collective=None):
        if collective != 'allreduce':
            raise Exception("Collective is not allreduce")
        
        # exclude the source's host
        bcast_n_dst_hosts = (n_nodes // self.n_gpus) - 1
        # excluse the source's leaf
        bcast_n_leafs = (n_nodes // (self.n_gpus * self.n_servers)) - 1

        n_trees = bcast_n_leafs + 1

        nv_link_cost_per_host = 2 * (self.n_gpus - 1) + (self.n_gpus * (bcast_n_dst_hosts+1)) # gathering data in one gpu and broadcasting in all other hosts and all other nodes inside the same host
        # multiply by the number of servers
        nv_link_cost = nv_link_cost_per_host * (bcast_n_dst_hosts+1)

        # tree_bw_cost is initialized to the cost of first reducing inside hosts and then sending the result to candidate host under each leaf
        tree_bw_cost = 2 * (self.n_servers - 1) * n_trees
        if (bcast_n_dst_hosts < self.n_servers):
            raise Exception("bcast_n_dst_hosts < self.n_servers not handled")
        tree_bw_cost = 2 + \
            bcast_n_leafs + \
            bcast_n_dst_hosts

        total_bw_cost = n_trees * tree_bw_cost + (2 * (bcast_n_leafs+1))    # for gathering the data in one host
        print(f'BECCA with HR under each leaf --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {total_bw_cost+nv_link_cost}')
    
    def get_ring_cost(self, n_nodes, collective=None):

        node_list = [i for i in range(n_nodes)]
        if (collective == 'allreduce'):
            transmission_per_host = n_nodes - 1 # each hosts sends n-1 times
            transmission_cost = 2.0 / n_nodes   # shards: the reason it is 2/n_nodes and not 1 is that once for reduce scatter and once for allgather phase of allreduce with ring so 2.0
        elif (collective == 'broadcast'):
            transmission_per_host = 1
            transmission_cost = 1   # whole message
        elif (collective == 'allgather'):
            transmission_per_host = n_nodes - 1 # each hosts sends n-1 times
            transmission_cost = 1   # shards
        
        # Sending shards for reduction
        nv_link_cost = 0
        total_bw_cost = 0
        for idx, src in enumerate(node_list):
            if (collective == 'broadcast' and idx == len(node_list)-1):
                break
            src_host_idx = src // self.n_gpus
            src_leaf_idx = src // (self.n_gpus * self.n_servers)
            dst = (src + 1) % len(node_list)
            dst_host_idx = dst // self.n_gpus
            dst_leaf_idx = dst // (self.n_gpus * self.n_servers)
            for _ in range(transmission_per_host):
                if (src_host_idx == dst_host_idx):
                    nv_link_cost += (2 * transmission_cost)   # intra-host communication
                elif (src_leaf_idx == dst_leaf_idx):
                    total_bw_cost += 2 * transmission_cost
                else:
                    total_bw_cost += 4 * transmission_cost
        
        print(f'Ring --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {math.ceil(total_bw_cost+nv_link_cost)}')    
    
    def get_tree_cost(self, n_nodes, collective=None):
        # consider one tree but for the whole message
        src = TreeNode(0)
        src.children.append(create_tree(1, n_nodes-1))
        tree_path_lenght, tree_nvlink_path_length = dfs(src, 0, 0, self.n_servers, self.n_gpus)

        total_bw_cost = 0
        nv_link_cost = 0

        if (collective == 'broadcast'):
            total_bw_cost = tree_path_lenght    #broadcast
            nv_link_cost = tree_nvlink_path_length
        if (collective == 'allreduce'):
            total_bw_cost = 2 * tree_path_lenght    # up and down
            nv_link_cost = 2 * tree_nvlink_path_length
        if (collective == 'allgather'):
            total_bw_cost = n_nodes * tree_path_lenght  # everyone broadcasts
            nv_link_cost = n_nodes * tree_nvlink_path_length

        print(f'Tree --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {total_bw_cost+nv_link_cost}')  

    def get_ina_cost(self, n_nodes, collective='allreduce'):
        if (collective != 'allreduce'):
            raise Exception("INA only works with allreduce")
        # excluse the source's leaf
        bcast_n_leafs = (n_nodes // (self.n_gpus * self.n_servers))
        # tree up + tree down
        total_bw_cost = 2 * (n_nodes + bcast_n_leafs)
        print(f'INA w/o nvls --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {0} -- aggregate cost: {total_bw_cost+0}')  

    def get_ina_nvls_cost(self, n_nodes, collective='allreduce'):
        if (collective != 'allreduce'):
            raise Exception("INA only works with allreduce")
        
        # exclude the source's host
        bcast_n_dst_hosts = (n_nodes // self.n_gpus)
        # excluse the source's leaf
        bcast_n_leafs = (n_nodes // (self.n_gpus * self.n_servers))
        nv_link_cost = 2 * n_nodes
        # tree up + tree down
        total_bw_cost = 2 * (bcast_n_dst_hosts + bcast_n_leafs)
        print(f'INA w. nvls --> collective: {collective} -- node#: {n_nodes} -- -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {total_bw_cost+nv_link_cost}') 

    def get_optireduce_cost(self, n_nodes, collective='allreduce'):
        if (collective != 'allreduce'):
            raise Exception("OPTIReduce only works with allreduce")

        shard_bw_cost = 1.0 / n_nodes
        total_bw_cost = 0
        nv_link_cost = 0
        node_list = [i for i in range(n_nodes)]

        # Sending shards for reduction
        for src in node_list:
            src_host_idx = src // self.n_gpus
            src_leaf_idx = src // (self.n_gpus * self.n_servers)
            for dst in node_list:
                if (src == dst):
                    continue
                dst_host_idx = dst // self.n_gpus
                dst_leaf_idx = dst // (self.n_gpus * self.n_servers)

                if (src_host_idx == dst_host_idx):
                    nv_link_cost += 2 * shard_bw_cost  # nvlink cost
                    continue

                if (src_leaf_idx == dst_leaf_idx):
                    total_bw_cost += 2 * shard_bw_cost
                else:
                    total_bw_cost += 4 * shard_bw_cost
        
        # Broadcasting the reduced shards (lets assume that the number of nodes is more than under a leaf)
        # each node sends its shard to everyone
        # the cost of broadcast is the same as the cost of reduction I just coppied the code to be sure :)
        # for src in node_list:
        #     src_host_idx = src // self.n_gpus
        #     src_leaf_idx = src // (self.n_gpus * self.n_servers)
        #     for dst in node_list:
        #         if (src == dst):
        #             continue
        #         dst_host_idx = dst // self.n_gpus
        #         dst_leaf_idx = dst // (self.n_gpus * self.n_servers)

        #         if (src_host_idx == dst_host_idx):
        #             continue

        #         if (src_leaf_idx == dst_leaf_idx):
        #             total_bw_cost += 2 * shard_bw_cost
        #         else:
        #             total_bw_cost += 4 * shard_bw_cost
        nv_link_cost += shard_bw_cost * n_nodes * (2 * (self.n_gpus - 1))   # each node broadcasts its chunk to the all nodes inside the same host
        total_bw_cost *= 2  #the cost of broadcast is the same as the cost of reduction
        print(f'OPTIReduce --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {math.ceil(total_bw_cost+nv_link_cost)}')  

        
    def get_becca_optireduce_cost(self, n_nodes, collective='allreduce'):
        if (collective != 'allreduce'):
            raise Exception("BECCAOptiReduce only works with allreduce")

        shard_bw_cost = 1.0 / n_nodes
        total_bw_cost = 0
        nv_link_cost = 0
        node_list = [i for i in range(n_nodes)]

        # Sending shards for reduction (the reduction is like in optiReduce)
        for src in node_list:
            src_host_idx = src // self.n_gpus
            src_leaf_idx = src // (self.n_gpus * self.n_servers)
            for dst in node_list:
                if (src == dst):
                    continue
                dst_host_idx = dst // self.n_gpus
                dst_leaf_idx = dst // (self.n_gpus * self.n_servers)

                if (src_host_idx == dst_host_idx):
                    nv_link_cost += 2 * shard_bw_cost  # nvlink cost
                    continue

                if (src_leaf_idx == dst_leaf_idx):
                    total_bw_cost += 2 * shard_bw_cost
                else:
                    total_bw_cost += 4 * shard_bw_cost
        
        # The broadcast is like becca# exclude the source's host
        bcast_n_dst_hosts = (n_nodes // self.n_gpus) - 1
        # excluse the source's leaf
        bcast_n_leafs = (n_nodes // (self.n_gpus * self.n_servers)) - 1
        for src in node_list:
            nv_link_cost += (self.n_gpus * shard_bw_cost)
            total_bw_cost += (2 + \
                    bcast_n_leafs + \
                    bcast_n_dst_hosts) * shard_bw_cost
        print(f'BECCA+OPTIReduce --> collective: {collective} -- node#: {n_nodes} -- network cost: {total_bw_cost} -- nvlink cost: {nv_link_cost} -- aggregate cost: {math.ceil(total_bw_cost+nv_link_cost)}')  

        

collectives = ['broadcast', 'allgather', 'allreduce']
topo = LeafSpine(16, 48, 2, 8)

for collective in collectives:
    for nodes_count in (2**i for i in range(5, 10)):
        topo.get_becca_cost(nodes_count, collective)
        topo.get_ring_cost(nodes_count, collective)
        topo.get_tree_cost(nodes_count, collective)
        if (collective == 'allreduce'):
            topo.get_becca_with_local_leaf_reduction_cost(nodes_count, collective)
            topo.get_ina_cost(nodes_count, collective)
            topo.get_ina_nvls_cost(nodes_count, collective)
            topo.get_optireduce_cost(nodes_count, collective)
            topo.get_becca_optireduce_cost(nodes_count, collective)
        print('\n')
    print('------------------------------------\n')

