# running peel benchmark,

# before start, please make sure peel repos are cloned and gloo benchmark are compiled without any error.
in this document, we assume the compiled gloo benchmark binary can be found at
/opt/PEEL/Benchmark/gloo/build_bench/gloo/benchmark/benchmark

> If not already, refers to /PEEL/Benchmark/gloo_install.md

# 1. Redis Installation
# for Gloo to handle handshake correctly, we also need a redis server running on a host that is accessible to all participanting hosts.

apt update
apt install redis-server
systemctl enable redis-server

# edit the config file to make it accept connection from all ips
nano /etc/redis/redis.conf

find the line "bind 127.0.0.1 ::1"
# comment it and add the following
bind 0.0.0.0

# If not already search for protected-mode and have it turned off
protected-mode no

> Note this will incur some risk and is not recommended outside of trusted network.

# restart to apply changes
systemctl restart redis-server

# 2. Topology File Generation
For PEEL, we always need a topology file (even if it is set as optional) using --peel-topology-file. Peel reads an adjacency-list file that describes the physical switch/GPU graph. It builds a minimum spanning tree rooted at the sender rank and partitions it into subtrees — one per multicast group.

Format: each line is a pair of node IDs separated by whitespace. Node IDs are either a dotted-decimal IP (GPU/server) or switch_N (switch).

# Example: two ToR switches, four servers
switch_0  10.0.0.1
switch_0  10.0.0.2
switch_1  10.0.0.3
switch_1  10.0.0.4
switch_0  switch_1

Without a topology file, all ranks are placed in one flat transport (single multicast group, no tree).

Note: Either provide the complete address to the topology file (wherever it is located in your system), or copy it into the directory where your binary is created.
Note: The topology file assume one way edges and for bidirectional traffic both direction needs to be added separately
Note: The topology file assume index based location so switch_0 should be placed before switch_1 for correct ordering. refers to the attached topology file for a detailed example. ==> /PEEL/Benchmark/peel_topology.txt

Credit: From Sana Mahmood (https://github.com/foundational-networks/gloo/blob/peel_integration/gloo/transport/tcp/peel/README.md#topology-file)





# 3. Composing the experiment command
Below is a sample command:
./gloo/benchmark/benchmark --redis-host 10.169.144.14 --redis-port 6379 --rank 0  --transport tcp --size 3 --prefix 00 --threads 1 --tcp-device=ens18 --peel-iface=ens18 --iteration-count 1 --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt  --peel-max-payload=8900 peel_broadcast_ring

Where we should:
--redis-host 10.169.144.14 ---> replace 10.169.144.14 with real redis server address, if different.
--redis-port 6379  --->  replace 6379 with actual redis port, if different 
--rank 0 --->  the rank of the participants, the benchmark results will be outputted to rank 0. each host should have different rank, like rank 0 1, 2,..., etc. until size - 1
--size 3 --->  the number of host in the benchmark, including rank 0. for a broadcast benchmark from one sender to two receiver, use size 3.
--prefix 00 ---> can be any string, but should be unique across different benchmark until redis is cleared.
--threads 1 ---> peel is single-threaded and relies on the network rules to replicate packets, so keep this unchanged.
--tcp-device=ens18 ---> the nic used for gloo to perform handhsake and control connection establishment.
--peel-ifact=ens18 ---> the nic used for peel transport, usually the same as --tcp-device
--iteration-count 1 ---> iterations of the benchmark
--peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt ---> topology file required for peel to run
--peel-max-payload=8900 ---> Max packet size
peel_broadcast_ring ---> collective benchmarked refers to the list below fo all supported collectives:

supported colelctives:
  allgather
  allgather_v
  allgather_ring
  allreduce_ring
  allreduce_ring_chunked
  allreduce_halving_doubling
  allreduce_bcube
  allreduce_local
  alltoall
  alltoall_v
  barrier_all_to_all
  broadcast
  broadcast_one_to_all
  pairwise_exchange
  reduce
  reduce_scatter
  scatter
  sendrecv_roundtrip
  sendrecv_stress
  isendirecv_stress
  *peel_broadcast
  *peel_broadcast_ring
  *peel_broadcast_stop_and_wait
  *peel_allgather
  *peel_allgather_ring
  *peel_allreduce_ring
  *broadcast_ring
  *broadcast_stop_and_wait

The collective marked with "*" is what we added, the rest is what gloo originally support.
Among those added, only peel_* without any suffix uses real peel tranport, the rest serves as a baseline that uses the same stop and wait mechanism under 1-1 udp connection
  




# Required info
Redis host IP: 10.169.144.14:6379
Generated Topology File Location: /opt/PEEL/Benchmark/peel_topology.txt
Gloo Benchmark Binary: /opt/PEEL/Benchmark/gloo/build_bench/gloo/benchmark/benchmark
NIC id on all participating hosts: ens18

# Composed commands
suppose we have one sender on host 105 port 0 (tovin 10.169.155.11) and two sender on host 106 port0 and 1 (tovra 10.169.157.11 and xelar 10.169.157.12)
suppose we ran peel_broadcast for example,

we have the following command for each host, execute them in a sequence.
tovin:
./gloo/benchmark/benchmark --redis-host 10.169.144.14 --redis-port 6379 --rank 0  --transport tcp --size 3 --prefix peel_brdcst0807b --threads 1 --tcp-device=ens18 --peel-iface=ens18 --iteration-count 1 --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt --peel-max-payload=8900 peel_broadcast

tovra:
./gloo/benchmark/benchmark --redis-host 10.169.144.14 --redis-port 6379 --rank 1  --transport tcp --size 3 --prefix peel_brdcst0807b --threads 1 --tcp-device=ens18 --peel-iface=ens18 --iteration-count 1 --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt --peel-max-payload=8900 peel_broadcast

xelar:
./gloo/benchmark/benchmark --redis-host 10.169.144.14 --redis-port 6379 --rank 2  --transport tcp --size 3 --prefix peel_brdcst0807b --threads 1 --tcp-device=ens18 --peel-iface=ens18 --iteration-count 1 --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt --peel-max-payload=8900 peel_broadcast



# results
Once finished, a result should be printed to rank 0 in the following format:
# This is a sample result and does not reflect any performance metrics.

====================================================================================================
                                          PEEL_BROADCAST

Device:      tcp, pci=, iface=ens18, speed=-1, addr=[10.169.155.11]
Options:     processes=3, inputs=1, threads=1, verify=true

====================================================================================================
                                        BENCHMARK RESULTS

   size (B)   elements   min (us)   p50 (us)   p99 (us)   max (us)   bandwidth (GB/s)   iterations
        400        100        172        172        172        172              0.002            1
        800        200        164        164        164        164              0.005            1
       2000        500        164        164        164        164              0.011            1
       4000       1000        125        125        125        125              0.030            1
       8000       2000        178        178        178        178              0.042            1
      20000       5000        353        353        353        353              0.053            1
      40000      10000        602        602        602        602              0.062            1
      80000      20000       1001       1001       1001       1001              0.074            1
     200000      50000       2457       2457       2457       2457              0.076            1
     400000     100000       4749       4749       4749       4749              0.078            1
     800000     200000       9096       9096       9096       9096              0.082            1
    2000000     500000      22653      22653      22653      22653              0.082            1
    4000000    1000000      47290      47290      47290      47290              0.079            1
    8000000    2000000      85243      85243      85243      85243              0.087            1
   20000000    5000000     231673     231673     231673     231673              0.080            1

====================================================================================================
