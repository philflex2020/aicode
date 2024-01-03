###################################################
#
#  pcap_modbus.py
#  p. wilshire
#      01_03_2023
#
# find tihngs of interest in a pcap (tcpdump) file
# we will run a capture on a specific interface 
# with a list of client and server ports
# we get these from the
#  /var/log/gcom/pcap_monitor.json

    # "ip_addrs": {
    #     "10.128.103.91": {
    #         "ports": [
    #             502
    #         ]
    #     },
    #     "172.17.0.3": {
    #         "ports": [
    #             502,
    #             10006,
    #             10501,
    #             11000,
    #             2502
    #         ]
    #     },
    # client port queries go from a src_ip -> dest_ip:dest_port
    # we retain the src_port for reference and look  for a transaction from dest_ip -> src_ip:src_port 
    # when we see a transaction headed for a dest_ip:dest_port we can assume that this is a query or request.
    # 
###################################################

import argparse
from scapy.all import *
from scapy.layers.inet import TCP
from scapy.layers.l2 import LLC, STP
from datetime import datetime
import json
from decimal import Decimal


max_resp_time = 0.0
max_resp_id = 0
hb_start = 768
hb_reg = 768
hb_size = 20

num_acks = 0

# Replace this with the actual server port for Modbus TCP/IP
MODBUS_TCP_PORT = 502
FUNCTION_CODE_READ_HOLDING_REGISTERS = 0x03

client_server_pairs = {}
hb_info = {}


def register_pair(packet, modbus_tcp_port):

    source_ip = packet[IP].src
    destination_ip = packet[IP].dst
    source_port = packet[TCP].sport
    destination_port = packet[TCP].dport
    #timestamp = datetime.utcfromtimestamp(packet.time).isoformat()
    timestamp = float(packet.time)
    pair_id = ""

    if destination_port == modbus_tcp_port:

        # Construct a unique identifier for the client-server pair
        pair_id = f"{source_ip}->{destination_ip}:{destination_port}"
    else:
        # Construct a unique identifier for the client-server pair
        pair_id = f"{destination_ip}->{source_ip}:{source_port}"

    # Determine if this pair is new or if status has changed
    if pair_id not in client_server_pairs: # or client_server_pairs[pair_id] != 1:
        print(f" registering pair {pair_id}")

        # Create pair data
        json_body_client_server = {
                #"time": timestamp,
                "client_ip": source_ip,
                "server_ip": destination_ip,
                "client_port": source_port,
                "server_port": destination_port,
                "active": 1,  # or some other logic to determine activity
                "queries" : 0,
                "responses" : 0,
                "exceptions" : 0,
                "max_resp_time" : 0,
                "reconnects" : 0,
                "trans_ids" :[],

                }
        client_server_pairs[pair_id] = json_body_client_server  # or other logic for determining active status

    return pair_id,client_server_pairs[pair_id]

def print_client_server_pairs():
    print("List of Client-Server Pairs:")
    for pair_id, details in client_server_pairs.items():
        print(f"Pair ID: {pair_id}")
        #print(f"\tTime: {details['time']}")
        print(f"\tClient IP: {details['client_ip']}")
        print(f"\tClient Port: {details['client_port']}")
        print(f"\tServer IP: {details['server_ip']}")
        print(f"\tServer Port: {details['server_port']}")
        #print(f"\tActive: {details['active']}")
        print(f"\tReconnects: {details['reconnects']}")
        print(f"\tQueries: {details['queries']}")
        print(f"\tResponses: {details['responses']}")
        print(f"\tDuplicatd Responses: {details['duplicated_responses']}")
        print(f"\tExceptions: {details['exceptions']}")
        print(f"\tMax Resp Time: {details['max_resp_time']}")
        print("--------")  # Separator for readability

def fims_cmd(topic, details):
    details_json = json.dumps(details)
    command = ["fims_send", "-m", "pub", "-u", topic, details_json]
        
    # Execute the command
    try:
        subprocess.run(command, check=True)  # Using run method from subprocess module
        #print(f"Data sent for {pair_id}: {details_json}")
    except subprocess.CalledProcessError as e:
        print(f"An error occurred: {e}")

