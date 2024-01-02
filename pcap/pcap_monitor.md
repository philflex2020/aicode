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
