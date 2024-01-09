To analyze a pcap (packet capture) file in Go, you'll typically use a library like `gopacket`, which is a Go library that provides utilities for packet processing. It's capable of parsing and handling packets from various sources including pcap files. Here's a simple example of how you might use `gopacket` to read and analyze a pcap file in Go:

### Step 1: Install the `gopacket` Library

First, you need to get the `gopacket` library. You can install it using go get:

```bash
go get github.com/google/gopacket
```

### Step 2: Write Go Code to Analyze pcap File

Here's a simple Go program that opens a pcap file, iterates through the packets in the file, and prints out some basic information about each packet.

```go
package main

import (
    "fmt"
    "log"

    "github.com/google/gopacket"
    "github.com/google/gopacket/pcap"
)

func main() {
    // Open file instead of device
    handle, err := pcap.OpenOffline("path_to_your_file.pcap")
    if err != nil {
        log.Fatal(err)
    }
    defer handle.Close()

    // Loop through packets in file
    packetSource := gopacket.NewPacketSource(handle, handle.LinkType())
    for packet := range packetSource.Packets() {
        // Print out packet data
        fmt.Println(packet)
    }
}
```

Replace `"path_to_your_file.pcap"` with the actual file path to your pcap file.

### What This Code Does:

- **OpenOffline**: Opens a pcap file.
- **NewPacketSource**: Creates a packet source from the handle.
- **Packets()**: Iterates over all the packets in the pcap file.
- **Println(packet)**: Prints out the raw packet data. You might see a lot of hexadecimal numbers representing the packet contents.

### Further Analysis:

`gopacket` allows for much more sophisticated analysis. For instance, you can parse the layers of each packet, inspect specific fields like IP addresses or ports, and so on. Here's a more advanced snippet showing how you might parse the network (IP) layer and transport (TCP/UDP) layer of each packet:

```go
for packet := range packetSource.Packets() {
    // Get the IP layer from this packet
    if ipLayer := packet.Layer(layers.LayerTypeIPv4); ipLayer != nil {
        fmt.Println("IPv4 Layer Detected.")
        ip, _ := ipLayer.(*layers.IPv4)

        // Print source and destination IP addresses
        fmt.Printf("From %s to %s\n", ip.SrcIP, ip.DstIP)
    }

    // Get the TCP layer from this packet
    if tcpLayer := packet.Layer(layers.Layer

TypeTCP); tcpLayer != nil {
        fmt.Println("TCP Layer Detected.")
        tcp, _ := tcpLayer.(*layers.TCP)

        // Print TCP source and destination ports
        fmt.Printf("From port %d to %d\n", tcp.SrcPort, tcp.DstPort)
    }

    // Similarly, you can add analysis for other protocols, like UDP, ICMP, etc.
}
```

### How to Run:

1. Save your Go code with a `.go` extension, for example, `pcap_analysis.go`.
2. Run your script using `go run pcap_analysis.go` in your terminal or command prompt.

### Notes:

- Ensure you have Go installed and configured correctly on your system.
- Make sure the pcap file path in the script is correct and accessible.
- The advanced example assumes IPv4 and TCP protocols for demonstration. Depending on the pcap file content, you might need to handle other protocols or layers.

By using `gopacket`, you can create powerful, customized pcap file analysis tools tailored to your specific needs, whether that's simply logging packet information or conducting deep inspection and statistical analysis of network traffic. The `gopacket` library is quite comprehensive, and further exploration of its documentation and examples will reveal much more about its capabilities and how you might leverage them.

To create a program `pcap_modbus.go` that processes a pcap file to find and list host-to-target IP and port pairs with specific ports and includes transaction IDs decoded from the Modbus payload, you'll need to use the `gopacket` library for packet processing and have some understanding of the Modbus protocol structure to decode it from the payload.

Modbus is a simple protocol often used in industrial environments. A typical Modbus TCP/IP packet includes a Transaction ID field which is what you seem to be interested in capturing. Here's how you might structure the program:

1. **Parse Arguments**: Accept a pcap file path and a list of ports as arguments.
2. **Open and Read pcap File**: Use `gopacket` to read through the pcap file.
3. **Filter and Process Packets**: Look for packets that match the specified ports and are likely to contain Modbus protocol data.
4. **Decode Modbus Data**: Extract the transaction ID from the Modbus payload.
5. **Store and Display Results**: Keep a record of all host/target pairs along with their transaction IDs and display them.

Here's a basic outline of what the code might look like:

### pcap_modbus.go

