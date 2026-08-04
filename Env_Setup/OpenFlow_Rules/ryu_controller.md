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

