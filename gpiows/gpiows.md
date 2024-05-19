Creating a WebSocket application to control GPIO pins on a Raspberry Pi 3 involves a few steps. Here's a high-level overview:

1. **Set up the Raspberry Pi**: Install the necessary libraries and software.
2. **Create a WebSocket server**: This server will handle incoming WebSocket connections and control the GPIO pins.
3. **Create a WebSocket client**: This client will connect to the WebSocket server and send commands to turn GPIO pins on and off.

### Step 1: Set Up the Raspberry Pi

1. **Update and Upgrade the System**:
   ```bash
   sudo apt update
   sudo apt upgrade
   ```

2. **Install Required Libraries**:
   - **Python**: Ensure Python is installed.
   - **RPi.GPIO**: This library provides GPIO control.
   - **websockets**: This library provides WebSocket server and client functionality.

   ```bash
   sudo apt install python3-pip
   pip3 install RPi.GPIO
   pip3 install websockets
   ```

### Step 2: Create the WebSocket Server

Create a Python script (`websocket_gpio_server.py`) for the WebSocket server that controls the GPIO pins.

```python
import asyncio
import RPi.GPIO as GPIO
import websockets

# GPIO setup
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# Define GPIO pins to control
PINS = [17, 18, 27, 22]

for pin in PINS:
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)

async def gpio_handler(websocket, path):
    async for message in websocket:
        print(f"Received message: {message}")
        command, pin_str = message.split()
        pin = int(pin_str)
        
        if command == "ON" and pin in PINS:
            GPIO.output(pin, GPIO.HIGH)
            await websocket.send(f"Pin {pin} turned ON")
        elif command == "OFF" and pin in PINS:
            GPIO.output(pin, GPIO.LOW)
            await websocket.send(f"Pin {pin} turned OFF")
        else:
            await websocket.send(f"Invalid command or pin")

start_server = websockets.serve(gpio_handler, "0.0.0.0", 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
```

### Step 3: Create the WebSocket Client

Create an HTML file (`index.html`) with JavaScript to connect to the WebSocket server and send commands.

```html
<!DOCTYPE html>
<html>
<head>
    <title>WebSocket GPIO Control</title>
</head>
<body>
    <h1>GPIO Control via WebSocket</h1>
    <label for="pin">Pin:</label>
    <input type="number" id="pin" name="pin" min="0"><br><br>
    <button onclick="sendCommand('ON')">Turn ON</button>
    <button onclick="sendCommand('OFF')">Turn OFF</button>
    <p id="status"></p>

    <script>
        const ws = new WebSocket('ws://<RaspberryPi_IP>:8765');

        ws.onopen = function() {
            document.getElementById('status').innerHTML = 'Connected';
        };

        ws.onmessage = function(event) {
            document.getElementById('status').innerHTML = event.data;
        };

        ws.onclose = function() {
            document.getElementById('status').innerHTML = 'Disconnected';
        };

        function sendCommand(command) {
            const pin = document.getElementById('pin').value;
            ws.send(`${command} ${pin}`);
        }
    </script>
</body>
</html>
```

Replace `<RaspberryPi_IP>` with the IP address of your Raspberry Pi.

### Running the Application

1. **Run the WebSocket Server**:
   ```bash
   python3 websocket_gpio_server.py
   ```

2. **Open the WebSocket Client**:
   - Open the `index.html` file in a web browser.
   - Enter the GPIO pin number and click the buttons to turn the pin on and off.

### Summary

This example sets up a WebSocket server on the Raspberry Pi to control GPIO pins and a simple web-based client to send commands to the server. The server listens for commands to turn specific GPIO pins on or off and controls the pins accordingly. The client connects to the server, sends commands, and displays the server's responses.


testing

Without `websocat`, testing WebSocket communication using `curl` is not possible because `curl` does not support WebSocket. However, you can use Python scripts or other programming languages that support WebSocket communication for testing purposes. Below, I'll show you how to do this using a Python script with the `websockets` library.

### Installing the `websockets` Library

If you haven't already installed the `websockets` library, you can do so using `pip`:

```bash
pip install websockets
```

### WebSocket Client Script in Python

Create a Python script (`websocket_client.py`) to test the WebSocket server:

