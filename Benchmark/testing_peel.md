Once all environment and nodes are setup, we recommend testing the peel first before running any benchmark or experiment

Before testing, please double-check openflow port and rules are matched correctly.

For the purpose of demonstration, our testbeds using the following settings:

a **static hierarchical multicast forwarding policy for PEEL** across a small leaf-spine topology:

- **Sender-side leaf:** switch **105**
- **Core / spine:** Mellanox SN2700
- **Receiver-side leaves:** switches **106** and **107**

The design uses:

- **OpenFlow 1.3**
- **DSCP = 7** to identify experiment traffic
- **VLAN 144** on trunk/inter-switch paths
- **Destination MAC bit fields** as an encoded hop-selection mechanism
- **Group tables** for replication
- **NORMAL fallback** for ordinary traffic
- **Guard drop on the core** for experiment traffic that does not match a valid core rule

---

## 2. Topology and Static Assumptions


- Switch **105** acts as the sender-side leaf
- Switches **106** and **107** act as receiver-side leaves
- The **core switch** sits between them
- Experiment traffic is identified by:
  - `ip_dscp = 7`
- Inter-switch experiment traffic carries **VLAN 144** due to upper stream limits.
- Packets delivered to receivers have:
  - VLAN tag removed
  - destination MAC rewritten to `01:00:5e:00:00:07`

### Static DPIDs

- **Core:** `0000b8599f5c4400`
- **Switch 105:** `0000f46b8c134345`
- **Switch 106:** `0000f46b8c132d65`
- **Switch 107:** `0000f46b8c1335c5`

### Ports

#### Switch 105
- Uplink: **1**
- Management: **2**
- Host ports: **3**, **4**

#### Core
- From 105: **103**
- To 106: **73**
- To 107: **75**

#### Switch 106
- Uplink: **1**
- Host ports: **3, 4, 5, 6, 7, 8, 9, 10**

#### Switch 107
- Uplink: **1**
- Host ports: **3, 4, 5, 6, 7, 8, 9, 10**

Sometime Host ports become **11 - 18 after OVS restarts**.


---

## 3. High-Level Forwarding Logic

The forwarding decision is split across **three hop bytes** encoded in the destination MAC address.

### Hop-byte meaning

- **byte 0** → decision at switch **105**
- **byte 1** → decision at the **core**
- **byte 2** → decision at switch **106** or **107**

We have implemented TTL matching, so generic rules are loaded on all switch and the forwarding behavior will only be activated with corresponding TTL value.

The script matches the full **8 bits** of the selected byte:

So each rule effectively matches the **-bit code** embedded in one MAC byte.


## 4. Installation Behavior on Switch Connection

Whenever a switch connects and emits `EventOFPSwitchFeatures`, the controller:

1. Logs the switch DPID
2. **Deletes all flows**
3. **Deletes all groups**
4. **Deletes all meters** (best effort)
5. Sends a **barrier**
6. Installs rules based on the DPID
7. Sends another **barrier**

---

## 5. Prefix Rules and Port Mapping (SPINE / LEAF)

### 5.1. General Assumptions

The full 16-port rule set has been deployed on LEAF switches:
- switches 106/107 currently have 8 active VMs,
- switch 105 currently has 2 active VMs.

***Traffic directed to unused ports is currently dropped.***

---

#### Matching Table

| Port / Range | Description | Prefix (bin) | HEX |
|--------------|------------|--------------|-----|
| 0..1         | leaf 0–1   | `100 00000`  | `0x80` |
| 0            | leaf 0     | `101 00000`  | `0xA0` |
| 1            | leaf 1     | `101 00001`  | `0xA1` |
| 2            | leaf 2     | `101 00010`  | `0xA2` |

---

### 5.3. Rules for LEAF

Thanks for @Sepehr Abdous for coming up these rules.

**Parameters:**
- total physical ports: `17`
- port range: `0..16`
- node-facing ports: `0..15`
- uplink port: `16`
- rule-space size: `17`
- logical port ID bits: `5`
- prefix-length bits: `3`
- number of rules: `32`
- format: `<prefix_len_bits> <prefix_value_with_*>`

#### Range Aggregation Rules

| Port Range | Type | Prefix (bin) | HEX |
|------------|------|--------------|-----|
| 0..15      | node | `001 00000`  | `0x20` |
| 0..7       | node | `010 00000`  | `0x40` |
| 8..15      | node | `010 01000`  | `0x48` |
| 0..3       | node | `011 00000`  | `0x60` |
| 4..7       | node | `011 00100`  | `0x64` |
| 8..11      | node | `011 01000`  | `0x68` |
| 12..15     | node | `011 01100`  | `0x6C` |

#### Pair-Based Rules

