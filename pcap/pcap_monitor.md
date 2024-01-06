To create a Python script that starts a `tcpdump` process for a given duration and then triggers the `pcap_modbus.py` program, you'll need to use the `subprocess` module for process management and `time` for delays. 

Here is a step-by-step approach:

### 1. Import Necessary Libraries:

```python
import subprocess
import time
from datetime import datetime
```

### 2. Define the Function to Start Tcpdump:

```python
def start_tcpdump(interface, duration=300):
    # Generate a unique filename for each pcap
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"capture_{timestamp}.pcap"

    # Start tcpdump
    tcpdump_cmd = ["tcpdump", "-i", interface, "-w", filename]
    proc = subprocess.Popen(tcpdump_cmd)

    # Return the process and filename for later use
    return proc, filename
```

### 3. Define the Main Function to Manage the Processes:

```python
def manage_captures(interface, overlap=10):
    # Start the first capture
    first_proc, first_filename = start_tcpdump(interface)

    # Wait for duration minus overlap
    time.sleep(300 - overlap)

    # Start the second capture
    second_proc, second_filename = start_tcpdump(interface)

    # Wait for the overlap period
    time.sleep(overlap)

    # Stop the first capture and start analysis
    first_proc.terminate()
    subprocess.run(["python3", "pcap_modbus.py", first_filename])

    # Wait for the second capture to complete its duration
    time.sleep(300 - overlap)

    # Stop the second capture and start analysis
    second_proc.terminate()
    subprocess.run(["python3", "pcap_modbus.py", second_filename])

    # Here you might include logic to archive the file or further actions
```

### 4. Execute the Script with Given Parameters:

```python
if __name__ == "__main__":
    # Specify the network interface to monitor
    interface = "eth0"  # Replace with your actual network interface
    manage_captures(interface)
```

### Notes:

- **Running Tcpdump**: The script requires that `tcpdump` is installed and accessible. Ensure you have the necessary permissions to run `tcpdump`, as it typically requires root or equivalent privileges.
- **Overlapping Captures**: The script calculates the duration of each capture to ensure there's an overlap based on the `overlap` parameter. Adjust the overlap duration as needed for your specific scenario.
- **File Naming**: Each capture file is named with a timestamp to avoid collisions. Ensure your system has sufficient storage for these files, especially if running this script repeatedly.
- **Running pcap_modbus.py**: After each capture, `pcap_modbus.py` is triggered with the filename of the capture. Ensure that `pcap_modbus.py` is in the correct path or provide the full path to the script.
- **Permissions and Environment**: Running network capture tools and scripts that interact with them might require elevated permissions. Make sure to execute this script in an environment where it has the necessary permissions and resources.
- **Graceful Shutdowns and Error Handling**: This script provides a basic structure. Consider adding more robust error handling, especially for ensuring that `tcpdump` processes are terminated gracefully even if the script encounters errors or is interrupted.

This script serves as a framework to automate the monitoring and analysis of network traffic related to Modbus communications. You'll need to integrate it with your existing systems, handle permissions, and potentially add more sophisticated error handling and logging based on your operational requirements. Make sure to thoroughly test in a controlled environment before deploying in a live setting.

Your script is designed to continuously capture network packets on multiple interfaces, determined by the configuration file, and analyze them using a specified program (`pcap_modbus.py` or similar). It's set to handle overlapping captures to ensure continuous data capture and uses a signal handler for graceful termination.

Here's a breakdown and documentation of the script:

### Script: pcap_monitor.py

#### Description:

A Python script that continuously captures network packets on multiple interfaces using `tcpdump`. It handles overlapping captures to ensure continuous coverage and processes each capture file using `pcap_modbus.py` or similar programs. The script is designed to be run as a long-lived process, suitable for monitoring network traffic in a robust manner.

#### Features:

- **Continuous Network Packet Capturing**: Captures packets on specified interfaces continuously with overlapping periods for seamless data coverage.
- **Dynamic Interface Management**: Reads a list of interfaces to monitor from a configuration file, allowing flexible deployment across different network setups.
- **Graceful Termination**: Handles SIGINT and SIGTERM signals to terminate captures and exit cleanly, suitable for running as a service or long-lived process.
- **Logging**: Stores pcap files in a specified log directory, creating the directory if it doesn't exist.

