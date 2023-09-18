// Client package implements generic client data retrieval
package config

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"strings"

	"github.com/buger/jsonparser"
)

type ArrHandler func([]byte, jsonparser.ValueType, int, error) //TODO: try to get rid of

type Layout struct {
	Outputs []*Output `json:"outputs"`
}

type Output struct {
	Uri         string                 `json:"uri"`
	PublishRate float64                `json:"publishRate"`
	Heartbeat   string                 `json:"heartbeat,omitempty"`
	Inputs      []Input                `json:"inputs"`
	Echo        map[string]interface{} `json:"echo,omitempty"`
}
type Input struct {
	Uri       string            `json:"uri"`
	Registers map[string]string `json:"registers"` // map key is the input (twins for example) app's reg id's
}

// Client interface implements methods for modbus and dnp file management
type ClientConfig interface {
	Validate() error
	SystemInfoCreation(ipaddress string) error
	CreateServerFile() ([]byte, error)
	GenerateEchoStruct() (Layout, error)
}

// Generic client struct for storing dnp or modbus specific data
type Client struct {
	cfgType ClientConfig // Holds modClient or dnpClient structs

}

func GenerateClientAndServerFile(mode string, cfgfile string, outputpath string, ipaddress string) (*Client, error) {
	// Create our client
	c := &Client{}

	// Getting and saving config into cache, and what kind of client you are using
	switch mode {
	case "dnp3", "dnp", "d":
		if err := c.getDNPConfig(cfgfile); err != nil {
			fmt.Errorf("error retrieving dnp3 config file: %v", err)
		}
	case "modbus", "mod", "m":
		if err := c.getModConfig(cfgfile); err != nil {
			fmt.Errorf("error retrieving modbus config file: %v", err)
		}
	default:
		fmt.Errorf("error determining mode. Pick [modbus|dnp3], use -h for help")
	}

	// Create system settings
	err := c.cfgType.SystemInfoCreation(ipaddress)
	if err != nil {
		fmt.Errorf("Error setting up server system info")
	}

	// Makes the server.json
	output, err := c.cfgType.CreateServerFile()
	if err != nil {
		fmt.Errorf("Error adjusting client register maps")
	}

	//Adding the server.json suffix
	strList := strings.Split(cfgfile, "/")
	name := strList[len(strList)-1]
	if outputpath == "" {
		if len(strList) > 1 {
			outputpath = cfgfile[:len(cfgfile)-len(name)-1]
		} else {
			outputpath = "."
		}
	}

	if strings.Contains(name, "client") {
		filename := strings.Replace(name, "client", "server", 1)
		outputpath = outputpath + "/" + filename
	} else {
		filename := name[:len(name)-5] + "_server.json"
		outputpath = outputpath + "/" + filename
	}

	// Output the data to the file
	err = ioutil.WriteFile(outputpath, output, 0644)
	if err != nil {
		fmt.Errorf("Error writing to output file")
	}
	return c, nil
}

// Function generates an Echo Json file for a given client
func (c *Client) GenerateEchoJSONFile(cfgfile string, outputpath string) error {
	cepts, err := c.cfgType.GenerateEchoStruct() //Generates the entire echo.json layout
	if err != nil {
		return fmt.Errorf("error creating Echo Layout: %v", err)
	}
	// Marshal up the echo.json
	ceptBytes, err := json.MarshalIndent(cepts, "", "\t")
	if err != nil {
		return fmt.Errorf("error getting interceptor bytes: %v", err)
	}

	// Rename the filename with echo.json suffix
	strList := strings.Split(cfgfile, "/")
	name := strList[len(strList)-1]
	if outputpath == "" {
		if len(strList) > 1 {
			outputpath = cfgfile[:len(cfgfile)-len(name)-1]
		} else {
			outputpath = "."
		}
	}

	if strings.Contains(name, "client") {
		filename := strings.Replace(name, "client", "echo", 1)
		outputpath = outputpath + "/" + filename
	} else {
		filename := name[:len(name)-5] + "_echo.json"
		outputpath = outputpath + "/" + filename
	}

	// Write out our echo.json file
	if err := ioutil.WriteFile(outputpath, ceptBytes, 0644); err != nil {
		return fmt.Errorf("error writing to output file: %v", err)
	}

	return nil
}

//Used only for generating order of server files
func MarshalOrderJSON(template map[string]interface{}, order []string) ([]byte, error) {
	var b []byte
	buf := bytes.NewBuffer(b)
	buf.WriteRune('{')
	l := len(order)
	for i, key := range order {
		km, err := json.Marshal(key)
		if err != nil {
			return nil, err
		}
		buf.WriteRune('\n')
		buf.WriteRune('\t')
		buf.Write(km)
		buf.WriteRune(':')
		vm, err := json.MarshalIndent(template[key], "\t", "\t")
		if err != nil {
			return nil, err
		}
		buf.Write(vm)
		if i != l-1 {
			buf.WriteRune(',')
		}
	}
	buf.WriteRune('\n')
	buf.WriteRune('}')
	return buf.Bytes(), nil
}

//Takes a map[string]interface{} and key of that map and outputs the []bytes of the value
func GetObjectBytes(v interface{}, name string) ([]byte, error) {

	// Get data in a byte slice format []byte
	b, err := json.MarshalIndent(v, "", "\t")
	if err != nil {
		return nil, fmt.Errorf("error retrieving server bytes: %v", err)
	}

	// Retrieve component level bytes
	bytes, _, _, err := jsonparser.Get(b, name)
	if err != nil {
		return nil, fmt.Errorf("error retrieving %s bytes: %v", name, err)
	}

	return bytes, nil
}

// Open our file and return data in form of bytes
func GetFileBytes(filename string) ([]byte, error) {

	// Open file
	file, err := os.Open(filename)
	if err != nil {
		return nil, fmt.Errorf("error opening our modbus client file: %v", err)
	}
	defer file.Close()

	// Retrieve byte format of file
	fileBytes, _ := ioutil.ReadAll(file)

	return fileBytes, nil
}

