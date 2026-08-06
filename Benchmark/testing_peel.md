# PEEL Forwarding Validation

Once the environment and all test nodes have been configured, we recommend validating PEEL forwarding before running any benchmarks or large-scale experiments.

Before testing, double-check that:

* All OpenFlow switches are connected to the Ryu controller.
* Switch DPIDs match those configured in `leafspine.py`.
* VM-to-OpenFlow-port mappings are correct.
* Core-switch port mappings are correct.
* The expected OpenFlow rules and group-table entries have been installed.

---

## Testbed Configuration

For demonstration purposes, our testbed uses a **static hierarchical multicast forwarding policy for PEEL** across a small leaf-spine topology:

* **Sender-side leaf:** PVE switch **105**
* **Core/spine:** NVIDIA Mellanox SN2700
* **Receiver-side leaves:** PVE switches **106** and **107**

The forwarding design uses:

* **OpenFlow 1.3**
* **DSCP = 7** to identify PEEL experiment traffic
* **VLAN 144** on inter-switch/trunk paths
* **Destination MAC address bit fields** to encode hop-selection decisions
* **OpenFlow group tables** for packet replication
* **NORMAL forwarding fallback** for ordinary traffic
* A **guard drop rule on the core** for PEEL experiment traffic that does not match a valid core forwarding rule

---

## Topology and Static Assumptions

The following assumptions are used by the current PEEL controller configuration:

* Switch **105** acts as the sender-side leaf.
* Switches **106** and **107** act as receiver-side leaves.
* The **core switch** sits between them and connects the three PVE software switches.
* PEEL experiment traffic is identified using `ip_dscp = 7`.
* Inter-switch PEEL traffic carries **VLAN 144** because of restrictions in our upstream network.
* Before packets are delivered to receiver VMs:

  * The VLAN tag is removed.
  * The destination MAC address is rewritten to `01:00:5e:00:00:07`

> **Note:** VLAN 144 is specific to our test environment and is not inherently required by PEEL.

### Static DPIDs

Our current testbed uses the following switch DPIDs:

| Switch  | DPID               |
| ------- | ------------------ |
| Core    | `0000b8599f5c4400` |
| PVE 105 | `0000f46b8c134345` |
| PVE 106 | `0000f46b8c132d65` |
| PVE 107 | `0000f46b8c1335c5` |

These values must be replaced if the switches in the target environment use different DPIDs.

### Current OpenFlow Ports

Ports for PVE Switch 105:

| Function        | OpenFlow Port |
| --------------- | ------------: |
| Uplink          |             1 |
| Management      |             2 |
| Active VM ports |          3, 4 |

Ports for Core Switch:

| Connection | OpenFlow Port |
| ---------- | ------------: |
| PVE 105    |           103 |
| PVE 106    |            73 |
| PVE 107    |            75 |

Ports for PVE Switch 106:

| Function        | OpenFlow Port |
| --------------- | ------------: |
| Uplink          |             1 |
| Active VM ports |          3–10 |

Ports for PVE Switch 107:

| Function        | OpenFlow Port |
| --------------- | ------------: |
| Uplink          |             1 |
| Active VM ports |          3–10 |

> **Important:** VM OpenFlow port numbers may change after Open vSwitch or the VMs are restarted. For example, host ports that normally appear as `3–10` may instead appear as `11–18`.
>
> Always verify the current VM-to-OpenFlow-port mappings before running an experiment.

---

## High-Level Forwarding Logic

PEEL encodes forwarding decisions into bytes of the destination MAC address.
The current implementation uses three hop-selection bytes:

| MAC Byte | Forwarding Decision                   |
| -------- | ------------------------------------- |
| Byte 0   | Sender-side leaf, switch 105          |
| Byte 1   | Core switch                           |
| Byte 2   | Receiver-side leaf, switch 106 or 107 |

For example, `b0:a1:80:00:00:00` contains three relevant forwarding bytes:

```text
b0 -> decision on switch 105
a1 -> decision on the core
80 -> decision on the receiver-side leaf
```

The remaining MAC bytes are not used by the current forwarding logic and can be set to `00`.
The controller also uses TTL matching so that the same generic forwarding rules can be installed across switches while only the rule corresponding to the current hop is activated.
Each forwarding rule matches the full **8-bit value** of the corresponding destination-MAC byte.

---

## Controller Behavior When a Switch Connects

Whenever an OpenFlow switch connects to the controller and generates an `EventOFPSwitchFeatures` event, the controller performs the following initialization sequence:

1. Logs the switch DPID.
2. Deletes all existing flow entries.
3. Deletes all existing group-table entries.
4. Deletes all meter entries when supported.
5. Sends an OpenFlow barrier request.
6. Installs the PEEL rules corresponding to the switch DPID.
7. Sends another barrier request.

