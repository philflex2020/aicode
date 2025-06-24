#!/bin/bash

set -e

# Where to install the system monitor
INSTALL_DIR="/opt/sysmon"
VENV_DIR="$INSTALL_DIR/venv"
SERVICE_FILE="/etc/systemd/system/sysmon.service"

echo "Creating install dir at $INSTALL_DIR"
sudo mkdir -p "$INSTALL_DIR"
sudo cp sysmon_server.py system_config.json -t "$INSTALL_DIR"
sudo cp -r static "$INSTALL_DIR"

# Create virtual environment
echo "Creating virtual environment in $VENV_DIR"
sudo python3 -m venv "$VENV_DIR"

# Install dependencies
echo "Installing Flask in virtual environment..."
sudo "$VENV_DIR/bin/pip" install --upgrade pip
sudo "$VENV_DIR/bin/pip" install flask

# Create systemd service
echo "Creating systemd service..."
sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=System Monitor Web Server
After=network.target

[Service]
Type=simple
WorkingDirectory=$INSTALL_DIR
ExecStart=$VENV_DIR/bin/python sysmon_server.py
Restart=always
User=pi
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd and enable the service
echo "Enabling and starting sysmon service..."
sudo systemctl daemon-reload
sudo systemctl enable sysmon.service
sudo systemctl start sysmon.service

echo "✅ System monitor installed and running on http://<this-pi>:8081"