def send_to_fims(pcap_file, num_anom, num_stp, num_acks):
    details = {}
    details['number_of_delays'] = num_anom
    details['number_of_stp_packets'] = num_stp
    details['number_of_acks'] = num_acks
    details['pcap_file'] = pcap_file
    topic = f"/status/modbus_connections/{pcap_file}/summary"
    fims_cmd(topic, details)

    for pair_id, details in client_server_pairs.items():
        # Convert details to JSON string
        for key, value in details.items():
            if isinstance(value, Decimal):
                # Convert Decimal to float or string (choose based on what's best for your use case)
                details[key] = str(value) #float(value)  # or str(value)
        details['pcap_file'] = pcap_file
        # details_json = json.dumps(details)
        
        # Construct the FIMS send command
        #client_ip, server_port = pair_id.split("-")[-1].split(":")  # Extracting the server's IP and port
        client_ip = details['client_ip']
        server_ip = details['server_ip']
        server_port = details['server_port']
        dest = f"{client_ip}_{server_ip}_{server_port}"
        topic = f"/status/modbus_connections/{pcap_file}/{dest}"
        fims_cmd(topic, details)

def analyze_modbus_pcap(pcap_file, client_ports, server_ports,delay_threshold=1.0):
    global max_resp_time
    global max_resp_id
    # Reading the pcap file
    print("rdpcap start")
    packets = rdpcap(pcap_file)
    print("rdpcap done")

    # Dictionary to hold ongoing queries with timestamps
    queries = {}

    # List to hold anomalies (i.e., delayed responses)
    anomalies = []

    stp_details = []

    for packet in packets:
        mb_port = None #packet[TCP].dport
        # Filter TCP packets on Modbus port
        # am I talking to a server port (502 for example) (dport==502) or is a server port talking (sport==502) to me
        if TCP in packet and (packet[TCP].dport in client_ports or packet[TCP].sport in client_ports):
            # Ensure the payload exists, indicative of Modbus TCP/IP
            mb_port = None #packet[TCP].dport
            if packet[TCP].dport in client_ports:
                mb_port = packet[TCP].dport
            elif packet[TCP].sport in client_ports:
                mb_port = packet[TCP].sport
            # are we interested in the packet , if so get the pair_id client->server:port 192.168.112.5->192.168.112.13:502
            # note that we have the same pair_id for client queries and server responses 
            if len(packet[TCP].payload) and mb_port:
                pair_id, pair_data = register_pair(packet, mb_port)
                #max_resp_time = pair_data['max_resp_time']
                modbus_frame = bytes(packet[TCP].payload)
                #now get the transaction id for this pair 
                # Modbus TCP/IP ADU: Transaction Identifier is the first 2 bytes
                transaction_id = int.from_bytes(modbus_frame[0:2], byteorder='big')
                if transaction_id > 0:
                    #print (f" trans_id {transaction_id} sport {packet[TCP].sport}  dport {packet[TCP].dport} mb_port {mb_port}")
                    timestamp = packet.time  # Capture the timestamp of the packet

                    function_code = 0
                    # also get the function code
                    if len(modbus_frame) >= 7:
                        function_code = modbus_frame[7]  # 8th byte in Modbus ADU, immediately after Unit Identifier

                    # Determine if it's a query or a response based on ports
                    if mb_port in client_ports:
                        if pair_id not in queries:
                            queries[pair_id] = {}
                        if transaction_id not in queries[pair_id]:
                            # Query to a server, mark the timestamp
                            queries[pair_id][transaction_id] = {}
                            queries[pair_id][transaction_id]["packet_time"] = packet.time
                            pair_data["queries"] += 1
                            pair_data["max_resp_time"] = 0
                            pair_data["duplicated_responses"] = 0

                            if pair_data["client_port"] != packet[TCP].sport:
                                pair_data["reconnects"] += 1
                                pair_data["client_port"] = packet[TCP].sport
 
                    #elif packet[TCP].sport in client_ports and transaction_id in queries:
                        elif pair_id in queries:
                            if transaction_id in queries[pair_id]:
                                if  "packet_time" in queries[pair_id][transaction_id]:
                                    # Response from a server, calculate the response time
                                    #print(f"           response from {pair_id} for trans {transaction_id}")
                                    if queries[pair_id][transaction_id]["packet_time"] == 0:
                                        print(f"           duplicated response from {pair_id} dport {packet[TCP].dport} for trans {transaction_id} code {function_code:0x}")
                                        pair_data["duplicated_responses"] += 1

                                    else:    
                                        response_time = packet.time - queries[pair_id][transaction_id]["packet_time"]
                                        pair_data["responses"] += 1
                                        queries[pair_id][transaction_id]["packet_time"] = 0
                                        if response_time > pair_data["max_resp_time"]:
                                            pair_data["max_resp_time"] =  response_time 
                                            pair_data["max_resp_id"] = transaction_id
                        #     pair_data["max_resp_time"]=max_resp_time
                            if function_code >= 0x80:
                                pair_data["exceptions"] += 1
                                print(f"           exception response from {pair_id} dport {packet[TCP].dport} for trans {transaction_id} code {function_code:0x}")



                        # if response_time > delay_threshold:
                        #     event_details = {
                        #         'timestamp': timestamp,
                        #         'transaction_id': transaction_id,
                        #         'response_time': response_time,
                        #         # Add more fields as per your requirement
                        #     }

                        #     # If response time exceeds the threshold, log it as an anomaly
                        #     anomalies.append(event_details)
        elif TCP in packet:
            dp = packet[TCP].dport
            sp = packet[TCP].sport
            plen = len(packet[TCP].payload)
            if plen:
                print(f"unknown ports; source {sp} dest {dp} packet_len {plen}")
        # STP section
        if packet.haslayer(STP):
            stp_packet = packet[STP]
            timestamp = packet.time  # Capture the timestamp of the packet

            # Extract STP event details you're interested in
            event_details = {
                'timestamp': timestamp,
                'root_id': stp_packet.rootid,
                'bridge_id': stp_packet.bridgeid,
                'port_id': stp_packet.portid,
                # Add more fields as per your requirement
            }
            stp_details.append(event_details)
    return anomalies, stp_details


