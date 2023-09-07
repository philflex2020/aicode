Yes, you can create a simple Modbus server using Python on Windows that will read a JSON config file to set up registers and respond to a Modbus client.

You can use the `pymodbus` library to create the Modbus server. `pymodbus` is a full Modbus protocol implementation in Python. You can install it via pip:

```
pip install pymodbus
```

Here's a simple example of a Modbus server using `pymodbus` that reads a JSON config file to set up registers:

```python
import json
from pymodbus.server.sync import StartTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

def run_server():
    # Load JSON config file
    with open('config.json') as f:
        config = json.load(f)

    # Create data blocks from config
    store = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, config['di']),
        co=ModbusSequentialDataBlock(0, config['co']),
        hr=ModbusSequentialDataBlock(0, config['hr']),
        ir=ModbusSequentialDataBlock(0, config['ir']),
    )

    context = ModbusServerContext(slaves=store, single=True)

    # Start the Modbus server
    StartTcpServer(context, address=("localhost", 5020))

if __name__ == "__main__":
    run_server()
```

In this example, the `config.json` file should contain the initial values of the registers, for example:

```json
{
    "di": [0, 0, 0, 0],
    "co": [0, 0, 0, 0],
    "hr": [0, 0, 0, 0],
    "ir": [0, 0, 0, 0]
}
```

The server will use this configuration to initialize the Discrete Inputs (DI), Coils (CO), Holding Registers (HR), and Input Registers (IR).

Make sure to install the required libraries, and you may need to customize the code to suit your specific requirements.