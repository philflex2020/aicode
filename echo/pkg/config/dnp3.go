package config

import (
	"encoding/json"
	"fmt"
	"log"
	"net/url"
	"strings"
)

// Struct stores dnp specific client file data and server output data
type dnpClient struct {
	Config  map[string]interface{}
	Server  map[string]interface{}
	Handler ArrHandler
	Uris    []string
	Erm     map[string]interface{}
}

// Perform dnp3 client file validation here
func (dc *dnpClient) Validate() error {
	if dc == nil {
		return fmt.Errorf("dnpclient is nil")
	}

	if dc.Config == nil {
		return fmt.Errorf("dnpclient config is nil")
	}

	return nil
}

// Parse the input config file into dnp3 json format
func (c *Client) getDNPConfig(filename string) error {
	dc := dnpClient{}
	var err error

	// Get Cfg File data
	configBytes, err := GetFileBytes(filename)
	if err != nil {
		return fmt.Errorf("error retrieving client config data: %v", err)
	}
	json.Unmarshal(configBytes, &dc.Config)

	// Validate our data
	if err := dc.Validate(); err != nil {
		return fmt.Errorf("error validating modbus client data: %v", err)
	}

	// Set config to main client
	c.cfgType = &dc
	return nil
}

// Function fills the "system" object in our dnp3 server file
func (dc *dnpClient) SystemInfoCreation(ipaddress string) error {
	var systemInfo map[string]interface{} = make(map[string]interface{})
	var fileInfo map[string]interface{} = make(map[string]interface{})
	dc.Server = make(map[string]interface{})

	// Retrieve our system structure
	systemBytes, err := GetObjectBytes(dc.Config, "system")
	if err != nil {
		return fmt.Errorf("error retrieving system data: %v", err)
	}

	// Unmarshal our json data
	if err := json.Unmarshal(systemBytes, &systemInfo); err != nil {
		return fmt.Errorf("error comp unmarshalling object: %v", err)
	}

	if ipaddress != "0.0.0.0" { //default ip address is 0.0.0.0
		systemInfo["ip_address"] = ipaddress
	} else {
		systemInfo["ip_address"] = ipaddress
	}

	systemInfo["connection name"] = strings.Replace(systemInfo["connection name"].(string), " ", "_", -1)
	systemInfo["name"] = strings.Replace(systemInfo["name"].(string), " ", "_", -1)

	// Set our server values
	dc.Server["system"] = systemInfo
	dc.Server["fileInfo"] = fileInfo

	return nil
}

// Function iterates through our dnp client "registers" object and populates the dnp struct server data
func (dc *dnpClient) CreateServerFile() ([]byte, error) {
	var err error
	var serverRegInfo = make([]map[string]interface{}, 0)
	var echoReg url.Values = url.Values{}

	// Iterate through register map and then map map to get the type and register name
	for _, reg := range dc.Config["registers"].([]interface{}) {
		var v map[string]interface{}
		b, err1 := json.Marshal(reg)
		if err1 != nil {
			fmt.Errorf("could not marsh register: %v", err1)
		}
		json.Unmarshal(b, &v)
		var regType = make(map[string]interface{})

		regType["type"] = v["type"]

		for _, echo := range v["map"].([]interface{}) {
			if !findString(echo.(map[string]interface{})["uri"].(string), dc.Uris) { //Makes sure that all uris are unique
				dc.Uris = append(dc.Uris, echo.(map[string]interface{})["uri"].(string))
			}
			if echo.(map[string]interface{})["echo_id"] == nil { //Make sure that all registers that dont have echo_ids are being saved
				echoReg.Add(echo.(map[string]interface{})["uri"].(string), echo.(map[string]interface{})["id"].(string))
			}
			delete(echo.(map[string]interface{}), "echo_id")
		}

		regType["map"] = v["map"]
		serverRegInfo = append(serverRegInfo, regType)
	}

	// Set register array back to our server file
	dc.Server["registers"] = serverRegInfo

	// Order our server file by specific order
	output, err := MarshalOrderJSON(dc.Server, []string{"fileInfo", "system", "registers"})
	if err != nil {
		return nil, fmt.Errorf("error ordering our server file: %v", err)
	}

	// Save our echomap - transform into a map[string]interface{}
	m := make(map[string]interface{})
	for k, v := range echoReg {
		n := make(map[string]interface{})
		for _, v := range v {
			n[v] = 0
		}
		m[k] = n
	}
	dc.Erm = m //assign map to struct variable

	return output, nil
}

