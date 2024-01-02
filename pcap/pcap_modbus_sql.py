import argparse
from scapy.all import *
from scapy.layers.inet import TCP
from scapy.layers.l2 import LLC, STP
import sqlite3


max_resp_time = 0.0
max_resp_id = 0

# Replace this with the actual server port for Modbus TCP/IP
MODBUS_TCP_PORT = 502
FUNCTION_CODE_READ_HOLDING_REGISTERS = 0x03

#setup_sql('modbus_traffic.db')
def setup_sql(dbname):
    # Connect to SQLite database (or create if it doesn't exist)
    conn = sqlite3.connect(dbname)
    cursor = conn.cursor()

    # Create payloads table
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS payloads (
        timestamp REAL,
        sourceip TEXT,
        sport INTEGER,
        destip TEXT,
        dport INTEGER,
        payload BLOB
    )
    ''')

    # Create transactions table
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS transactions (
        timestamp REAL,
        trans_id INTEGER,
        resp_time REAL,
        query_payload BLOB,
        resp_payload BLOB,
        device_id INTEGER,
        function_code INTEGER,
        start_offset INTEGER,
        num_regs INTEGER
    )
    ''')

    # Committing changes and closing the connection to the database for now
    conn.commit()
    conn.close()  
#setup_sql('modbus_traffic.db')
def capture_and_insert(pcap_file,modbus_port):
    entries = 0
    found = 0
    packets = rdpcap(pcap_file)
    #modbus_port = 502  # Default Modbus port

    # Reopen the database connection
    conn = sqlite3.connect('modbus_traffic.db')
    cursor = conn.cursor()

    for packet in packets:
        if TCP in packet and (packet[TCP].sport == modbus_port or packet[TCP].dport == modbus_port):
            timestamp = float(packet.time)
            sourceip = packet[IP].src
            sport = packet[TCP].sport
            destip = packet[IP].dst
            dport = packet[TCP].dport
            payload = bytes(packet[TCP].payload)
            # Check for existing entry
            if entries== 0 and found == 0:
                print(f" checking timestamp {timestamp} type {type(timestamp)}")
            cursor.execute('''SELECT * FROM payloads WHERE timestamp = ? AND 
                                                      sourceip = ? AND 
                                                      sport = ? AND 
                                                      destip = ? AND 
                                                      dport = ?''',
                           (timestamp, sourceip, sport, destip, dport))

            if cursor.fetchone() is None:
                entries += 1
                # Inserting into payloads table if no existing entry is found
                cursor.execute('''INSERT INTO payloads (timestamp, sourceip, sport, destip, dport, payload) 
                                  VALUES (?, ?, ?, ?, ?, ?)''',
                               (timestamp, sourceip, sport, destip, dport, payload))
    
            else:
                found += 1
            # # Inserting into payloads table
            # cursor.execute('''INSERT INTO payloads (timestamp, sourceip, sport, destip, dport, payload) 
            #                   VALUES (?, ?, ?, ?, ?, ?)''',
            #                (timestamp, sourceip, sport, destip, dport, payload))
    
    # Commit changes and close connection
    conn.commit()
    conn.close()
    print(f" Added {entries} entries Found {found} entries")
    return

def analyze_modbus_pcap(pcap_file, delay_threshold=1.0):
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
        if TCP in packet and (packet[TCP].dport == MODBUS_TCP_PORT or packet[TCP].sport == MODBUS_TCP_PORT):
            # Ensure the payload exists, indicative of Modbus TCP/IP
            if len(packet[TCP].payload):
                modbus_frame = bytes(packet[TCP].payload)

                # Modbus TCP/IP ADU: Transaction Identifier is the first 2 bytes
                transaction_id = int.from_bytes(modbus_frame[0:2], byteorder='big')
                timestamp = packet.time  # Capture the timestamp of the packet

                # Determine if it's a query or a response based on ports
                if packet[TCP].dport == MODBUS_TCP_PORT:
                    # Query to a server, mark the timestamp
                    queries[transaction_id] = packet.time
                elif packet[TCP].sport == MODBUS_TCP_PORT and transaction_id in queries:
                    # Response from a server, calculate the response time
                    response_time = packet.time - queries[transaction_id]
                    if response_time > max_resp_time:
                        max_resp_time =  response_time 
                        max_resp_id = transaction_id

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
    analyze_heartbeats(packets)
    return anomalies, stp_details
