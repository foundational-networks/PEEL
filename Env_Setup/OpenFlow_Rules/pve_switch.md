Our experiment topology requires a core switch (Nvidia Mellanox SN2700) and three physical nodes, each has PVE installed with 16 VMs configured.

In addition to openflow on the core switch, we still need openflow configured for each PVE's soft switch, which can be configured with the following steps:

Our environment has the following specs:
PVE version: 9.1.1
openvswitch(ovs-vsctl) version: 3.5.0
openflow (ovs-ofctl): 3.5.0

# The first step would be install PVE on all three physical hosts with physical connection to the core switch.
The version we use is PVE 9.1.1
follow the official guide for a step by step instruction: https://www.proxmox.com/en/products/proxmox-virtual-environment/get-started


# On a fresh PVE installtion, connecto to it management IP using ssh
# install required packages
apt update
apt install openvswitch-switch

# Enable and confirm the servie is running
systemctl enable --now openvswitch-switch
systemctl status openvswitch-switch --no-pager

# Verify the installed version
ovs-vsctl --version

#sample output
root@hydra:~# ovs-ofctl --version
ovs-ofctl (Open vSwitch) 3.5.0
OpenFlow versions 0x1:0x6

# verify the ofctl version
ovs-ofctl --version

# sample output
root@hydra:~# ovs-ofctl --version
ovs-ofctl (Open vSwitch) 3.5.0
OpenFlow versions 0x1:0x6


# 2. Config the OVS bridge for openflow

# first backup the current Config
cp /etc/network/interfaces /etc/network/interfaces.backup

# edit the interface file with the following
nano /etc/network/interfaces

# The content 
```
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
> Note you will nee to replace 'nic4" with the actual nic number of the host, as well as replace the management interface mgmt144 with tthe correct settings and gateway or it will not be accessible.
> we use mtu 9216 as the transport mtu, the upper layer, the core switch, will also need to have mtu set to 9216. The provided config already have this set, but it is recommended to double-check after import.
> The controller address and port, for example 10.169.144.22:6653, will also needs to be updated to match the actual ryu controller, and make sure the network does not have firewall or vlan settings that block settings.


# reload the config from console
ifreload -a

# We commend reboot the host for a complete setup
reboot

# after reconnecting verify the port is connected
ovs-vsctl show

> Note the network config for VMs is only visible when the they are started.
For now we can proceed with empty output
for setting up VMs, refers to PEEL/Benchmark/vm_setup.md

# sample output 
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
...(more ports) 

> Note due to the restrictions of our lab network, our traffic are required to have vlan 144 tagged. This is not strictly needed for peel to run as long as the controller are accessible.

# if theres any error, try to set the openflow version to 1.3 again with the following command
ovs-vsctl set bridge br-ovs protocols=OpenFlow13

# as well as the controller address again
ovs-vsctl set-controller br-ovs tcp:CONTROLLER_IP:6653

# check controller status
ovs-vsctl list Controller

# sample output
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

> note the status: state=ACTIVE, this indicate the controller is working correctly and connected.

# Use the followoing command to retain access when openflow controller is not is operational state
ovs-vsctl set-fail-mode br-ovs standalone


# verify the state of openflow
ovs-ofctl -O OpenFlow13 show br-ovs

# Sample output
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

> Important: the dpid is needed for the ryu controller to match the switch, it needs to be replaced into the openflow rules "OFPT_FEATURES_REPLY (OF1.3) (xid=0x2): dpid:0000f46b8c1335c5"
> important: the number before the (tapxxxix) is the openflow port number, it will change when a VM is rebooted, and the mapping in the script will needs to be updated. to reflect the corret port for that VM.