#### Usage:

```bash
python pcap_monitor.py [config_file] --logdir [log_directory]
```

- **config_file**: Optional. The path to the JSON configuration file containing the list of interfaces. Defaults to `pcap_monitor.json`.
- **log_directory**: Optional. The path to the directory where pcap files will be stored. Defaults to `/var/log/gcom`.

#### Configuration File Format:

The configuration file should be a JSON file with the following structure:

```json
{
    "interfaces": ["eth0", "wlan0", ...]
}
```

- **interfaces**: A list of strings, each representing a network interface to monitor.

#### Key Functions:

- **`ensure_log_dir(log_dir)`:** Ensures the specified log directory exists, creating it if necessary.
- **`signal_handler(signum, frame)`:** Handles termination signals to gracefully stop all captures and terminate the script.
- **`load_config(config_file)`:** Loads the configuration file and returns the configuration data.
- **`start_tcpdump(interface, log_dir, duration)`:** Starts a tcpdump process for the specified interface and duration, saving the capture to a file in the log directory.
- **`manage_captures(interfaces, log_dir, overlap, duration)`:** Manages continuous overlapping captures for each specified interface.

#### Signal Handling:

The script registers custom handlers for SIGINT and SIGTERM to ensure that all ongoing captures are terminated cleanly when the script receives a termination signal. This is particularly important for running the script as a service or in production environments.

#### Error Handling:

- Checks if the configuration file exists and is valid JSON.
- Handles errors related to log directory creation.

#### System Requirements:

- Python 3
- `tcpdump` installed and accessible in the system path.
- Sufficient permissions to capture packets on the specified interfaces and to create and write to the log directory.

#### Deployment:

The script is suitable for deployment as a system service using tools like `systemctl`. Ensure it's run with appropriate permissions, typically as root, to allow packet capturing.

### Conclusion:

`pcap_monitor.py` provides a robust, continuous packet capturing solution suitable for network monitoring and analysis tasks. It's designed to be flexible, configurable, and deployable across various environments. Always ensure to test in your specific environment and adjust the configuration, paths, and permissions as needed.


To run your script as a service, you can create a systemd service file. This file will instruct the system how to start and manage your script, including how to start it at boot, how to restart it if it fails, and more.

Here is an example of what the service file might look like for your `pcap_monitor.py` script. This assumes your script is located at `/path/to/pcap_monitor.py` and you want to use the default configuration file and log directory. Adjust the paths and parameters according to your actual setup.

### Creating a Service File for pcap_monitor.py

1. **Create a Service File**:

First, create a new service file under `/etc/systemd/system/`. Let's call it `pcap_monitor.service`.

```bash
sudo nano /etc/systemd/system/pcap_monitor.service
```

2. **Add the Following Content**:

```ini
[Unit]
Description=Continuous Network Packet Capture
After=network.target

[Service]
Type=simple
User=root
ExecStart=/usr/bin/python3 /usr/local/bin/to/pcap_monitor.py
WorkingDirectory=/var/log/gcom
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
```

### Explanation:

- **[Unit] Section**:
    - `Description`: A human-readable description of the service.
    - `After`: Ensures that the script starts after the network is up.
- **[Service] Section**:
    - `Type=simple`: The service is considered started immediately as the main process is run.
    - `User=root`: Specifies that the service should run as the root user, necessary for most network capturing tools.
    - `ExecStart`: The command to start your script. Change `/path/to/pcap_monitor.py` to the actual path to your script.
    - `WorkingDirectory`: The directory in which to execute the command (should be the directory of your script or where your logs/configs are expected).
    - `Restart=on-failure`: The service will restart if it fails.
    - `RestartSec`: The time to wait before restarting the service.
- **[Install] Section**:
    - `WantedBy`: Defines the target that the service should attach to if enabled (commonly `multi-user.target` for scripts needing full multi-user functionality).

### Activating the Service:

After saving and exiting the service file, refresh the systemd manager configuration, enable, and start your service:

```bash
sudo systemctl daemon-reload        # Reload systemd manager configuration
sudo systemctl enable pcap_monitor  # Enable pcap_monitor service
sudo systemctl start pcap_monitor   # Start pcap_monitor service
```