#Offset - 768
# num 20
#PLC Address - 40769 (edited) 
HEARTBEAT_REGISTERS = [768, 40769]  # Example addresses of heartbeat registers

def analyze_heartbeats(packets):
    # Dictionary to track the last value and count of each heartbeat register
    heartbeats = {reg: {'last_value': None, 'count': 0} for reg in HEARTBEAT_REGISTERS}
    old_trans_ids = {}
    trans_ids = {}
    for packet in packets:
        if packet.haslayer(TCP) and (packet[TCP].dport == MODBUS_TCP_PORT or packet[TCP].sport == MODBUS_TCP_PORT):
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
                        print(f"            this is a response id {trans_id} after {resp_time} ") 
                        if old_trans_ids[trans_id]['start_offset'] == 768 and old_trans_ids[trans_id]['num_registers'] == 20:
                            old_trans_ids[trans_id]['heartbeat']= True
                            hb_register_start = 9 #+ offset
                            hb_val = int.from_bytes(raw_payload[hb_register_start:hb_register_start+2], byteorder='big')
                            old_trans_ids[trans_id]['hb_val']= hb_val
                            print(f"                         this is a heartbeat  {trans_id} value {hb_val} ") 
 

                        trans_ids[trans_id] = old_trans_ids[trans_id]
                        trans_ids[trans_id]["resp_timestamp"] = timestamp
                        del old_trans_ids[trans_id]
                    else:
                        old_trans_ids[trans_id] = event_details
                        print(f"Device ID: {event_details['device_id']}, trans_id: {event_details['trans_id']}, Function Code: {event_details['function_code']}, start: {event_details['start_offset']}, num : {event_details['num_registers']}")


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
    len_old_ids = len(old_trans_ids)
    len_ids = len(trans_ids)
    total_ids = len_old_ids + len_ids 
    print(f"\n\nRemaining Requests without Responses: count {len_old_ids}/{total_ids}")
    for trans_id, details in old_trans_ids.items():
        print(f"Transaction ID {trans_id}: {details}")
    return heartbeats

if __name__ == '__main__':
    # Argument parsing setup
    parser = argparse.ArgumentParser(description='Analyze Modbus TCP communication for delayed responses')
    parser.add_argument('pcap_file', help='Path to the pcap file containing Modbus TCP traffic')
    parser.add_argument('modbus_port', type=int, default=502, help='Delay threshold in seconds for considering a response delayed')
    parser.add_argument('delay_threshold', type=float, default=5.0, help='Delay threshold in seconds for considering a response delayed')

    # Parse arguments
    args = parser.parse_args()

    setup_sql('modbus_traffic.db')

    capture_and_insert(args.pcap_file,args.modbus_port)
    #print(f"Total Number of Entries added : {entries}")
    

    # # Run the analysis with provided arguments
    # anomalies, stp_details = analyze_modbus_pcap(args.pcap_file, args.delay_threshold)
    # num_anom=len(anomalies)
    # print(f"Total Number of Delayed Responses Detected: {num_anom} max delay of {max_resp_time:.3f} seconds at id {max_resp_id}")
    # for anomaly in anomalies:
    #     print(f"Delayed Response Detected: Timestamp: {anomaly['timestamp']},Transaction ID {anomaly['transaction_id']} with a delay of {anomaly['response_time']:.3f} seconds")

    # num_stp=len(stp_details)
    # print(f"\nSTP Packet Details Detected :{num_stp}")
    # for detail in stp_details:
    #     print(f"Timestamp: {detail['timestamp']}, Root ID: {detail['root_id']}, Bridge ID: {detail['bridge_id']}, Port ID: {detail['port_id']}")