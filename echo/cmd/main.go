package main

import (
	"flag"
	"fmt"
	"os"
	"log"
	"time"

	"github.com/sysdcs/echo/pkg/config"

)

// Variables used for command line flags
var mode string
var scfg string
var outputpath string
var ipaddress string

// Structures for echo json and processMap structure
type Config struct {
	Outputs []Output `json:"outputs"`
}

type Output struct {
	Uri         string                 `json:"uri"`
	PublishRate float64                `json:"publishRate"`
	Heartbeat   string                 `json:"heartbeat,omitempty"`
	Inputs      []Input                `json:"inputs"`
	Echo        map[string]interface{} `json:"echo"`
}

type Input struct {
	Uri       string                 `json:"uri"`
	Registers map[string]interface{} `json:"registers"`
}

var cfg Config
var outputTicker *time.Ticker
var pubTicker = make(chan string)
var firsttime bool = true
var uris []string

var inputMap map[string]map[string]interface{}
var outputMap map[string]interface{}
var start = time.Now()

func main() {

	//cfgpath := os.Args[len(os.Args)-1]

        log.SetPrefix("Echo_test: ")

	fmt.Printf("Running args %d\n",len(os.Args))

	// Handle initial flag inputs for cli commands
	flag.StringVar(&mode, "mode", "", "set mode: [modbus | dnp3]")
	flag.StringVar(&scfg, "c", "", "client config file path for server echo generation only")
	flag.StringVar(&outputpath, "output", "", "this is the file path where the server and echo file will be going")
	flag.StringVar(&ipaddress, "ip", "0.0.0.0", "address used in the server files for client server connection")
	flag.Parse()

	if len(os.Args) == 1 {
		flag.PrintDefaults()
		log.Fatalf("No arguments provided.")
	}

	if scfg != "" {
		// Generate a server.json, and a echo.json
		// Generate our files, retrieve our client data
		c, err := config.GenerateClientAndServerFile(mode, scfg, outputpath, ipaddress)
		if err != nil {
			fmt.Printf("error generating client server file: %v\n", err)
			log.Fatalf("error generating client server file: %v\n", err)
			return
		}

		// Generate an echo json file for the given client
		if err := c.GenerateEchoJSONFile(scfg, outputpath); err != nil {
			fmt.Printf("error generating echo json file: %v\n", err)
			log.Fatalf("error generating echo json file: %v\n", err)
			return
		}
		fmt.Printf("ok generated client server files \n")

	}
}
