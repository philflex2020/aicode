import socket
import json

def uri_server(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()

        print(f"Server listening on {host}:{port}")

        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Connected by {addr}")
                while True:
                    data = conn.recv(1024)
                    if not data:
                        break

                    # Parse received data as JSON
                    try:
                        json_data = json.loads(data.decode())
                        print(f"Received URI: {json_data['uri']}")
                        print(f"Received Body: {json_data['body']}")
                    except json.JSONDecodeError:
                        print("Invalid JSON received")

                    # Echo back the JSON data
                    conn.sendall(data)

if __name__ == "__main__":
    HOST = '127.0.0.1'
    PORT = 65432

    uri_server(HOST, PORT)