This ensures that the forwarding state is reinitialized whenever a switch reconnects to the controller.

---

## Prefix Rules and Port Mapping

### General Assumptions

The current PEEL leaf-switch rule set supports up to:

* **16 node-facing ports**
* **1 uplink port**
* **17 logical ports in total**

In our current testbed:

* Switch 105 has **2 active VMs**.
* Switch 106 has **8 active VMs**.
* Switch 107 has **8 active VMs**.

The complete 16-node rule space is still installed on the leaf switches.

> **Traffic directed toward an unused VM port is dropped.**

### Prefix-Encoding Examples

The forwarding byte encodes both a prefix length and a logical port/range.
For example:

| Port / Range | Description    | Prefix (binary) | HEX    |
| ------------ | -------------- | --------------- | ------ |
| 0–1          | Leaf ports 0–1 | `100 00000`     | `0x80` |
| 0            | Leaf port 0    | `101 00000`     | `0xA0` |
| 1            | Leaf port 1    | `101 00001`     | `0xA1` |
| 2            | Leaf port 2    | `101 00010`     | `0xA2` |


### Leaf-Switch Rules

The current leaf-rule encoding uses:

* Total logical ports: `17`
* Logical port range: `0..16`
* Node-facing ports: `0..15`
* Uplink port: `16`
* Rule-space size: `17`
* Logical port ID bits: `5`
* Prefix-length bits: `3`
* Number of installed rules: `32`
* Encoding format: `<prefix_length_bits> <prefix_value>`


Range Aggregation Rules:

| Port Range | Type | Prefix (binary) | HEX    |
| ---------- | ---- | --------------- | ------ |
| 0–15       | node | `001 00000`     | `0x20` |
| 0–7        | node | `010 00000`     | `0x40` |
| 8–15       | node | `010 01000`     | `0x48` |
| 0–3        | node | `011 00000`     | `0x60` |
| 4–7        | node | `011 00100`     | `0x64` |
| 8–11       | node | `011 01000`     | `0x68` |
| 12–15      | node | `011 01100`     | `0x6C` |

Pair-Based Rules:

| Port Range | Prefix (binary) | HEX    |
| ---------- | --------------- | ------ |
| 0–1        | `100 00000`     | `0x80` |
| 2–3        | `100 00010`     | `0x82` |
| 4–5        | `100 00100`     | `0x84` |
| 6–7        | `100 00110`     | `0x86` |
| 8–9        | `100 01000`     | `0x88` |
| 10–11      | `100 01010`     | `0x8A` |
| 12–13      | `100 01100`     | `0x8C` |
| 14–15      | `100 01110`     | `0x8E` |

Per-Port Rules:

| Port | Type   | Prefix (binary) | HEX    |
| ---: | ------ | --------------- | ------ |
|    0 | node   | `101 00000`     | `0xA0` |
|    1 | node   | `101 00001`     | `0xA1` |
|    2 | node   | `101 00010`     | `0xA2` |
|    3 | node   | `101 00011`     | `0xA3` |
|    4 | node   | `101 00100`     | `0xA4` |
|    5 | node   | `101 00101`     | `0xA5` |
|    6 | node   | `101 00110`     | `0xA6` |
|    7 | node   | `101 00111`     | `0xA7` |
|    8 | node   | `101 01000`     | `0xA8` |
|    9 | node   | `101 01001`     | `0xA9` |
|   10 | node   | `101 01010`     | `0xAA` |
|   11 | node   | `101 01011`     | `0xAB` |
|   12 | node   | `101 01100`     | `0xAC` |
|   13 | node   | `101 01101`     | `0xAD` |
|   14 | node   | `101 01110`     | `0xAE` |
|   15 | node   | `101 01111`     | `0xAF` |
|   16 | uplink | `101 10000`     | `0xB0` |

These rules encode both individual destinations and aggregated destination ranges, allowing a single encoded forwarding value to replicate traffic toward multiple receivers.


### Current Logical Port Mapping

The core switch uses the following logical mapping:

| Logical Port | Connection     |
| -----------: | -------------- |
|            0 | PVE switch 105 |
|            1 | PVE switch 106 |
|            2 | PVE switch 107 |

Each leaf switch uses:

| Logical Port Range | Assignment           |
| ------------------ | -------------------- |
| 0–15               | VM/node-facing ports |
| 16                 | Uplink to the core   |

The logical port numbers are mapped to the actual OpenFlow port numbers configured in `leafspine.py`.


### Fallback Rule

All switches include a lowest-priority fallback rule:

```text
Priority: 0
Action: OUTPUT:NORMAL
```

This allows ordinary non-PEEL traffic to use normal switching behavior.

---

## Testing PEEL Forwarding

After the controller, switches, and VMs are configured, use the following procedure to verify that PEEL multicast forwarding is working correctly.

### Verify `tcpdump`

