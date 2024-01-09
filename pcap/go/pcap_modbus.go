package main

import (
    "fmt"
    "log"
    "os"
    "strconv"
    "strings"
	"time"
	"path/filepath"
	"encoding/json"
	"io/ioutil"
	"encoding/binary"
    "reflect"
	"bytes"


    "github.com/google/gopacket"
    "github.com/google/gopacket/layers"
    "github.com/google/gopacket/pcap"
)
// ./pcap_modbus ../data/ess_capture.pcap "502,1024"
// go build pcap_modbus.go
// Define a struct to hold our connection data
type ConnectionData struct {
    HostIP        string
    TargetIP      string
    TargetPort    string
	Connections   int
    TransactionID []uint16
	QueryCount    int                 // Count of queries issued
    ResponseCount int                 // Count of responses received
    QueryTimes    map[uint16]time.Time  // Track time of each query by transaction ID
    ResponseTimes map[uint16]time.Time  // Track time of each response by transaction ID
	DuplicateResponses int            // Count of duplicate responses received
	TotalResponseTime time.Duration // Total of all response times
    MaxResponseTime   time.Duration // Maximum response time
    MinResponseTime   time.Duration // Minimum response time, initialized to a large value
	connectionDuration time.Duration 
}

// Define a struct to hold connection data and state
type ConnectionAttempt struct {
    ClientIP      string
    ServerIP      string
    ServerPort    uint16
    StartTime     time.Time
    AckTime       time.Time
    State         string
	Connections   int
	connectionDuration time.Duration 

	QueryCount    int                 // Count of queries issued
    ResponseCount int                 // Count of responses received
    QueryTimes    map[uint16]time.Time  // Track time of each query by transaction ID
    ResponseTimes map[uint16]time.Time  // Track time of each response by transaction ID
	DuplicateResponses int            // Count of duplicate responses received
	TotalResponseTime time.Duration // Total of all response times
    MaxResponseTime   time.Duration // Maximum response time
    MinResponseTime   time.Duration // Minimum response time, initialized to a large value

}

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

// Map to track ongoing connection attempts
var connectionAttempts = make(map[string]*ConnectionAttempt)
    // Prepare a map to hold our connections
var  connections = make(map[string]*ConnectionData)
var allResults []ConnectionResult

// getPayload dynamically extracts a value from a payload at a given offset.
// The value's type is determined by the type of the 'output' argument.
func autoPayload(payload []byte, offset int, output interface{}) error {
    if offset < 0 || offset+binary.Size(output) > len(payload) {
        return fmt.Errorf("offset out of range")
    }

    outputValue := reflect.ValueOf(output)
    if outputValue.Kind() != reflect.Ptr {
        return fmt.Errorf("output must be a pointer")
    }

    outputElem := outputValue.Elem()
    if !outputElem.CanSet() {
        return fmt.Errorf("cannot set output value")
    }

    data := payload[offset : offset+binary.Size(output)]
    reader := bytes.NewReader(data)
    return binary.Read(reader, binary.BigEndian, output)
}
//func getPayload
func getPayload(payload []byte, offset int) (uint16, error) {
    if offset < 0 || offset+2 > len(payload) {
        return 0, fmt.Errorf("offset out of range")
    }

    // Read an int16 value from the payload starting at the given offset
    var value uint16
	value = uint16(payload[0])<<8 + uint16(payload[1])
    // reader := bytes.NewReader(payload[offset:])
    // err := binary.Read(reader, binary.BigEndian, &value)
    // if err != nil {
    //     return 0, err
    // }

    return value, nil
}

