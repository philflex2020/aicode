import socket
import json
import threading

def handle_set(data):
    print(f"Handling 'set' with data: {data}")

def handle_run(data):
    print(f"Handling 'run' with data: {data}")

def handle_get(data):
    print(f"Handling 'get' with data: {data}")

class DataMeshServer:
    def __init__(self, port):
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.bind(('', self.port))
        self.sock.listen(5)

    def start(self):
        print(f"DataMeshServer started on port {self.port}")
        while True:
            client, addr = self.sock.accept()
            threading.Thread(target=self.handle_client, args=(client,)).start()

    def handle_client(self, client_socket):
        while True:
            data = client_socket.recv(1024)
            if not data:
                break

            message = json.loads(data.decode('utf-8'))
            if message.get("message") == "hello":
                print("Received hello from DataMeshController")
                # Respond to hello message or perform some initialization
            elif "method" in message:
                if message["method"] == "set":
                    handle_set(message)
                elif message["method"] == "run":
                    handle_run(message)
                elif message["method"] == "get":
                    handle_get(message)
            else:
                print("Unknown message type received")

        client_socket.close()

# Example to start the server
if __name__ == "__main__":
    server_port = 1234  # Port where the DataMeshServer will listen
    server = DataMeshServer(server_port)
    server.start()