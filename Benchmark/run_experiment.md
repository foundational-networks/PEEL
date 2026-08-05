# Running the PEEL Benchmark

Before running any PEEL benchmark, make sure that:

* The PEEL repository has been cloned on all participating hosts.
* The Gloo benchmark has been compiled successfully.
* The OpenFlow controller, switch DPIDs, and OpenFlow port mappings have been configured and validated.
* The PEEL forwarding test has completed successfully.

In this document, we assume that the compiled Gloo benchmark binary is located at `/opt/PEEL/Benchmark/gloo/build_bench/gloo/benchmark/benchmark`. If Gloo has not yet been compiled, refer to `PEEL/Benchmark/gloo_install.md`.

---

# Step 1: Install and Configure Redis

Gloo uses Redis for rendezvous and connection setup between participating ranks. Therefore, a Redis server must be running on a host that is reachable from all benchmark participants. To install Redis and enable it, run the following commands:

```bash
apt update
apt install redis-server
systemctl enable redis-server
```

Edit the Redis configuration:

```bash
nano /etc/redis/redis.conf
```

Find `bind 127.0.0.1 ::1`, comment it out and add `bind 0.0.0.0`.
Then locate `protected-mode` and configure `protected-mode no`.

> **Security Note:** Binding Redis to `0.0.0.0` and disabling protected mode allows remote clients to connect to Redis. This configuration should only be used inside a trusted and isolated experimental network. Do not expose this Redis instance to an untrusted network or the public Internet.

Restart Redis to apply the configuration and verify that it is running:

```bash
systemctl restart redis-server
systemctl status redis-server
```

---

# Step 2: Create the PEEL Topology File

PEEL requires a topology file specified using:

```text
--peel-topology-file
```

The topology file describes the physical graph connecting servers and switches.

PEEL reads this adjacency-list representation, constructs a minimum spanning tree rooted at the sender rank, and partitions the tree into subtrees used for multicast forwarding.

## Topology File Format

Each line contains two node identifiers separated by whitespace:

```text
NODE_A NODE_B
```

A node identifier can be either:

* A dotted-decimal IP address representing a server/VM, such as `10.0.0.1`
* A switch identifier such as `switch_0`

For example:

```text
switch_0 10.0.0.1
switch_0 10.0.0.2
switch_1 10.0.0.3
switch_1 10.0.0.4
switch_0 switch_1
```

This example represents two switches with two servers connected to each switch.

> **Important:** The topology representation uses directed edges. If communication is required in both directions, both directions must be explicitly included in the file.

For example:

```text
switch_0 switch_1
switch_1 switch_0
```

The switch identifiers are also index-based. Therefore, use identifiers such as:

```text
switch_0
switch_1
switch_2
```

in the expected topology order.

A complete topology file for our testbed is provided at:

```text
/opt/PEEL/Benchmark/peel_topology.txt
```

You can either:

1. Provide the absolute path to the topology file using `--peel-topology-file`, or
2. Copy the topology file into the directory from which the benchmark is executed.

> **Note:** Even though the topology-file option may appear optional in some interfaces, PEEL experiments should always provide a topology file. Without one, all ranks are placed into a single flat transport group rather than using the intended hierarchical tree structure.

For additional implementation details, refer to the PEEL topology documentation in the Gloo repository:

```text
https://github.com/foundational-networks/gloo/blob/peel_integration/gloo/transport/tcp/peel/README.md#topology-file
```

Credit: Sana Mahmood.

---

# Step 3: Compose the Benchmark Command

A PEEL benchmark command has the following general form:

```bash
./gloo/benchmark/benchmark \
    --redis-host 10.169.144.14 \
    --redis-port 6379 \
    --rank 0 \
    --transport tcp \
    --size 3 \
    --prefix 00 \
    --threads 1 \
    --tcp-device=ens18 \
    --peel-iface=ens18 \
    --iteration-count 1 \
    --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt \
    --peel-max-payload=8900 \
    peel_broadcast_ring
```

The following sections describe each argument.

---

## `--redis-host`

Example:

```text
--redis-host 10.169.144.14
```

