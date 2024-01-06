###
#pcap_file_scanner seeks ip addresses, port and heartbeat info from a list of config files.

# python3 /home/docker/aicode/pcap/pcap_file_scanner.py -c/home/docker/configs/file_scan/modbus_*/

# we discover if the selected ip_address is routable , pingable and that a port exists
# then we determine which interface we need to use to talk to the devices
# this list is used by the tcpdump manager to capture interface data
# lastly for each ip_address we need a list of ports that are to be serviced
# this is used by the pcap_modbus.py (and pcap_dnp3.py) packages to determne if a pcap file contains any 
# significant information regarding communications failures.
# 
# python3 -m pip install netifaces

# sample output

# {
#     "heartbeats": [
#         {
#             "config_file": "risen_bms_addressed_client.json",
#             "component_id": "bms_running_info",
#             "dest_ip": "172.17.0.13",
#             "dest_port": 11000,
#             "device_id": 255,
#             "heartbeat_reg_type": "Input Registers",
#             "heartbeat_size": 2,
#             "heartbeat_offset": 278,
#             "heartbeat_uri": "heartbeat"
#         },
#     ],
#     "ips": {
#        "client"[
#         {
#             "config_file": "risen_bms_addressed_client.json",
#             "dest_ip": "172.17.0.13",
#             "src_ip": "172.17.0.2",
#             "dest_port": 11000,
#             "reachable": false,
#             "pingable": false,
#             "device": "eth0"
#         }
#     ],
#    "server": [
#             {
#                 "config_file": "broken_test_device_id.json",
#                 "file_type": "server",
#                 "src_ip": "127.0.0.1",
#                 "srv_port": 502,
#                 "reachable": false,
#                 "pingable": true,
#                 "device": "lo"
#             }
#     ],
#     "ip_addrs": {
#         "172.17.0.13": {
#             "ports": [
#                 11000
#             ]
#         }
#     },
#     "ifaces": [
#         "eth0"
#     ]
# }
### 


import os
import json
import argparse
import socket
import subprocess
import netifaces as ni
import glob


ips = []
devs = []

def ensure_log_dir(log_dir):
    # Create log directory if it does not exist
    if not os.path.exists(log_dir):
        try:
            os.makedirs(log_dir)
        except OSError as e:
            print(f"Error: {e.strerror} - Log directory ({log_dir}) could not be created.")
            exit(1)


def find_interface_name(ip_address):
    #print(ni.interfaces())
    for interface in ni.interfaces():
        addresses = ni.ifaddresses(interface)
        #print(addresses)
        if ni.AF_INET in addresses:
            for link in addresses[ni.AF_INET]:
                if link['addr'] == ip_address:
                    return interface
    return None

def find_local_ip(target_ip):
    try:
        # Temporarily create a socket to determine the local interface used to reach target_ip
        # We use a dummy port as we are not going to actually establish a connection
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect((target_ip, 1))  # You can use any target port here
            local_ip = s.getsockname()[0]  # Get the local interface IP
        return local_ip
    except socket.error:
        return None

def is_ip_pingable(ip):
    """Check if an IP address is reachable using the ping command."""
    # Adjust the ping command and count number as necessary
    # Here '-c 1' is for sending 1 packet, '-W 3' is timeout of 3 seconds
    # '-n' option is used to prevent DNS resolution in ping for faster performance
    command = ['ping', '-c', '1', '-W', '3', '-n', ip]

    # Running the ping command
    status = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return status.returncode == 0  # Return True if ping command was successful