| Port Range | Prefix (bin) | HEX |
|------------|--------------|-----|
| 0..1       | `100 00000`  | `0x80` |
| 2..3       | `100 00010`  | `0x82` |
| 4..5       | `100 00100`  | `0x84` |
| 6..7       | `100 00110`  | `0x86` |
| 8..9       | `100 01000`  | `0x88` |
| 10..11     | `100 01010`  | `0x8A` |
| 12..13     | `100 01100`  | `0x8C` |
| 14..15     | `100 01110`  | `0x8E` |

#### Per-Port Rules

| Port | Type   | Prefix (bin) | HEX |
|------|--------|--------------|-----|
| 0    | node   | `101 00000`  | `0xA0` |
| 1    | node   | `101 00001`  | `0xA1` |
| 2    | node   | `101 00010`  | `0xA2` |
| 3    | node   | `101 00011`  | `0xA3` |
| 4    | node   | `101 00100`  | `0xA4` |
| 5    | node   | `101 00101`  | `0xA5` |
| 6    | node   | `101 00110`  | `0xA6` |
| 7    | node   | `101 00111`  | `0xA7` |
| 8    | node   | `101 01000`  | `0xA8` |
| 9    | node   | `101 01001`  | `0xA9` |
| 10   | node   | `101 01010`  | `0xAA` |
| 11   | node   | `101 01011`  | `0xAB` |
| 12   | node   | `101 01100`  | `0xAC` |
| 13   | node   | `101 01101`  | `0xAD` |
| 14   | node   | `101 01110`  | `0xAE` |
| 15   | node   | `101 01111`  | `0xAF` |
| 16   | uplink | `101 10000`  | `0xB0` |

---

### 5.4. Current Port Mapping (Topology)

#### Core (SPINE)

| Port | Connection |
|------|------------|
| 0    | switch 105 |
| 1    | switch 106 |
| 2    | switch 107 |

#### LEAF (VM Mapping)

| Port Range | Assignment |
|------------|------------|
| 0–15       | VM OF Port 1–16 (105, 106, 107) |
| 16         | uplink to spine |


---

#### Fallback on both all switches.
**Priority:** 0  
**Action:** `OUTPUT:NORMAL`


################################################# END OF BACKGROUND SECTION #################################################

#################################################      Perform Testing      #################################################

to test peel, first make sure tcpdump package has been installed.
If followed the vm_setup.md correctly, it should already been installed

tcpdump --version

#Sample Output
root@hydra:~# tcpdump --version
tcpdump version 4.99.5
libpcap version 1.10.5 (with TPACKET_V3)
OpenSSL 3.5.5 27 Jan 2026
64-bit build, 64-bit time_t


# 1. picking the right sender and the Receiver
In our testing environment, we have three pve hosts, 105, 106, and 107.
Suppose we pick the first node in 105 (OF port 0), and multicast to the firt two nodes in 106 (OF port 0 - 1)

In our network, this refers to host tovin (10.169.155.11) on 105, and host tovra (10.169.157.11) on 106 port 0, and host xelar (10.169.157.12) on 106 port 1. optinally, connect to an additional host on 106 for verfication, in our case we use host kynar (10.169.157.13) on 106 port 2.

# 2. Preparing the reciever
We currently use hardcoded mac "01:00:5e:00:00:07" to identify the traffic on the host level, so start by using the following command on all receivers.
tcpdump -eni ens18 -v -p 'ether dst 01:00:5e:00:00:07'
> Note replace ens18 with the actual openflow NIC id.

# 3. sending the multicast traffic
on sender, if not already, clone the PEEL repo
cd /opt
git clone https://github.com/foundational-networks/PEEL.git

# Enter he helper script dir
cd /opt/PEEL/Env_Setup/Helper_Scripts

# make sure python 3 is installed. 
python3 --version

# since we want to multicast to the first two host on 106, from the sender on 105, we need to compose the following MAC:
b0:a1:80:00:00:00

where b0 --> uplink from 105 to the core
a1 -> port 1 on core, which is 106
80 -> port 0 - 1 on 106
The rest digits does not matter in the current scope, so we use "00" as placeholder.

# using the following command to send testing traffic on sender and observe output on the Receiver
python3 send_of_mcast.py --iface ens18 --count 3 --interval 1 --dst-mac  b0:a1:80:00:00:00

# 4. Interpret the output
if everything works, we should observe tcpdump output three udp packets arrive on host tovra and host xelar, but not on host kynar

# Optionally test with other macs and pairs
We have a few profile pre-encoded in the testing script send_of_mcast.py
--profile {106_107_all,106_port0,107_port0_to_port3,106_all_ports,107_port0_to_port1,105_all}

> Note they are designed to be executed on a host connected to PVE host 105. For sending from other host, use manual mac specification --dst-mac


# If testing looks good, we can start performaning real peel benchmark!