def orig_analyze_modbus_pcap(pcap_file, modbus_tcp_port, delay_threshold=1.0):
    global max_resp_time
    global max_resp_id
    # Reading the pcap file
    packets = rdpcap(pcap_file)

    # Dictionary to hold ongoing queries with timestamps
    queries = {}

    # List to hold anomalies (i.e., delayed responses)
    anomalies = []

    stp_details = []

    for packet in packets:
        # Filter TCP packets on Modbus port
        if TCP in packet and (packet[TCP].dport == modbus_tcp_port or packet[TCP].sport == modbus_tcp_port):
            # Ensure the payload exists, indicative of Modbus TCP/IP
            if len(packet[TCP].payload):
                pair_id, pair_data = register_pair(packet, modbus_tcp_port)
                max_resp_time = pair_data['max_resp_time']
                modbus_frame = bytes(packet[TCP].payload)

                # Modbus TCP/IP ADU: Transaction Identifier is the first 2 bytes
                transaction_id = int.from_bytes(modbus_frame[0:2], byteorder='big')
                timestamp = packet.time  # Capture the timestamp of the packet

                function_code = 0
                if len(modbus_frame) >= 7:
                    function_code = modbus_frame[7]  # 8th byte in Modbus ADU, immediately after Unit Identifier

                # Determine if it's a query or a response based on ports
                if packet[TCP].dport == modbus_tcp_port:
                    # Query to a server, mark the timestamp
                    queries[transaction_id] = packet.time
                    pair_data["queries"] += 1
                    if pair_data["client_port"] != packet[TCP].sport:
                        pair_data["reconnects"] += 1
                        pair_data["client_port"] = packet[TCP].sport
 
                elif packet[TCP].sport == modbus_tcp_port and transaction_id in queries:
                    # Response from a server, calculate the response time
                    response_time = packet.time - queries[transaction_id]
                    pair_data["responses"] += 1

                    if function_code >= 0x80:
                        pair_data["exceptions"] += 1

                    if response_time > max_resp_time:
                        max_resp_time =  response_time 
                        max_resp_id = transaction_id
                        pair_data["max_resp_time"]=max_resp_time


                    if response_time > delay_threshold:
                        event_details = {
                            'timestamp': timestamp,
                            'transaction_id': transaction_id,
                            'response_time': response_time,
                            # Add more fields as per your requirement
                        }

                        # If response time exceeds the threshold, log it as an anomaly
                        anomalies.append(event_details)
        # STP section
        if packet.haslayer(STP):
            stp_packet = packet[STP]
            timestamp = packet.time  # Capture the timestamp of the packet

            # Extract STP event details you're interested in
            event_details = {
                'timestamp': timestamp,
                'root_id': stp_packet.rootid,
                'bridge_id': stp_packet.bridgeid,
                'port_id': stp_packet.portid,
                # Add more fields as per your requirement
            }
            stp_details.append(event_details)


    #analyze_heartbeats(packets, modbus_tcp_port)
    global hb_start
    global hb_reg
    global hb_size

    analyze_heartbeats(packets, modbus_tcp_port, hb_start, hb_reg, hb_size)

    return anomalies, stp_details
