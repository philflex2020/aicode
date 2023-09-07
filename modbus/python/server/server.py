import json
from pymodbus.server.sync import StartTcpServer
from pymodbus.datastore import ModbusSparseDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext

# Load configuration from JSON file
with open('client.json', 'r') as f:
    config = json.load(f)

# Create data blocks for each type of register
coil_block = ModbusSparseDataBlock([0]*1000)
input_register_block = ModbusSparseDataBlock([0]*1000)

# Initialize the datastore with the configured values
registers = config['registers']

for register in registers:
    register_type = register['type']
    register_map = register['map']
    
    for reg in register_map:
        address = reg['offset']
        size = reg['size']
        
        if register_type == 'coil':
            coil_block.setValues(address, [0]*size)
        elif register_type == 'input_register':
            input_register_block.setValues(address, [0]*size)

# Create a Modbus slave context
store = ModbusSlaveContext(co=coil_block, ir=input_register_block)
context = ModbusServerContext(slaves=store, single=True)

# Start the Modbus server
StartTcpServer(context, address=(config['host'], config['port']))
