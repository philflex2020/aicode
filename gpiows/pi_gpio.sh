#!/bin/bash

# Variables for IP address and port
IP_ADDRESS="192.168.86.191"
PORT="5000"

# Function to map port names to GPIO pin numbers
get_gpio_pin() {
    case "$1" in
        imx93)
            echo "17"
            ;;
        port2)
            echo "18"
            ;;
        port3)
            echo "27"
            ;;
        port4)
            echo "22"
            ;;
        *)
            echo "Unknown port name"
            exit 1
            ;;
    esac
}

# Function to send curl command
send_curl_command() {
    local action="$1"
    local pin_name="$2"
    local pin_number

    pin_number=$(get_gpio_pin "$pin_name")

    if [ "$pin_number" == "Unknown port name" ]; then
        echo "Invalid port name: $pin_name"
        exit 1
    fi

    curl -X POST -d "command=${action} ${pin_number}" http://${IP_ADDRESS}:${PORT}/gpio
    echo 
}

# Main script logic
if [ $# -ne 2 ]; then
    echo "Usage: $0 <on|off> <port_name>"
    exit 1
fi

ACTION=$(echo "$1" | tr '[:lower:]' '[:upper:]')
PORT_NAME="$2"

send_curl_command "$ACTION" "$PORT_NAME"
