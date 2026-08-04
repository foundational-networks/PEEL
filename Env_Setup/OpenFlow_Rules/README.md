files:
src/leafspine.py : openflow rules loaded to all four sdn switches
src/peel : switch config loaded to the core mellanox sn2700 openflow switch

Note:
Due to the limit of the lab networks, the switch is configured with VLAN 144 for all traffic, including management port and all experiment traffic.


1. configuring ryu controller
--> ryu_controller.md
Note that the actual port mapping entirely depend on the actual host and switches setup, and is likely different from the mapping we used in the leafspine.py



2. configure the sdn switch
--> sdn_switch.md
Note that the config provided is an exact copy exported from our experiment switch, Nvidia Mellanox SN2700. the port mapping and configurations will needs to be updated to match the actual equipment used