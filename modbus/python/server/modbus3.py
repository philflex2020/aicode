import json
from pymodbus.client.sync import ModbusTcpClient

def main():
    with open('client_config.json') as f:
        config = json.load(f)

    host = config['host']
    port = config['port']
    registers = config['registers']

    client = ModbusTcpClient(host, port)
    client.connect()

    while True:
        operation = input("Operation (get/set/exit): ")
        if operation == 'exit':
            break
        register_id = input("Register ID: ")
        value = None
        if operation == 'set':
            value = int(input("Value: "))

        for reg in registers:
            reg_type = reg['type']
            for item in reg['map']:
                if item['id'] == register_id:
                    offset = item['offset']
                    size = item['size']
                    if operation == 'get':
                        if reg_type == 'coil':
                            response = client.read_coils(offset, size)
                            print(f"{register_id}: {response.bits}")
                        elif reg_type == 'input_register':
                            response = client.read_input_registers(offset, size)
                            print(f"{register_id}: {response.registers}")
                    elif operation == 'set':
                        if reg_type == 'coil':
                            client.write_coil(offset, value)
                            print(f"Set {register_id} to {value}")
                        else:
                            print("Cannot set input_register")
                    break

    client.close()

if __name__ == '__main__':
    main()
