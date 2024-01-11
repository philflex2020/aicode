#!/bin/bash
# install the datameshcontroller 
#!/bin/bash

model=$(tr -d '\0' < /proc/device-tree/model)

if [[ $model == *"Raspberry Pi Compute Module 4"* ]]; then
    echo "Running on Raspberry Pi Compute Module 4"
    cd /home/pi
    cp work/aicode/datamesh/src/*.py /usr/local/bin
    cp work/aicode/datamesh/service/datameshcontroller4.service /etc/systemd/system/datameshcontroller.service
    mkdir -p /var/datamesh
elif [[ $model == *"Raspberry Pi 5"* ]]; then
    echo "Running on Raspberry Pi 5"
    cd /home/pi5
    cp work/aicode/datamesh/src/*.py /usr/local/bin
    cp work/aicode/datamesh/service/datameshcontroller5.service /etc/systemd/system/datameshcontroller.service
    mkdir -p /var/datamesh

else
    echo "Unknown model: $model"
fi



sudo systemctl daemon-reload                    # Reload systemd manager configuration
sudo systemctl enable datameshcontroller.service  # Enable the service
sudo systemctl start datameshcontroller.service   # Start the service



