import json
from pymodbus.client.sync import ModbusTcpClient

# Load configuration from JSON file
with open('client.json', 'r') as f:
    config = json.load(f)

# Configure Modbus client
client = ModbusTcpClient(config['host'], port=config['port'])

# Connect to the server
client.connect()

# Read registers based on the configuration
registers = config['registers']

for register in registers:
    register_type = register['type']
    register_map = register['map']
    
    for reg in register_map:
        address = reg['offset']
        count = reg['size']
        
        if register_type == 'coil':
            response = client.read_coils(address, count)
        elif register_type == 'input_register':
            response = client.read_input_registers(address, count)
        
        print(f"{reg['id']}: {response.registers}")

# Close the connection
client.close()
