import socket
import json

host = '127.0.0.1'  # The server's hostname or IP address
port = 65432        # The port used by the server

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((host, port))

    # Sample data to send
    data = {
        "People": [
            {"name": "Alice", "age": 25, "city": "New York"},
            {"name": "Bob", "age": 30, "city": "San Francisco"},
            {"name": "Jimmy", "age": 45, "city": "London"},
            {"name": "Charlie", "age": 35, "city": "Los Angeles"},
        ]
    }

    # Convert data to JSON string
    json_data = json.dumps(data)
    s.sendall(json_data.encode('utf-8'))

    # Receive and display response from the server 
    # (You might need to handle this depending on your needs)
    received_data = s.recv(1024)
    print(f"Received from server: {received_data}")