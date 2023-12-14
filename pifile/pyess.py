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
    myStore["uri"] = arg1
    count = 0
    myStore["count"] = 0
    while True:  # 'True' should be capitalized
        count += 1
        myStore["count"] = count
        print("             bms_running :"+ arg1)
        time.sleep(5)
    # Implement bms_master logic here


# Thread function mappings
thread_functions = {
    'ess_master': ess_master,  # Reference the function directly
    'bms_master': bms_master,  # Reference the function directly
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
    current_level[keys[-1]] = body

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
                        update_data_store(uri, body)
                        if method == "run":
                            if 'type' in body:
                                #print(f"Received type: {body['type']}")
                                start_thread(body['type'],uri)

                        print(f"Data store updated: {data_store}")
                    except json.JSONDecodeError:
                        print("Invalid JSON received")

                    conn.sendall(data)

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