```go
package main

import (
    "fmt"
    "log"
    "os"
    "strconv"
    "strings"

    "github.com/google/gopacket"
    "github.com/google/gopacket/layers"
    "github.com/google/gopacket/pcap"
)

// Define a struct to hold our connection data
type ConnectionData struct {
    HostIP        string
    TargetIP      string
    TargetPort    string
    TransactionID []uint16
}

func main() {
    if len(os.Args) != 3 {
        log.Fatalf("Usage: %s <pcap file> <port list>", os.Args[0])
    }

    // Parse ports argument into a slice of integers
    portStrs := strings.Split(os.Args[2], ",")
    ports := make(map[uint16]bool)
    for _, p := range portStrs {
        port, err := strconv.Atoi(p)
        if err != nil {
            log.Fatalf("Invalid port %s: %s", p, err)
        }
        ports[uint16(port)] = true
    }

    // Open the pcap file
    handle, err := pcap.OpenOffline(os.Args[1])
    if err != nil {
        log.Fatal(err)
    }
    defer handle.Close()

    // Prepare a map to hold our connections
    connections := make(map[string]*ConnectionData)

    // Loop through packets in file
    packetSource := gopacket.NewPacketSource(handle, handle.LinkType())
    for packet := range packetSource.Packets() {
        // Process packet here
        // ...

        // Check if it's TCP and IPv4
        networkLayer := packet.NetworkLayer()
        transportLayer := packet.TransportLayer()
        if networkLayer == nil || transportLayer == nil || transportLayer.LayerType() != layers.LayerTypeTCP {
            continue
        }
        ip, _ := networkLayer.(*layers.IPv4)
        tcp, _ := transportLayer.(*layers.TCP)

        // Check if the TCP port matches our list
        if _, ok := ports[uint16(tcp.DstPort)]; !ok {
            continue
        }

        // At this point, we assume it might be a Modbus packet
        // A typical Modbus TCP/IP packet starts with a Transaction ID in the first two bytes of the payload
        payload := tcp.LayerPayload()
        if len(payload) < 2 {
            continue // Not enough data for it to be a valid Modbus packet
        }

        // Extract Transaction ID (First 2 bytes of payload in Modbus TCP/IP)
        transactionID := uint16(payload[0])<<8 + uint16(payload[1])

        // Generate a unique key for each connection
        connectionKey := fmt.Sprintf("%s->%s:%d", ip.SrcIP, ip.DstIP, tcp.DstPort)

        // If we don't have this connection yet, create it
        if _, exists := connections[connectionKey]; !exists {
            connections[connectionKey] = &ConnectionData{
                HostIP:        ip.SrcIP.String(),
                TargetIP:      ip.DstIP.String(),
                TargetPort:    tcp.DstPort.String(),
                TransactionID: make([]uint16, 0),
            }
        }

        // Append the transaction ID to the connection data
        connections[connectionKey].TransactionID = append(connections[connectionKey].TransactionID, transactionID)
    }

    // Now print all connections with their transaction IDs
    for _, conn := range connections {
        fmt.Printf("%s->%s:%s - Transaction IDs: %v\n", conn.HostIP, conn.TargetIP, conn.TargetPort, conn.TransactionID)
    }
}
```

### Explanation and Next Steps:

1. **Argument Parsing**: The program starts by ensuring it has the right number of arguments (the pcap file and a comma-separated list of ports). It then parses the port list into a map for easy checking.
2. **Opening the pcap File**: It uses `pcap.OpenOffline` to read the pcap file.
3. **Processing Packets**: As it iterates through packets, it filters for IPv4 and TCP layers and then checks whether the destination port matches one of our ports of interest.
4. **Decoding Modbus**: Assuming packets to the specified port could be Modbus packets, it tries to extract the Transaction ID from the first two bytes of the TCP payload. This is based on the Modbus TCP/IP specification where the Transaction ID is the first 2 bytes.
5. **Tracking Connections**: It maintains a map of connections, uniquely identified by the combination of source IP, destination IP, and destination port. Each connection tracks the series of Transaction IDs observed.
6. **Displaying Results**: Finally, it prints out the connections and their associated Transaction IDs.

### Important Notes:

- **Modbus Payload Assumption**: This code assumes that any packet sent to the specified ports will contain a Modbus message starting with a Transaction ID. In a real-world scenario, you would want to add more robust checks to ensure the packet is indeed a Modbus packet and handle cases where it might not be.
- **Error Handling and Robustness**: The provided example is a simplified demonstration. You would need to add comprehensive error handling, possibly more nuanced parsing based on Modbus specifics, and ensure it aligns with the exact Modbus TCP/IP specifications and your application requirements.
- **Performance and Scalability**: If you're processing large pcap files or need real-time analysis, consider performance optimizations and possibly using concurrent processing where appropriate.