func main() {
    if len(os.Args) != 3 {
        log.Fatalf("Usage: %s <pcap file> <port list>", os.Args[0])
    }
	// Extract base filename without extension
	baseFilename := strings.TrimSuffix(os.Args[1], filepath.Ext(os.Args[1]))

	// Create JSON filename
	jsonFilename := baseFilename + ".json"


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
		// Check if either the source or destination TCP port matches our list
		srcPortInList := ports[uint16(tcp.SrcPort)]
		dstPortInList := ports[uint16(tcp.DstPort)]
	
		// If neither the source nor destination port is in the list, skip this packet
		if !srcPortInList && !dstPortInList {
			continue
		}
	
		var connectionKey string
		if dstPortInList {
			// If destination port is in the list
			connectionKey = fmt.Sprintf("%s->%s:%d", ip.SrcIP, ip.DstIP, tcp.DstPort)
		} else {
			// If source port is in the list
			connectionKey = fmt.Sprintf("%s->%s:%d", ip.DstIP, ip.SrcIP, tcp.SrcPort)
		}

		 // Check for SYN flag to detect connection initiation
		 if tcp.SYN && !tcp.ACK {
			// This is a new connection attempt
			connectionAttempts[connectionKey] = &ConnectionAttempt{
				ClientIP:   ip.SrcIP.String(),
				ServerIP:   ip.DstIP.String(),
				ServerPort: uint16(tcp.DstPort),
				StartTime:  packet.Metadata().Timestamp,
				State:      "Initiating",
				QueryTimes:    make(map[uint16]time.Time),
            	ResponseTimes: make(map[uint16]time.Time),
				DuplicateResponses: 0,
				Connections: 0,
			}
			continue
		}
	
		// Check for SYN-ACK flag indicating the server has acknowledged the connection attempt
		if tcp.SYN && tcp.ACK {
			if conn, ok := connectionAttempts[connectionKey]; ok && conn.State == "Initiating" {
				conn.State = "Acknowledged"
				// Update any other relevant details
			}
			continue
		}
	
		// Finally, check for ACK flag indicating the connection is fully established
		if tcp.ACK {
			if conn, ok := connectionAttempts[connectionKey]; ok && conn.State == "Acknowledged" {
				conn.State = "Established"
				//conn.AckTime = packet.Metadata().Timestamp
				conn.AckTime = packet.Metadata().Timestamp.UTC() // Convert to UTC

				// Calculate connection time
				conn.connectionDuration = conn.AckTime.Sub(conn.StartTime)
				conn.Connections += 1
				fmt.Printf("Connection from %s to %s:%d established at %s, took %s\n",
				            conn.ClientIP, conn.ServerIP, conn.ServerPort, conn.AckTime, conn.connectionDuration)
				//newConnect =  true
			}
		}

        // At this point, we assume it might be a Modbus packet
        // A typical Modbus TCP/IP packet starts with a Transaction ID in the first two bytes of the payload
        payload := tcp.LayerPayload()
        if len(payload) < 2 {
            continue // Not enough data for it to be a valid Modbus packet
        }
       
        // Extract Transaction ID (First 2 bytes of payload in Modbus TCP/IP)
        transactionID := uint16(payload[0])<<8 + uint16(payload[1])
		// var transId uint16
		// autoPayload(payload, 0, &transId)
		// fmt.Printf("transId %d transactionId %d \n", transId, transactionID)


        // // Generate a unique key for each connection
        // connectionKey := fmt.Sprintf("%s->%s:%d", ip.SrcIP, ip.DstIP, tcp.DstPort)

        // If we don't have this connection yet, create it
        conn, exists := connections[connectionKey];
		if  !exists {
            connections[connectionKey] = &ConnectionData{
                HostIP:        ip.SrcIP.String(),
                TargetIP:      ip.DstIP.String(),
                TargetPort:    tcp.DstPort.String(),
                TransactionID: make([]uint16, 0),
				Connections: 0,
				QueryTimes:    make(map[uint16]time.Time),
            	ResponseTimes: make(map[uint16]time.Time),
				DuplicateResponses: 0,
				ResponseCount: 0,
				QueryCount: 0,

            }
			conn = connections[connectionKey]
        }

        // Append the transaction ID to the connection data
		// Check if the packet is a query
		if dstPortInList {
			conn.QueryCount++
			conn.QueryTimes[transactionID] = packet.Metadata().Timestamp.UTC()
	
		}
	
		// Handle response packets
		if srcPortInList {
			if queryTime, queryExists := conn.QueryTimes[transactionID]; queryExists {
				if !queryTime.IsZero() {
					// This is the first response to the query
					responseTime := packet.Metadata().Timestamp.UTC()
					duration := responseTime.Sub(queryTime)
					// Update statistics
					conn.TotalResponseTime += duration
					conn.ResponseCount++
					// Check and update maximum response time
                	if conn.MaxResponseTime == time.Duration(0) || duration > conn.MaxResponseTime {
                    	conn.MaxResponseTime = duration
                	}

                	// Check and update minimum response time
                	if conn.MinResponseTime == time.Duration(0) || duration < conn.MinResponseTime {
                    	conn.MinResponseTime = duration
                	}
					//fmt.Printf("Transaction ID %d: Query to Response time: %s\n", transactionID, duration)
	
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
        //connections[connectionKey]
		conn.TransactionID = append(connections[connectionKey].TransactionID, transactionID)
        // if newConnect {
		// 	//connections[connectionKey
		// 	conn.Connections += 1
		// 	conn.connectionDuration =  connectionDuration
		// }
	}

    // Now print all connections with their transaction IDs
    // for _, conn := range connections {
    //     fmt.Printf("%s->%s:%s - Transaction IDs: %v\n", conn.HostIP, conn.TargetIP, conn.TargetPort, conn.TransactionID)
    // }
	for key, conn := range connections {
		var avgResponseTime time.Duration
		if conn.ResponseCount > 0 {
    		avgResponseTime = conn.TotalResponseTime / time.Duration(conn.ResponseCount)
		}
		conna, exists := connectionAttempts[key]

		if exists {
			conn.Connections = conna.Connections
			conn.connectionDuration = conna.connectionDuration
		}
        fmt.Printf("%s  \n",                         key) //conn.HostIP, conn.TargetIP, conn.TargetPort)
        fmt.Printf("     key: %s\n",                      key) //conn.HostIP, conn.TargetIP, conn.TargetPort)
		fmt.Printf("     Connections: %d\n",              conn.Connections)
		fmt.Printf("     Connection Duration: %s\n",      conn.connectionDuration)
		fmt.Printf("     Query Count: %d\n",              conn.QueryCount)
		fmt.Printf("     Response Count: %d\n",           conn.ResponseCount)
		fmt.Printf("     Duplicate Response Count: %d\n", conn.DuplicateResponses)
		fmt.Printf("     Average Response Time: %s\n",    avgResponseTime)
		fmt.Printf("     Maximum Response Time: %s\n",    conn.MaxResponseTime)
		fmt.Printf("     Minimum Response Time: %s\n",    conn.MinResponseTime)
		result := ConnectionResult{
			Key: key,
			Connections: conn.Connections,
			ConnectionDuration: conn.connectionDuration,
			QueryCount: conn.QueryCount,
			ResponseCount: conn.ResponseCount,
			DuplicateResponseCount: conn.DuplicateResponses,
			AverageResponseTime: avgResponseTime,
			MaximumResponseTime: conn.MaxResponseTime,
			MinimumResponseTime: conn.MinResponseTime,
	
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
}