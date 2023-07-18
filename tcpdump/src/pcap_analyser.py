import sys
from scapy.all import *
from influxdb import InfluxDBClient

# Configure InfluxDB connection
influx_config = {
    'host': 'localhost',
    'port': 8086,
    'username': 'your_username',
    'password': 'your_password',
    'database': 'your_database',
}

# Open the PCAP file
pcap_file = 'capture.pcap'
packets = rdpcap(pcap_file)

# Analyze packets and detect problems
dropped_packets = []
duplicate_returns = []
excessive_delays = []

for packet in packets:
    print (packet)
    if packet.haslayer(ARP):
        arp_packet = packet[ARP]
        print('ARP opcode: ', str(arp_packet.op))
        print('ARP sender MAC: ', arp_packet.hwsrc)
        print('ARP target MAC: ', arp_packet.hwdst)
        print('ARP sender IP: ', arp_packet.psrc)
        print('ARP target IP: ', arp_packet.pdst)
    # Detect dropped packets
    try:
        if packet.time - packets[packet.time - 1].time > 0.1:
            dropped_packets.append(packet)
    except:
        print ( " ...... unable to get packet.time")

    # Detect duplicate returns
    #if packet.duplicated == 1:
    #    duplicate_returns.append(packet)

    # Detect excessive response delays
    if packet.haslayer(TCP) and packet.haslayer(IP):
        try:
            if packet.time - packets[packet.time - 1].time > 0.5:
                excessive_delays.append(packet)
        except:
            print ( " ...... TCP layer unable to get packet.time")

sys.exit()

# Connect to InfluxDB
client = InfluxDBClient(**influx_config)

# Create the InfluxDB database if it doesn't exist
database = influx_config['database']
if database not in client.get_list_database():
    client.create_database(database)

# Switch to the specified database
client.switch_database(database)

# Write findings into InfluxDB
for packet in dropped_packets:
    json_body = [
        {
            "measurement": "dropped_packets",
            "time": packet.time,
            "fields": {
                "source_ip": packet[IP].src,
                "destination_ip": packet[IP].dst
            }
        }
    ]
    client.write_points(json_body)

for packet in duplicate_returns:
    json_body = [
        {
            "measurement": "duplicate_returns",
            "time": packet.time,
            "fields": {
                "source_ip": packet[IP].src,
                "destination_ip": packet[IP].dst
            }
        }
    ]
    client.write_points(json_body)

for packet in excessive_delays:
    json_body = [
        {
            "measurement": "excessive_delays",
            "time": packet.time,
            "fields": {
                "source_ip": packet[IP].src,
                "destination_ip": packet[IP].dst
            }
        }
    ]
    client.write_points(json_body)

# Close the InfluxDB connection
client.close()
#Make sure to replace 'localhost', 8086, 'your_username', 'your_password', 'your_database', 'dropped_packets', 'duplicate_returns', and 'excessive_delays' with your actual InfluxDB configuration and measurement names.

#To capture TCPDump data into multiple files, each lasting for 60 seconds, you can create a bash script. Here's an example:

#b