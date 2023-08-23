import socket
import json

# Define the server address and port
server_address = ('localhost', 12345)

# Sample data to send
data = {
    "category": "setvalue",
    "item": "temperature",
    "id": "sensor1",
    "value": 25.0
}

# Create a socket and connect to the server
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(server_address)

try:
    # Send the JSON-encoded data
    serialized_data = json.dumps(data)
    sock.sendall(serialized_data.encode())

finally:
    sock.close()