func (dc *dnpClient) GenerateEchoStruct() (Layout, error) {
	var layout Layout
	var err error

	// Generate the echo.json basically
	if layout.Outputs, err = GenerateDNPOutputs(dc.Uris, dc.Erm, dc.Config); err != nil {
		log.Fatalf("error generating outputs: %v", err)
	}

	return layout, nil
}

func GenerateDNPOutputs(opuris []string, emap map[string]interface{}, cfgs map[string]interface{}) ([]*Output, error) {
	var regmaps []interface{}
	var err error

	outputs := make([]*Output, len(opuris))

	// Initialize our outputs
	for i := range outputs {
		outputs[i] = new(Output)
	}

	// Populate outputs with output uris
	for _, op := range outputs {
		if cfgs["system"].(map[string]interface{})["frequency"] != nil {
			op.PublishRate = cfgs["system"].(map[string]interface{})["frequency"].(float64) // setting pubrate to file frequency
		} else {
			op.PublishRate = 100
		}
		op.Echo = make(map[string]interface{})
		ip := make([]Input, 0)
		op.Inputs = ip
	}
	for _, op := range outputs {
		for i := range cfgs["registers"].([]interface{}) {
			if cfgs["registers"].([]interface{})[i].(map[string]interface{})["heartbeat_enabled"] != nil {
				if cfgs["registers"].([]interface{})[i].(map[string]interface{})["heartbeat_enabled"].(bool) {
					op.Heartbeat = cfgs["registers"].([]interface{})[i].(map[string]interface{})["component_heartbeat_read_uri"].(string)
				}
			}
		}
	}

	outstruct := outputs

	// Populate our list of uris
	if len(outstruct) != len(opuris) {
		return nil, fmt.Errorf("error mismatch count of outputs and uris")
	}

	for i, op := range outstruct {
		op.Uri = opuris[i]
	}

	// Populate all our outputs with their corresponding echo maps
	for _, op := range outstruct {
		if emap[op.Uri] != nil {
			op.Echo = emap[op.Uri].(map[string]interface{})
		}
	}

	// Generate our input array for a given output
	for _, op := range outstruct {
		var ipm map[string]interface{} = make(map[string]interface{})
		for _, typename := range cfgs["registers"].([]interface{}) {
			regmaps = typename.(map[string]interface{})["map"].([]interface{})

			// Retrieve register map that ties to the specific op.Uri
			ipm, err = generateDNPInputMap(regmaps, ipm, op.Uri)
			if err != nil {
				return nil, fmt.Errorf("error generating input mapping for %s: %v", op.Uri, err)
			}
		}
		for key, v := range ipm {
			ip := Input{}
			ip.Registers = make(map[string]string)
			ip.Registers = v.(map[string]string)
			ip.Uri = key
			op.Inputs = append(op.Inputs, ip)
		}
	}
	return outputs, nil
}

// Function creates a map[input_uri]map[cid]eid for a given output uri
func generateDNPInputMap(regmaps []interface{}, ipm map[string]interface{}, uri string) (map[string]interface{}, error) {

	// Loop through the registers under a given regmap
	for _, reg := range regmaps {
		if reg.(map[string]interface{})["echo_id"] != nil && reg.(map[string]interface{})["uri"] == uri {
			var euri string
			// Retrieve our euri and eid
			strList := strings.Split(reg.(map[string]interface{})["echo_id"].(string), "/")
			for i := 1; i < len(strList)-1; i++ {
				euri = euri + "/" + strList[i]
			}
			echo_id := strList[len(strList)-1]

			// Check if we have a mapping for the euri yet
			if ipm[euri] == nil {
				ipm[euri] = make(map[string]string)
			}

			// Add a new entry for a given reg id = echo id
			ipm[euri].(map[string]string)[reg.(map[string]interface{})["id"].(string)] = echo_id
		}
	}
	return ipm, nil
}

func findString(target string, list []string) bool {
	for _, a := range list {
		if a == target {
			return true
		}
	}
	return false
}

