import socket
import json
import threading
import argparse
import signal
import sys

data_store = {}

def is_valid_json(test_string):
    try:
        json_object = json.loads(test_string)
        return True
    except json.JSONDecodeError:
        return False

def get_store(uri):
    keys = uri.strip("/").split("/")
    current_level = data_store
    #for key in keys[:-1]:
    for key in keys:
        if key not in current_level:
            current_level[key] = {}
        current_level = current_level[key]
    return current_level

def get_last(uri):
    keys = uri.strip("/").split("/")
    return keys[-1]

def update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store

    for i, key in enumerate(keys):
        if i == len(keys) - 1:  # If it's the last key
            if is_valid_json(body):
                print(" body is json")
                new_data = json.loads(body)
                if isinstance(new_data, dict) and isinstance(current_level.get(key, {}), dict):
                    print("Merge dictionaries")
                    current_level[key] = merge_dicts(current_level.get(key, {}), new_data)
                else:
                    # Replace the existing value
                    current_level[key] = new_data
            else:
                print("If body is not a JSON string, just assign the value")
                current_level[key] = body
        else:
            # Navigate or create new nested dictionary
            current_level = current_level.setdefault(key, {})

def merge_dicts(original, new_data):
    for key, value in new_data.items():
        if key in original and isinstance(original[key], dict) and isinstance(value, dict):
            original[key] = merge_dicts(original[key], value)
        else:
            original[key] = value
    return original

def handle_set(dm, data):
    print(f"Handling 'set' with data: {data}")
    body = data["body"]
    uri = data["uri"]
    jbody = json.loads(body)
    update_data_store(uri, body)

    print(f"Handling 'set' got uri: {uri} jbody: {jbody}")
    #json_data = json.dumps({"command": "set"})
    dm.sendall("set ok".encode())

def handle_run(dm, data):
    print(f"Handling 'run' with data: {data}")
    json_data = json.dumps({"command": "run"})
    dm.sendall(json_data.encode())

def handle_get(dm,data):
    print(f"Handling 'get' with data: {data}")
    #print(f"Data store updated: {data_store}")
    body = data["body"]
    uri = data["uri"]
    myStore = get_store(uri)
    print(f"uri: {uri}")
    print(f"Mystore: {myStore}")
    sdata = json.dumps(myStore)
    dm.sendall(sdata.encode())
    # json_data = json.dumps({"command": "get"})
    # dm.sendall(json_data.encode())

class DataMeshServer:
    def __init__(self, port):
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('', self.port))
        self.sock.listen(5)
        self.running = True

    def start(self):
        print(f"DataMeshServer started on port {self.port}")
        while self.running:
            client, addr = self.sock.accept()
            threading.Thread(target=self.handle_client, args=(client,)).start()

    def handle_client(self, client_socket):
        while self.running:
            data = client_socket.recv(1024)
            if not data:
                break

            message = json.loads(data.decode('utf-8'))
            if message.get("message") == "hello":
                print("Received hello from DataMeshController")
                # Respond to hello message or perform some initialization
            elif "method" in message:
                if message["method"] == "set":
                    handle_set(client_socket, message)
                elif message["method"] == "run":
                    handle_run(client_socket,message)
                elif message["method"] == "get":
                    handle_get(client_socket,message)
            else:
                print("Unknown message type received")
        client_socket.close()

    def stop(self):
        self.running = False
        self.sock.close()
        print("DataMeshServer stopped")

def signal_handler(sig, frame):
    print('Signal received, stopping the server...')
    server.stop()
    sys.exit(0)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Start a DataMeshServer on a given port.')
    parser.add_argument('port', type=int, help='Port number for the DataMeshServer')
    args = parser.parse_args()

    server = DataMeshServer(args.port)
    
    # Setup signal handling for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    server.start()