Specifies the IP address of the Redis server used for rendezvous.

Replace:

```text
10.169.144.14
```

with the actual Redis server address if necessary.

---

## `--redis-port`

Example:

```text
--redis-port 6379
```

Specifies the Redis server port.

The default Redis port is:

```text
6379
```

Change this value if Redis is configured to use a different port.

---

## `--rank`

Example:

```text
--rank 0
```

Specifies the rank of the participating process.

Each participant must use a unique rank from:

```text
0 ... size-1
```

For example, for three participating hosts:

```text
rank 0
rank 1
rank 2
```

Benchmark results are printed by **rank 0**.

---

## `--size`

Example:

```text
--size 3
```

Specifies the total number of participating ranks.

For example, a broadcast experiment with:

* 1 sender
* 2 receivers

uses:

```text
--size 3
```

---

## `--prefix`

Example:

```text
--prefix peel_brdcst0807b
```

The prefix identifies a specific rendezvous instance in Redis.

All ranks participating in the same benchmark run must use the **same prefix**.

Different benchmark runs should use different prefixes, unless the corresponding Redis state has been cleared.

For example:

```text
--prefix peel_brdcst_run1
--prefix peel_brdcst_run2
```

---

## `--threads`

Use:

```text
--threads 1
```

PEEL is currently single-threaded and relies on the network forwarding rules to replicate packets.

Therefore, this value should remain:

```text
1
```

---

## `--tcp-device`

Example:

```text
--tcp-device=ens18
```

Specifies the network interface used by Gloo's TCP transport for rendezvous and control-plane connection establishment.

Replace `ens18` if the participating VM uses a different network interface.

---

## `--peel-iface`

Example:

```text
--peel-iface=ens18
```

Specifies the network interface used by the PEEL transport.

In our setup, this is the same interface used for the Gloo TCP control connection:

```text
ens18
```

---

## `--iteration-count`

Example:

```text
--iteration-count 1
```

Specifies the number of benchmark iterations performed for each tested message size.

For initial validation, we recommend using:

```text
--iteration-count 1
```

Larger values can be used for performance evaluation.

---

## `--peel-topology-file`

Example:

```text
--peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt
```

Specifies the topology file used by PEEL.

For our testbed:

```text
/opt/PEEL/Benchmark/peel_topology.txt
```

---

## `--peel-max-payload`

Example:

```text
--peel-max-payload=8900
```

Specifies the maximum PEEL packet payload size.

Our experimental configuration uses:

```text
8900 bytes
```

which is compatible with our jumbo-frame network configuration.

---

## Collective

The final argument specifies the collective benchmark to execute.

For example:

```text
peel_broadcast
```

or:

```text
peel_broadcast_ring
```

---

# Step 4: Supported Collectives

The benchmark currently supports the following collectives:

```text
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

peel_broadcast
peel_broadcast_ring
peel_broadcast_stop_and_wait
peel_allgather
peel_allgather_ring
peel_allreduce_ring
broadcast_ring
broadcast_stop_and_wait
```

The following collectives were added as part of PEEL:

```text
peel_broadcast
peel_broadcast_ring
peel_broadcast_stop_and_wait
peel_allgather
peel_allgather_ring
peel_allreduce_ring
broadcast_ring
broadcast_stop_and_wait
```

The original Gloo collectives are the remaining entries.

## PEEL vs. Baseline Implementations

Among the newly added collectives, the commands beginning directly with:

```text
peel_*
```

and without an additional baseline suffix use the actual PEEL transport where applicable.

The `*_ring` and `*_stop_and_wait` variants are provided primarily as comparison baselines. They use similar stop-and-wait behavior over one-to-one UDP communication rather than relying on PEEL's in-network multicast forwarding.

---

# Step 5: Example Testbed Configuration

For the following example, our environment uses:

| Parameter                | Value                                                           |
| ------------------------ | --------------------------------------------------------------- |
| Redis server             | `10.169.144.14:6379`                                            |
| Topology file            | `/opt/PEEL/Benchmark/peel_topology.txt`                         |
| Gloo benchmark binary    | `/opt/PEEL/Benchmark/gloo/build_bench/gloo/benchmark/benchmark` |
| NIC on participating VMs | `ens18`                                                         |

