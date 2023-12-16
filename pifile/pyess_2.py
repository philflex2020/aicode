import socket
import json
import threading
import time

# note on shared objects (vlinks)
# my_data = {
#     "level1": {
#         "level2": {
#             "value": 1234
#        }
#   }
# }

# my_other_data = {}
# Assign the dictionary object
#my_other_data["sample"] = my_data["level1"]["level2"]

# assign "actions" to variables

# my_data = {
#     "level1": {
#         "level2": {
#             "value": 1234,
#             "key": 123456  << this is the look up for actions
#        }
#   }
# }

# def lookup(uri):
#     parts = uri.split('/')
#     current = my_data
#     for part in parts:
#         current = current.get(part)
#         if current is None:
#             return None
#     return current

# def set_key_value(obj, value, key):
#     # Define how to set the 'key'
#     print(f"Setting {key} to {value}")
#     obj[key] = value

# setFuncs = {
#     123456: set_key_value
# }

# def setValue(uri, value):
#     myObj = lookup(uri)
#     if myObj is not None and "key" in myObj:
#         setFuncs["key"](myObj, value)
#     else:
#         print(f"No 'key' found at {uri}")
#
# def setValue(uri, value, key):
#     myObj = lookup(uri)
#     if myObj is not None:
#         if "key" in myObj:
#             # Use the special function for 'key' if it exists
#             setFuncs[myObj["key"]](myObj, value, key)
#         else:
#             # Otherwise, directly set the 'value'
#             myObj[key] = value
#     else:
#         print(f"No object found at {uri}")
# setValue("level1/level2", 3456, "value")

# import uuid

#unique_key_counter = 0

#def get_unique_key():
#    global unique_key_counter
#    unique_key_counter += 1
#    return unique_key_counter

# def addSetFunc(data, uri, set_func):
#     unique_key = str(uuid.uuid4())  # Generate a unique key
#     unique_key = get_unique_key()
#     myObj = lookup(uri)

#     if myObj is not None:
#         # Add the unique key to the object at the specified URI
#         myObj["setFuncKey"] = unique_key

#         # Associate the unique key with the set function in setFuncs
#         setFuncs[unique_key] = set_func
#     else:
#         print(f"Object not found at {uri}")

# # Update setValue to use the unique key for setFuncs
# def setValue(uri, value):
#     myObj = lookup(uri)
#     if myObj is not None:
#         setFuncKey = myObj.get("setFuncKey")
#         if setFuncKey and setFuncKey in setFuncs:
#             setFuncs[setFuncKey](myObj, value)
#         else:
#             myObj["value"] = value
#     else:
#         print(f"No object found at {uri}")

# # Usage example
# addSetFunc(my_data, "level1/level2", set_key_value)
# setValue("level1/level2", 3456)



# Global data store and thread function mappings
data_store = {}

def limit_depth(data, max_depth):
    if max_depth <= 0 or not isinstance(data, dict):
        return data
    else:
        return {k: limit_depth(v, max_depth - 1) for k, v in data.items()}

def json_dumps_limited_depth(data, max_depth, **kwargs):
    limited_data = limit_depth(data, max_depth)
    return json.dumps(limited_data, **kwargs)
#json_string = json_dumps_limited_depth(my_data, 2, indent=4)


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
                #print(f"Data store: {data_store}")
                #print(f"Data store updated: {data_store}")
                myStore = get_store(uri)

                data = json.dumps(myStore, indent=4)
                conn.sendall(data.encode())

            elif method == "showall":
                #print(f"Data store: {data_store}")
                #print(f"Data store updated: {data_store}")
                myStore = get_store(uri)

                data = json.dumps(data_store, indent=4)
                conn.sendall(data.encode())

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
