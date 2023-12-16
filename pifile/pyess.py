import socket
import json
import threading
import time

# Global data store and thread function mappings
data_store = {}

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

def ess_master(arg1):
    myStore = get_store(arg1)
    myStore["uri"] = arg1
    count = 0
    myStore["count"] = 0
    while True:  # 'True' should be capitalized
        count += 1
        myStore["count"] = count
        print("ess_running :"+ arg1)
        #print(arg1)
        print(myStore)
        time.sleep(5)


# Define another thread function for example
def bms_master(arg1):
    myStore = get_store(arg1)
    myStore["name"] = get_last(arg1)
    print(f" bms myStore {myStore}")
    myStore["uri"] = arg1
    myStore["status"] = {}
    myStore["status"]["state"] = "off"
    myStore["command"] = "off"
    myStore["status"]["SOC"] = 100
    myStore["status"]["capacity"] = 40000
    myStore["max_capacity"] = 40000
    myStore["status"]["power"] = 0
    myStore["maxpower"] = 400
    myStore["powerRequest"] = 0



    count = 0
    if "count" not in myStore:
        myStore["count"] = 0
    else:
        count = myStore["count"]

    while True:  # 'True' should be capitalized
        mycount = myStore.get("mycount", 0)
        count = myStore["count"]
        count += 1
        myStore["count"] = count
        myname = myStore["name"]
        mycap = myStore["status"]["capacity"]
        mystate = myStore["status"]["state"]
        mycommand = myStore["command"]
        prequest = myStore["powerRequest"]
        if prequest > myStore["maxpower"]:
            prequest = myStore["maxpower"]
        elif prequest < -myStore["maxpower"]:
            prequest = -myStore["maxpower"]
        if mystate in ["off" ,"charge", "discharge"] :
            if mycommand == "standby": 
                print(f" changing state to standby")
                myStore["status"]["power"] = 0
                myStore["status"]["state"] = "standby"
        elif mystate in ["standby" ] :
            if mycommand == "charge":
                print(f" changing state to charge")
                myStore["status"]["state"] = "charge"
            elif mycommand == "discharge": 
                print(f" changing state to discharge")
                myStore["status"]["state"] = "discharge"
        mystate = myStore["status"]["state"]
        if mystate in ["charge"] :
            if mycap <  myStore["max_capacity"] and prequest > 0:
                myStore["status"]["capacity"] += prequest
                myStore["status"]["power"] = prequest
            else:
                myStore["status"]["power"] = 0

        if mystate in ["discharge"] :
            if mycap >  0 and prequest < 0:
                myStore["status"]["capacity"] += prequest
                myStore["status"]["power"] = prequest
            else:
                myStore["status"]["power"] = 0


        print(f"             bms_running : {myname}  state  {mystate}  capacity {mycap}")
        time.sleep(5)
    # Implement bms_master logic here

def pcs_master(arg1):
    myStore = get_store(arg1)
    print(f" pcs myStore {myStore}")
    myStore["uri"] = arg1
    count = 0
    if "count" not in myStore:
        myStore["count"] = 0
    else:
        count = myStore["count"]

    while True:  # 'True' should be capitalized
        mycount = myStore.get("mycount", 0)
        count = myStore["count"]
        count += 1
        myStore["count"] = count
        print("             pcs_running :"+ arg1 + " count "+ str(count) + " mycount "+ str(mycount))
        time.sleep(5)
    # Implement bms_master logic here


# Thread function mappings
thread_functions = {
    'ess_master': ess_master,  # Reference the function directly
    'bms_master': bms_master,  # Reference the function directly
    'pcs_master': pcs_master,  # Reference the function directly
}

# Start a thread based on the type
def start_thread(thread_type, *args):
    if thread_type in thread_functions:
        thread = threading.Thread(target=thread_functions[thread_type],args=args)
        thread.start()
    else:
        print(f"Unknown thread type: {thread_type}")

# Update the data store based on URI
def update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store

    for key in keys[:-1]:
        if key not in current_level:
            current_level[key] = {}
        current_level = current_level[key]

    last_key = keys[-1]
    if last_key in current_level and isinstance(current_level[last_key], dict):
        # If the last key exists and its value is a dictionary, merge the dictionaries
        print(f" merge last key {last_key}")
        print(body)
        for mkey in body:
            print(mkey)
            print(last_key)
            #current_level[last_key].update(body)
            current_level[last_key][mkey] = body[mkey]
    else:
        # Otherwise, just set the value to body
        print(f" set last key {last_key}")
        print(body)
        current_level[last_key] = body
    #current_level[keys[-1]] = body

