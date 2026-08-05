# Ryu Controller Setup

This section provides instructions for setting up and configuring the Ryu OpenFlow controller used by PEEL.
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

This script installs Python 3.9.25 under `/opt`.

---

## Step 2: Create the Python Virtual Environment

Create a dedicated Python virtual environment for the Ryu controller:

```bash
/opt/python-3.9.25/bin/python3.9 -m venv /opt/venvs/ryu39
```

Install and pin the required versions of `pip`, `setuptools`, and `wheel`:

```bash
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

The Ryu controller can now be started with the PEEL OpenFlow application using:

```bash
/opt/venvs/ryu39/bin/python \
    /opt/venvs/ryu39/bin/ryu-manager \
    --ofp-tcp-listen-port 6653 \
    /opt/PEEL/Env_Setup/OpenFlow_Rules/src/leafspine.py
```

This starts the Ryu controller on OpenFlow TCP port `6653` and loads the PEEL leaf-spine OpenFlow application.

---

## Optional: Configure Ryu as a systemd Service

Ryu can optionally be configured as a `systemd` service so that the controller automatically starts when the host boots.

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

Enable the service so that it starts automatically when the host boots:

```bash
systemctl enable ryu
```

Start the service:

```bash
systemctl start ryu
```

Verify that the controller is running:

```bash
systemctl status ryu
```

If the setup is successful, the service should be reported as:

```text
active (running)
```

---

## Step 5: Configure `leafspine.py`

Before running experiments, the switch DPIDs and OpenFlow port mappings in `leafspine.py` must be updated to match the actual experimental topology. The relevant file is:

```text
PEEL/Env_Setup/OpenFlow_Rules/src/leafspine.py
```

### Step 5.1: Identify the Switch DPIDs and OpenFlow Ports

The Ryu controller identifies each OpenFlow switch using its **Datapath ID (DPID)**.
Refer to the following setup instructions to determine the DPID and OpenFlow port numbers of each switch:

* `pve_switch.md`: DPID and OpenFlow ports of each PVE Open vSwitch
* `sdn_switch.md`: DPID and OpenFlow ports of the physical core switch

Record these values before modifying `leafspine.py`.

---

### Step 5.2: Configure the Switch DPIDs

Around lines 22–26 of `leafspine.py`, you will find the DPID configuration:

```python
# ---------- DPIDs ----------
DPID_CORE = int("0000b8599f5c4400", 16)
DPID_105  = int("0000f46b8c134345", 16)
DPID_106  = int("0000f46b8c132d65", 16)
DPID_107  = int("0000f46b8c1335c5", 16)
```

Replace the hexadecimal DPID values with the actual DPIDs of:

* The physical core switch
* PVE host 105
* PVE host 106
* PVE host 107

For example:

```python
DPID_CORE = int("<CORE_SWITCH_DPID>", 16)
DPID_105  = int("<PVE_105_DPID>", 16)
DPID_106  = int("<PVE_106_DPID>", 16)
DPID_107  = int("<PVE_107_DPID>", 16)
```

The DPID values must exactly match those reported by the corresponding OpenFlow switches.

---

### Step 5.3: Configure the PVE VM OpenFlow Port Mappings

Around lines 57–72 of `leafspine.py`, you will find the VM-to-OpenFlow-port mappings:

```python
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
```

Each PVE host supports up to **16 VMs** in the current controller script. Each position in the list corresponds to a specific VM, while the value stored at that position is the VM's OpenFlow port number. For example:

```python
SW105_HOST_PORTS = [
    3, 4, None, ...
]
```

means:

* VM 0 on host 105 uses OpenFlow port `3`
* VM 1 on host 105 uses OpenFlow port `4`

These values must match the actual OpenFlow port numbers reported by the Open vSwitch running on the corresponding PVE host.

> **Note:** If a VM is not configured or is not used in the experiment, leave its corresponding entry as `None`.

For example:

```python
SW105_HOST_PORTS = [
    3, 4, None, None, ...
]
```

indicates that only the first two VM positions are currently active.

> **Important:** VM OpenFlow port numbers may change when VMs are restarted. Always verify the current VM-to-port mapping before running an experiment.

---

### Step 5.4: Configure the Core-Switch Port Mapping

Around lines 74–81 of `leafspine.py`, you will find the mappings between the core switch and the three PVE hosts:

```python
# ---------- Core/spine ports ----------
# logical core port 0 -> switch 105
# logical core port 1 -> switch 106
# logical core port 2 -> switch 107

CORE_TO_105 = 103
CORE_TO_106 = 73
CORE_TO_107 = 75

CORE_LEAF_PORTS = [CORE_TO_105, CORE_TO_106, CORE_TO_107]
```

Each value represents the OpenFlow port on the physical core switch that connects to the corresponding PVE host.
For example, `CORE_TO_105 = 103` means that PVE host 105 is connected to OpenFlow port `103` on the core switch.



Update:

```python
CORE_TO_105
CORE_TO_106
CORE_TO_107
```

to match the actual physical/OpenFlow port mappings of the target topology.
The order of:

```python
CORE_LEAF_PORTS = [CORE_TO_105, CORE_TO_106, CORE_TO_107]
```

defines the logical mapping between the core switch and PVE hosts 105, 106, and 107.

---

### Step 5.5: Apply the Updated Configuration

Before restarting the controller, verify that:

1. The DPID of each switch matches the value configured in `leafspine.py`.
2. The OpenFlow port mappings for all active VMs are correct.
3. The core-switch ports correctly map to the corresponding PVE hosts.

After making the required changes, restart the Ryu controller:

```bash
systemctl restart ryu
```

Verify that it restarted successfully:

```bash
systemctl status ryu
```

If Ryu is not configured as a `systemd` service, stop the currently running controller process and restart it manually using:

```bash
/opt/venvs/ryu39/bin/python \
    /opt/venvs/ryu39/bin/ryu-manager \
    --ofp-tcp-listen-port 6653 \
    /opt/PEEL/Env_Setup/OpenFlow_Rules/src/leafspine.py
```

The controller is now configured to recognize the physical core switch, the three PVE Open vSwitch instances, and the VMs participating in the PEEL experiments.
