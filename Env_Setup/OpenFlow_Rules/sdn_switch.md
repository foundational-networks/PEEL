Requirement:
our test network has the following topology
a core NVIDIA Mellanox SN2700 switch
Three PVE software switch
All with openflow enabled.

For the SN2700 core switch,
To load the switch config:
1. login to the management interface via Web
2. Go to Setup - Configurations
3. In the Upload Configuration tab, choose "Upload local binary file" and select 'src/peel" config.
4. Once uploaded, check the box in front of the "Configuration Files" tab on top of the page, and click the "Switch To" button.

> NOTE: The config is an exact export from our experiment switch, please make sure to adjust the setting and test before switch to it, or the management access via network might be lost.

