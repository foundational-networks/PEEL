# PVE Open vSwitch and OpenFlow Setup

Our experimental topology consists of:

* One **NVIDIA Mellanox SN2700** core switch
* Three **physical nodes**, each running Proxmox Virtual Environment (PVE)
* **16 virtual machines (VMs)** configured on each PVE node

In addition to enabling OpenFlow on the core switch, OpenFlow must also be configured on the software switch running on each PVE node.
Our environment was configured and tested with the following versions:

| Component                      | Version |
| ------------------------------ | ------- |
| PVE                            | 9.1.1   |
| Open vSwitch (`ovs-vsctl`)     | 3.5.0   |
| OpenFlow utility (`ovs-ofctl`) | 3.5.0   |

---

## Step 1: Install Proxmox VE

Install **Proxmox VE 9.1.1** on all three physical hosts and connect each host to the core switch through its designated physical network interface.
For detailed installation instructions, refer to the [official Proxmox VE installation guide](https://www.proxmox.com/en/products/proxmox-virtual-environment/get-started).
After completing the PVE installation, connect to the management IP address of each host using SSH.

---

## Step 2: Install Open vSwitch

To install Open vSwitch and start it, run the following commands:

```bash
apt update
apt install openvswitch-switch
systemctl enable --now openvswitch-switch
```

You can use the following commands to verify things:

```bash
# Verify that the service is running
systemctl status openvswitch-switch --no-pager

# Verify the installed Open vSwitch version
ovs-vsctl --version

# Verify the installed `ovs-ofctl` version
ovs-ofctl --version

```

Example output:

```text
root@hydra:~# ovs-ofctl --version
ovs-ofctl (Open vSwitch) 3.5.0
OpenFlow versions 0x1:0x6
```

---

## Step 3: Configure the Open vSwitch Bridge

Before modifying the network configuration, create a backup of the existing PVE network configuration:

```bash
cp /etc/network/interfaces /etc/network/interfaces.backup
```

Edit the network interface configuration:

```bash
nano /etc/network/interfaces
```

And configure the Open vSwitch bridge as follows:

```text
iface nic4 inet manual

auto br-ovs
iface br-ovs inet manual
    ovs_type OVSBridge
    ovs_ports nic4 mgmt144
    ovs_mtu 9216
    ovs_extra set-controller br-ovs tcp:10.169.144.22:6653
    ovs_extra set bridge br-ovs protocols=OpenFlow13
    ovs_extra set-fail-mode br-ovs standalone

auto nic4
iface nic4 inet manual
    ovs_type OVSPort
    ovs_bridge br-ovs
    ovs_mtu 9216

auto mgmt144
iface mgmt144 inet static
    address 10.169.158.107/20
    gateway 10.169.144.2
    ovs_type OVSIntPort
    ovs_bridge br-ovs
    ovs_options tag=144
    ovs_mtu 9216
```

> **Important:** The configuration above reflects our experimental environment and must be adapted to the target system before use.
>
> In particular:
>
> * Replace `nic4` with the physical NIC on the PVE host that is connected to the core switch.
> * Replace the `mgmt144` interface configuration, IP address, subnet, and gateway with the appropriate management network settings for the target environment.
> * Incorrect management network settings may cause the PVE host to become inaccessible over the network.

### MTU Configuration

Our experimental network uses an MTU of `9216` bytes for the transport network. The corresponding interfaces on the core switch must also be configured with a compatible MTU.
The provided core-switch configuration already includes this setting, but we recommend verifying the MTU after importing the switch configuration.

### Ryu Controller Address

The following line specifies the address of the Ryu OpenFlow controller:

```text
ovs_extra set-controller br-ovs tcp:10.169.144.22:6653
```

Replace `10.169.144.22:6653` with the IP address and port of the Ryu controller in the target environment.
Make sure that routing, firewall rules, and VLAN configuration allow the PVE hosts to communicate with the Ryu controller on this address and port.

### VLAN Configuration

Our laboratory network requires experiment traffic to use **VLAN 144**, which is why the management interface contains:

```text
ovs_options tag=144
```

> **Note:** VLAN 144 is specific to our laboratory environment and is **not required by PEEL**. A different VLAN, or no VLAN tagging, can be used as long as the switches and Ryu controller can communicate correctly.

---

## Step 4: Apply the Network Configuration

Reload the network configuration from the host console:

```bash
ifreload -a
```

We recommend rebooting the PVE host after completing the configuration:

```bash
reboot
```

> **Important:** Because modifying `/etc/network/interfaces` can interrupt network connectivity, we recommend applying the configuration while console access to the physical host is available.

After the host reboots, reconnect to it and verify the Open vSwitch configuration:

```bash
ovs-vsctl show
```

Before the VMs are started, it is normal for VM-specific `tap` interfaces to be absent from this output.
For instructions on creating and configuring the VMs, refer to:

```text
PEEL/Env_Setup/vm_setup.md
```

After the VMs are configured and running, the output should look similar to:

```text
root@hydra:~# ovs-vsctl show
c6e1984a-c628-4e73-b6b2-d98cae561d4d
    Bridge br-ovs
        Controller "tcp:10.169.144.22:6653"
            is_connected: true
        fail_mode: standalone
        Port tap205i0
            tag: 144
            Interface tap205i0
        Port tap207i0
            tag: 144
            Interface tap207i0
        Port tap223i0
            tag: 144
            Interface tap223i0
        ...
```

The following field indicates that the Open vSwitch bridge is connected to the configured controller: `is_connected: true`

---

## Step 5: Troubleshoot the OpenFlow Connection

If the OpenFlow controller is not connected correctly, first explicitly configure the bridge to use OpenFlow 1.3:

```bash
ovs-vsctl set bridge br-ovs protocols=OpenFlow13
```

Then configure the controller address again:

```bash
ovs-vsctl set-controller br-ovs tcp:CONTROLLER_IP:6653
```

Replace `CONTROLLER_IP` with the IP address of the Ryu controller. Check the controller status:

```bash
ovs-vsctl list Controller
```

Example output:

```text
_uuid               : cd34b7cf-b03b-4ee2-9ad4-3d274d848eaf
connection_mode     : []
controller_burst_limit: []
controller_queue_size: []
controller_rate_limit: []
enable_async_messages: []
external_ids        : {}
inactivity_probe    : []
is_connected        : true
local_gateway       : []
local_ip            : []
local_netmask       : []
max_backoff         : []
other_config        : {}
role                : other
status              : {last_error="Connection refused", sec_since_connect="88327", sec_since_disconnect="88329", state=ACTIVE}
target              : "tcp:10.169.144.22:6653"
type                : []
```

The most useful field for determining the current connection state is: `is_connected : true`. The `status` field can also provide additional debugging information about the controller connection.

---

## Step 6: Configure Standalone Fail Mode

To retain network connectivity when the OpenFlow controller is unavailable, configure the bridge to use `standalone` fail mode:

```bash
ovs-vsctl set-fail-mode br-ovs standalone
```

With this configuration, Open vSwitch can continue forwarding traffic using normal switching behavior when the controller is unavailable.

---

## Step 7: Verify the OpenFlow Configuration

Verify that the bridge is operating with OpenFlow 1.3:

```bash
ovs-ofctl -O OpenFlow13 show br-ovs
```

Example output:

```text
root@hydra:~# ovs-ofctl -O OpenFlow13 show br-ovs
OFPT_FEATURES_REPLY (OF1.3) (xid=0x2): dpid:0000f46b8c1335c5
n_tables:254, n_buffers:0
capabilities: FLOW_STATS TABLE_STATS PORT_STATS GROUP_STATS QUEUE_STATS
OFPST_PORT_DESC reply (OF1.3) (xid=0x3):
 1(nic4): addr:f4:6b:8c:13:35:c5
     config:     0
     state:      LIVE
     current:    100GB-FD AUTO_NEG
     advertised: 1GB-FD 10GB-FD 40GB-FD AUTO_NEG AUTO_PAUSE
     supported:  1GB-FD 10GB-FD 40GB-FD AUTO_NEG AUTO_PAUSE
     speed: 100000 Mbps now, 40000 Mbps max
 2(mgmt144): addr:3a:bf:9e:d5:a2:26
     config:     0
     state:      LIVE
     speed: 0 Mbps now, 0 Mbps max
 3(tap200i0): addr:ba:59:c3:f7:c5:90
     config:     0
     state:      LIVE
     current:    10GB-FD COPPER
     speed: 10000 Mbps now, 0 Mbps max
 4(tap201i0): addr:ae:cc:be:27:0d:f5
     config:     0
     state:      LIVE
     current:    10GB-FD COPPER
     speed: 10000 Mbps now, 0 Mbps max
 5(tap202i0): addr:3e:6c:64:d7:43:f2
     config:     0
     state:      LIVE
     current:    10GB-FD COPPER
     speed: 10000 Mbps now, 0 Mbps max
```

### Datapath ID (DPID)

The first line contains the OpenFlow datapath ID (DPID) of the switch:

```text
OFPT_FEATURES_REPLY (OF1.3) (xid=0x2): dpid:0000f46b8c1335c5
```

In this example, the DPID is:

```text
0000f46b8c1335c5
```

> **Important:** The Ryu controller uses the DPID to identify and match each OpenFlow switch. Update the DPID values in the PEEL OpenFlow rules to match the switches in the target environment.

---

## Step 8: Verify VM-to-OpenFlow Port Mapping

The output of:

```bash
ovs-ofctl -O OpenFlow13 show br-ovs
```

also shows the OpenFlow port number assigned to each interface. For example:

```text
3(tap200i0)
4(tap201i0)
5(tap202i0)
```

Here:

* `tap200i0` corresponds to OpenFlow port `3`
* `tap201i0` corresponds to OpenFlow port `4`
* `tap202i0` corresponds to OpenFlow port `5`

> **Important:** OpenFlow port numbers assigned to VM `tap` interfaces may change when VMs are restarted. The VM-to-OpenFlow-port mapping used by the PEEL controller scripts must therefore be checked and updated whenever these port assignments change.

Before running an experiment, verify both:

1. The **DPID** of each PVE Open vSwitch bridge.
2. The **OpenFlow port number** associated with each VM interface.

These values must match the corresponding mappings in the PEEL Ryu controller configuration.
