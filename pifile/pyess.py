import socket
import json
import threading
import time
from bms import bms_master , bms_unit
from pcs import pcs_master , pcs_unit

# python3 pyclient.py -mrun -u/mysys/ess/ess_2/bms/bms_2 -b'{"type":"bms_master", "data":"data for bms2","count":123}'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"standby"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"charge"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"discharge"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/powerRequest  -b-40
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"standby"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"discharge"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"standby"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/command  -b'"charge"'
# python3 pyclient.py -mset -u/mysys/ess/ess_2/bms/bms_2/powerRequest  -b40


# python3 pyclient.py -mset -u/mytest2/templates -b'{"mytemp":{"num_bms":3,"num_pcs":2 }}'
# python3 pyclient.py -mrun -u/mytest2 -b'{"type":"ess_master", "system":"mytemp"}'
# python3 pyclient.py -mset -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mset -u/mytest2/ess/status/state -b'discharge'
# python3 pyclient.py -mshow -u/mytest2/ess/status/state -b'discharge'
# python3 pyclient.py -mshow -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mset -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mshow -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mshow -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mset -u/mytest2/ess/bms/bms_0/status/capacity -b'20000'
# python3 pyclient.py -mshow -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mshow -u/mytest2/ess/bms/bms_0/status/SOC -b'50'
# python3 pyclient.py -mset -u/mytest2/ess/bms/bms_2/status/capacity -b'30000'

# Global data store and thread function mappings
from pyutils import data_store 
from pyutils import get_store, get_last, update_data_store, new_update_data_store, start_thread, set_thread_func

#set_thread_func('ess_master', ess_master)
set_thread_func('bms_master', bms_master)
set_thread_func('pcs_master', pcs_master)
set_thread_func('bms_unit',   bms_unit)
set_thread_func('pcs_unit',   pcs_unit)

#data_store = {}

# def get_store(uri):
#     keys = uri.strip("/").split("/")
#     current_level = data_store
#     #for key in keys[:-1]:
#     for key in keys:
#         if key not in current_level:
#             current_level[key] = {}
#         current_level = current_level[key]
#     return current_level