To check the status of your service:

```bash
sudo systemctl status pcap_monitor
```

To view logs produced by your service:

```bash
journalctl -u pcap_monitor
```

### Notes:

1. **Permissions**: Since network capturing typically requires root permissions, this service runs as the root user. Ensure that this is acceptable in your environment and that your script is secure.
2. **Paths**: Replace `/path/to/pcap_monitor.py` and `/path/to/` with the actual paths on your system.
3. **Python Path**: Ensure that `/usr/bin/python3` is the correct path to Python3 on your system. You can find out by running `which python3`.
4. **Safety**: Be cautious when running scripts as root, especially if they're custom or untested, as this can pose significant security risks.

By following these steps, you should be able to set up your script as a service that starts automatically and runs continuously in the background, capturing network packets as specified. Ensure to test everything in a controlled environment before deploying to production.

Your script aims to parse arguments for configuration file, configuration directory, and log directory, set up signal handling, ensure the log directory exists, and run a function called `run_pcap_file_scanner` with the configuration directory as an argument. Here's a breakdown of whether it will work and potential issues:

1. **Argument Parsing**: You're using `argparse` to set up arguments for the script. This part looks fine, but it's worth noting that both `config_file` and `config_dir` are positional arguments with optional defaults due to `nargs='?'`. This means if a user provides one argument, it will be assumed to be `config_file`, and if they provide two, the second will be taken as `config_dir`. Ensure this is the intended behavior.

2. **Signal Handling**: You're setting up signal handlers for `SIGINT` and `SIGTERM` signals, which should allow the script to handle interrupt and termination signals gracefully. This part seems appropriate for long-running scripts and is especially useful when running as a service.

3. **Log Directory**: The script checks and ensures that the log directory exists. This is good practice, especially for scripts that write out logs or data files.

4. **Running pcap_file_scanner**: The script attempts to run a function `run_pcap_file_scanner(args.config_dir)`. For this to work:
   - Ensure that the function `run_pcap_file_scanner` is defined somewhere in your script or imported from another module.
   - Ensure that the function is designed to accept the configuration directory as an argument and performs the expected actions, like running the `pcap_file_scanner.py` script or equivalent functionality.

5. **Permissions**: If the script is meant to perform network packet capturing and write to `/var/log/gcom`, it might require elevated permissions (root). Ensure that it's run with appropriate permissions, especially when deployed as a service.

6. **Testing**: Before deploying, test the script manually with various combinations of arguments to ensure it behaves as expected.

Here's an improved version of your script segment considering the above points and a bit more clarification on the `run_pcap_file_scanner` function:

```python
import argparse
import signal
# ... other necessary imports ...

# Ensure this function is defined or imported
def run_pcap_file_scanner(config_dir):
    # Your code to initiate pcap_file_scanner with the given directory
    pass

if __name__ == "__main__":
    # Setup argument parser
    parser = argparse.ArgumentParser(description="Continuously capture network packets on multiple interfaces")
    parser.add_argument("config_file", help="Configuration file containing the list of interfaces", nargs='?', default="pcap_monitor.json")
    parser.add_argument("config_dir", help="Configuration dir containing the list of config files", nargs='?', default="/home/docker/configs/modbus/client")
    parser.add_argument("--logdir", help="Directory to store log files", default="/var/log/gcom")

    args = parser.parse_args()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    ensure_log_dir(args.logdir)

    # Assuming this function exists and is correctly implemented
    run_pcap_file_scanner(args.config_dir)
```

Ensure all referenced functions are defined and tested, and be aware of permissions and execution contexts, especially when dealing with network interfaces and system directories. With these considerations in mind, your script segment is on the right track for its intended purpose.

Detecting TCP retransmissions using Python and the pcap library involves several steps. TCP retransmissions occur when segments of data are not acknowledged by the recipient and need to be resent. Here's a general approach you might consider:

1. **Capture Packets**: First, you need to capture the network packets. This can be done using the `pcap` library, which can capture packets over the network interface.

2. **Analyze TCP Segments**: Once you have the packets, you need to analyze the TCP segments within those packets. Specifically, you'll want to look at the sequence and acknowledgment numbers, as well as the retry intervals.

