# SDN Switch Configuration

Our test network uses the following topology:

* One **NVIDIA Mellanox SN2700** core switch
* Three **PVE software switches**

OpenFlow is enabled on all switches.

---

## Configure the NVIDIA Mellanox SN2700 Core Switch

To load the provided PEEL configuration onto the SN2700 switch:

1. Log into the switch management interface through the web UI (i.e., the IP of the management interface).
2. Navigate to **Setup → Configurations**.
3. In the **Upload Configuration** tab, select **Upload local binary file**.
4. Select the provided `src/peel` configuration file and upload it.
5. After the upload completes, locate the uploaded configuration under the **Configuration Files** tab.
6. Select the checkbox next to the uploaded configuration.
7. Click **Switch To** to activate the configuration.

> **Important:** The provided configuration is an exact export of the configuration used on our experimental switch. Before activating it, carefully review and adjust settings such as management IP addresses, interfaces, routing, and other environment-specific parameters.
>
> Applying the configuration without adapting it to your environment may cause loss of network-based management access to the switch. We strongly recommend verifying the configuration and ensuring that an alternative management method, such as console access, is available before switching to it.


## Find the dpid and openflow port
#1. log in to ssh of the switch using its management interface ip

#2. Examine the OpenFlow config
switch-eth100a [standalone: master] > show openflow

# sample output
switch-eth100a [standalone: master] > show openflow
OpenFlow Version: OpenFlow 1.3
Datapath ID: 0000b8599f5c4400

Controllers Information:
  ----------------------------------------------------------------------------------------
  Controller                State            Role       Changed (sec)  Last Error
  ----------------------------------------------------------------------------------------
  tcp:10.169.144.22:6653    ACTIVE           other      89071          N/A      


Mapping of OpenFlow ports to their OpenFlow numbers:
  -----------------------
  Interface       OF-Port
  -----------------------
  Eth1/21         OF-73
  Eth1/22         OF-75
  Eth1/14         OF-103
switch-eth100a [standalone: master] >


# From above info, "0000b8599f5c4400" is the dpid, and "73/75/103" is the openflow ports needs to be recorded. we recommend take a note on the corresponding physical interface and the PVE hosts it connects to.