#!/bin/bash
INSTALL_DIR=/opt/sysmon

echo "Creating $INSTALL_DIR..."
sudo mkdir -p $INSTALL_DIR/static

echo "Copying files..."
sudo cp sysmon_server.py $INSTALL_DIR/
sudo cp system_config.json $INSTALL_DIR/
sudo cp static/index.html $INSTALL_DIR/static/

echo "Setting permissions..."
sudo chmod +x $INSTALL_DIR/sysmon_server.py