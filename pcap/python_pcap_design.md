### Documentation: Python Modbus TCPdump Analyzer

p. wilshire
12_29_2023


**Script Name:** `pcap_modbus.py`

**Usage:** 

```bash
python3 pcap_modbus.py <tcpdump_file> <modbus_port> <response_delay_threshold> --hb_file <hb_defs_file>
```

**Description:** 

The Python Modbus TCPdump Analyzer (`pcap_modbus.py`) is a tool for analyzing tcpdump captures focusing on Modbus TCP traffic. It is especially useful for diagnosing network issues related to Modbus communications, identifying missing responses, reconnects, and monitoring heartbeat signals within the network.

**Arguments:**

- `<tcpdump_file>`: The path to the pcap file containing the Modbus TCP traffic capture.
- `<modbus_port>`: The port used for Modbus TCP communications, typically 502.
- `<response_delay_threshold>`: The threshold in seconds for considering a response as delayed.
- `--hb_file <hb_defs_file>`: (Optional) Path to a JSON file defining heartbeat registers.

**Output:**

- **FIMS Output Messages**: Publishes a series of FIMS messages containing summaries of the capture analysis:
    - General summary including delays, STP packets, and ACK messages.
    - Client-server specific activity including queries, responses, exceptions, max response time, and reconnects.
- **Command-Line Output**: Provides detailed information about client-server pairs, including the number of queries, responses, exceptions, and detected heartbeats.

**Heartbeat Definition File (`hb_defs.json`):**

- A JSON formatted file containing definitions for heartbeat registers. Each definition includes:
    - `pair_id`: Identifies the client-server pair.
    - `hb_reg_type`: Type of register (e.g., holding register).
    - `hb_dev_id`: The device ID for Modbus communication.
    - `hb_start`: The starting address of the register set.
    - `hb_reg`: The specific register address used for heartbeat.
    - `hb_size`: The size or count of registers.

```
{
    "hb1": {
        "pair_id": "192.168.112.5->192.168.112.10:502",
        "hb_reg_type": 3,
        "hb_dev_id": 255,
        "hb_start": 768,
        "hb_reg": 768,
        "hb_size": 20
    },
    "hb2": {
        "pair_id": "192.168.112.5->192.168.112.20:502",
        "hb_reg_type": 3,
        "hb_dev_id": 1,
        "hb_start": 1000,
        "hb_reg": 1002,
        "hb_size": 10
    }
}
```

**Heartbeat Detection:**

- The script utilizes the, optional,  `hb_defs.json` file to monitor specific registers for heartbeat signals. When it detects a heartbeat, it prints the value and transaction details to the command line.


**Usage Scenario:**

1. **Before Running the Script:**
   - Ensure that a tcpdump capture is completed, capturing the network traffic of interest, specifically targeting Modbus TCP communications.
   
2. **Running the Script:**
   - Execute the script after the tcpdump capture is complete. The script analyzes the pcap file, identifies Modbus queries and responses, detects missing responses, and tracks client-server reconnections.

3. **After Running the Script:**
   - Review the FIMS output messages for a summary of the Modbus communication health and specific details.
   - Check the command-line output for a detailed list of client-server pairs and heartbeat detection logs.

**Intended Automation:**

- This script is intended to be run automatically after a tcpdump capture completes, especially following a watchdog failure detection. A separate script should initiate the tcpdump capture, and once the capture is complete, this Python system should automatically analyze the capture to provide insights into system behavior and network health.

### Conclusion:

The Python Modbus TCPdump Analyzer is a targeted tool for integration and commissioning engineers who need to understand the behavior and health of Modbus TCP communications. By automating the analysis of tcpdump captures, it provides timely insights into possible issues, ensures the reliability of Modbus communications, and helps maintain system uptime and performance.
