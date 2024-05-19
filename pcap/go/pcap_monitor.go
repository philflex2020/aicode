package main

import (
	//"encoding/json"
	"flag"
	"fmt"
	//"io/ioutil"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"
	"strings"
    // "bytes"
    // "encoding/binary"
)

type Config struct {
	Interfaces []string `json:"interfaces"`
}

func main() {
	//configFile := flag.String("c", "/var/log/gcom/pcap_monitor.json", "Configuration file containing the list of interfaces")
    logDir := flag.String("l", "/var/log/gcom", "Directory to store log files")
    logTime := flag.Int("t", 300, "Time for each file log files")

	// Define a custom help flag
	//help := flag.Bool("h", false, "Display this help message")
    // Rest of your main function...
	ifList := flag.String("i", "", "Comma-separated list of interfaces")

	flag.Parse()

	// if *help {
	// 	fmt.Printf("Usage of %s:\n", os.Args[0])
	// 	flag.PrintDefaults()
	// 	return
	// }
	
	// Validate arguments
    if *ifList == "" {
        flag.Usage()
		fmt.Printf(" For example :./pcap_monotor -i eth0,eth1  \n")
		fmt.Printf("              ./pcap_monotor -i eth0,eth1  -t 20 -l /var/logs/mydir \n")
        os.Exit(1)
    }

    //fmt.Printf("Config file: %s\n", *configFile)
    fmt.Printf("Log directory: %s\n", *logDir)
	//configFile := flag.String("config_file", "/var/log/gcom/pcap_monitor.json", "Configuration file containing the list of interfaces")
	//logDir = flag.String("logdir", "/var/log/gcom", "Directory to store log files")

	hostname, err := os.Hostname()
	if err != nil {
		fmt.Printf("Failed to get hostname: %v\n", err)
		return
	}

	fmt.Printf("Hostname: %s\n", hostname)
	
	if *ifList == "" {
		fmt.Printf("please provide an interface list\n")
		return
	}
	ifStrs := strings.Split(*ifList, ",")
	// config, err := loadConfig(*configFile)
	// if err != nil {
	// 	fmt.Printf("Failed to load config: %v\n", err)
	// 	return
	// }

	if len(ifStrs) == 0 {
		fmt.Println("No interfaces found on command line.")
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

	manageCaptures(hostname, ifStrs, *logDir, *logTime)
}

// func loadConfig(filePath string) (Config, error) {
// 	var config Config
// 	data, err := ioutil.ReadFile(filePath)
// 	if err != nil {
// 		return config, err
// 	}
// 	err = json.Unmarshal(data, &config)
// 	return config, err
// }

func manageCaptures(hostname string, interfaces []string, logDir string, logTime int) {
	overlap := 10 * time.Second
	duration := time.Duration(logTime * 60)

	for _, intf := range interfaces {
		if intf == "lo" {
			continue
		}
		go startTcpdump(hostname, intf, logDir, duration, overlap)
	}

	select {}
}

func startTcpdump(hostname, intf, logDir string, duration, overlap time.Duration) {
	for {
		timestamp := time.Now().Format("20060102_150405")
		hostname = "mything"
		logDir = "/tmp/pcap"
		pcapFilename := filepath.Join(logDir, fmt.Sprintf("%s_%s_%s.pcap", hostname, intf, timestamp))
		logFilename := filepath.Join(logDir, fmt.Sprintf("%s_%s_%s.log", hostname, intf, timestamp))

		fmt.Printf("Starting tcpdump on %s, output file: %s\n", intf, pcapFilename)
		//cmd := exec.Command("timeout", "60", "tcpdump", "-i", intf, "-w", pcapFilename)
		cmd := exec.Command("sh", "run_tcp.sh", "60", intf, pcapFilename)
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