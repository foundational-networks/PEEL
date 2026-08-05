# Ryu Controller Setup

This section provides instructions for setting up the Ryu OpenFlow controller used by PEEL.

Our Ryu controller was configured and tested with the following environment:

| Component  | Version             |
| ---------- | ------------------- |
| OS         | Ubuntu Server 22.04 |
| Python     | 3.9.25              |
| pip        | 23.0.1              |
| setuptools | 67.6.1              |
| wheel      | 0.40.0              |
| Ryu        | 4.34                |
| eventlet   | 0.30.2              |

The following instructions assume that the controller and its dependencies are installed under `/opt`.

---

## Step 1: Install Python 3.9.25

Navigate to the helper scripts directory and run the provided installation script:

```bash
cd PEEL/Env_Setup/Helper_Scripts
bash install_python.sh
```

---

## Step 2: Create the Python Virtual Environment

Create a dedicated Python virtual environment for the Ryu controller and install and pin the required versions of `pip`, `setuptools`, and `wheel`:

```bash
/opt/python-3.9.25/bin/python3.9 -m venv /opt/venvs/ryu39
/opt/venvs/ryu39/bin/python -m pip install \
    "pip==23.0.1" \
    "setuptools==67.6.1" \
    "wheel==0.40.0"
```

---

## Step 3: Install the Ryu Controller

Install Ryu 4.34 and the required version of `eventlet`:

```bash
/opt/venvs/ryu39/bin/python -m pip install \
    --no-build-isolation \
    "ryu==4.34" \
    "eventlet==0.30.2"
```

---

## Step 4: Run the Ryu Controller

The Ryu controller can now be started using the PEEL OpenFlow rules with the following command:

```bash
/opt/venvs/ryu39/bin/python \
    /opt/venvs/ryu39/bin/ryu-manager \
    --ofp-tcp-listen-port 6653 \
    /opt/PEEL/Env_Setup/OpenFlow_Rules/src/leafspine.py
```

This starts the Ryu controller on OpenFlow TCP port `6653` and loads the PEEL leaf-spine OpenFlow application.

---

## Optional: Configure Ryu as a systemd Service

Optionally, Ryu can be configured as a `systemd` service so that the controller automatically starts when the host boots.

Create the following service file:

```text
/etc/systemd/system/ryu.service
```

Add the following contents:

```ini
[Unit]
Description=Ryu OpenFlow Controller
After=network.target

[Service]
Type=simple
#User=ryu
WorkingDirectory=/opt/venvs/ryu39
ExecStart=/opt/venvs/ryu39/bin/ryu-manager --ofp-tcp-listen-port 6653 /opt/PEEL/Env_Setup/OpenFlow_Rules/src/leafspine.py
Restart=always
RestartSec=5

# Optional hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full

[Install]
WantedBy=multi-user.target
```

Reload the `systemd` configuration:

```bash
systemctl daemon-reload
```

Enable the Ryu service so that it starts automatically when the host boots:

```bash
systemctl enable ryu
```

Start the service:

```bash
systemctl start ryu
```

Verify that the controller is running successfully:

```bash
systemctl status ryu
```

If the setup is successful, the service should be reported as `active (running)`.

---

## Step 5: Update the leafspine.py script with actual dpid and port amppings. 
## 5.1 Find dpid
Our script will match openflow switch by their dpid. 
Please refer to pve_switch.md to find the dpid and openflow ports of the each PVE openswitch.
Please refer to sdn_switch.md to find the dpid and the openflow ports of the core switch.

## 5.2 map the dpid to the openflow rules leafspine.py
From line 22 - 26 of the leafspine.py, you will find the following info:

    # ---------- DPIDs ----------
    DPID_CORE = int("0000b8599f5c4400", 16)
    DPID_105  = int("0000f46b8c134345", 16)
    DPID_106  = int("0000f46b8c132d65", 16)
    DPID_107  = int("0000f46b8c1335c5", 16)

Replace the dpid in the int function to match the actual dpid of the core switch and each pve hosts.

# 5.3 map the openflow port for PVE hosts and VMs.
fron line 57 - 72 of the leafspine.py, you will find the following:
    SW105_HOST_PORTS = [
        3, 4, None, None, None, None, None, None,
        None, None, None, None, None, None, None, None,
    ]

    SW106_HOST_PORTS = [
        3, 4, 5, 6, 7, 8, 9, 10,
        None, None, None, None, None, None, None, None,
    ]

    SW107_HOST_PORTS = [
        3, 4, 5, 6, 7, 8, 9, 10,
        None, None, None, None, None, None, None, None,
    ]

Each PVe hosts has space of 16 vms in our current script, and their index is matched by the position in the list.
For example, position 0 in arrary SW105_HOST_PORTS is "3", which means, the openflow port of the first VM on host 105 is "3". this needs to match the actual openflow port outputted by the pve openswitch. which we record earlier.
> if a VM is not configured or not needed in the experiment, leave the corresponding index as "None"

# 5.4 map the openflow port for the core switch.
from line 74 - 81, you will find the following:

    # ---------- Core/spine ports ----------
    # logical core port 0 -> switch 105
    # logical core port 1 -> switch 106
    # logical core port 2 -> switch 107
    CORE_TO_105 = 103
    CORE_TO_106 = 73
    CORE_TO_107 = 75
    CORE_LEAF_PORTS = [CORE_TO_105, CORE_TO_106, CORE_TO_107]

same as the PVE port, the list "CORE_LEAF_PORTS" contains the logical mapping of the actual physical hosts on core switch.


# 5.5 apply the changes.
make sure the dpid and port mappings are correct, then apply the changes by restarting the openflow Controller
systemctl restart ryu.