3. **Identify Retransmissions**: A TCP retransmission can be identified if you observe:

    - **Duplicate Segments**: The same segment (identified by sequence number) is sent more than once.
    - **Out-of-Order Segments**: Received segments are not in the expected sequence.
    - **Acknowledgment Numbers**: An acknowledgment number that goes back instead of progressing can indicate a retransmission.

Here's a simplified structure of how you might code this:

```python
import pcap
import socket
import struct

# Open live pcap session on interface
pc = pcap.pcap(name='en0', promisc=True, immediate=True, timeout_ms=50)

# Filter for TCP packets
pc.setfilter('tcp')

for timestamp, packet in pc:
    # Unpack the Ethernet frame (assuming Ethernet + IPv4)
    eth_header = packet[:14]
    eth_unpacked = struct.unpack("!6s6s2s", eth_header)
    ip_header = packet[14:34]
    iph = struct.unpack('!BBHHHBBH4s4s', ip_header)

    # Extract IP headers to calculate offsets
    iph_length = (iph[0] & 0xF) * 4
    tcp_header = packet[14+iph_length:34+iph_length]

    # Unpack TCP header
    tcph = struct.unpack('!HHLLBBHHH', tcp_header)

    # Extract sequence and acknowledgment numbers
    seq_number = tcph[2]
    ack_number = tcph[3]

    # Logic to detect retransmissions based on sequence and acknowledgment numbers
    # This would likely involve maintaining a history of observed segments and
    # detecting duplicates or unusual patterns indicating a retransmission.

    # Print or log suspected retransmissions
```

This is a very simplified outline, and in practice, you'd need to handle various edge cases and potentially use a more robust method of dissecting packets (like using a library specifically for packet dissection such as dpkt or scapy).

Remember, working with live network data and interpreting TCP can be complex, and ensuring that you're correctly identifying retransmissions involves understanding the nuances of TCP behavior under various network conditions. Always test your implementation in a controlled environment before deploying it in a production setting.
Using Scapy to detect TCP retransmissions involves capturing packets, analyzing them for TCP protocol details, and looking for signs of retransmissions. Scapy is a powerful Python-based interactive packet manipulation program that can handle tasks from packet sniffing to packet crafting and more.

Here's a basic outline of steps and a sample code structure you might use:

1. **Install Scapy**: Make sure you have Scapy installed in your Python environment. You can typically install it via pip:

    ```bash
    pip install scapy
    ```

2. **Capture Packets**: Use Scapy to capture TCP packets and analyze them.

3. **Analyze for Retransmissions**: Look for sequence numbers of packets. A TCP retransmission is typically identified by the same sequence number being used more than once with no acknowledgment from the other side.

Here is how you might write the script:

```python
from scapy.all import sniff, TCP

# Define a packet handling function
def packet_callback(packet):

    if packet[TCP].flags == 'PA':  # Adjust the flags as needed for your context
        seq_num = packet[TCP].seq
        ack_num = packet[TCP].ack

        # Assuming you're storing seen sequence numbers in some form of data structure
        # For the sake of example, let's assume a global dictionary with IP pairs as keys
        global seen_seq_nums
        ip_pair = (packet[IP].src, packet[IP].dst)

        if ip_pair not in seen_seq_nums:
            seen_seq_nums[ip_pair] = set()

        # Check if sequence number has been seen before
        if seq_num in seen_seq_nums[ip_pair]:
            print(f"Retransmission Detected: {packet.summary()}")
        else:
            seen_seq_nums[ip_pair].add(seq_num)

# Initialize a structure to hold seen sequence numbers
seen_seq_nums = {}

# Start sniffing the network.
sniff(filter="tcp", prn=packet_callback, store=0)
```

### Notes:

- **Flags**: The script above is checking for packets with a particular TCP flag ('PA' - PSH and ACK). In practice, you might need to adjust the flags you're looking for based on the type of traffic you expect to capture or the characteristics of retransmissions you're interested in.
- **Performance**: Scapy is quite powerful, but it's not designed for performance in high-throughput environments. For monitoring or real-time analysis on busy networks, consider using more performant solutions or directly capturing packets with Scapy and then analyzing them offline.
- **Environment**: Ensure you have the necessary permissions and are compliant with all relevant policies and laws when capturing network traffic, especially on networks that you do not own or manage.