```python
import asyncio
import websockets

async def test_gpio():
    uri = "ws://<RaspberryPi_IP>:8765"
    async with websockets.connect(uri) as websocket:
        # Turn GPIO pin 17 ON
        await websocket.send("ON 17")
        response = await websocket.recv()
        print(f"Response: {response}")

        # Turn GPIO pin 17 OFF
        await websocket.send("OFF 17")
        response = await websocket.recv()
        print(f"Response: {response}")

asyncio.get_event_loop().run_until_complete(test_gpio())
```

Replace `<RaspberryPi_IP>` with the IP address of your Raspberry Pi.

### Running the WebSocket Client Script

Run the client script to test the WebSocket server:

```bash
python websocket_client.py
```

### Using `curl` for HTTP Server (if needed)

If you want to use `curl` to interact with an HTTP server (not WebSocket), you can set up a simple HTTP server on your Raspberry Pi that communicates with the WebSocket server. Below is an example using Flask:

#### Installing Flask

```bash
pip install flask
```

#### Creating the HTTP to WebSocket Bridge

Create an HTTP server (`http_to_ws_bridge.py`) that forwards HTTP requests to the WebSocket server:

```python
from flask import Flask, request
import asyncio
import websockets

app = Flask(__name__)

async def send_ws_command(command):
    uri = "ws://<RaspberryPi_IP>:8765"
    async with websockets.connect(uri) as websocket:
        await websocket.send(command)
        response = await websocket.recv()
        return response

@app.route('/gpio', methods=['POST'])
def control_gpio():
    command = request.form['command']
    response = asyncio.run(send_ws_command(command))
    return response

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
```

Replace `<RaspberryPi_IP>` with the IP address of your Raspberry Pi.

### Running the HTTP to WebSocket Bridge

Run the HTTP server:

```bash
python http_to_ws_bridge.py
```

### Testing with `curl`

Now you can use `curl` to send commands to the HTTP server, which will forward them to the WebSocket server:

```bash
# Turn GPIO pin 17 ON
curl -X POST -d "command=ON 17" http://<RaspberryPi_IP>:5000/gpio

# Turn GPIO pin 17 OFF
curl -X POST -d "command=OFF 17" http://<RaspberryPi_IP>:5000/gpio
```

Replace `<RaspberryPi_IP>` with the IP address of your Raspberry Pi.

### Summary

While `curl` cannot directly communicate with a WebSocket server, you can use a Python script with the `websockets` library to test the WebSocket server. Alternatively, you can set up an HTTP server as a bridge to forward HTTP requests to the WebSocket server, allowing you to use `curl` for testing.

pi@192.168.86.191

# Turn GPIO pin 17 ON
curl -X POST -d "command=ON 17" http://192.168.86.191:5000/gpio

# Turn GPIO pin 17 OFF
curl -X POST -d "command=OFF 17" http://192.168.86.191:5000/gpio

To see the GPIO pins working on a Raspberry Pi 3, you can use LEDs as visual indicators. Here's a step-by-step guide on how to wire up the Raspberry Pi and the necessary components.

### Components Needed
- Raspberry Pi 3
- Breadboard
- LEDs (1 or more)
- Resistors (330Ω or 220Ω for each LED)
- Jumper wires

### GPIO Pin Layout
Refer to the GPIO pin layout for the Raspberry Pi 3:
```
 3.3V  (1) (2)  5V
 GPIO2 (3) (4)  5V
 GPIO3 (5) (6)  GND
 GPIO4 (7) (8)  GPIO14 (TXD)
   GND (9) (10) GPIO15 (RXD)
GPIO17 (11) (12) GPIO18
GPIO27 (13) (14) GND
GPIO22 (15) (16) GPIO23
 3.3V (17) (18) GPIO24
GPIO10 (19) (20) GND
 GPIO9 (21) (22) GPIO25
GPIO11 (23) (24) GPIO8
   GND (25) (26) GPIO7
 GPIO0 (27) (28) GPIO1
 GPIO5 (29) (30) GND
 GPIO6 (31) (32) GPIO12
GPIO13 (33) (34) GND
GPIO19 (35) (36) GPIO16
GPIO26 (37) (38) GPIO20
   GND (39) (40) GPIO21
```

### Wiring Diagram
1. **Connect LEDs to GPIO Pins**:
   - Connect the anode (longer leg) of each LED to a GPIO pin (e.g., GPIO17, GPIO18, GPIO27, GPIO22).
   - Connect the cathode (shorter leg) of each LED to one end of a resistor.
   - Connect the other end of each resistor to a ground (GND) pin on the Raspberry Pi.