Before using or adapting this code for any production or critical analysis, ensure it's thoroughly tested with representative data and scenarios, and fully accommodates all the Mod

bus protocol details and edge cases you expect to encounter in your specific use case. Additionally, be mindful of the security implications of parsing and handling potentially malicious or malformed packets. Ensure your implementation does not expose you to buffer overflows, denial of service, or other common vulnerabilities.

To build and run the `pcap_modbus.go` program, you'll need to have Go installed on your system and any necessary libraries, particularly `gopacket`. Here are the steps you would generally follow:

### Step 1: Install Go

If you haven't already, install Go from the [official Go website](https://golang.org/dl/). Follow the installation instructions for your specific operating system.

### Step 2: Install `gopacket`

You will need the `gopacket` library to run the program. Install it using the following command:

```bash
go get github.com/google/gopacket
```

### Step 3: Create Your Go File

1. Create a new file named `pcap_modbus.go`.
2. Paste the provided Go code into this file.

### Step 4: Build Your Program

Navigate to the directory containing your `pcap_modbus.go` file in the terminal or command prompt and use the following command to build your program:

```bash
go build pcap_modbus.go
```

This command compiles the Go source file and generates an executable file in the same directory.

### Step 5: Run Your Program

After building the program, run it by specifying the path to your pcap file and the list of ports. Ensure you have a `.pcap` file available to analyze and you know which ports you're interested in.

```bash
./pcap_modbus path_to_your_file.pcap "502,503,504"
```

Replace `path_to_your_file.pcap` with the actual file path to your pcap file and `"502,503,504"` with the actual list of ports you're interested in.

### Notes:

- **Permissions**: Make sure the `.pcap` file is accessible and you have the necessary permissions to read it.
- **Ports Argument**: The ports are expected to be a comma-separated list with no spaces, like `"502,503,504"`. Adjust this based on your actual ports of interest.
- **Error Handling**: If you encounter errors, ensure all dependencies are correctly installed, paths are correct, and the Go environment is properly set up.
- **Go Path**: If you're having trouble with Go finding your packages, ensure your GOPATH is set correctly and that your workspace is structured according to Go's expected format.

By following these steps, you should be able to build and run your `pcap_modbus.go` program to analyze pcap files and extract Modbus-related data based on specified ports. Make sure to adapt and test the code according to the specifics of your project and data.

Detecting attempts to connect from a client host to a server port and determining the time taken for the connection can be quite involved, especially if you need to work directly with packet data from a pcap file. You would typically look for TCP SYN (connection request), SYN-ACK (connection acknowledgment), and ACK (final acknowledgment) packets to track the entire TCP three-way handshake process, which establishes a connection.

Here's a high-level approach to how you might modify the Go program to detect connection attempts and calculate connection times:

### Step 1: Track Connection States

You need to keep track of the state of each connection attempt. A simple way might be to create a struct that represents each connection and stores:

- Client IP and port
- Server IP and port
- Timestamps for significant packets (SYN, SYN-ACK, ACK)
- Connection state (e.g., initiating, acknowledged, established)

### Step 2: Process Each Packet

As you iterate through packets, determine if they are part of a new or existing connection attempt based on the TCP flags and the IPs/ports. Update the connection state and timestamps accordingly.

### Step 3: Calculate Connection Times

Once a connection is fully established (after the ACK in the three-way handshake), calculate the time taken for the connection by subtracting the timestamp of the initial SYN packet from the timestamp of the final ACK packet.

### Here's how you might start coding this:

```go
// Define a struct to hold connection data and state
type ConnectionAttempt struct {
    ClientIP      string
    ServerIP      string
    ServerPort    uint16
    StartTime     time.Time
    AckTime       time.Time
    State         string
}

// Map to track ongoing connection attempts
var connections = make(map[string]*ConnectionAttempt)

// ...in your packet processing loop...

for packet := range packetSource.Packets() {
    // Assume you've already filtered IPv4 and TCP packets and have ip and tcp variables

    // Generate a unique identifier for the connection
    connectionID := fmt.Sprintf("%s:%d->%s:%d", ip.SrcIP, tcp.SrcPort, ip.DstIP, tcp.DstPort)

    // Check for SYN flag to detect connection initiation
    if tcp.SYN && !tcp.ACK {
        // This is a new connection attempt
        connections[connectionID] = &ConnectionAttempt{
            ClientIP:   ip.SrcIP.String(),
            ServerIP:   ip.DstIP.String(),
            ServerPort: uint16(tcp.DstPort),
            StartTime:  packet.Metadata().Timestamp,
            State:      "Initiating",
        }
        continue
    }

    // Check for SYN-ACK flag indicating the server has acknowledged the connection attempt
    if tcp.SYN && tcp.ACK {
        if conn, ok := connections[connectionID]; ok && conn.State == "Initiating" {
            conn.State = "Acknowledged"
            // Update any other relevant details
        }
        continue
    }

    // Finally, check for ACK flag indicating the connection is fully established
    if tcp.ACK {
        if conn, ok := connections[connectionID]; ok && conn.State == "Acknowledged" {
            conn.State = "Established"
            conn.AckTime = packet.Metadata().Timestamp

            // Calculate connection time
            connectionDuration := conn.AckTime.Sub(conn.StartTime)
            fmt.Printf("Connection from %s to %s:%d established in %s\n", conn.ClientIP, conn.ServerIP, conn.ServerPort, connectionDuration)
        }
    }

    // ... handle disconnection or other states as necessary ...
}
```

### Notes:

- **Accuracy**: The actual time taken for connection setup might be slightly shorter than measured due to processing delays in capturing and handling the packets. For more accurate timing, you might need to consider the specific hardware and software setup used for packet capture.
- **Error Handling and Edge Cases**: This is a simplified example. A robust implementation should handle various edge cases and possible errors, such as packets arriving out of order, connection attempts that never complete, and packet loss.
- **Cleanup**: The map tracking connection attempts should be periodically cleaned up to remove old or completed connections to prevent it from growing indefinitely.

By enhancing the program with this kind of connection tracking and timing logic, you can analyze pcap data to detect connection attempts, monitor their progress, and calculate the time taken for connections to be established. As always, thoroughly test and validate your implementation with a variety of data to ensure it works correctly and efficiently.

To ensure that the time of the connection is displayed in UTC, you will need to make sure that the timestamps are interpreted or converted to UTC. In Go, the `time` package provides functionalities to manage and format time. When you capture timestamps from packets in the pcap file, they are usually in the form of `time.Time` type, which can be converted to UTC using the `UTC()` method.

In the example provided earlier, ensure that when you capture the start time (`StartTime`) and acknowledgment time (`AckTime`) of the connection, you convert these times to UTC. Here's how you might adjust the relevant sections:

### Modified Connection Time Calculation:

```go
// ...inside your packet processing loop...

// Finally, check for ACK flag indicating the connection is fully established
if tcp.ACK {
    if conn, ok := connections[connectionID]; ok && conn.State == "Acknowledged" {
        conn.State = "Established"
        conn.AckTime = packet.Metadata().Timestamp.UTC() // Convert to UTC

        // Calculate connection time
        connectionDuration := conn.AckTime.Sub(conn.StartTime)
        fmt.Printf("Connection from %s to %s:%d established at %s UTC, took %s\n",
                   conn.ClientIP, conn.ServerIP, conn.ServerPort, conn.AckTime, connectionDuration)
    }
}

// ... rest of your code ...
```

### Notes:

- **UTC Conversion**: The `.UTC()` method is called on `packet.Metadata().Timestamp` to ensure that the times are in Coordinated Universal Time (UTC).
- **Displaying Time**: The `fmt.Printf` statement now includes both the UTC time when the connection was established and the duration it took for the connection to be established.

Ensure that your packet's metadata does indeed contain the correct timestamp data. The `gopacket` library typically captures this information from the pcap data, but the accuracy might depend on the precision of the timestamp data in the pcap file and the system that generated it.

By converting and handling all times in UTC, you can maintain consistency, especially if you're correlating events from different systems or locations, or if you're presenting the data to an audience across different time zones. This approach helps ensure that the times are accurate and standardized.


To modify the program to count queries issued to destination ports in the ports list and responses received from source ports in the ports list, and to measure the time between a query and a response for the same transaction ID, you'll need to enhance the connection tracking and state management.

Here's how you can approach these enhancements:

1. **Track Queries and Responses**: Add counters to the `ConnectionAttempt` struct for tracking queries and responses.
2. **Identify Queries and Responses**: Determine if a packet is a query or a response based on whether the destination port or source port, respectively, is in the list of ports.
3. **Match Responses to Queries**: For measuring time, you need to match responses to the initial queries based on the transaction ID. You'll need to keep track of when each query was sent and then calculate the duration when the corresponding response is received.

### Modifying the ConnectionAttempt Structure:

First, modify the `ConnectionAttempt` struct to include counters and timing information:

```go
type ConnectionAttempt struct {
    ClientIP      string
    ServerIP      string
    ServerPort    uint16
    StartTime     time.Time
    AckTime       time.Time
    State         string
    QueryCount    int                 // Count of queries issued
    ResponseCount int                 // Count of responses received
    QueryTimes    map[uint16]time.Time  // Track time of each query by transaction ID
    ResponseTimes map[uint16]time.Time  // Track time of each response by transaction ID
    DuplicateResponses int            // Count of duplicate responses received


}
```

### Modifying the Packet Processing Loop:

Next, modify the packet processing loop to include logic for tracking queries, responses, and timing:

```go
for packet := range packetSource.Packets() {
    // ... existing packet filtering ...

    // Check if it's a Modbus packet by looking at length, protocol, etc.
    // Extract the Modbus transaction ID and determine if it's a query or response
    transactionID := uint16(payload[0])<<8 + uint16(payload[1])

    // Update connection attempt or create new if necessary
    conn, exists := connections[connectionID]
    if !exists {
        conn = &ConnectionAttempt{
            ClientIP:   ip.SrcIP.String(),
            ServerIP:   ip.DstIP.String(),
            ServerPort: uint16(tcp.DstPort),
            QueryTimes:    make(map[uint16]time.Time),
            ResponseTimes: make(map[uint16]time.Time),
        }
        connections[connectionID] = conn
    }

    // Check if the packet is a query
    if dstPortInList {
        conn.QueryCount++
        conn.QueryTimes[transactionID] = packet.Metadata().Timestamp.UTC()
    }

    // Check if the packet is a response
    if srcPortInList {
        conn.ResponseCount++
        conn.ResponseTimes[transactionID] = packet.Metadata().Timestamp.UTC()

        // If there's a matching query, calculate the time difference
        if queryTime, ok := conn.QueryTimes[transactionID]; ok {
            duration := conn.ResponseTimes[transactionID].Sub(queryTime)
            fmt.Printf("Transaction ID %d: Query to Response time: %s\n", transactionID, duration)
        }
    }

    // ... rest of your code ...
}
```

### Explanation:

- **Query and Response Counters**: Each connection attempt object now includes counters for queries and responses. These are incremented based on whether the destination port (for queries) or the source port (for responses) is in the list.
- **Tracking Query and Response Times**: Maps are used to track the times of individual queries and responses keyed by the transaction ID. When a response is received, the program looks for the corresponding query time and calculates the duration.

### Notes:

- **Accuracy and Edge Cases**: This simplified example assumes that every Modbus packet is either a query or a response and that transaction IDs are unique and matched correctly. In a real-world scenario, you may need to handle retransmissions, missing packets, or duplicate transaction IDs, among other issues.
- **Performance**: Tracking individual query and response times for many connections and transaction IDs can consume significant memory and processing time, especially for high-volume or long-running captures. Make sure to consider performance implications and add appropriate cleanup or limits as necessary.

By adding these enhancements, your program will now track and count Modbus queries and responses and calculate the time between queries and responses for the same transaction ID, providing valuable insights into the timing and flow of Modbus communications. Make sure to thoroughly test and validate your program with representative data and scenarios to ensure it works correctly and efficiently.
To modify the program such that once a response is received, the corresponding query time is set to zero, and if another response comes in for the same transaction ID, a duplicate response counter is incremented, you'll need to adjust the `ConnectionAttempt` struct and packet processing logic. Essentially, you're adding logic to handle the case of duplicate responses.

Here's how you can approach these modifications:

### Step 1: Modify the ConnectionAttempt Structure

Add a counter for duplicate responses in the `ConnectionAttempt` struct:

```go
type ConnectionAttempt struct {
    // ... existing fields ...
    DuplicateResponses int            // Count of duplicate responses received
}
```

### Step 2: Modify the Packet Processing Loop

Update the packet processing loop to set query times to zero after a response is received and to handle potential duplicate responses:

