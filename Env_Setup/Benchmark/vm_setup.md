# Setting up the test VM.
Our test network consist of 3 physical pve hosts in a cluster, and a total of 48 test nodes with 16 on each PVE host.

Each has the following config:
OS: Ubuntu Server 22.04.5 LTS
CPU: at least 2 Cores with modern CPU. (for example we use Intel Xeon Platinum 8275CL/8269CY/8259CL)
Memory: 2GB DDR4
Network: PVE virtual NIC with Model VirtIO(paravirtualized), MTU same as bridge (9216), and Multiqueue set to 8. bridge: br-ovs
Disk: 32GB SSD PVE Virtual Disk


# Install the OS
Follow the official PVE and Ubuntu installation guide
https://support.us.ovhcloud.com/hc/en-us/articles/360010916620-How-to-Create-a-VM-in-Proxmox-VE#create
https://ubuntu.com/tutorials/install-ubuntu-server

# once the installation is completed, logged in to the SSH and switch to the root user for convenient.
sudo su root

# optionaly change the password
passwd

# Install basic packages
apt update
apt install -y unzip build-essential libssl-dev zlib1g-dev libncurses5-dev libncursesw5-dev libreadline-dev libsqlite3-dev libgdbm-dev libdb5.3-dev libbz2-dev libexpat1-dev liblzma-dev tk-dev wget curl xz-utils nano screen iptables sudo autoconf net-tools tcpdump

# Network config
Our environment use DHCP acquire static lease from the DHCP Server
A manually configured IP is also acceptable

While a dynamic IP will work, we recommend pining the IP of each VM for consistency.


# Configure other VMs.
Repeat the steps on each hosts until the number of nodes are sufficient for experiment

We recommend using PVE's template function to quickly distribute configured VM, see official guide:
https://pve.proxmox.com/wiki/VM_Templates_and_Clones

> Note: Once all VMs are in place, we recommend power off them all and use the bulk start function in PVE to start them in sequence, so the OpenFlow port mapping stays in order.


# If using template function
# It is also recommended to have gloo precomiled and installed in the OS, and then distributed with the template.
refers to /PEEL/Benchmark/gloo_install
# It is also recommended to have your SSH public key added to the host before turning it to a template.