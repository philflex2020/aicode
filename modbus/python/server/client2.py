import json
from pymodbus.client.sync import ModbusTcpClient

# Load configuration from json file
with open('client_config.json') as f:
    config = json.load(f)

host = config['host']
port = config['port']
registers = config['registers']

client = ModbusTcpClient(host, port)
client.connect()
for reg in registers:
    reg_type = reg['type']
    for item in reg['map']:
        offset = item['offset']
        size = item['size']
        if reg_type == 'coil':
            response = client.read_coils(offset, size)
            print(f"{item['id']}: {response.bits}")
        elif reg_type == 'input_register':
            response = client.read_input_registers(offset, size)
            print(f"{item['id']}: {response.registers}")

client.close()