# def get_last(uri):
#     keys = uri.strip("/").split("/")
#     return keys[-1]
# python3 pyclient.py -mset -u/mytest2/templates -b'{"mytemp":{"num_bms":3,"num_pcs":2 }}'
# python3 pyclient.py -mrun -u/mytest2 -b'{"type":"ess_master", "system":"mytemp"}'
def ess_master(arg1):
    count = 0
    total_power = 0
    print(f" Ess_master {arg1}")
    myStore = get_store(arg1)
    myStore["ess"] = {}
    myStore["ess"]["status"] = {}
    myStore["ess"]["controls"] = {}
    myStore["ess"]["controls"]["command"] = "off"
    myStore["ess"]["controls"]["ChargeCurrent"] = "off"

    myStore["ess"]["controls"]["Discharge"] = 0
    myStore["ess"]["status"]["SOC"] = 100
    myStore["ess"]["status"]["capacity"] = 40000
    myStore["ess"]["status"]["state"] = "idle"
    


    template =  myStore.get("system", '')
    if template == '':

        num_bms =  myStore.get("num_bms")
        num_pcs =  myStore.get("num_pcs")
    else:
        num_bms =  myStore["templates"][template].get("num_bms", 1)
        num_pcs =  myStore["templates"][template].get("num_pcs", 1)


    print(f" num_bms {num_bms} num_pcs {num_pcs}")
    for x in range(num_bms):
        arg3=f"{arg1}/ess/bms/bms_{x}"
        start_thread("bms_unit", arg3)
    for x in range(num_pcs):
        arg3=f"{arg1}/ess/pcs/pcs_{x}"
        start_thread("pcs_unit", arg3)

    oldState =  myStore["ess"]["status"]["state"]
    while True:  # 'True' should be capitalized
        sysState =  myStore["ess"]["status"]["state"]
        count += 1
        myStore["ess"]["count"] = count
        print("  ess_running :")
        #print(arg1)
        #print(myStore)
        if sysState == "charge":
            print("charging")    

        elif sysState == "discharge":
            cap = 0
            Soc = 0
            for x in range(num_bms):
                bms_id=f"bms_{x}"
                bmsStore = myStore["ess"]["bms"][bms_id]
                bms_cap = bmsStore["status"]["capacity"]
                bms_SOC = bmsStore["status"]["SOC"]

                cap+=bms_cap*bms_SOC/100
                Soc+=bms_SOC
            total_discharge = Soc / 2
            if oldState != "discharge":
                oldState = "discharge"
                total_power = total_discharge / 4
            if Soc == 0:
                total_power = 0

            print(f">>>>   ess discharging total capacity {cap} total Soc {Soc} total_power {total_power}")    
            for x in range(num_bms):
                bms_id=f"bms_{x}"
                bmsStore = myStore["ess"]["bms"][bms_id]
                bms_SOC = bmsStore["status"]["SOC"]
                bms_cap = bmsStore["status"]["capacity"]
                bmsPower = total_power * (bms_SOC /2) / total_discharge 
                bms_cap -= bmsPower
                if bms_cap == 0:
                    bms_cap = 0
                bmsStore["status"]["capacity"] = bms_cap
                max_cap = bmsStore["max_capacity"]
                bms_cap = bmsStore["status"]["capacity"]
                bms_SOC = bms_cap * 100 / max_cap
                bmsStore["status"]["SOC"]= bms_SOC

                print(f">>>>  >>> {bms_id} discharge {bmsPower} cap {bms_cap}")    


        # def distribute_power_demand(battery_socs, total_power_demand):
        #     # Calculate the total discharge power availability (50% of each battery's power based on SoC)
        #     total_discharge_capability = sum(soc / 2 for soc in battery_socs)  # 50% of each battery's SoC

        #     # Calculate the proportion of total power demand each battery should supply
        #     power_distribution = [total_power_demand * (soc / 2) / total_discharge_capability for soc in battery_socs]

        #     return power_distribution

        # # Example Usage
        # battery_socs = [60, 50, 40]  # SoC of three batteries in percent
        # total_power_demand = 100  # Total power demand

        # power_distribution = distribute_power_demand(battery_socs, total_power_demand)
        # print("Power distribution:", power_distribution)
        # def adjust_discharge_rates(battery_socs, total_power_demand, discharge_period_hours):
        #     initial_power_distribution = [soc / 2 for soc in battery_socs]  # 50% of each battery's SoC
        #     total_initial_discharge_power = sum(initial_power_distribution)

        #     # Calculate initial discharge rates
        #     discharge_rates = [power / total_initial_discharge_power * total_power_demand for power in initial_power_distribution]

        #     # Simulate discharge to adjust rates
        #     adjusted_discharge_rates = list(discharge_rates)
        #     for _ in range(int(discharge_period_hours * 60)):  # Assuming adjustment every minute
        #         # Simulate discharge for one minute and update SoCs
        #         battery_socs = [max(0, soc - rate / 60) for soc, rate in zip(battery_socs, adjusted_discharge_rates)]
                
        #         # Recalculate total discharge power based on new SoCs
        #         total_discharge_power = sum(soc / 2 for soc in battery_socs)
                
        #         # Adjust discharge rates to balance SoCs
        #         adjusted_discharge_rates = [total_power_demand * (soc / 2) / total_discharge_power for soc in battery_socs]

        #     return adjusted_discharge_rates

        # # Example Usage
        # battery_socs = [60, 50, 40]  # Initial SoC of three batteries in percent
        # total_power_demand = 100     # Total power demand in Watts
        # discharge_period_hours = 0.5  # Half the discharge period in hours

        # adjusted_rates = adjust_discharge_rates(battery_socs, total_power_demand, discharge_period_hours)
        # print("Adjusted Discharge Rates:", adjusted_rates)


        time.sleep(5)