#Offset - 768
# num 20
#PLC Address - 40769 (edited) 
HEARTBEAT_REGISTERS = [768, 40769]  # Example addresses of heartbeat registers

#analyze_heartbeats(packets, modbus_tcp_port, 768, 768, 20):
def analyze_heartbeats(packets, modbus_tcp_port, hb_start, hb_reg, hb_size):
    global hb_info
    global num_acks
    # Dictionary to track the last value and count of each heartbeat register
    heartbeats = {reg: {'last_value': None, 'count': 0} for reg in HEARTBEAT_REGISTERS}
    old_trans_ids = {}
    trans_ids = {}
    for packet in packets:
        if packet.haslayer(TCP) and (packet[TCP].dport == modbus_tcp_port or packet[TCP].sport == modbus_tcp_port):

            pair_id, pair_data = register_pair(packet, modbus_tcp_port)
            hb_list = get_hb_defs_for_pair(pair_id, hb_info)

            # Get the raw bytes of the TCP payload, which contains the Modbus ADU
            raw_payload = bytes(packet[TCP].payload)
            timestamp = packet.time  # Capture the timestamp of the packet

            # Ensure the payload is long enough to contain a Modbus ADU (minimum 7 bytes)
            if len(raw_payload) >= 12:
                # Extract the unit identifier, which is at byte 6 in the Modbus ADU
                trans_id = int.from_bytes(raw_payload[0:2], byteorder='big')

                device_id = raw_payload[6]  # Unit Identifier is the 7th byte in Modbus ADU
                function_code = raw_payload[7]  # 8th byte in Modbus ADU, immediately after Unit Identifier
                byte_count = raw_payload[8]  # 9th byte
                #hb_register_start = 9 + offset
                #hb_val = int.from_bytes(raw_payload[hb_register_start:hb_register_start+2], byteorder='big')
                # tenth_register = int.from_bytes(raw_payload[tenth_register_start:tenth_register_start+2], byteorder='big')
                
                # Check if this is a read holding registers request
                if function_code == FUNCTION_CODE_READ_HOLDING_REGISTERS:
                    # Extract Start Offset and Number of Registers
                    start_offset = int.from_bytes(raw_payload[8:10], byteorder='big')
                    num_registers = int.from_bytes(raw_payload[10:12], byteorder='big')

                    # Record the results
                        
                    event_details = {
                        'timestamp': timestamp,
                        'device_id': device_id,
                        'trans_id': trans_id,
                        'function_code': function_code,
                        'start_offset': start_offset,
                        'num_registers': num_registers,
                        'heartbeat': False,
                        'hb_val': 0
                    }
                    # if this is a resp print out the first value
                    if trans_id in old_trans_ids:
                        resp_time = timestamp - old_trans_ids[trans_id]['timestamp']
                        #print(f"            this is a response id {trans_id} after {resp_time} ") 
                        if old_trans_ids[trans_id]['start_offset'] == hb_start and old_trans_ids[trans_id]['num_registers'] == hb_size:
                            old_trans_ids[trans_id]['heartbeat']= True
                            hb_register_start = 9 +(hb_start - hb_reg)
                            hb_val = int.from_bytes(raw_payload[hb_register_start:hb_register_start+2], byteorder='big')
                            old_trans_ids[trans_id]['hb_val']= hb_val
                            hb_reg = old_trans_ids[trans_id]['start_offset']
                            print(f"                {pair_id} ... this is a heartbeat @{hb_reg}  trans {trans_id} value {hb_val} ") 
 

                        trans_ids[trans_id] = old_trans_ids[trans_id]
                        trans_ids[trans_id]["resp_timestamp"] = timestamp
                        del old_trans_ids[trans_id]
                    else:
                        old_trans_ids[trans_id] = event_details
                        #print(f"Device ID: {event_details['device_id']}, trans_id: {event_details['trans_id']}, Function Code: {event_details['function_code']}, start: {event_details['start_offset']}, num : {event_details['num_registers']}")


                    # Do something with the function_code, e.g., print or analyze further...


            # # Implement your logic here to extract register address and value from the Modbus response packet
            # # This will be highly specific to how your Modbus packets are structured and might involve
            # # dissecting the ModbusTCP layer to get to the holding registers
            # register_address = None  # Extract from packet
            # register_value = None    # Extract from packet

            # # Check if this is one of the heartbeat registers
            # if register_address in HEARTBEAT_REGISTERS:
            #     # Check if the value has incremented
            #     if heartbeats[register_address]['last_value'] is not None:
            #         if register_value > heartbeats[register_address]['last_value']:
            #             heartbeats[register_address]['count'] += 1

            #     # Update the last value
            #     heartbeats[register_address]['last_value'] = register_value
        if TCP in packet and packet[TCP].flags.A:
            num_acks += 1

    len_old_ids = len(old_trans_ids)
    len_ids = len(trans_ids)
    total_ids = len_old_ids + len_ids 
    #print(f"\n\nRemaining Requests without Responses: count {len_old_ids}/{total_ids}")
    #for trans_id, details in old_trans_ids.items():
    #    print(f"Transaction ID {trans_id}: {details}")
    return heartbeats