def is_ip_reachable(ip, port, timeout=3):
    """Check if an IP address is reachable by trying to connect to it."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        # Try connecting to the port
        result = sock.connect_ex((ip, port))
        sock.close()
        return result == 0  # Return True if the connection attempt was successful
    except socket.error:
        return False

def scan_config_files(config_dir):
    configs = {}
    configs["heartbeats"] = []
    configs["ips"] = {}
    configs["ip_addrs"] = {}
    configs["interfaces"] = []

    for c_dir in glob.glob(config_dir):
        if os.path.isdir(c_dir):
            for filename in os.listdir(c_dir):
                if filename.endswith(".json"):  # Ensure it's a JSON file
                    filepath = os.path.join(c_dir, filename)
                    print(f" scanning file {filename}")
                    with open(filepath, 'r') as file:
                        server_file = False
                        client_file = False
                        try:
                            data = json.load(file)
                            # client file
                            connection = data.get("connection", None)
                            components = data.get("components", None)
                            #server file
                            system = data.get("system", {})
                            registers = data.get("registers", None)
                            if components and connection:
                                print(" found a client file")
                                client_file = True
                            elif system and registers:
                                print(" found a server file")
                                server_file = True
                            else:
                                print(" file passed")
                                continue

                            if  client_file:
                                ip_address = connection.get("ip_address", "")
                                port = connection.get("port", 502)
                                device_id = connection.get("device_id", 255)
                                is_pingable = is_ip_pingable(ip_address)
                                is_reachable = is_ip_reachable(ip_address, port)
                                my_ip = find_local_ip(ip_address)
                                my_device = find_interface_name(my_ip)


                                ip_info = {
                                            "config_file": filename,  # Track which file the heartbeat info came from
                                            "dest_ip": ip_address,
                                            "src_ip": my_ip,
                                            "dest_port": port,
                                            "file_type": "client",  # Track which file the heartbeat info came from
                                            "reachable": is_reachable,  # Mark the IP as reachable or not
                                            "pingable": is_pingable,  # Mark the IP as reachable or not
                                            "device": my_device,  # Mark the IP as reachable or not
                                }
                                if "client" not in configs["ips"]:
                                    configs["ips"]["client"] = []
                                configs["ips"]["client"].append(ip_info)
                                if ip_address not in configs["ip_addrs"]:
                                    configs["ip_addrs"][ip_address] = {}
                                if "ports" not in  configs["ip_addrs"][ip_address]:
                                    configs["ip_addrs"][ip_address]["ports"] = []
                                if port not in  configs["ip_addrs"][ip_address]["ports"]:
                                    configs["ip_addrs"][ip_address]["ports"].append(port)

                                if "interfaces" not in configs:
                                    configs["interfaces"] = []
                                if my_device not in configs["interfaces"]:
                                    configs["interfaces"].append(my_device)
                                for component in components:
                                    if component.get("heartbeat_enabled", True):
                                        hb_uri = component.get("heartbeat_read_uri","heartbeat")
                                        heartbeat_info = {
                                            "config_file": filename,  # Track which file the heartbeat info came from
                                            "component_id": component.get("id", "Unknown"),  # Track component ID for uniqueness
                                            "dest_ip": ip_address,
                                            "dest_port": port,
                                            "device_id": device_id,
                                            "heartbeat_reg_type": "Holding Registers",  # Assuming default as Holding Registers
                                            "heartbeat_size": 1,  # Default size
                                            "heartbeat_offset": None,  # Will set this next
                                            "heartbeat_uri" : hb_uri,
                                        }

                                        for register in component.get("registers", []):
                                            # Check for heartbeat by id
                                            reg_type =  register.get("type")
                                            reg_map =  register.get("map")
                                            for item in reg_map:
                                                if item.get("id") == hb_uri:
                                                    heartbeat_info['heartbeat_offset'] = item.get('offset', 0)
                                                    heartbeat_info['heartbeat_size'] = item.get('size', 1)
                                                    heartbeat_info['heartbeat_reg_type'] = reg_type

                                                    configs["heartbeats"].append(heartbeat_info)
                                                    break  # Assuming only one heartbeat per component

                            if  server_file:
                                ip_address = system.get("ip_address", "")
                                port = system.get("port", 502)
                                device_id = system.get("device_id", 255)
                                is_pingable = is_ip_pingable(ip_address)
                                is_reachable = is_ip_reachable(ip_address, port)
                                my_ip = find_local_ip(ip_address)
                                my_device = find_interface_name(my_ip)


                                ip_info = {
                                            "config_file": filename,  # Track which file the heartbeat info came from
                                            "file_type": "server",  # Track which file the heartbeat info came from
                                            "dest_ip": my_ip,
                                            "srv_port": port,
                                            "reachable": is_reachable,  # Mark the IP as reachable or not
                                            "pingable": is_pingable,  # Mark the IP as reachable or not
                                            "device": my_device,  # Mark the IP as reachable or not
                                }
                                if "server" not in configs["ips"]:
                                    configs["ips"]["server"] = []
                                configs["ips"]["server"].append(ip_info)
                                if ip_address not in configs["ip_addrs"]:
                                    configs["ip_addrs"][ip_address] = {}
                                if "ports" not in  configs["ip_addrs"][ip_address]:
                                    configs["ip_addrs"][ip_address]["ports"] = []
                                if port not in  configs["ip_addrs"][ip_address]["ports"]:
                                    configs["ip_addrs"][ip_address]["ports"].append(port)
                                if "interfaces" not in configs:
                                    configs["interfaces"] = []
                                if my_device not in configs["interfaces"]:
                                    configs["interfaces"].append(my_device)

                        except json.JSONDecodeError as e:
                            print(f"Error decoding JSON from {filename}: {e}")

    return configs

# Usage example
config_directory = "configs/modbus_client"  # Replace with your actual config directory path

parser = argparse.ArgumentParser(description="Scan Config Files for Modbus Client Details")
parser.add_argument("-c" , "--config", help="Directory containing the config files", default=config_directory)
parser.add_argument("-d", "--outdir", help="Output dir to write the JSON results", default="/var/log/gcom")
parser.add_argument("-o", "--outfile", help="Output file to write the JSON results", default="pcap_monitor.json")

# Parse arguments
args = parser.parse_args()
ensure_log_dir(args.outdir)


extracted_configs = scan_config_files(args.config)
# Convert dictionary to an indented JSON string
json_output = json.dumps(extracted_configs, indent=4)

# Print to console or write to file based on the argument
if args.outfile:
    outfile = f"{args.outdir}/{args.outfile}"
    with open(outfile, 'w') as outf:
        outf.write(json_output)
    print(f"Results written to {outfile}")
else:
    print(json_output)