set_thread_func('ess_master', ess_master)

    # myStore = get_store(arg1)
    # myStore["uri"] = arg1
    # count = 0
    # myStore["count"] = 0
    # while True:  # 'True' should be capitalized
    #     count += 1
    #     myStore["count"] = count
    #     print("ess_running :"+ arg1)
    #     #print(arg1)
    #     print(myStore)
    #     time.sleep(5)



# def pcs_master(arg1):
#     myStore = get_store(arg1)
#     print(f" pcs myStore {myStore}")
#     myStore["uri"] = arg1
#     count = 0
#     if "count" not in myStore:
#         myStore["count"] = 0
#     else:
#         count = myStore["count"]

#     while True:  # 'True' should be capitalized
#         mycount = myStore.get("mycount", 0)
#         count = myStore["count"]
#         count += 1
#         myStore["count"] = count
#         print("             pcs_running :"+ arg1 + " count "+ str(count) + " mycount "+ str(mycount))
#         time.sleep(5)
#     # Implement bms_master logic here


# # Thread function mappings
# thread_functions = {
#     'ess_master': ess_master,  # Reference the function directly
#     'bms_master': bms_master,  # Reference the function directly
#     'pcs_master': pcs_master,  # Reference the function directly
#     'bms_unit':   bms_unit,  # Reference the function directly
#     'pcs_unit':   pcs_unit,  # Reference the function directly
# }

# Start a thread based on the type
# def start_thread(thread_type, *args):
#     if thread_type in thread_functions:
#         thread = threading.Thread(target=thread_functions[thread_type],args=args)
#         thread.start()
#     else:
#         print(f"Unknown thread type: {thread_type}")


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
                        # print(" so far so good ")
                        try:
                            jbody = json_data.get('body', {})  # Assuming 'body' is already a dictionary
                            print(" body is  a dict")
                            print(type(jbody))
                        except:
                            print(" body is not a dict")
                            try:
                                jbody = json_data.get('body', '')  # Assuming 'body' is already a dictionary
                                print(" body is  a string")
                            except:
                                print(" body is not a string")
                                try:
                                    jbody = json_data.get('body', 1)  # Assuming 'body' is already a dictionary
                                    print(" body is  a number")
                                except:
                                    err = f" Json not decoded"

                        # print(" so far so good 2 ")
                        if type(jbody) == type(""):
                            # print(" so far so good 3 ")
                            body = jbody   
                            # print(" so far so good 4 ")
                        else :
                            #body = json.loads(jbody)
                            body = jbody   
                        #print("Keys in body:")
                        #for key in body:
                        #    print(key)

                        # Process the data
                        print(" so far so good 3 ")
                        if method == "run":
                            update_data_store(uri, body)

                            if 'type' in body:
                                print(body)
                                print(type(body))
                                if type(body) == type(''):
                                     body = json.loads(body)

                                print(f"Received function: {body['type']}")

                                start_thread(body["type"], uri)
                            conn.sendall(data)


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
                            print(" running set ")
                            update_data_store(uri, body)
                            myStore = get_store(uri)
                            data = json.dumps(myStore)
                            conn.sendall(data.encode())
                            
                        elif method == "setnew":
                            print(" running setnew ")
                            new_update_data_store(uri, body)
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
                            err = f"Invalid ugly JSON received"
                        conn.sendall(err.encode())

                    #conn.sendall(data)


# Handle client requests needs fixup
def handle_client(conn, addr):
    print(f"Connected by {addr}")
    while True:
        data = conn.recv(1024)
        if not data:
            break

        json_data = json.loads(data.decode())
        uri = json_data.get('uri', '')
        method = json_data.get('method', '')
        body = json_data.get('body', {}) if isinstance(json_data.get('body'), dict) else {}

        #try:

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

        #except json.JSONDecodeError:
        #    print("Invalid JSON received")
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