This script provides a starting point. You'd likely need to refine the detection logic based on the specific characteristics of the network traffic you're dealing with and the nature of TCP retransmissions you're trying to identify.

To detect connection attempts from TCP dump files using Python and Scapy, you'll need to follow a general process that involves reading the TCP dump files, filtering for TCP packets, and then examining those packets for connection attempts. Here's a step-by-step guide on how you might approach this:

1. **Install Scapy**: If you haven't already, you will need to install the Scapy library, which is a powerful interactive packet manipulation program. You can install it using pip:

    ```bash
    pip install scapy
    ```

2. **Read the TCP dump files**: Use Scapy to read the TCP dump files. TCP dump files are typically saved in the `.pcap` format. Scapy has a `rdpcap()` function that can read these files.

3. **Filter for TCP packets**: Once you have read the dump file, you will need to filter out the TCP packets as they are the ones that are used to establish connections.

4. **Detect Connection Attempts**: A TCP connection is established through a process known as the three-way handshake. This involves three steps: SYN, SYN-ACK, and ACK. To detect an initial connection attempt, you should look for packets with the SYN flag set, which indicates the initiation of a TCP connection.

Here is a basic script that demonstrates these steps:

```python
from scapy.all import rdpcap, TCP

# Read the tcpdump file
packets = rdpcap('path_to_file.pcap')

# Filter for TCP packets and then look for SYN packets which indicate connection attempts
for packet in packets:
    if packet.haslayer(TCP):
        # Check for SYN and not ACK to identify a new connection attempt
        if packet[TCP].flags == 'S':
            src_ip = packet[IP].src
            dst_ip = packet[IP].dst
            print(f"Connection attempt from {src_ip} to {dst_ip}")
```

Replace `'path_to_file.pcap'` with the path to your actual .pcap file.

**Important Notes:**

- This script assumes that the TCP dump file is named 'file.pcap' and is located in the same directory as the script. You will need to adjust the path as necessary.
- The script is quite simplistic and demonstrates the basics of detecting TCP connection attempts (SYN packets). Depending on your specific needs, you might want to handle SYN-ACK packets or add more sophisticated analysis.
- Ensure that you run these scripts with the necessary permissions. Accessing network interfaces and reading pcap files might require administrative privileges depending on your system's configuration.

Always test this in a controlled environment before deploying in a live network, and make sure you're compliant with legal restrictions and ethical guidelines when working with network traffic.

Capturing the response to a connection attempt and timing it involves several additional steps. First, you'll need to track not just the SYN packets that represent the initiation of a connection attempt but also the SYN-ACK packets that represent the server's response and the final ACK packet that completes the client's request. This series is what forms the well-known "three-way handshake" in TCP connections.

Here's an outline of the steps and a script to guide you:

1. **Track the Three-Way Handshake**: Monitor for the SYN, SYN-ACK, and ACK packets to understand the complete flow of a TCP connection establishment.

2. **Capture the Response**: Look for SYN-ACK packets that are responses to initial SYN packets.

3. **Time the Connection**: Record the time when you first see a SYN packet and then the time when you see the corresponding ACK packet. The difference between these timestamps will give you the duration of the connection setup.

Here's how you might implement this:

```python
from scapy.all import rdpcap, TCP, IP

# Read the tcpdump file
packets = rdpcap('path_to_file.pcap')

# Initialize a dictionary to keep track of SYN times
syn_times = {}

# Iterate through packets
for packet in packets:
    if packet.haslayer(TCP) and packet.haslayer(IP):
        src_ip = packet[IP].src
        dst_ip = packet[IP].dst
        tcp_layer = packet[TCP]
        # Check for SYN flag (connection initiation)
        if tcp_layer.flags == 'S':
            syn_times[(src_ip, dst_ip)] = packet.time
        # Check for SYN-ACK flag (connection response)
        elif tcp_layer.flags == 'SA':
            if (dst_ip, src_ip) in syn_times:  # Note the reversed src and dst
                # Calculate the time difference
                duration = packet.time - syn_times[(dst_ip, src_ip)]
                print(f"Connection established from {dst_ip} to {src_ip} in {duration} seconds")
                # Remove the recorded SYN time if you no longer need it
                del syn_times[(dst_ip, src_ip)]
```