```go
for packet := range packetSource.Packets() {
    // ... existing packet filtering ...

    // Extract the Modbus transaction ID and determine if it's a query or response
    transactionID := uint16(payload[0])<<8 + uint16(payload[1])

    // Get or create the connection attempt object
    conn, exists := connections[connectionID]
    if !exists {
        conn = &ConnectionAttempt{
            // ... initialize fields ...
            QueryTimes:    make(map[uint16]time.Time),
            ResponseTimes: make(map[uint16]time.Time),
        }
        connections[connectionID] = conn
    }

    // Handle query packets
    if dstPortInList {
        // ... handle query ...
        conn.QueryTimes[transactionID] = packet.Metadata().Timestamp.UTC()
    }

    // Handle response packets
    if srcPortInList {
        if queryTime, queryExists := conn.QueryTimes[transactionID]; queryExists {
            if !queryTime.IsZero() {
                // This is the first response to the query
                responseTime := packet.Metadata().Timestamp.UTC()
                duration := responseTime.Sub(queryTime)
                fmt.Printf("Transaction ID %d: Query to Response time: %s\n", transactionID, duration)

                // Set the query time to zero to mark it as responded
                conn.QueryTimes[transactionID] = time.Time{}
            } else {
                // Query time is zero, this is a duplicate response
                conn.DuplicateResponses++
                fmt.Printf("Transaction ID %d: Duplicate response received\n", transactionID)
            }
        }

        // Track the response time regardless
        conn.ResponseTimes[transactionID] = packet.Metadata().Timestamp.UTC()
    }

    // ... rest of your code ...
}
```

### Explanation:

- **Query Handling**: When a query is issued (detected by destination port), the time is recorded. No changes here from the previous implementation.
- **Response Handling**: When a response is received (detected by source port), the program checks if it corresponds to a known query by looking up the transaction ID in `QueryTimes`.
    - If the query time is not zero, it's the first response to that query. The program calculates the duration, prints the timing information, and then sets the query time to zero to indicate that a response has been received.
    - If the query time is already zero, it means this transaction ID has already received a response, and thus, this is a duplicate. The program increments the `DuplicateResponses` counter and prints a message.

### Notes:

- **Time Zero and Default Values**: In Go, `time.Time{}` represents the zero value for a time, effectively "no time" or "time not set." This is used to mark queries that have been responded to.
- **Map Entry Existence**: It's important to check whether a transaction ID exists in the `QueryTimes` map before trying to access or modify it to avoid panics.
- **Cleanup and Performance**: If your program runs for a long time or handles many connections, consider adding logic to remove old or completed connection attempts from the `connections` map to prevent memory issues.

With these changes, your program will now track duplicate responses in addition to the original query/response pairs and durations, providing a more nuanced view of the Modbus communication and its reliability. As always, thoroughly test and validate your program with a variety of data and scenarios to ensure it behaves as expected.

To create a JSON file containing the results of your analysis and name the file based on the input pcap file, you'll need to perform a few steps:

1. **Extract the Base Filename**: Extract the base filename from the input pcap file path and change its extension to `.json`.
2. **Convert Data to JSON Format**: Convert your results into a JSON-compatible format.
3. **Write to JSON File**: Save the JSON data to the file.

Here's how you can modify your program to include these steps:

### Step 1: Extract the Base Filename and Create JSON Filename

Before the loop where you process packets, add code to create the JSON filename:

```go
// Extract base filename without extension
baseFilename := strings.TrimSuffix(os.Args[1], filepath.Ext(os.Args[1]))

// Create JSON filename
jsonFilename := baseFilename + ".json"
```

### Step 2: Create a Struct for JSON Output

Define a struct that matches the format of the data you want to output in JSON. Here's an example:

```go
type ConnectionResult struct {
    Key                     string        `json:"key"`
    Connections             int           `json:"connections"`
    ConnectionDuration      time.Duration `json:"connection_duration"`
    QueryCount              int           `json:"query_count"`
    ResponseCount           int           `json:"response_count"`
    DuplicateResponseCount  int           `json:"duplicate_response_count"`
    AverageResponseTime     time.Duration `json:"average_response_time"`
    MaximumResponseTime     time.Duration `json:"maximum_response_time"`
    MinimumResponseTime     time.Duration `json:"minimum_response_time"`
}
```

### Step 3: Convert Data to JSON and Write to File

At the end of your program, where you print the results, also populate the JSON struct and write it to a file:

```go
// Create a slice to hold all connection results
var allResults []ConnectionResult

for key, conn := range connections {
    // ... your existing code to calculate and print results ...

    // Append the result to the slice
    result := ConnectionResult{
        Key: key,
        // ... populate other fields ...
    }
    allResults = append(allResults, result)
}

// Convert the slice to JSON
jsonData, err := json.MarshalIndent(allResults, "", "    ")
if err != nil {
    log.Fatalf("Error converting results to JSON: %v", err)
}

// Write JSON data to file
err = ioutil.WriteFile(jsonFilename, jsonData, 0644)
if err != nil {
    log.Fatalf("Error writing JSON to file: %v", err)
}
```

### Explanation:

- **JSON Filename Creation**: The program takes the input filename, removes the `.pcap` extension, and appends `.json` to create the output filename.
- **Data Conversion**: Each connection's data is stored in a `ConnectionResult` struct, which is then appended to a slice. This allows for easy conversion to JSON.
- **Writing to File**: The `json.MarshalIndent` function converts the data into a nicely formatted JSON string, which is then written to a file with the same base name as the input file but with a `.json` extension.

With these modifications, your program will output the results of your analysis both to the console and to a JSON file, providing a convenient and structured way to review and share the data. Be sure to thoroughly test the code with your input data to ensure that the JSON output is correctly formatted and contains all the necessary information.


Converting the provided Python script to Go involves several steps, as Go handles networking, JSON parsing, and file operations differently than Python. Below is a Go implementation that covers the core functionalities of your Python script:

### Go Implementation:

```go
package main

import (
    "encoding/json"
    "flag"
    "fmt"
    "io/ioutil"
    "net"
    "os"
    "path/filepath"
    "strings"
    "time"
)

type Config struct {
    Heartbeats  []HeartbeatInfo
    IPs         map[string][]IPInfo
    IPAddrs     map[string]IPAddrInfo
    Interfaces  []string
}

type HeartbeatInfo struct {
    // Define fields...
}

type IPInfo struct {
    ConfigFile string `json:"config_file"`
    DestIP     string `json:"dest_ip"`
    SrcIP      string `json:"src_ip"`
    DestPort   int    `json:"dest_port"`
    FileType   string `json:"file_type"`
    Reachable  bool   `json:"reachable"`
    Pingable   bool   `json:"pingable"`
    Device     string `json:"device"`
}

type IPAddrInfo struct {
    Ports []int `json:"ports"`
}

func ensureLogDir(logDir string) {
    if _, err := os.Stat(logDir); os.IsNotExist(err) {
        err := os.MkdirAll(logDir, 0755)
        if err != nil {
            fmt.Printf("Error: %v - Log directory (%s) could not be created.\n", err, logDir)
            os.Exit(1)
        }
    }
}

func isIPPingable(ip string) bool {
    // Simple ping check (not ICMP-based)
    _, err := net.DialTimeout("tcp", net.JoinHostPort(ip, "80"), 3*time.Second)
    return err == nil
}

func isIPReachable(ip string, port int) bool {
    // Check if an IP address is reachable by trying to connect to it
    conn, err := net.DialTimeout("tcp", net.JoinHostPort(ip, fmt.Sprint(port)), 3*time.Second)
    if err != nil {
        return false
    }
    conn.Close()
    return true
}

func scanConfigFiles(configDir string) Config {
    var configs Config
    configs.Heartbeats = []HeartbeatInfo{}
    configs.IPs = make(map[string][]IPInfo)
    configs.IPAddrs = make(map[string]IPAddrInfo)
    configs.Interfaces = []string{}

    // Implement the logic to scan and process JSON files in configDir
    // You will need to adjust this part based on the structure of your JSON files
    // and the specific details you want to extract.

    return configs
}

func main() {
    config := flag.String("config", "configs/modbus_client", "Directory containing the config files")
    outDir := flag.String("outdir", "/var/log/gcom", "Output dir to write the JSON results")
    outFile := flag.String("outfile", "pcap_monitor.json", "Output file to write the JSON results")
    flag.Parse()

    ensureLogDir(*outDir)

    extractedConfigs := scanConfigFiles(*config)

    // Convert extractedConfigs to JSON
    jsonOutput, err := json.MarshalIndent(extractedConfigs, "", "    ")
    if err != nil {
        fmt.Printf("Error converting results to JSON: %v\n", err)
        return
    }

    // Write to file
    filePath := filepath.Join(*outDir, *outFile)
    err = ioutil.WriteFile(filePath, jsonOutput, 0644)
    if err != nil {
        fmt.Printf("Error writing JSON to file: %v\n", err)
        return
    }

    fmt.Printf("Results written to %s\n", filePath)
}
```

### Notes:

1. **Networking Functions**: Go's standard library `net` is used for network operations. This example includes simple implementations for checking if an IP is reachable. Adjust these as needed for your specific requirements.

2. **JSON Processing**: Go's `encoding/json` package is used to handle JSON data. You'll need to define the structure of your JSON data with appropriate structs.

3. **Filepath and I/O**: The `path/filepath` and `io/ioutil` packages are used for file path manipulation and file I/O operations.

4. **Command-Line Arguments**: Go's `flag` package is used for parsing command-line arguments.

5. **Implementing Specific Logic**: The core logic for processing JSON files and extracting data should be implemented within the `scanConfigFiles` function. You'll need to adjust this part based on the specific structure of your JSON files and the details you want to extract.

Remember to thoroughly test this Go program and adjust it according to the specifics of your JSON files and networking environment.


