# Switch Configuration

This directory contains the configuration files and instructions required to set up the OpenFlow controller, configure the SDN switches, and deploy the VMs on the PVE nodes used by PEEL.


## Files

* `src/leafspine.py`: Ryu OpenFlow application containing the forwarding rules installed on all four SDN switches in our testbed.
* `src/peel`: Configuration file exported from the NVIDIA Mellanox SN2700 core OpenFlow switch used in our experiments.

> **Note:** Due to restrictions in our laboratory network, VLAN `144` is used for both management and experimental traffic. This VLAN configuration is specific to our environment and should be adjusted as needed for other deployments.

---

## Configure the Ryu Controller

Refer to [`ryu_controller.md`](ryu_controller.md) for instructions on installing and configuring the Ryu controller.

> **Important:** The switch DPIDs and OpenFlow port mappings depend on the physical topology and VM configuration of the target environment. The mappings currently defined in `src/leafspine.py` correspond to our experimental testbed and will likely need to be updated before use.

---

## Configure the SDN Core Switch

Refer to: [`sdn_switch.md`](sdn_switch.md) for instructions on configuring the physical SDN core switch.

> **Important:** The provided `src/peel` configuration is an exact export from the NVIDIA Mellanox SN2700 switch used in our experiments. Before applying it, review and update all environment-specific settings, including interface assignments, OpenFlow port mappings, management addresses, VLAN configuration, and controller information, to match the target hardware and network.
---

## Configure the PVE Open vSwitch

Refer to: [`pve_switch.md`](pve_switch.md) for instructions on configuring virtual machines on each PVE nodes.