def load_hb_definitions(hb_file_path):
    try:
        with open(hb_file_path, 'r') as hb_file:
            hb_definitions = json.load(hb_file)
            return hb_definitions
    except Exception as e:
        print(f"Error loading heartbeat definitions: {e}")
        return None

def get_hb_defs_for_pair(pair_id, hb_info):
    hb_list = []
    if hb_info:
        for hb_id, details in hb_info.items():
            if details.get("pair_id") == pair_id:  # Check if the pair_id matches
                hb_list.append({hb_id: details})  # Add the hb_def to the list
    
    return hb_list

# Example usage
#pair_id = "192.168.112.5->192.168.112.10:502"
#hb_list_for_pair = get_hb_defs_for_pair(pair_id, hb_defs)
#print(f"Heartbeat definitions for {pair_id}: {json.dumps(hb_list_for_pair, indent=2)}")

if __name__ == '__main__':
    # Argument parsing setup
    parser = argparse.ArgumentParser(description='Analyze Modbus TCP communication for delayed responses')
    parser.add_argument('pcap_file', help='Path to the pcap file containing Modbus TCP traffic')
    parser.add_argument('client_ports', type=str, help='client_ports ', default='[502]')
    parser.add_argument('server_ports', type=str, help='server_ports ', default='[502]')
    #parser.add_argument('delay_threshold', type=float, help='Delay threshold in seconds for considering a response delayed', default=10.0)
    #parser.add_argument("--hb_file", help="Path to the heartbeat definition file", required=False)
    # Parse arguments
    args = parser.parse_args()
    c_ports=[]
    try:
        c_ports = json.loads(args.client_ports)
        if not all(isinstance(port, int) for port in c_ports):
            raise ValueError("All ports must be integers")
    except (json.JSONDecodeError, ValueError) as e:
        print("Error processing client ports list:", e)
        exit(1)
    s_ports=[]
    try:
        s_ports = json.loads(args.server_ports)
        if not all(isinstance(port, int) for port in s_ports):
            raise ValueError("All ports must be integers")
    except (json.JSONDecodeError, ValueError) as e:
        print("Error processing server ports list:", e)
        exit(1)
    # if args.hb_file:
    #     hb_info = load_hb_definitions(args.hb_file)

    # Run the analysis with provided arguments
    anomalies, stp_details = analyze_modbus_pcap(args.pcap_file, c_ports, s_ports ,0 )
    num_anom=len(anomalies)

    #print(f"Total Number of Delayed Responses Detected: {num_anom} max delay of {max_resp_time:.3f} seconds at id {max_resp_id}")
    #for anomaly in anomalies:
    #    print(f"Delayed Response Detected: Timestamp: {anomaly['timestamp']},Transaction ID {anomaly['transaction_id']} with a delay of {anomaly['response_time']:.3f} seconds")

    #num_stp=len(stp_details)
    #print(f"\nSTP Packet Details Detected :{num_stp}")
    #for detail in stp_details:
    #    print(f"Timestamp: {detail['timestamp']}, Root ID: {detail['root_id']}, Bridge ID: {detail['bridge_id']}, Port ID: {detail['port_id']}")


    print("--------")  # Separator for readability    
    print("--------")  # Separator for readability
    print_client_server_pairs()
    print("--------")  # Separator for readability
    # num_stp=len(stp_details)
    # print(f"\nSTP Packets Detected :{num_stp}")
    # print("--------")  # Separator for readability
    # print(f"\nAcks Detected :{num_acks}")
    # print("--------")  # Separator for readability


    # send_to_fims(args.pcap_file, num_anom, num_stp, num_acks)