Suppose we run a broadcast using three hosts:

| Rank | Host    | IP              | PVE Host | Role     |
| ---: | ------- | --------------- | -------: | -------- |
|    0 | `tovin` | `10.169.155.11` |      105 | Sender   |
|    1 | `tovra` | `10.169.157.11` |      106 | Receiver |
|    2 | `xelar` | `10.169.157.12` |      106 | Receiver |

The experiment therefore uses:

```text
--size 3
```

and executes:

```text
peel_broadcast
```

All three ranks must use:

* The same Redis server
* The same Redis port
* The same benchmark size
* The same prefix
* The same topology file
* A unique rank

---

# Step 6: Run the Benchmark

Navigate to the benchmark build directory on each participating VM:

```bash
cd /opt/PEEL/Benchmark/gloo/build_bench
```

For this example, use the prefix:

```text
peel_brdcst0807b
```

## Rank 0 — `tovin`

Run:

```bash
./gloo/benchmark/benchmark \
    --redis-host 10.169.144.14 \
    --redis-port 6379 \
    --rank 0 \
    --transport tcp \
    --size 3 \
    --prefix peel_brdcst0807b \
    --threads 1 \
    --tcp-device=ens18 \
    --peel-iface=ens18 \
    --iteration-count 1 \
    --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt \
    --peel-max-payload=8900 \
    peel_broadcast
```

---

## Rank 1 — `tovra`

Run:

```bash
./gloo/benchmark/benchmark \
    --redis-host 10.169.144.14 \
    --redis-port 6379 \
    --rank 1 \
    --transport tcp \
    --size 3 \
    --prefix peel_brdcst0807b \
    --threads 1 \
    --tcp-device=ens18 \
    --peel-iface=ens18 \
    --iteration-count 1 \
    --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt \
    --peel-max-payload=8900 \
    peel_broadcast
```

---

## Rank 2 — `xelar`

Run:

```bash
./gloo/benchmark/benchmark \
    --redis-host 10.169.144.14 \
    --redis-port 6379 \
    --rank 2 \
    --transport tcp \
    --size 3 \
    --prefix peel_brdcst0807b \
    --threads 1 \
    --tcp-device=ens18 \
    --peel-iface=ens18 \
    --iteration-count 1 \
    --peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt \
    --peel-max-payload=8900 \
    peel_broadcast
```

Start the commands on all participating hosts for the same benchmark run.

> **Important:** All ranks must use the same value for `--prefix`. If one rank uses a different prefix, the participants will not join the same rendezvous instance.

---

# Step 7: Verify the Benchmark Results

After the benchmark completes, the results are printed on **rank 0**.

The output should have a format similar to:

```text
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
```

> **Note:** The values above are provided only to illustrate the expected output format and should not be interpreted as representative PEEL performance results.

The table reports performance for multiple message sizes, including:

* Message size in bytes
* Number of elements
* Minimum latency
* Median (`p50`) latency
* `p99` latency
* Maximum latency
* Effective bandwidth
* Number of benchmark iterations

If the benchmark completes successfully on all ranks and rank 0 prints the result table, the PEEL benchmark environment is functioning correctly.

---

# Step 8: Running Additional Experiments

After successfully completing the three-node `peel_broadcast` example, you can extend the experiment by changing:

* The number of participating hosts using `--size`
* Rank assignments using `--rank`
* The topology file
* The collective
* The number of iterations
* The maximum PEEL payload size
* The sender and receiver placement across PVE hosts

For each new experiment, make sure that:

1. The topology file accurately represents all participating nodes.
2. OpenFlow port mappings still match the Ryu configuration.
3. All participating ranks use the same Redis host and port.
4. All participating ranks use the same unique benchmark prefix.
5. Each rank uses a unique value in the range `0 ... size-1`.

Once these conditions are satisfied, the PEEL benchmark can be used for larger-scale performance evaluation.
















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
