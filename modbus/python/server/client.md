To create a Python Modbus client using the same `config.json` file, you can follow these steps:

1. Install `pymodbus` package.
2. Create a Python script to read the `config.json` file and set up the Modbus client.

Here is an example Python script that you can use as a starting point:

```python
import json
from pymodbus.client.sync import ModbusTcpClient

# Load configuration from JSON file
with open('config.json', 'r') as f:
    config = json.load(f)

# Configure Modbus client
client = ModbusTcpClient(config['host'], port=config['port'])

# Connect to the server
client.connect()

# Read registers based on the configuration
for register in config['registers']:
    address = register['address']
    count = register['count']
    
    if register['type'] == 'coil':
        response = client.read_coils(address, count)
    elif register['type'] == 'discrete_input':
        response = client.read_discrete_inputs(address, count)
    elif register['type'] == 'input_register':
        response = client.read_input_registers(address, count)
    elif register['type'] == 'holding_register':
        response = client.read_holding_registers(address, count)
    
    print(response.registers)

# Close the connection
client.close()
```

In this example, the `config.json` file should have the following format:

```json
{
  "host": "localhost",
  "port": 502,
  "registers": [
    {
      "type": "coil",
       "map": [
        {"id":"coil1","offset":10,"size":1}
        {"id":"coil2","offset":11,"size":1}
        {"id":"coil3","offset":12,"size":1}
        {"id":"coil4","offset":13,"size":1}
       ]
    },
    {
      "type": "input_register",
       "map": [
        {"id":"ip1","offset":110,"size":1}
        {"id":"ip2","offset":111,"size":2}
        {"id":"ip3","offset":113,"size":4}
        {"id":"ip4","offset":117,"size":1}
       ]
    }
  ]
}
```

Modify the `config.json` file according to your needs. This example reads 10 coils starting from address 0 and 2 input registers starting from address 0.

Note: This is a simple example and doesn't include any error handling. You should add appropriate error handling for your use case.