import json
import tkinter as tk
from tkinter import ttk
from pymodbus.client.sync import ModbusTcpClient

class ModbusClientApp:

    def __init__(self, root):
        self.root = root
        self.root.title("Modbus Client")

        self.host_label = ttk.Label(root, text="Host")
        self.host_label.grid(column=0, row=0)
        self.host_entry = ttk.Entry(root)
        self.host_entry.grid(column=1, row=0)

        self.port_label = ttk.Label(root, text="Port")
        self.port_label.grid(column=0, row=1)
        self.port_entry = ttk.Entry(root)
        self.port_entry.grid(column=1, row=1)

        self.register_label = ttk.Label(root, text="Register ID")
        self.register_label.grid(column=0, row=2)
        self.register_entry = ttk.Entry(root)
        self.register_entry.grid(column=1, row=2)

        self.value_label = ttk.Label(root, text="Value")
        self.value_label.grid(column=0, row=3)
        self.value_entry = ttk.Entry(root)
        self.value_entry.grid(column=1, row=3)

        self.get_button = ttk.Button(root, text="Get", command=self.get_value)
        self.get_button.grid(column=0, row=4)
        self.set_button = ttk.Button(root, text="Set", command=self.set_value)
        self.set_button.grid(column=1, row=4)

        self.result_label = ttk.Label(root, text="")
        self.result_label.grid(column=0, row=5, columnspan=2)

    def get_value(self):
        host = self.host_entry.get()
        port = int(self.port_entry.get())
        register_id = self.register_entry.get()

        with open('config.json') as f:
            config = json.load(f)

        registers = config['registers']
        client = ModbusTcpClient(host, port)
        client.connect()

        for reg in registers:
            reg_type = reg['type']
            for item in reg['map']:
                if item['id'] == register_id:
                    offset = item['offset']
                    size = item['size']
                    if reg_type == 'coil':
                        response = client.read_coils(offset, size)
                        self.result_label.config(text=f"{register_id}: {response.bits}")
                    elif reg_type == 'input_register':
                        response = client.read_input_registers(offset, size)
                        self.result_label.config(text=f"{register_id}: {response.registers}")

        client.close()

    def set_value(self):
        host = self.host_entry.get()
        port = int(self.port_entry.get())
        register_id = self.register_entry.get()
        value = int(self.value_entry.get())

        with open('client_config.json') as f:
            config = json.load(f)

        registers = config['registers']
        client = ModbusTcpClient(host, port)
        client.connect()

        for reg in registers:
            reg_type = reg['type']
            for item in reg['map']:
                if item['id'] == register_id:
                    offset = item['offset']
                    if reg_type == 'coil':
                        client.write_coil(offset, value)
                        self.result_label.config(text=f"Set {register_id} to {value}")
                    else:
                        self.result_label.config(text="Cannot set input_register")

        client.close()

if __name__ == '__main__':
    root = tk.Tk()
    app = ModbusClientApp(root)
    root.mainloop()
