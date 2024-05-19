#!/bin/sh
# install the datameshcontroller 

cd /home/pi5
cp work/aicode/datamesh/src/*.py /usr/local/bin
cp work/aicode/datamesh/service/datameshcontroller.service /etc/systemd/system
mkdir -p /var/datamesh

sudo systemctl daemon-reload                    # Reload systemd manager configuration
sudo systemctl enable datameshcontroller.service  # Enable the service
sudo systemctl start datameshcontroller.service   # Start the service



