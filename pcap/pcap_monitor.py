#####
#Create a Python script that starts overlapping  `tcpdump` processes for a given duration and then 
# triggers the `pcap_modbus.py` or `pcap+dnp3.py` programs.
# The captures are each for a set period of time and have an 'overlap' period to ensure continuous data 
# capture
#The `subprocess` module is used for process management and `time` for delays. 
# python pcap_monitor.py config_file (pcap_monitor.json ) log_dir (/var/log/gcom)  # Replace with actual interface names
# uses a config file default name pcap_monitor.json with this data
# note this file will be produced by the pcap_fil_scanner.py program
#{
#    "interfaces": ["eth0", "wlan0"]
#}

####

import os
import json
import argparse
import signal
import subprocess
import time
from datetime import datetime
dt_format = "%Y%m%d_%H%M%S"

procs = {}


def ensure_log_dir(log_dir):
    # Create log directory if it does not exist
    if not os.path.exists(log_dir):
        try:
            os.makedirs(log_dir)
        except OSError as e:
            print(f"Error: {e.strerror} - Log directory ({log_dir}) could not be created.")
            exit(1)

def run_pcap_file_scanner():
    scanner_script = '/usr/local/bin/pcap_file_scanner.py'  # Update with the actual path
    try:
        # Run the scanner script and wait for it to complete
        subprocess.run(['python3', scanner_script], check=True)
    except subprocess.CalledProcessError as e:
        print(f"Failed to run pcap_file_scanner: {e}")
        # Handle error or possibly exit if the scanner is critical to subsequent operations

# Ensure the log directory exists at the beginning of your script
#ensure_log_dir(args.logdir)

def signal_handler(signum, frame):
    print(f"Received termination signal {signum}")
    # Terminate all tcpdump processes
    for proc in procs.values():
        proc.terminate()
    print("All captures terminated. Exiting.")
    exit(0)  # Exit the program

def load_config(config_file):
    with open(config_file, 'r') as file:
        config = json.load(file)
    return config

def start_tcpdump(interface, log_dir, duration=300):
    # Generate a unique filename for each pcap
    timestamp = datetime.now().strftime(dt_format)
    filename = f"{log_dir}/capture_{interface}_{timestamp}.pcap"

    # Start tcpdump
    tcpdump_cmd = ["tcpdump", "-i", interface, "-w", filename]
    proc = subprocess.Popen(tcpdump_cmd)

    # Return the process and filename for later use
    return proc, filename

def manage_captures(interfaces, log_dir, overlap=10, duration=300):
    global procs
    try:
        while True:
            filenames = {}
            
            # Start captures for each interface
            for interface in interfaces:
                procs[interface], filenames[interface] = start_tcpdump(interface, log_dir, duration)

            # Wait for duration minus overlap
            time.sleep(duration - overlap)

            # Stop previous captures and analyze them
            for interface in interfaces:
                procs[interface].terminate()
                subprocess.run(["python3", "pcap_modbus.py", filenames[interface]])

                # Immediately start the next capture for no gap
                procs[interface], filenames[interface] = start_tcpdump(interface, duration)

            # Wait for the overlap period
            time.sleep(overlap)

    except KeyboardInterrupt:
        # On a keyboard interrupt/termination signal, terminate all tcpdump processes
        print("\nTerminating all captures...")
        for proc in procs.values():
            proc.terminate()
        print("All captures terminated.")

# Main execution
if __name__ == "__main__":
    # Setup argument parser
    parser = argparse.ArgumentParser(description="Continuously capture network packets on multiple interfaces")
    parser.add_argument("config_file", help="Configuration file containing the list of interfaces", nargs='?', default="pcap_monitor.json")
    parser.add_argument("--logdir", help="Directory to store log files", default="/var/log/gcom")

    args = parser.parse_args()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    ensure_log_dir(args.logdir)

    run_pcap_file_scanner()

    try:
        config = load_config(args.config_file)
        interfaces = config.get('interfaces', [])  # Provide default empty list if not found

        # Check if interfaces are provided
        if interfaces:
            manage_captures(interfaces,args.logdir)
        else:
            print("No interfaces found in configuration file.")
    except FileNotFoundError:
        print(f"Configuration file {args.config_file} not found.")
    except json.JSONDecodeError:
        print(f"Error decoding JSON from the file {args.config_file}.")

