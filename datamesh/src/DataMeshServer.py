import socket
import json
import threading
import argparse
import signal
import sys
import time
from DataMeshUtils import *

class DataRequest:
    def __init__(self, name, every, uris):
        self.name = name
        self.every = every  # in seconds
        self.uris = uris

requests = {} 

def fetch_data_for_request(request, data_store):
    result = {}
    for uri_dict in request.uris:
        for uri, key in uri_dict.items():
            # Fetch the data from the uri
            # Assuming a function get(uri, data_store) returns the data at the uri
            data = get(uri, data_store)
            result[key] = data
    return result


def schedule_request(request, data_store):
    def run_request():
        while True:
            # Fetch and process data for request
            result = fetch_data_for_request(request, data_store)
            print(f"/my/request/{request.name} {result}")  # Or handle the result appropriately

            # Wait for the next interval
            time.sleep(request.every)

    # Start the periodic task
    threading.Thread(target=run_request).start()


def handle_set(dm, uri, data):
    print(f"Handling 'set' with data: {data}")
    body = data["body"]
    #jbody = json.loads(body)
    update_data_store(uri, body)

    print(f"Handling 'set' got uri: {uri} body: {body}")
    #json_data = json.dumps({"command": "set"})
    dm.sendall("set ok".encode())


def handle_request(dm, uri, data):
    name = data.get('name')
    every = data.get('every', 1)  # Frequency in seconds
    uris = data.get('uris', [])  # List of URIs to fetch
    print(f" request [{name}] every {every} uris [{uris}]")
    # Create and store the request
    new_request = DataRequest(name, every, uris)
    requests[name] = new_request
    myStore = get_store(uri)
    schedule_request(new_request, myStore)

    data = json.dumps(myStore, indent=4)
    dm.sendall(data.encode())

def handle_show(dm, uri, data):
    myStore = get_store(uri)
    data = json.dumps(myStore, indent=4)
    dm.sendall(data.encode())

def handle_run(dm, uri, data):
    print(f"Handling 'run' with data: {data}")
    json_data = json.dumps({"command": "run"})
    dm.sendall(json_data.encode())

def handle_get(dm,uri,data):
    body = data["body"]
    myStore = get_store(uri)
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
            elif "method" in message and "uri" in message:
                uri = message["uri"]
                if message["method"] == "set":
                    handle_set(client_socket, uri, message)
                elif message["method"] == "request":
                    print(" request method received")
                    handle_request(client_socket, uri, message)
                elif message["method"] == "run":
                    handle_run(client_socket, uri, message)
                elif message["method"] == "get":
                    handle_get(client_socket, uri, message)
                elif message["method"] == "show":
                    handle_show(client_socket, uri, message)
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