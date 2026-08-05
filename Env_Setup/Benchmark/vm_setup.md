# Setting Up the Test VMs

Our test network consists of **three physical PVE hosts** configured as a cluster, with a total of **48 test VMs**. Each PVE host runs **16 VMs**.
Each VM is configured with the following resources:

| Component  | Configuration                              |
| ---------- | ------------------------------------------ |
| OS         | Ubuntu Server 22.04.5 LTS                  |
| CPU        | At least 2 CPU cores on a modern processor |
| Memory     | 2 GB DDR4                                  |
| Network    | PVE VirtIO (paravirtualized) NIC           |
| MTU        | 9216, matching the Open vSwitch bridge     |
| Multiqueue | 8                                          |
| Bridge     | `br-ovs`                                   |
| Disk       | 32 GB SSD-backed PVE virtual disk          |

For reference, our physical hosts use Intel Xeon Platinum CPUs, including models such as the 8275CL, 8269CY, and 8259CL.

---

## Step 1: Create and Install the VM

Create a VM on the PVE host and install Ubuntu Server 22.04.5 LTS.

For detailed instructions, refer to the official guides:

* [Creating a VM in Proxmox VE](https://support.us.ovhcloud.com/hc/en-us/articles/360010916620-How-to-Create-a-VM-in-Proxmox-VE#create)
* [Installing Ubuntu Server](https://ubuntu.com/tutorials/install-ubuntu-server)

After the installation is complete, connect to the VM through SSH.
For convenience during setup, switch to the root user:

```bash
sudo su root
# Optionally, set or update the root password
passwd
```

---

## Step 2: Install Basic Packages

Update the package index and install the required utilities and development packages:

```bash
apt update
apt install -y \
    unzip \
    build-essential \
    libssl-dev \
    zlib1g-dev \
    libncurses5-dev \
    libncursesw5-dev \
    libreadline-dev \
    libsqlite3-dev \
    libgdbm-dev \
    libdb5.3-dev \
    libbz2-dev \
    libexpat1-dev \
    liblzma-dev \
    tk-dev \
    wget \
    curl \
    xz-utils \
    nano \
    screen \
    iptables \
    sudo \
    autoconf \
    net-tools \
    tcpdump
```

---

## Step 3: Configure Networking

In our environment, each VM obtains its IP address through DHCP using a static lease configured on the DHCP server.
A manually configured static IP address is also acceptable.
While dynamically assigned IP addresses can work, we recommend assigning each VM a persistent IP address for consistency across experiments.
Each VM should use:

* VirtIO as the virtual NIC model
* `br-ovs` as the bridge
* MTU `9216`
* Multiqueue `8`

The VM MTU should match the MTU configured on the PVE Open vSwitch bridge and the upstream core switch.

---

## Step 4: Create the Remaining VMs

Repeat the setup procedure until the required number of VMs has been created.
Our experimental setup uses:

```text
3 PVE hosts × 16 VMs per host = 48 VMs
```

To simplify deployment, we recommend configuring one VM completely and then using the PVE template and cloning functionality to replicate it.
Refer to the official Proxmox documentation:

* [VM Templates and Clones](https://pve.proxmox.com/wiki/VM_Templates_and_Clones)

Before converting a VM into a template, we recommend completing the remaining software setup described below.

---

## Step 5: Preinstall Gloo Before Creating the Template

If the PVE template functionality is used, we recommend compiling and installing Gloo on the base VM before converting it into a template.
This allows all cloned VMs to inherit the same Gloo installation and dependency versions.
Refer to `PEEL/Benchmark/gloo_install` for the Gloo installation instructions.


---

## Step 6: Configure SSH Access

Before converting the base VM into a template, we also recommend adding the required SSH public keys.
This allows the cloned VMs to be accessed without having to configure SSH authentication individually on every node.
Make sure that the resulting SSH configuration matches the security requirements of the target environment.

---

## Step 7: Start the VMs and Verify OpenFlow Port Ordering

After all VMs have been created, we recommend powering them off and then starting them in a consistent sequence using PVE's bulk-start functionality.
This helps keep the OpenFlow port assignments of the VM `tap` interfaces as predictable as possible.

> **Important:** OpenFlow port numbers associated with VM interfaces may change when VMs are restarted. After starting the VMs, always verify the VM-to-OpenFlow-port mappings on each PVE host before running an experiment.

The verified mappings must match the corresponding entries configured in the Ryu controller's `leafspine.py` file.