To port the provided Python script to Go, we need to handle subprocess management, signal handling, and JSON parsing. Go's standard library provides the necessary tools for these tasks.

The script's primary function is to continuously start overlapping `tcpdump` processes for network packet capture and then trigger analysis scripts like `pcap_modbus.py`. Here's how you can achieve this in Go:

### Go Implementation:

```go
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"
    "bytes"
    "encoding/binary"
)

type Config struct {
	Interfaces []string `json:"interfaces"`
}

func main() {
	configFile := flag.String("config_file", "/var/log/gcom/pcap_monitor.json", "Configuration file containing the list of interfaces")
	logDir := flag.String("logdir", "/var/log/gcom", "Directory to store log files")
	flag.Parse()

	hostname, err := os.Hostname()
	if err != nil {
		fmt.Printf("Failed to get hostname: %v\n", err)
		return
	}
	fmt.Printf("Hostname: %s\n", hostname)

	config, err := loadConfig(*configFile)
	if err != nil {
		fmt.Printf("Failed to load config: %v\n", err)
		return
	}

	if len(config.Interfaces) == 0 {
		fmt.Println("No interfaces found in configuration file.")
		return
	}

	// Create log directory if it does not exist
	err = os.MkdirAll(*logDir, 0755)
	if err != nil {
		fmt.Printf("Failed to create log directory: %v\n", err)
		return
	}

	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)

	go func() {
		<-c
		fmt.Println("\nReceived termination signal. Exiting.")
		os.Exit(0)
	}()

	manageCaptures(hostname, config.Interfaces, *logDir)
}

func loadConfig(filePath string) (Config, error) {
	var config Config
	data, err := ioutil.ReadFile(filePath)
	if err != nil {
		return config, err
	}
	err = json.Unmarshal(data, &config)
	return config, err
}

func manageCaptures(hostname string, interfaces []string, logDir string) {
	overlap := 10 * time.Second
	duration := 30 * time.Second

	for _, intf := range interfaces {
		go startTcpdump(hostname, intf, logDir, duration, overlap)
	}

	select {}
}

func startTcpdump(hostname, intf, logDir string, duration, overlap time.Duration) {
	for {
		timestamp := time.Now().Format("20060102_150405")
		pcapFilename := filepath.Join(logDir, fmt.Sprintf("%s_%s_%s.pcap", hostname, intf, timestamp))
		logFilename := filepath.Join(logDir, fmt.Sprintf("%s_%s_%s.log", hostname, intf, timestamp))

		fmt.Printf("Starting tcpdump on %s, output file: %s\n", intf, pcapFilename)
		cmd := exec.Command("tcpdump", "-i", intf, "-w", pcapFilename)
		logFile, err := os.Create(logFilename)
		if err != nil {
			fmt.Printf("Failed to create log file: %v\n", err)
			continue
		}
		cmd.Stdout = logFile
		cmd.Stderr = logFile

		if err := cmd.Start(); err != nil {
			fmt.Printf("Failed to start tcpdump: %v\n", err)
			logFile.Close()
			continue
		}

		time.Sleep(duration - overlap)

		if err := cmd.Process.Signal(syscall.SIGINT); err != nil {
			fmt.Printf("Failed to send SIGINT to tcpdump: %v\n", err)
		}

		// Wait for tcpdump to finish
		cmd.Wait()
		logFile.Close()

		// You can add code here to call pcap_modbus or pcap_dnp3 if needed

		time.Sleep(overlap)
	}
}
```

### Explanation:

1. **Config Loading**: The configuration file is loaded using `ioutil.ReadFile` and `json.Unmarshal`.

2. **Signal Handling**: Signal handling is set up using `os/signal` to gracefully handle termination signals.

3. **Continuous Packet Capture**: The `startTcpdump` function is responsible for continuously running `tcpdump` in a loop with the specified overlap.

4. **Concurrency**: Each interface starts its own goroutine for parallel captures.

### Running the Script:

1. Build the Go program: `go build -o pcap_monitor`.
2. Execute the program: `./pcap_monitor`.

### Notes:

- This Go program mimics the logic of your Python script but may require adjustments depending on the specifics of your environment and the exact behavior you expect.
- The program assumes `tcpdump` is installed and
Error handling is simplified for brevity. Consider enhancing it based on your production needs.
The integration with pcap_modbus.py or pcap_dnp3.py is mentioned but not implemented. You can use exec.Command to run these scripts, passing the generated pcap filenames as arguments.
The Go program runs indefinitely. Implement additional logic if you want it to stop after a certain condition or time.
