Our Ryu controller are configured with the following packages and environment:
OS: Ubuntu 22.04 Server
Python: 3.9.25
pip: 23.0.1
setuptools: 67.6.1
wheel: 0.40.0
ryu: 4.34
eventlet: 0.30.2

# Pick a directory for the controller, we use /opt for example
cd /opt

# install git if not already
apt update
apt install git -y

# Clone the repo
git clone https://github.com/foundational-networks/PEEL.git

# Enter the helper scrips dir
cd PEEL/Env_Setup/Helper_Scripts

# This will install and compile python 3.9.25 to our /opt dir
bash install_python.sh

# Create python venv
/opt/python-3.9.25/bin/python3.9 -m venv /opt/venvs/ryu39

# Install and pin the setuptools.
/opt/venvs/ryu39/bin/python -m pip install \
    "pip==23.0.1" \
    "setuptools==67.6.1" \
    "wheel==0.40.0"

# Install ryu controller
/opt/venvs/ryu39/bin/python -m pip install \
    --no-build-isolation \
    "ryu==4.34" \
    "eventlet==0.30.2"

# The ryu controller will be able to be ran with the following commands, with our openflow rules.
/opt/venvs/ryu39/bin/python /opt/venvs/ryu39/bin/ryu-manager --ofp-tcp-listen-port 6653 /opt/PEEL/Env_Setup/OpenFlow_Rules/src/leafspine.py

# optionally to configure it to run as a systemd service and start with the system.

create a new systemd file at /etc/systemd/system/ryu.service and put the following there:
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

# Reload systemd
systemctl daemon-reload

# Start with Host boot
systemctl enable ryu

# Start now
systemctl start ryu

# Check status
systemctl status ryu 