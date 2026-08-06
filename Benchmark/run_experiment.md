# Running the PEEL Benchmark

Before running any PEEL benchmark, make sure that:

* The PEEL repository has been cloned on all participating hosts.
* The Gloo benchmark has been compiled successfully.
* The OpenFlow controller, switch DPIDs, and OpenFlow port mappings have been configured and validated.
* The PEEL forwarding test has completed successfully.

In this document, we assume that the compiled Gloo benchmark binary is located at `/opt/PEEL/Benchmark/gloo/build_bench/gloo/benchmark/benchmark`. If Gloo has not yet been compiled, refer to `PEEL/Benchmark/gloo_install.md`.

> **Note:** If you have been provided with credentials to access our preconfigured VMs, where Gloo and all required dependencies are already installed, you can skip the setup steps and jump directly to [the example testbed configuration](#example-testbed-configuration) to run a validation scenario.

> **Note:** For running the experiments, you require `sudo` access.


---

## Step 1: Install and Configure Redis

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

## Step 2: Create the PEEL Topology File

PEEL requires a topology file specified using `--peel-topology-file`.
The topology file describes the physical graph connecting servers and switches.
PEEL reads this adjacency-list representation, constructs a minimum spanning tree rooted at the sender rank, and partitions the tree into subtrees used for multicast forwarding.

### Topology File Format

Each line contains two node identifiers separated by whitespace: `NODE_A NODE_B`.
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
A complete topology file for our testbed is provided at: `/opt/PEEL/Benchmark/peel_topology.txt`.
You can either:

1. Provide the absolute path to the topology file using `--peel-topology-file`, or
2. Copy the topology file into the directory from which the benchmark is executed.

> **Note:** Even though the topology-file option may appear optional in some interfaces, PEEL experiments should always provide a topology file. Without one, all ranks are placed into a single flat transport group rather than using the intended hierarchical tree structure.

For additional implementation details, refer to the PEEL topology documentation in the Gloo repository:

```text
https://github.com/foundational-networks/gloo/blob/peel_integration/gloo/transport/tcp/peel/README.md#topology-file
```

---

## Step 3: Compose the Benchmark Command

> **Note:** For running the experiments, you need `sudo` access.

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

### `--redis-host`

Example: `--redis-host 10.169.144.14`

Specifies the IP address of the Redis server used for rendezvous.
Replace `10.169.144.14` with the actual Redis server address if necessary.


### `--redis-port`

Example: `--redis-port 6379`

Specifies the Redis server port.
The default Redis port is `6379`.
Change this value if Redis is configured to use a different port.


### `--rank`

Example: `--rank 0`

Specifies the rank of the participating process.
Each participant must use a unique rank from `0 ... size-1`.
For example, for three participating hosts:

```text
rank 0
rank 1
rank 2
```

Benchmark results are printed by **rank 0**.

### `--size`

Example: `--size 3`

Specifies the total number of participating ranks.
For example, a Broadcast experiment with one sender and two receivers uses `--size 3`.


### `--prefix`

Example: `--prefix peel_brdcst0807b`

The prefix identifies a specific rendezvous instance in Redis.
All ranks participating in the same benchmark run must use the **same prefix**.
Different benchmark runs should use different prefixes, unless the corresponding Redis state has been cleared.

For example:

```text
--prefix peel_brdcst_run1
--prefix peel_brdcst_run2
```

> **Note:** `prefix` can be set to any arbitrary string. If you encounter the error `Key 'prefix' already set.` when starting an experiment, simply rerun the experiment with a different `prefix` value.



### `--threads`

**Use:** `--threads 1`

PEEL is currently single-threaded and relies on the network forwarding rules to replicate packets.
Therefore, **this value should remain 1**.


### `--tcp-device`

Example: `--tcp-device=ens18`

Specifies the network interface used by Gloo's TCP transport for rendezvous and control-plane connection establishment.
Replace `ens18` if the participating VM uses a different network interface.



### `--peel-iface`

Example: `--peel-iface=ens18`

Specifies the network interface used by the PEEL transport.
In our setup, this is the same interface used for the Gloo TCP control connection: `ens18`.


### `--iteration-count`

Example: `--iteration-count 1`

Specifies the number of benchmark iterations performed for each tested message size.
For initial validation, we recommend using `--iteration-count 1`.
Larger values can be used for performance evaluation.



### `--peel-topology-file`

Example: `--peel-topology-file /opt/PEEL/Benchmark/peel_topology.txt`

Specifies the topology file used by PEEL.
For our testbed, use `/opt/PEEL/Benchmark/peel_topology.txt`.


### `--peel-max-payload`

Example: `--peel-max-payload=8900`

Specifies the maximum PEEL packet payload size.
Our experimental configuration uses `8900 bytes` which is compatible with our jumbo-frame network configuration.


### `Collective`

The final argument specifies the collective benchmark to execute.
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

------------------------------------

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

The rest are the original Gloo collectives.
Among the newly added collectives, the commands beginning directly with `peel_*`
and without an additional baseline suffix use the actual PEEL transport where applicable.
The `*_ring` and `*_stop_and_wait` variants are provided primarily as comparison baselines. They use similar stop-and-wait behavior over one-to-one UDP communication rather than relying on PEEL's in-network multicast forwarding.

---

## Example Testbed Experiment

This section provides an example experiment in our environment. For this example, our environment uses:

| Parameter                | Value                                                           |
| ------------------------ | --------------------------------------------------------------- |
| Redis server             | `10.169.144.14:6379`                                            |
| Topology file            | `/opt/PEEL/Benchmark/peel_topology.txt`                         |
| Gloo benchmark binary    | `/opt/PEEL/Benchmark/gloo/build_bench/gloo/benchmark/benchmark` |
| NIC on participating VMs | `ens18`                                                         |

We run a broadcast using three hosts:

| Rank | Host    | IP              | PVE Host | Role     |
| ---: | ------- | --------------- | -------: | -------- |
|    0 | `tovin` | `10.169.155.11` |      105 | Sender   |
|    1 | `tovra` | `10.169.157.11` |      106 | Receiver |
|    2 | `xelar` | `10.169.157.12` |      106 | Receiver |

The experiment therefore uses `--size 3` and executes `peel_broadcast`.

All three ranks must use:

* The same Redis server
* The same Redis port
* The same benchmark size
* The same prefix
* The same topology file
* A unique rank

### Run the Benchmark

> **Note:** Make sure you have `sudo` access on all VMs before running the experiment.

Navigate to the benchmark build directory on each participating VM:

```bash
cd /opt/PEEL/Benchmark/gloo/build_bench
```

For this example, use the prefix `peel_brdcst0807b`.

> **Note:** `prefix` can be set to any arbitrary string. If you encounter the error `Key 'prefix' already set.` when starting an experiment, simply rerun the experiment with a different `prefix` value.

> **Important:** All ranks must use the same value for `--prefix`. If one rank uses a different prefix, the participants will not join the same rendezvous instance.

On **Rank 0 — `tovin`** run:

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

On **Rank 1 — `tovra`** run:

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

On **Rank 2 — `xelar`** run:

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


### Verify the Benchmark Results

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

## Running Additional Experiments

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

