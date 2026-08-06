# Setting up the environment

# 1. Physical Topology & configuration
Our Testbed physically contains the following parts:
- a core switch Nvidia Mellanox SN2700, running firmware Onyx X86_64 3.9.3202 2021-08-11 15:04:27 x86_64
- Three Physical servers have 100g Mellanox NIC installed, and each capable of hosting 16 VMs or more, each VM should be at least has two modern CPU cores and 2GB of memory.
- We assumed there is also a separate VM or physical host available to server as the openflow controller, and connected and accessibled by all four euipqment (core switch & three servers)
- Proxmox VE 9.1.1


# 2. Setting up the Topology
connect all three server to the core switch through its designated physical network interface using either a DAC/AOC/Fiber&Tranceiver connection. Make sure the link is up at 100Gbps.
Depending on the actual network, also connect the host prepared for the openflow controller
Depending on the actual network, make sure the core switch's management interface is connected (to itself or an upper layer swtich, needs to be accessible by the openflow switch)


# 3. Setting up the Core switch
Refers to sdn_switch.md [step 1] to have the Mellanox SN2700 switch setup. we will come back to step 2 and 3 later.
You might need to adjust the management interface to match the actual network condition and obtain a valid IP for it.

# 4. Install Proxmox VE

Install **Proxmox VE 9.1.1** on all three physical hosts and connect each host to the core switch 
For detailed installation instructions, refer to the [official Proxmox VE installation guide](https://www.proxmox.com/en/products/proxmox-virtual-environment/get-started).
After completing the PVE installation, connect to the management IP address of each host using SSH.

# 5. Management IP
Once installed, make sure to have PVE management interface a valid IP, you can either manually assign or automatic get one using DHCP if local network support it.
Also double check here to make sure the core switch's management interface IP is static and accessible

# 6. Prepare the OpenFlow Ryu Controller
Refers to ryu_controller.md until step 4 to have it installed with our provided rules.

# 7. Prepare the PVE OpenSwitch
Refers to pve_switch.md until step 4 to have the openswitch configured.

# 8. Config the VM
Refers to Benchmark/vm_setup.md to have VMs configured on all hosts.

# 9. Obtain the DataPath ID and OpenFlow Port numbers
For the core switch, refers to sdn_switch.md step 2 onward
For the pve switch, refers to pve_switch.md setp 5 onward
take a note of all the port mappings and datapath id.

# 10. Apply the updated mappings
Refers to step 5 onwards in ryu_ctonroller.md to have the mapping loaded into the openflow controller.

# 11. Testing & Experimenting
If everything configured correctly, refers to the PEEL/Benchmark to start experimenting.