Here's what this script does:

- It reads the pcap file and iterates through the packets, looking for TCP layers.
- When it finds a SYN packet (indicating an attempt to start a connection), it records the time.
- When it finds a SYN-ACK packet (indicating a response to a connection attempt), it looks up the time of the original SYN packet, calculates the duration, and then outputs the time it took to establish the connection.

**Important Considerations:**

- **Accuracy and Precision**: The accuracy of the timing will depend on the precision of your packet timestamps which is often dependent on the capturing tool and system resolution.
- **Packet Loss**: In real networks, packets might get lost. Make sure to handle cases where you might not see a SYN-ACK or the final ACK for every SYN.
- **Duplicate SYNs**: Sometimes, especially in poor network conditions, a client might send multiple SYN packets. Ensure that your script can handle these cases, possibly by updating the timestamp for the last SYN packet received from the same source.
- **Permissions and Environment**: Similar to before, ensure you have the right permissions and are aware of legal and ethical guidelines in your environment.

This script is quite basic and might need to be expanded or modified to fit specific needs or to handle more complex scenarios in network traffic. Always thoroughly test in a controlled environment before deploying any network analysis tool in a live or production environment.


To modify the script to keep track of connection attempts, responses, and their respective times without deleting the entries in `syn_times`, you can use a more comprehensive dictionary structure. Also, to translate times to UTC, you will typically use the `datetime` module from Python to handle and format the timestamps properly. Here's how you can do it:

1. **Keep Connection Details**: Store connection attempt and result details in the `syn_times` dictionary.
2. **Translate Time to UTC**: Use the `datetime` module to convert timestamps to UTC.

Here's the modified script:

```python
from scapy.all import rdpcap, TCP, IP
from datetime import datetime, timezone

# Read the tcpdump file
packets = rdpcap('path_to_file.pcap')

# Initialize a dictionary to keep track of connection details
syn_times = {}

# Iterate through packets
for packet in packets:
    if packet.haslayer(TCP) and packet.haslayer(IP):
        src_ip = packet[IP].src
        dst_ip = packet[IP].dst
        tcp_layer = packet[TCP]
        # Check for SYN flag (connection initiation)
        if tcp_layer.flags == 'S':
            syn_times[(src_ip, dst_ip)] = {'attempt_time': datetime.utcfromtimestamp(packet.time).replace(tzinfo=timezone.utc), 'duration': None, 'result': 'Initiated'}
        # Check for SYN-ACK flag (connection response)
        elif tcp_layer.flags == 'SA':
            if (dst_ip, src_ip) in syn_times:  # Note the reversed src and dst
                # Calculate the duration and record the response time
                duration = packet.time - syn_times[(dst_ip, src_ip)]['attempt_time'].timestamp()
                syn_times[(dst_ip, src_ip)]['response_time'] = datetime.utcfromtimestamp(packet.time).replace(tzinfo=timezone.utc)
                syn_times[(dst_ip, src_ip)]['duration'] = duration
                syn_times[(dst_ip, src_ip)]['result'] = 'Success'

# Print out connection details
for conn, details in syn_times.items():
    print(f"Connection from {conn[0]} to {conn[1]}: {details}")
```

Here's what's changed:

- **syn_times Dictionary**: Now, `syn_times` is used to store detailed information about each connection attempt, including the attempt time, duration (if the connection was established), and the result of the attempt.
- **Time Conversion**: The script converts the packet times to UTC using the `datetime` module and attaches timezone information for clarity.
- **Preserve Data**: It no longer deletes entries from `syn_times` after processing them, allowing you to print out all connection attempts at the end.

**Note:**

- This script assumes the pcap file includes the correct timestamps. Ensure the system capturing the packets is synchronized with a reliable time source.
- The script labels a connection as 'Success' upon seeing a SYN-ACK packet, which might not always be accurate in real-world scenarios where connections can fail later in the process. For more robust handling, consider tracking the final ACK in the three-way handshake or subsequent connection teardown packets.
- The script now prints out a detailed dictionary at the end, which includes all the tracked connection attempts and their outcomes. You might want to format this printout or further process the data depending on your needs.

As always, validate this approach in a controlled environment and ensure you're authorized to analyze any network traffic you work with.
