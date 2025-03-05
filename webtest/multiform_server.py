from flask import Flask, render_template
import socket

app = Flask(__name__)

# Create a TCP socket
host = '127.0.0.1'  # Standard loopback interface address (localhost)
port = 65432        # Port to listen on (non-privileged ports are > 1023)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((host, port))
    s.listen()
    conn, addr = s.accept()
    with conn:
        print(f"Connected by {addr}")
        while True:
            data = conn.recv(1024)
            if not data:
                break
            try:
                received_data = data.decode('utf-8') 
                # Assuming the received data is JSON
                import json
                data = json.loads(received_data) 
            except json.JSONDecodeError:
                print("Error: Received data is not valid JSON")
                data = {}  # Handle invalid data gracefully

            return render_template("index.html", data=data) 

if __name__ == "__main__":
    app.run(debug=True)