Make sure `tcpdump` is installed on the test VMs.
If the instructions in `vm_setup.md` were followed, it should already be available.
Check the installed version: `tcpdump --version`.
Example output:

```text
root@hydra:~# tcpdump --version
tcpdump version 4.99.5
libpcap version 1.10.5 (with TPACKET_V3)
OpenSSL 3.5.5 27 Jan 2026
64-bit build, 64-bit time_t
```


### Select the Sender and Receivers

For this example, we use:

* The first VM on PVE host **105** as the sender.
* The first two VMs on PVE host **106** as receivers.
* A third VM on PVE host **106** as a negative-control receiver.

Our testbed mapping is:

| Role             | Host    | IP Address      | Logical VM Port |
| ---------------- | ------- | --------------- | --------------: |
| Sender           | `tovin` | `10.169.155.11` | PVE 105, port 0 |
| Receiver 1       | `tovra` | `10.169.157.11` | PVE 106, port 0 |
| Receiver 2       | `xelar` | `10.169.157.12` | PVE 106, port 1 |
| Negative control | `kynar` | `10.169.157.13` | PVE 106, port 2 |

The expected behavior is:

```text
tovin
  |
  | multicast packet
  v
PVE 105
  |
  v
CORE
  |
  v
PVE 106
  |------- tovra   (should receive)
  |------- xelar   (should receive)
  `------- kynar   (should NOT receive)
```


### Start Packet Capture on the Receivers

PEEL currently rewrites the destination MAC address of packets delivered to receiver hosts to:

```text
01:00:5e:00:00:07
```

On each intended receiver, run:

```bash
tcpdump -eni ens18 -v -p 'ether dst 01:00:5e:00:00:07'
```

For this test, run the command on:

```text
tovra
xelar
```

Optionally, run the same command on `kynar` to verify that traffic is **not** delivered to the third VM.

> **Note:** Replace `ens18` with the actual network interface name inside the VM if it differs.



### Prepare the Sender

On the sender VM, clone PEEL if it is not already available:

```bash
cd /opt
git clone https://github.com/foundational-networks/PEEL.git
```

Navigate to the helper scripts directory:

```bash
cd /opt/PEEL/Env_Setup/Helper_Scripts
```

Verify that Python 3 is installed:

```bash
python3 --version
```


### Construct the PEEL Destination MAC

For this example, we want to send from `PVE 105` to `PVE 106, VM ports 0–1`.
The required PEEL forwarding MAC is `b0:a1:80:00:00:00`.
The forwarding decisions are encoded as follows:

```text
b0 -> switch 105: send to the uplink/core
a1 -> core: forward toward PVE switch 106
80 -> switch 106: replicate to logical ports 0–1
```

The remaining three bytes are not used by the current forwarding logic, so they are set to `00:00:00`.


### Send Test Traffic

From the sender VM, run:

```bash
python3 send_of_mcast.py \
    --iface ens18 \
    --count 3 \
    --interval 1 \
    --dst-mac b0:a1:80:00:00:00
```

Replace `ens18` if the sender uses a different network interface.
This command sends three test packets, one second apart.


### Verify the Result

If PEEL forwarding is working correctly:

* `tovra` should receive **3 packets**.
* `xelar` should receive **3 packets**.
* `kynar` should receive **no packets**.

The `tcpdump` sessions on `tovra` and `xelar` should therefore display the three test packets with destination MAC=`01:00:5e:00:00:07`
while the capture on `kynar` should remain empty.

This validates that:

1. Switch 105 correctly forwards the packet toward the core.
2. The core correctly selects switch 106.
3. Switch 106 correctly replicates the packet to VM ports 0 and 1.
4. The packet is not delivered to VM port 2.

---

## Additional Test Profiles

The `send_of_mcast.py` helper script includes several preconfigured forwarding profiles:

```text
--profile {
    106_107_all,
    106_port0,
    107_port0_to_port3,
    106_all_ports,
    107_port0_to_port1,
    105_all
}
```

These profiles can be used instead of manually specifying a destination MAC address.
For example:

```bash
python3 send_of_mcast.py \
    --iface ens18 \
    --count 3 \
    --interval 1 \
    --profile 106_port0
```

> **Note:** These predefined profiles assume that the sender is connected to PVE switch **105**.
>
> When sending from a host connected to a different PVE switch, specify the forwarding MAC manually using:
>
> ```text
> --dst-mac
> ```

---

## Next Step

If the forwarding test succeeds, the PEEL environment is ready for benchmark execution.
Before starting a benchmark, we still recommend performing one final verification of:

* Switch-controller connectivity
* DPIDs
* VM-to-OpenFlow-port mappings
* Core-to-leaf OpenFlow port mappings
* Installed OpenFlow rules
* Installed OpenFlow group entries

After these checks pass, you can proceed with the PEEL benchmark experiments.


