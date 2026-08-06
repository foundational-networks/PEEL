## Step 1: Configure the NVIDIA Mellanox SN2700 Core Switch

To load the provided PEEL configuration onto the SN2700 switch:

1. Log in to the switch management interface through the web UI using its management IP address.
2. Navigate to **Setup → Configurations**.
3. In the **Upload Configuration** tab, select **Upload local binary file**.
4. Select the provided `src/peel` configuration file and upload it.
5. After the upload completes, locate the uploaded configuration under the **Configuration Files** tab.
6. Select the checkbox next to the uploaded configuration.
7. Click **Switch To** to activate the configuration.

> **Important:** The provided configuration is an exact export of the configuration used on our experimental switch. Before activating it, carefully review and update all environment-specific settings, including management IP addresses, interfaces, routing configuration, and controller addresses.
>
> Applying the configuration without adapting it to the target environment may cause loss of network-based management access to the switch. We strongly recommend verifying the configuration beforehand and ensuring that an alternative management method, such as console access, is available.

---

## Find the Switch DPID and OpenFlow Port Numbers

The Ryu controller identifies the SN2700 switch using its **Datapath ID (DPID)** and communicates with individual switch interfaces through their assigned **OpenFlow port numbers**.
These values must be recorded and later added to the PEEL Ryu controller configuration.

### Step 2: Connect to the Switch

Connect to the switch through SSH using its management IP address.

### Step 3: Examine the OpenFlow Configuration

Run:

```text
show openflow
```

Example output:

```text
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
```

In this example, the switch DPID is:

```text
0000b8599f5c4400
```

The OpenFlow port mappings are:

| Physical Interface | OpenFlow Port |
| ------------------ | ------------: |
| `Eth1/21`          |          `73` |
| `Eth1/22`          |          `75` |
| `Eth1/14`          |         `103` |

Record the DPID and these OpenFlow port numbers.
We also recommend recording which PVE host is physically connected to each switch interface. For example:

```text
Eth1/21 -> OF-73  -> PVE host 106
Eth1/22 -> OF-75  -> PVE host 107
Eth1/14 -> OF-103 -> PVE host 105
```

The exact mapping depends on the physical cabling of the target environment.

> **Important:** The OpenFlow port numbers used by the Ryu controller must match the values reported by `show openflow`. These mappings are later configured in `leafspine.py` using variables such as `CORE_TO_105`, `CORE_TO_106`, and `CORE_TO_107`.
