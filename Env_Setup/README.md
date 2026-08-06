# Setting Up the Physical Testbed Environment

This document describes how to set up the physical testbed used for PEEL experiments, including the core switch, PVE hosts, OpenFlow controller, virtual machines, and switch mappings.

---

## Step 1: Physical Topology and Hardware Requirements

Our testbed consists of the following components:

* One **NVIDIA Mellanox SN2700** core switch running `Onyx X86_64 3.9.3202 2021-08-11 15:04:27 x86_64`
* Three physical servers, each equipped with a **100 Gbps Mellanox NIC**.
* Each physical server should be capable of hosting at least **16 VMs**.
* Each VM should have at least:

  * 2 modern CPU cores
  * 2 GB of memory
* One additional VM or physical host to serve as the **Ryu OpenFlow controller**.
* The OpenFlow controller must be reachable from:

  * The core switch
  * All three PVE hosts
* Proxmox VE 9.1.1 installed on all three physical servers.

---

## Step 2: Connect the Physical Topology

Connect each of the three physical servers to the core switch through its designated 100 Gbps network interface.

The physical connection can use any compatible medium, such as:

* DAC
* AOC
* Fiber with compatible transceivers

After connecting the hosts, verify that each link is active and operating at **100 Gbps**.

Depending on the target network environment:

* Connect the host running the OpenFlow controller so that it is reachable from all three PVE hosts and the core switch.
* Connect the management interface of the core switch to the appropriate management network or upstream switch.
* Ensure that the core switch management interface is reachable from the host running the OpenFlow controller.

---

## Step 3: Configure the Core Switch

Refer to [`sdn_switch.md`](OpenFlow_Rules/sdn_switch.md) and follow **Step 1** to configure the NVIDIA Mellanox SN2700 switch.

At this stage, only perform the initial switch configuration. The DPID and OpenFlow port mappings will be collected later.

You may need to modify environment-specific settings such as:

* Management IP address
* Gateway
* VLAN configuration
* Physical interface assignments

Make sure the switch management interface receives a valid and reachable IP address.

---

## Step 4: Install Proxmox VE

Install **Proxmox VE 9.1.1** on all three physical servers.

For detailed installation instructions, refer to the [official Proxmox VE installation guide](https://www.proxmox.com/en/products/proxmox-virtual-environment/get-started).

After installation:

1. Connect each PVE host to the core switch using its designated 100 Gbps NIC.
2. Configure the PVE management interface.
3. Verify that each host can be reached through SSH.

---

## Step 5: Configure Management Networking

Ensure that each PVE host has a valid management IP address.

The management IP can be assigned either:

* Manually using a static configuration, or
* Automatically through DHCP, if supported by the local network.

For reproducibility, we recommend using static IP addresses or persistent DHCP leases.

Also verify that:

* The core switch management IP is static.
* The core switch is reachable from the OpenFlow controller.
* All PVE hosts can reach the OpenFlow controller.

---

## Step 6: Set Up the Ryu OpenFlow Controller

Refer to [`ryu_controller.md`](OpenFlow_Rules/ryu_controller.md) and follow the instructions through **Step 4**.

At this stage, install and start the controller using the provided PEEL OpenFlow application.

Do not finalize the switch DPID or OpenFlow port mappings yet; these will be updated after the physical and virtual topology has been fully configured.

---

## Step 7: Configure Open vSwitch on the PVE Hosts

Refer to [`pve_switch.md`](OpenFlow_Rules/pve_switch.md) and follow the instructions through **Step 4**.

This configures the Open vSwitch bridge and connects each PVE host to the Ryu OpenFlow controller.

Make sure that:

* The correct physical NIC is attached to the Open vSwitch bridge.
* The controller IP and port are correct.
* The configured MTU matches the rest of the testbed.
* Management connectivity remains available after applying the configuration.

---

## Step 8: Configure the Test VMs

Refer to [`Benchmark/vm_setup.md`](Benchmark/vm_setup.md)
and create the required VMs on all three PVE hosts.

Our testbed uses:

```text
3 PVE hosts × 16 VMs per host = 48 VMs
```

After the VMs are created and started, verify that they have valid network connectivity.

---

## Step 9: Collect DPIDs and OpenFlow Port Mappings

After the switches and VMs are fully configured, collect the actual switch identifiers and port mappings.

### Core Switch

Refer to [`sdn_switch.md`](OpenFlow_Rules/sdn_switch.md) starting from **Step 2** to obtain:

* Core-switch DPID
* OpenFlow port numbers
* Physical-interface-to-PVE-host mappings

### PVE Open vSwitch Instances

Refer to [`pve_switch.md`](OpenFlow_Rules/pve_switch.md) starting from **Step 5** to obtain:

* DPID of each PVE Open vSwitch
* VM-to-OpenFlow-port mappings
* Uplink OpenFlow port numbers

Record all of these values carefully.

> **Important:** These mappings depend on the actual hardware, cabling, and VM configuration and may differ from the values used in our testbed.

---

## Step 10: Update the Ryu Controller Mappings

After collecting the actual DPIDs and OpenFlow port mappings, return to [ryu_controller.md](OpenFlow_Rules/ryu_controller.md)
and follow the instructions starting from **Step 5**.

Update `leafspine.py` with:

* The DPID of the core switch
* The DPID of each PVE Open vSwitch
* The VM-to-OpenFlow-port mappings
* The core-switch-to-PVE-host port mappings

Restart the Ryu controller after applying the changes.

---

## Step 11: Validate the Setup and Run Experiments

After completing the configuration, verify that:

* All OpenFlow switches are connected to the Ryu controller.
* The configured DPIDs match the actual switches.
* VM-to-OpenFlow-port mappings are correct.
* Core-switch port mappings are correct.
* OpenFlow rules and group entries have been installed successfully.
* VMs can communicate as expected.

Once these checks pass, refer to the documentation under `PEEL/Benchmark/`
to validate PEEL forwarding and begin running experiments.
