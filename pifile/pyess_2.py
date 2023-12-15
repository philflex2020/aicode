import socket
import json
import threading
import time

# Global data store and thread function mappings
data_store = {}

# Thread functions
def ess_master(arg1):
    myStore = get_store(arg1)
    myStore["uri"] = arg1
    count = myStore.get("count", 0)
    while True:
        count += 1
        myStore["count"] = count
        print(f"ess_running: {arg1}, Count: {count}")
        time.sleep(5)

def bms_master(arg1):
    myStore = get_store(arg1)
    myStore["uri"] = arg1
    count = myStore.get("count", 0)
    while True:
        count += 1
        myStore["count"] = count
        print(f"bms_running: {arg1}, Count: {count}")
        time.sleep(5)

# Thread function mappings
thread_functions = {
    'ess_master': ess_master,
    'bms_master': bms_master,
}

# Start a thread based on the type
def start_thread(thread_type, *args):
    if thread_type in thread_functions:
        thread = threading.Thread(target=thread_functions[thread_type], args=args)
        thread.start()
    else:
        print(f"Unknown thread type: {thread_type}")

# Update the data store based on URI
def update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store
    for key in keys[:-1]:
        current_level = current_level.setdefault(key, {})
    last_key = keys[-1]
    if isinstance(current_level.get(last_key), dict):
        current_level[last_key].update(body)
    else:
        current_level[last_key] = body

# Retrieve a specific store based on URI
def get_store(uri):
    keys = uri.strip("/").split("/")
    current_level = data_store
    for key in keys:
        current_level = current_level.get(key, {})
    return current_level

# Handle client requests
def handle_client(conn, addr):
    print(f"Connected by {addr}")
    while True:
        data = conn.recv(1024)
        if not data:
            break

        try:
            json_data = json.loads(data.decode())
            uri = json_data.get('uri', '')
            method = json_data.get('method', '')
            body = json_data.get('body', {}) if isinstance(json_data.get('body'), dict) else {}

            if method == "run":
                update_data_store(uri, body)
                if 'type' in body:
                    start_thread(body['type'], uri)
                conn.sendall(data)

            elif method == "get":
                myStore = get_store(uri)
                data = json.dumps(myStore)
                conn.sendall(data.encode())

            elif method == "set":
                update_data_store(uri, body)
                myStore = get_store(uri)
                data = json.dumps(myStore)
                conn.sendall(data.encode())

            elif method == "show":
                print(f"Data store: {data_store}")

        except json.JSONDecodeError:
            print("Invalid JSON received")
        #finally:
            #conn.sendall(data)

def my_server(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen()
        print(f"Server listening on {host}:{port}")

        while True:
            conn, addr = s.accept()
            threading.Thread(target=handle_client, args=(conn, addr)).start()

if __name__ == "__main__":
    HOST = '127.0.0.1'
    PORT = 65432
    my_server(HOST, PORT)