def echo_server(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
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

                    try:
                        # Parse received data as JSON
                        json_data = json.loads(data.decode())
                        uri = json_data.get('uri', '')
                        method = json_data.get('method', '')
                        jbody = json_data.get('body', {})  # Assuming 'body' is already a dictionary
                        if type(jbody) == type(""):
                            body = json.loads(jbody)
                        else :
                            body = jbody   
                        #print("Keys in body:")
                        #for key in body:
                        #    print(key)

                        # Process the data
                        if method == "run":
                            update_data_store(uri, body)

                            if 'type' in body:
                                #print(f"Received type: {body['type']}")
                                start_thread(body['type'],uri)
                            conn.sendall(data)

                        elif method == "showall":
                            #print(f"Data store updated: {data_store}")
                            data = json.dumps(data_store, indent=4)
                            conn.sendall(data.encode())

                        elif method == "showall":
                            #print(f"Data store updated: {data_store}")
                            data = json.dumps(data_store, indent=4)
                            conn.sendall(data.encode())
                        elif method == "show":
                            myStore = get_store(uri)
                            #print(f"Data store updated: {data_store}")
                            data = json.dumps(myStore, indent=4)
                            conn.sendall(data.encode())

                        elif method == "get":
                            #print(f"Data store updated: {data_store}")
                            myStore = get_store(uri)
                            print(f"uri: {uri}")
                            print(f"Mystore: {myStore}")
                            data = json.dumps(myStore)
                            conn.sendall(data.encode())

                        elif method == "set":
                            update_data_store(uri, body)
                            myStore = get_store(uri)
                            data = json.dumps(myStore)
                            conn.sendall(data.encode())
                        else:
                            out = f"unknown method: [{method}]"
                            conn.sendall(out.encode())

                    except json.JSONDecodeError:
                        print(data.decode())
                        if type(data) == type("") :
                            err = f"String received"
                        else:
                            err = f"Invalid JSON received"
                        conn.sendall(err.encode())

                    #conn.sendall(data)


# Handle client requests needs fixup
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

    echo_server(HOST, PORT)


# import socket
# import json
# import threading
# import time

# def ess_master():
#     while True:  # 'True' should be capitalized
#         print("ess_running")
#         time.sleep(1)

# # Define another thread function for example
# def bms_master():
#     print("bms_master is running")
#     # Implement bms_master logic here

# # Global data store and thread function mappings
# data_store = {}

# # Thread function mappings
# thread_functions = {
#     'ess_master': ess_master,  # Reference the function directly
#     'bms_master': bms_master,  # Reference the function directly
# }

# # Start a thread based on the type
# def start_thread(thread_type):
#     if thread_type in thread_functions:
#         thread = threading.Thread(target=thread_functions[thread_type])
#         thread.start()
#     else:
#         print(f"Unknown thread type: {thread_type}")

# # Update the data store based on URI
# def update_data_store(uri, body):
#     keys = uri.strip("/").split("/")
#     current_level = data_store
#     for key in keys[:-1]:
#         if key not in current_level:
#             current_level[key] = {}
#         current_level = current_level[key]
#     current_level[keys[-1]] = body

# def echo_server(host, port):
#     with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
#         s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
#         s.bind((host, port))
#         s.listen()

#         print(f"Server listening on {host}:{port}")

#         while True:
#             conn, addr = s.accept()
#             with conn:
#                 print(f"Connected by {addr}")
#                 while True:
#                     data = conn.recv(1024)
#                     if not data:
#                         break

#                     try:
#                         # Parse received data as JSON
#                         json_data = json.loads(data.decode())
#                         uri = json_data.get('uri', '')
#                         body = json_data.get('body', {})  # Assuming 'body' is already a dictionary

#                         print("Keys in body:")
#                         for key in body:
#                             print(key)

#                         # Process the data
#                         update_data_store(uri, body)
#                         if 'type' in body:
#                             print(f"Received type: {body['type']}")
#                             start_thread(body['type'])

#                         print(f"Data store updated: {data_store}")
#                     except json.JSONDecodeError:
#                         print("Invalid JSON received")

#                     conn.sendall(data)

# if __name__ == "__main__":
#     HOST = '127.0.0.1'
#     PORT = 65432

#     echo_server(HOST, PORT)