2. **Example Wiring for GPIO17**:
   - **GPIO17** (Pin 11) → Anode of LED
   - **Cathode of LED** → Resistor (330Ω or 220Ω)
   - **Resistor** → GND (Pin 6, 14, 20, 25, 30, 34, or 39)

### Detailed Wiring Steps
1. **Prepare the Breadboard**:
   - Insert the LEDs into the breadboard.
   - Connect one leg of each resistor to the cathode (shorter leg) of the LEDs.

2. **Connect the GPIO Pins**:
   - Use jumper wires to connect the GPIO pins on the Raspberry Pi to the anode (longer leg) of each LED.
   - Use additional jumper wires to connect the other leg of the resistors to the GND rail on the breadboard.

### Example Wiring for Multiple LEDs
Assuming you are using GPIO17, GPIO18, GPIO27, and GPIO22:

- **GPIO17** (Pin 11) → Anode of LED1
- **GPIO18** (Pin 12) → Anode of LED2
- **GPIO27** (Pin 13) → Anode of LED3
- **GPIO22** (Pin 15) → Anode of LED4

- **Cathode of LED1** → Resistor → GND
- **Cathode of LED2** → Resistor → GND
- **Cathode of LED3** → Resistor → GND
- **Cathode of LED4** → Resistor → GND

### Example Code to Control GPIO Pins
Make sure you have your WebSocket server script (`websocket_gpio_server.py`) set up as described earlier. This script will control the GPIO pins based on WebSocket messages.

### Running the Application
1. **Start the WebSocket Server**:
   ```bash
   python3 websocket_gpio_server.py
   ```

2. **Use a WebSocket Client** (HTML page) to control the GPIO pins:
   - Open the `index.html` file in a web browser.
   - Enter the GPIO pin number and click the buttons to turn the pin on and off.

### Summary
By wiring the LEDs to the GPIO pins on the Raspberry Pi 3 and using the WebSocket server, you can visually verify that the GPIO pins are being controlled as expected. The LEDs will turn on and off based on the commands sent through the WebSocket client. This setup allows you to test and demonstrate GPIO control via a web interface.

Sure, let's create a simple shell script that will allow you to trigger the `curl` commands to control the GPIO pins. This script will take commands in the form `gpio on <port_name>` or `gpio off <port_name>` and convert them into the appropriate `curl` command. 

We will define variables for the IP address, port, and mapping of port names to GPIO pin numbers.

### Shell Script: `gpio_control.sh`

```bash
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
}

# Main script logic
if [ $# -ne 3 ]; then
    echo "Usage: $0 <on|off> <port_name>"
    exit 1
fi

ACTION=$(echo "$1" | tr '[:lower:]' '[:upper:]')
PORT_NAME="$2"

send_curl_command "$ACTION" "$PORT_NAME"
```

### Usage

1. **Make the script executable**:
   ```bash
   chmod +x gpio_control.sh
   ```

2. **Run the script**:
   ```bash
   ./gpio_control.sh on imx93
   ./gpio_control.sh off imx93
   ```

### Explanation

- **IP Address and Port Variables**: `IP_ADDRESS` and `PORT` are set to the IP address and port of your HTTP server.
- **Port Name to GPIO Pin Mapping**: The `get_gpio_pin` function maps port names (like `imx93`) to GPIO pin numbers. You can add more mappings as needed.
- **Send CURL Command**: The `send_curl_command` function constructs and sends the `curl` command to control the GPIO pin.
- **Main Script Logic**: The script checks the number of arguments and converts the action to uppercase (`on` -> `ON`, `off` -> `OFF`). It then calls `send_curl_command` with the appropriate action and port name.

### Adding More Port Names

If you want to control more GPIO pins, simply add more cases to the `get_gpio_pin` function:

```bash
port5)
    echo "23"
    ;;
port6)
    echo "24"
    ;;
```

### Summary

This script allows you to control GPIO pins on your Raspberry Pi via HTTP requests using `curl`. By using the command format `gpio on <port_name>`, you can easily send the appropriate commands to the server to turn GPIO pins on or off. The IP address and port are configurable, and you can map custom port names to specific GPIO pin numbers.

