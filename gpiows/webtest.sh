#!/bin/bash
# sudo apt install websocat
# wget https://github.com/vi/websocat/archive/refs/tags/v1.13.0.tar.gz
# websocat is a rust thing so forget it for now



# Replace with the IP address of your Raspberry Pi
RPI_IP=<RaspberryPi_IP>
PORT=8765

# Test turning GPIO pin 17 ON
echo "Testing GPIO pin 17 ON"
echo "ON 17" | websocat ws://$RPI_IP:$PORT

# Test turning GPIO pin 17 OFF
echo "Testing GPIO pin 17 OFF"
echo "OFF 17" | websocat ws://$RPI_IP:$PORT
